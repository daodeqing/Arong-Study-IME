#include "../../common/candidate_cache.h"

#include <iostream>
#include <stdexcept>

namespace
{
void Require(bool condition)
{
    if (!condition)
        throw std::runtime_error("Cache budget regression");
}
} // namespace

int main()
{
    CandidateCache disabled(0, 0);
    disabled.insert("disabled", {});
    Require(disabled.empty());

    const std::vector<WordItem> entry = {WordItem("nihao", "hello", 1)};
    const size_t cost = CandidateCacheWeight{}(entry);
    CandidateCache cache(128, cost * 2);
    cache.insert("first", entry);
    cache.insert("second", entry);
    Require(cache.find("first") != nullptr);
    cache.insert("third", entry);
    Require(cache.find("second") == nullptr);
    Require(cache.find("first") != nullptr && cache.find("third") != nullptr);
    cache.insert("first", entry);
    Require(cache.weight() == cost * 2);
    Require(cache.remove("third") && cache.weight() == cost);
    cache.clear();
    Require(cache.empty() && cache.weight() == 0);

    const std::vector<WordItem> large(10000, WordItem("a", std::string(96, 'x'), 1));
    CandidateCache pressure(128, 1024 * 1024);
    pressure.insert("large", large);
    Require(pressure.empty());
    for (int index = 0; index < 1000; ++index)
        pressure.insert(std::to_string(index), std::vector<WordItem>(100, entry.front()));
    Require(pressure.weight() <= 1024 * 1024 && pressure.size() <= 128);
    Require(large.size() == 10000);
    std::cout << "Cache budget, LRU, disabled capacity and oversize checks passed\n";
    return 0;
}
