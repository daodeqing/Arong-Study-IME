#include "cloud/sentence_translation.h"
#include "ai/chat_completions.h"
#include "ai/chat_request_params.h"
#include "ipc/event_listener.h"
#include "ipc/output_language_policy.h"
#include "utils/common_utils.h"

#include <atomic>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
using Clock = std::chrono::steady_clock;
struct Cached
{
    std::string source;
    std::string translation;
    std::vector<SentenceVocabulary::Entry> vocabulary;
};
struct State
{
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool running = false;
    bool queued = false;
    std::atomic<std::uint64_t> revision{0};
    std::shared_ptr<const TranslationConfig> configured;
    AiTranslation::Config config;
    SentenceTranslation::Snapshot current;
    Clock::time_point ready_at;
    std::deque<Cached> cache;
    std::size_t cache_bytes = 0;
} state;

std::string Trim(std::string text)
{
    const auto nonspace = [](unsigned char c) { return !std::isspace(c); };
    const auto begin = std::find_if(text.begin(), text.end(), nonspace);
    const auto end = std::find_if(text.rbegin(), text.rend(), nonspace).base();
    return begin < end ? std::string(begin, end) : std::string{};
}
void Cache(const std::string &source, const std::string &translation)
{
    constexpr std::size_t maxEntries = 32, maxBytes = 64 * 1024;
    const auto bytes = source.size() + translation.size();
    if (bytes > maxBytes)
        return;
    while (!state.cache.empty() && (state.cache.size() >= maxEntries || state.cache_bytes + bytes > maxBytes))
    {
        state.cache_bytes -= state.cache.front().source.size() + state.cache.front().translation.size();
        state.cache.pop_front();
    }
    state.cache.push_back({source, translation, {}});
    state.cache_bytes += bytes;
}
void Loop()
{
    std::unique_ptr<SentenceVocabulary::Dictionary> dictionary;
    for (;;)
    {
        std::string source;
        AiTranslation::Config config;
        std::uint64_t revision;
        {
            std::unique_lock lock(state.mutex);
            state.wake.wait(lock, [] { return !state.running || state.queued; });
            if (!state.running)
                return;
            const auto due = state.ready_at;
            if (Clock::now() < due)
            {
                state.wake.wait_until(lock, due);
                continue;
            }
            source = state.current.source;
            config = state.config;
            revision = state.current.revision;
            state.queued = false;
        }
        const auto started = Clock::now();
        std::string translated;
        try
        {
            const auto content = ChatCompletions::FetchContent(
                config.endpoint, config.token, SentenceTranslation::detail::BuildRequest(config, source).dump(),
                120000L, [revision] { return state.revision.load(std::memory_order_acquire) != revision; });
            if (content)
                translated = SentenceTranslation::detail::ParseResult(*content);
        }
        catch (...)
        {
            // 请求正文与凭证只驻留内存，失败时也不打印服务返回或用户输入。
        }
        bool notify = false;
        {
            std::lock_guard lock(state.mutex);
            if (state.running && state.current.revision == revision &&
                state.revision.load(std::memory_order_acquire) == revision)
            {
                state.current.translation = std::move(translated);
                state.current.status = state.current.translation.empty() ? SentenceTranslation::Status::Failed
                                                                         : SentenceTranslation::Status::Ready;
                state.current.elapsed_ms = static_cast<long>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count());
                if (state.current.status == SentenceTranslation::Status::Ready)
                    Cache(state.current.source, state.current.translation);
                notify = true;
            }
        }
        if (notify)
        {
            FanyNamedPipe::EnqueueSentenceTranslationResultTask();
            // Publish the usable translation first. Local teaching never delays an exact commit.
            if (GetConfiguredSentenceVocabularyEnabled())
            {
                const auto snapshot = SentenceTranslation::Current();
                if (snapshot.revision == revision && snapshot.status == SentenceTranslation::Status::Ready)
                {
                    if (!dictionary)
                        dictionary = std::make_unique<SentenceVocabulary::Dictionary>(
                            std::filesystem::path(CommonUtils::get_ime_data_path_w()) / L"assets" / L"vocabulary" /
                            L"cet.json");
                    auto words = dictionary->Select(snapshot.translation);
                    {
                        std::lock_guard lock(state.mutex);
                        if (state.current.revision != revision || !state.running)
                            continue;
                        state.current.vocabulary = std::move(words);
                        for (auto &cached : state.cache)
                            if (cached.source == snapshot.source && cached.translation == snapshot.translation)
                                cached.vocabulary = state.current.vocabulary;
                    }
                    FanyNamedPipe::EnqueueRefreshCandidatePageTask();
                }
            }
        }
    }
}
} // namespace
namespace SentenceTranslation
{
namespace detail
{
std::string ResolvePrompt(const AiTranslation::Config &config)
{
    const std::string custom = Trim(config.prompt);
    if (!custom.empty())
        return custom;
    for (const PromptPreset &preset : BuiltinPromptPresets())
    {
        if (preset.id == config.prompt_id)
            return std::string(preset.prompt);
    }
    if (config.prompt_id == "custom_1" || config.prompt_id == "custom_2" || config.prompt_id == "custom_3")
        return std::string(kDailyPrompt);
    return std::string(kDailyPrompt);
}

nlohmann::json BuildRequest(const AiTranslation::Config &config, const std::string &source)
{
    nlohmann::json body = {
        {"model", config.model},
        {"stream", false},
        {"messages",
         {{{"role", "system"}, {"content", ResolvePrompt(config)}}, {{"role", "user"}, {"content", source}}}}};
    ChatCompletions::SetTokenLimit(body, config.provider, 2048);
    if (ChatCompletions::IsOpenAiReasoningRequest(config.provider, config.model))
        body["reasoning_effort"] = ChatCompletions::kOpenAiReasoningEffort;
    else
        body["temperature"] = 0.0;
    // Routed IDs such as cn/deepseek-v4.1-flash use the same DeepSeek option.
    std::string model = config.model;
    std::transform(model.begin(), model.end(), model.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (config.provider == "deepseek" || model.find("deepseek-") != std::string::npos)
        body["thinking"] = {{"type", "disabled"}};
    return body;
}
std::string ParseResult(const std::string &content)
{
    std::string result = Trim(content);
    if (!result.empty() &&
        (result.front() == '{' || result.front() == '[' || result.rfind(std::string(3, static_cast<char>(96)), 0) == 0))
    {
        const auto parsed = AiTranslation::ParseTranslations(result, 1);
        result = parsed.empty() ? std::string{} : parsed.front();
    }
    const bool latin = std::any_of(result.begin(), result.end(),
                                   [](unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); });
    return result.size() <= 4096 && latin && OutputLanguagePolicy::IsEnglishTranslation(result) ? result
                                                                                                : std::string{};
}
} // namespace detail
void Start()
{
    std::lock_guard lock(state.mutex);
    if (state.running)
        return;
    state.running = true;
    state.worker = std::thread(Loop);
}
void Cancel()
{
    std::lock_guard lock(state.mutex);
    const auto revision = state.revision.fetch_add(1, std::memory_order_acq_rel) + 1;
    state.queued = false;
    state.current = {};
    state.current.revision = revision;
    state.wake.notify_all();
}
void Stop()
{
    {
        std::lock_guard lock(state.mutex);
        state.running = false;
        state.queued = false;
        state.revision.fetch_add(1, std::memory_order_acq_rel);
        state.wake.notify_all();
    }
    if (state.worker.joinable())
        state.worker.join();
    Cancel();
    std::lock_guard lock(state.mutex);
    state.cache.clear();
    state.cache_bytes = 0;
    state.config = {};
    state.configured.reset();
}
void Request(std::string source, bool immediate, bool retry)
{
    const auto configured = GetConfiguredTranslation();
    std::lock_guard lock(state.mutex);
    // Config publication is immutable: avoid copying/decrypting configuration on each key.
    if (configured != state.configured)
    {
        state.config = AiTranslation::FromConfiguredTranslation(*configured);
        state.configured = configured;
        state.cache.clear();
        state.cache_bytes = 0;
        state.current = {};
    }
    if (source == state.current.source && state.current.status != Status::Idle &&
        !(retry && state.current.status == Status::Failed))
    {
        if (immediate && state.queued)
        {
            state.ready_at = Clock::now();
            state.wake.notify_all();
        }
        return;
    }
    const auto revision = state.revision.fetch_add(1, std::memory_order_acq_rel) + 1;
    state.current = {};
    state.current.source = std::move(source);
    state.current.revision = revision;
    state.queued = false;
    if (state.current.source.empty() || state.current.source.size() > 4096)
        state.current.status = Status::Idle;
    else if (!AiTranslation::IsUsableConfig(state.config))
        state.current.status = Status::Unconfigured;
    else
    {
        const auto hit = std::find_if(state.cache.begin(), state.cache.end(),
                                      [&](const auto &entry) { return entry.source == state.current.source; });
        if (hit != state.cache.end())
        {
            state.current.translation = hit->translation;
            state.current.vocabulary = hit->vocabulary;
            state.current.status = Status::Ready;
            state.current.cache_hit = true;
            auto entry = std::move(*hit);
            state.cache.erase(hit);
            state.cache.push_back(std::move(entry));
        }
        else if (state.running)
        {
            state.current.status = Status::Pending;
            state.ready_at = Clock::now() + std::chrono::milliseconds(immediate ? 0 : 200);
            state.queued = true;
        }
    }
    state.wake.notify_all();
}
Snapshot Current()
{
    const auto configured = GetConfiguredTranslation();
    std::lock_guard lock(state.mutex);
    return configured == state.configured ? state.current : Snapshot{};
}
} // namespace SentenceTranslation
