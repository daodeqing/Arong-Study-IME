#include "../src/Key/SentenceTranslationKeyPolicy.h"
int main()
{
    using SentenceTranslationKeyPolicy::OwnsCommitKey;
    if (!OwnsCommitKey(true, true, 0x0D, 2, false))
        return 1;
    if (!OwnsCommitKey(true, true, 0x0D, 0, false))
        return 2;
    if (!OwnsCommitKey(true, true, 0x20, 0, false))
        return 3;
    if (OwnsCommitKey(false, true, 0x0D, 2, false))
        return 4;
    if (OwnsCommitKey(true, false, 0x0D, 2, false))
        return 5;
    if (OwnsCommitKey(true, true, 0x0D, 2, true))
        return 6;
    if (OwnsCommitKey(true, true, 0x0D, 3, false))
        return 7;
    if (OwnsCommitKey(true, true, 0x20, 2, false))
        return 8;
    if (!SentenceTranslationKeyPolicy::KeepsComposition(2))
        return 9;
    if (!SentenceTranslationKeyPolicy::KeepsComposition(4))
        return 10;
    if (!SentenceTranslationKeyPolicy::KeepsComposition(14))
        return 11;
    if (SentenceTranslationKeyPolicy::KeepsComposition(10))
        return 12;
    using SentenceTranslationKeyPolicy::CanCommitRawLetters;
    if (!CanCommitRawLetters(true, L"cpu", false, false))
        return 13;
    if (CanCommitRawLetters(true, L"", false, false) || CanCommitRawLetters(true, L"cpu", true, false) ||
        CanCommitRawLetters(true, L"cpu", false, true) || CanCommitRawLetters(false, L"cpu", false, false) ||
        CanCommitRawLetters(true, L"Uzi", false, false) || CanCommitRawLetters(true, L"ni'hao", false, false))
        return 14;
    if (SentenceTranslationKeyPolicy::KeepsPinyinChannel(true, true, 'A', 0, false, true))
        return 15;
    if (!SentenceTranslationKeyPolicy::KeepsPinyinChannel(true, true, 'A', 0, false, false))
        return 16;
    if (SentenceTranslationKeyPolicy::KeepsPinyinChannel(true, true, 'A', 1, false, false))
        return 17;
    if (!SentenceTranslationKeyPolicy::OwnsUppercaseAbbreviation(true, true, 'C', 1, false, false))
        return 18;
    if (!SentenceTranslationKeyPolicy::OwnsUppercaseAbbreviation(true, true, 'C', 0, false, true))
        return 19;
    if (SentenceTranslationKeyPolicy::OwnsUppercaseAbbreviation(true, false, 'C', 1, false, false) ||
        SentenceTranslationKeyPolicy::OwnsUppercaseAbbreviation(true, true, 'C', 0, false, false) ||
        SentenceTranslationKeyPolicy::OwnsUppercaseAbbreviation(true, true, 'C', 2, false, false))
        return 20;
    if (!SentenceTranslationKeyPolicy::OwnsPunctuation(true, true, 1, false, L'>'))
        return 21;
    if (!SentenceTranslationKeyPolicy::OwnsPunctuation(true, true, 0, false, L'@'))
        return 22;
    if (SentenceTranslationKeyPolicy::OwnsPunctuation(true, true, 2, false, L'.') ||
        SentenceTranslationKeyPolicy::OwnsPunctuation(true, false, 1, false, L'>'))
        return 23;
    if (!SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, false, 'A', 1, false))
        return 24;
    if (!SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, false, 'z', 1, false))
        return 25;
    if (SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, true, 'A', 1, false) ||
        SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, false, 'A', 0, false) ||
        SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, false, 'A', 3, false) ||
        SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, false, 'A', 1, true) ||
        SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(false, false, 'A', 1, false) ||
        SentenceTranslationKeyPolicy::PassesShiftHeldEnglish(true, false, '1', 1, false))
        return 26;
    return 0;
}
