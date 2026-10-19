#include "tests/includes/test_framework.h"

#include "ipc/output_language_policy.h"
#include "cloud/output_translation.h"

TEST_CASE(OutputLanguageDefaultsToEnglishAndTogglesIndependently)
{
    REQUIRE(OutputLanguagePolicy::InitialOutputLanguageIsEnglish());
    bool english = OutputLanguagePolicy::InitialOutputLanguageIsEnglish();
    REQUIRE(OutputLanguagePolicy::ResolveCommit(english, true, false, true) ==
            OutputLanguagePolicy::CommitKind::EnglishTranslation);
    english = OutputLanguagePolicy::Toggle(english);
    REQUIRE(!english);
    REQUIRE(OutputLanguagePolicy::ResolveCommit(english, true, false, false) ==
            OutputLanguagePolicy::CommitKind::ExistingText);
    english = OutputLanguagePolicy::Toggle(english);
    REQUIRE(english);
}

TEST_CASE(OutputLanguageFallsBackToChineseAndPreservesNonChineseCandidates)
{
    REQUIRE(OutputLanguagePolicy::ResolveCommit(true, true, false, false) ==
            OutputLanguagePolicy::CommitKind::ChineseFallback);
    REQUIRE(OutputLanguagePolicy::ResolveCommit(true, false, false, true) ==
            OutputLanguagePolicy::CommitKind::ExistingText);
    REQUIRE(OutputLanguagePolicy::ResolveCommit(true, true, true, true) ==
            OutputLanguagePolicy::CommitKind::ExistingText);
}

TEST_CASE(OutputTranslationCacheIsBoundedAndSeparatesModels)
{
    OutputTranslation::TranslationCache cache(2);
    cache.Put("http://127.0.0.1/v1", "model-a", "你好", "Hello");
    cache.Put("http://127.0.0.1/v1", "model-b", "你好", "Hi");
    REQUIRE(cache.Contains("http://127.0.0.1/v1", "model-a", "你好"));
    REQUIRE(!cache.Contains("http://127.0.0.1/v1", "model-c", "你好"));

    cache.Put("http://127.0.0.1/v1", "model-a", "再见", "Goodbye");
    REQUIRE_EQ(cache.Size(), 2u);
    REQUIRE(!cache.Contains("http://127.0.0.1/v1", "model-a", "你好"));
    REQUIRE(cache.Contains("http://127.0.0.1/v1", "model-b", "你好"));
    REQUIRE(cache.Contains("http://127.0.0.1/v1", "model-a", "再见"));
}
