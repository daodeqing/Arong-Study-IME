#pragma once

#include <optional>
#include <deque>
#include <mutex>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace OutputTranslation
{
class TranslationCache
{
  public:
    explicit TranslationCache(std::size_t capacity = 512) : capacity_(capacity > 0 ? capacity : 1)
    {
    }

    static std::string Key(const std::string &endpoint, const std::string &model, const std::string &candidate)
    {
        return std::to_string(endpoint.size()) + ":" + endpoint + std::to_string(model.size()) + ":" + model +
               std::to_string(candidate.size()) + ":" + candidate;
    }

    void Put(const std::string &endpoint, const std::string &model, const std::string &candidate,
             const std::string &translation)
    {
        if (candidate.empty() || translation.empty() || candidate.size() > 4096 || translation.size() > 4096)
            return;
        const std::string key = Key(endpoint, model, candidate);
        std::lock_guard lock(mutex_);
        if (entries_.find(key) == entries_.end())
        {
            while (entries_.size() >= capacity_ && !insertion_order_.empty())
            {
                entries_.erase(insertion_order_.front());
                insertion_order_.pop_front();
            }
            insertion_order_.push_back(key);
        }
        entries_[key] = translation;
    }

    std::optional<std::string> Get(const std::string &endpoint, const std::string &model,
                                   const std::string &candidate) const
    {
        std::lock_guard lock(mutex_);
        const auto found = entries_.find(Key(endpoint, model, candidate));
        return found == entries_.end() ? std::nullopt : std::optional<std::string>(found->second);
    }

    bool Contains(const std::string &endpoint, const std::string &model, const std::string &candidate) const
    {
        return Get(endpoint, model, candidate).has_value();
    }

    std::size_t Size() const
    {
        std::lock_guard lock(mutex_);
        return entries_.size();
    }

    void Clear()
    {
        std::lock_guard lock(mutex_);
        entries_.clear();
        insertion_order_.clear();
    }

  private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> entries_;
    std::deque<std::string> insertion_order_;
};

void Start();
void Stop();
void Request(std::vector<std::string> candidates);
std::optional<std::string> Lookup(const std::string &candidate);
} // namespace OutputTranslation
