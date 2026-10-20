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
    return 0;
}
