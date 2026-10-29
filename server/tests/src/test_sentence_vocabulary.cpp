#include "tests/includes/test_framework.h"
#include "cloud/sentence_vocabulary.h"
#include "mixed/english_abbreviations.h"
#include <filesystem>
#include <string>
#include <unordered_set>
TEST_CASE(sentence_learning_words_are_local_deduplicated_and_bounded)
{
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "assets" /
                      "vocabulary" / "cet.json";
    const SentenceVocabulary::Dictionary dictionary(path);
    const auto words = dictionary.Select(
        "The environment requires sustainable development. Developed countries improve communication.");
    REQUIRE(!words.empty());
    REQUIRE(words.size() <= 5);
    std::unordered_set<std::string> seen;
    for (const auto &word : words)
    {
        REQUIRE(seen.insert(word.word).second);
        REQUIRE(word.level == "CET4" || word.level == "CET6");
        REQUIRE(!word.gloss.empty());
        REQUIRE(word.word != "the");
    }
    REQUIRE(SentenceVocabulary::Dictionary(path / "missing").Select("environment").empty());
    REQUIRE_EQ(EnglishAbbreviations::Canonical("ai"), std::string("AI"));
    REQUIRE_EQ(EnglishAbbreviations::Canonical("api"), std::string("API"));
    REQUIRE_EQ(EnglishAbbreviations::Canonical("cpu"), std::string("CPU"));
    REQUIRE_EQ(EnglishAbbreviations::Canonical("github"), std::string("GitHub"));
    REQUIRE(EnglishAbbreviations::Canonical("a").empty());
}
