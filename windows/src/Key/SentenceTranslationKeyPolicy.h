#pragma once

#include <cstdint>

namespace SentenceTranslationKeyPolicy
{
// Values are Win32 virtual keys and the existing IPC Shift/Ctrl/Alt bits.
// This stays independent of TSF so both key classifiers share the same rule.
inline bool OwnsCommitKey(bool enabled, bool hasSentence, std::uint32_t key, std::uint32_t modifiers,
                          bool windowsKeyDown)
{
    if (!enabled || !hasSentence || windowsKeyDown)
        return false;
    const auto chord = modifiers & 7u;
    return (key == 0x0Du && (chord == 0u || chord == 2u)) || (key == 0x20u && chord == 0u);
}

inline bool KeepsComposition(std::uint32_t replyType)
{
    // NeedToCreateWord, NavigationIgnored, CompositionRestored.
    return replyType == 2u || replyType == 4u || replyType == 14u;
}
} // namespace SentenceTranslationKeyPolicy
