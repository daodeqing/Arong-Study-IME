#include "cloud/output_translation.h"

#include "cloud/ai_translation.h"
#include "config/ime_config.h"
#include "engine/common/helpcode_utils.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

namespace
{
constexpr size_t kMaxPendingBatches = 32;
constexpr size_t kMaxBatchSize = 12;
struct Job
{
    AiTranslation::Config config;
    std::vector<std::string> candidates;
};

struct WorkerState
{
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool running = false;
    std::deque<Job> batches;
    std::unordered_set<std::string> pending;
    OutputTranslation::TranslationCache cache;
};

WorkerState g_state;

void Remember(const AiTranslation::Config &config, const std::string &candidate, const std::string &translation)
{
    std::lock_guard lock(g_state.mutex);
    g_state.pending.erase(OutputTranslation::TranslationCache::Key(config.endpoint, config.model, candidate));
    g_state.cache.Put(config.endpoint, config.model, candidate, translation);
}

void Forget(const AiTranslation::Config &config, const std::string &candidate)
{
    std::lock_guard lock(g_state.mutex);
    g_state.pending.erase(OutputTranslation::TranslationCache::Key(config.endpoint, config.model, candidate));
}

void TranslateBatch(const Job &job)
{
    std::vector<std::string> translated;
    try
    {
        translated = AiTranslation::TextTranslateBatch(job.config, job.candidates, "zh", "en");
    }
    catch (...)
    {
        for (const auto &candidate : job.candidates)
            Forget(job.config, candidate);
        return;
    }
    for (size_t i = 0; i < job.candidates.size(); ++i)
    {
        if (i < translated.size() && !translated[i].empty() && HelpcodeUtils::count_han_chars(translated[i]) == 0)
            Remember(job.config, job.candidates[i], translated[i]);
        else
        {
            std::lock_guard lock(g_state.mutex);
            g_state.pending.erase(
                OutputTranslation::TranslationCache::Key(job.config.endpoint, job.config.model, job.candidates[i]));
        }
    }
}

void WorkerLoop()
{
    for (;;)
    {
        Job job;
        {
            std::unique_lock lock(g_state.mutex);
            g_state.wake.wait(lock, [] { return !g_state.running || !g_state.batches.empty(); });
            if (!g_state.running)
                return;
            job = std::move(g_state.batches.front());
            g_state.batches.pop_front();
        }
        TranslateBatch(job);
    }
}
} // namespace

namespace OutputTranslation
{
void Start()
{
    std::lock_guard lock(g_state.mutex);
    if (g_state.running)
        return;
    g_state.running = true;
    g_state.worker = std::thread(WorkerLoop);
}

void Stop()
{
    {
        std::lock_guard lock(g_state.mutex);
        if (!g_state.running)
            return;
        g_state.running = false;
        g_state.batches.clear();
        g_state.pending.clear();
    }
    g_state.wake.notify_all();
    if (g_state.worker.joinable())
        g_state.worker.join();
    std::lock_guard lock(g_state.mutex);
    g_state.cache.Clear();
}

void Request(std::vector<std::string> candidates)
{
    if (candidates.empty())
        return;
    const auto snapshot = GetConfiguredTranslation();
    const AiTranslation::Config config = AiTranslation::FromConfiguredTranslation(*snapshot);
    if (config.provider != "local" || !AiTranslation::IsUsableConfig(config))
        return;
    std::lock_guard lock(g_state.mutex);
    if (!g_state.running)
        return;
    std::vector<std::string> batch;
    for (auto &candidate : candidates)
    {
        const std::string key = TranslationCache::Key(config.endpoint, config.model, candidate);
        if (candidate.empty() || g_state.cache.Contains(config.endpoint, config.model, candidate) ||
            g_state.pending.find(key) != g_state.pending.end())
            continue;
        g_state.pending.insert(key);
        batch.push_back(std::move(candidate));
        if (batch.size() == kMaxBatchSize)
        {
            if (g_state.batches.size() < kMaxPendingBatches)
                g_state.batches.push_back({config, std::move(batch)});
            else
                for (const auto &item : batch)
                    g_state.pending.erase(TranslationCache::Key(config.endpoint, config.model, item));
            batch.clear();
        }
    }
    if (!batch.empty())
    {
        if (g_state.batches.size() < kMaxPendingBatches)
            g_state.batches.push_back({config, std::move(batch)});
        else
            for (const auto &item : batch)
                g_state.pending.erase(TranslationCache::Key(config.endpoint, config.model, item));
    }
    g_state.wake.notify_one();
}

std::optional<std::string> Lookup(const std::string &candidate)
{
    const auto snapshot = GetConfiguredTranslation();
    const AiTranslation::Config config = AiTranslation::FromConfiguredTranslation(*snapshot);
    if (config.provider != "local" || !AiTranslation::IsUsableConfig(config))
        return std::nullopt;
    return g_state.cache.Get(config.endpoint, config.model, candidate);
}
} // namespace OutputTranslation
