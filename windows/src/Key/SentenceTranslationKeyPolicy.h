#pragma once

#include <cstdint>

namespace SentenceTranslationKeyPolicy
{
// Values are Win32 virtual keys and the existing IPC Shift/Ctrl/Alt bits.
// This stays independent of TSF so both key classifiers share the same rule.
inline bool HasLiveComposition(std::uint32_t rawLength, bool selectedChinese, bool candidates)
{
    return rawLength != 0 || selectedChinese || candidates;
}
inline bool KeepsPinyinChannel(bool enabled, bool composing, std::uint32_t key, std::uint32_t modifiers,
                               bool windowsKeyDown)
{
    return enabled && composing && !windowsKeyDown && (modifiers & 6u) == 0 && key >= 'A' && key <= 'Z';
}
inline bool OwnsBufferedEdit(bool enabled, bool selectedChinese, std::uint32_t rawLength, std::uint32_t key,
                             std::uint32_t modifiers, bool windowsKeyDown)
{
    if (!enabled || !selectedChinese || rawLength != 0 || windowsKeyDown)
        return false;
    const auto chord = modifiers & 7u;
    return ((key == 0x08u || key == 0x2Eu) && (chord == 0u || chord == 2u)) || (key == 0x1Bu && chord == 0u) ||
           (key >= '1' && key <= '9' && chord == 0u);
}
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
    return replyType == 2u || replyType == 4u || replyType == 14u || replyType == 16u;
}
} // namespace SentenceTranslationKeyPolicy
