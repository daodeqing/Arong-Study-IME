#pragma once

namespace OutputLanguagePolicy
{
enum class CommitKind
{
    ExistingText,
    EnglishTranslation,
    ChineseFallback,
};

inline bool Toggle(bool english_output)
{
    return !english_output;
}

inline bool InitialOutputLanguageIsEnglish()
{
    return true;
}

inline CommitKind ResolveCommit(bool english_output, bool chinese_candidate, bool japanese_mode, bool translated)
{
    if (!english_output || !chinese_candidate || japanese_mode)
        return CommitKind::ExistingText;
    return translated ? CommitKind::EnglishTranslation : CommitKind::ChineseFallback;
}
} // namespace OutputLanguagePolicy
