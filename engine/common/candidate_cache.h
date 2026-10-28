#pragma once

#include "cache.h"
#include "../core/word_item.h"

#include <vector>

struct CandidateCacheWeight
{
    size_t operator()(const std::vector<WordItem> &items) const
    {
        size_t bytes = sizeof(items) + items.size() * sizeof(WordItem);
        for (const auto &item : items)
            bytes += item.pinyin.size() + item.canonical_pinyin.size() + item.word.size() + item.corrected_from.size();
        return bytes;
    }
};

using CandidateCache = CircularBuffer<std::string, std::vector<WordItem>, CandidateCacheWeight>;
inline constexpr size_t kCandidateCacheBudget = 4ULL * 1024 * 1024;
