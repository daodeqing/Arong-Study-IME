#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace SentenceTranslationKeyPolicy
{
inline bool IsAsciiLetterKey(std::uint32_t key)
{
    return (key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z');
}

// Holding Shift while starting an ASCII word is an explicit English
// passthrough. It must be checked before the fresh-composition classifier, or
// the first letter becomes pinyin and the rest of the word is split between
// the IME and the host application.
inline bool PassesShiftHeldEnglish(bool imeOpen, bool hasSentence, std::uint32_t key, std::uint32_t modifiers,
                                   bool windowsKeyDown)
{
    constexpr std::uint32_t kShift = 1u;
    constexpr std::uint32_t kControlAlt = 2u | 4u;
    return imeOpen && !hasSentence && !windowsKeyDown && (modifiers & kShift) != 0 && (modifiers & kControlAlt) == 0 &&
           IsAsciiLetterKey(key);
}

// Values are Win32 virtual keys and the existing IPC Shift/Ctrl/Alt bits.
// This stays independent of TSF so both key classifiers share the same rule.
inline bool HasLiveComposition(std::uint32_t rawLength, bool selectedChinese, bool candidates)
{
    return rawLength != 0 || selectedChinese || candidates;
}
// Raw shortcuts only claim ordinary unselected spelling. Uppercase mode
// prefixes, separators, selected Chinese and Japanese retain their own actions.
inline bool CanCommitRawLetters(bool enabled, const std::wstring &raw, bool selectedChinese, bool japanese)
{
    if (!enabled || selectedChinese || japanese || raw.empty())
        return false;
    for (wchar_t ch : raw)
        if (ch < L'a' || ch > L'z')
            return false;
    return true;
}
inline bool KeepsPinyinChannel(bool enabled, bool composing, std::uint32_t key, std::uint32_t modifiers,
                               bool windowsKeyDown, bool capsLock = false)
{
    return enabled && composing && !capsLock && !windowsKeyDown && (modifiers & 7u) == 0u && key >= 'A' && key <= 'Z';
}
inline bool IsPunctuationCharacter(wchar_t ch)
{
    return (ch >= L'!' && ch <= L'/') || (ch >= L':' && ch <= L'@') || (ch >= L'[' && ch <= L'`') ||
           (ch >= L'{' && ch <= L'~') ||
           std::wstring_view(L"。，、！？；：（）【】《》“”‘’「」『』").find(ch) != std::wstring_view::npos;
}
inline bool OwnsPunctuation(bool enabled, bool hasSentence, std::uint32_t modifiers, bool windowsKeyDown, wchar_t ch)
{
    return enabled && hasSentence && !windowsKeyDown && (modifiers & 6u) == 0u && IsPunctuationCharacter(ch);
}
inline bool OwnsUppercaseAbbreviation(bool enabled, bool hasSentence, std::uint32_t key, std::uint32_t modifiers,
                                      bool windowsKeyDown, bool capsLock = false)
{
    if (!enabled || !hasSentence || windowsKeyDown || key < 'A' || key > 'Z')
        return false;
    const auto chord = modifiers & 7u;
    return chord == 1u || (capsLock && chord == 0u);
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
