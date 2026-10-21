#include <algorithm>
#include <limits>
#define max(a, b) windows_max_macro(a, b)
#include "../src/Key/SentenceAutoCommitState.h"
#include "../src/Key/SentenceTranslationKeyPolicy.h"
#undef max
#include <iostream>

#define CHECK(value)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(value))                                                                                                  \
        {                                                                                                              \
            std::cerr << "check failed at " << __LINE__ << '\n';                                                       \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)

int main()
{
    using namespace SentenceTranslationKeyPolicy;
    CHECK(HasLiveComposition(0, true, false));
    CHECK(HasLiveComposition(2, false, false));
    CHECK(!HasLiveComposition(0, false, false));
    CHECK(KeepsPinyinChannel(true, HasLiveComposition(0, true, false), 'H', 0, false));
    CHECK(!KeepsPinyinChannel(true, true, 'H', 2, false));
    CHECK(!KeepsPinyinChannel(true, true, 'H', 0, true));
    CHECK(OwnsBufferedEdit(true, true, 0, 0x08, 0, false));
    CHECK(OwnsBufferedEdit(true, true, 0, 0x08, 2, false));
    CHECK(OwnsBufferedEdit(true, true, 0, 0x1B, 0, false));
    CHECK(!OwnsBufferedEdit(true, true, 1, 0x08, 0, false));
    CHECK(!OwnsBufferedEdit(false, true, 0, 0x08, 0, false));

    SentenceAutoCommitState state;
    const SentenceAutoCommitState::Session session{11, 22, 33, 44};
    const auto frames = FanyImeSentenceTranslation::EncodeResult(71, L"I want to learn English.");
    CHECK(!frames.empty());
    state.Begin(71, session, 100);
    for (const auto &payload : frames)
        state.Append(FanyImeSentenceTranslation::ParseFrame(payload));
    CHECK(!state.Schedule(session, 200)); // Result before accepted Enter cannot write.
    CHECK(!state.Accept(70, L"source"));
    CHECK(state.Accept(71, L"source"));
    CHECK(state.Schedule(session, 300));
    CHECK(!state.Schedule(session, 300)); // One Enter can schedule only one edit.
    CHECK(state.CanApply(71, session, 301, L"source"));
    CHECK(!state.CanApply(70, session, 301, L"source"));
    CHECK(!state.CanApply(71, session, 301, L"edited"));
    auto changed = session;
    ++changed.focus;
    CHECK(!state.CanApply(71, changed, 301, L"source"));
    changed = session;
    ++changed.composition;
    CHECK(!state.CanApply(71, changed, 301, L"source"));
    changed = session;
    ++changed.generation;
    CHECK(!state.CanApply(71, changed, 301, L"source"));
    changed = session;
    ++changed.reset;
    CHECK(!state.CanApply(71, changed, 301, L"source"));
    CHECK(!state.CanApply(71, session, 5100, L"source"));
    state.Cancel(); // Editing, Shift, Esc, blur and reset all revoke the intent.
    CHECK(!state.CanApply(71, session, 301, L"source"));
    CHECK(!state.Append(FanyImeSentenceTranslation::ParseFrame(frames.front())));

    state.Begin(72, session, 400);
    CHECK(!state.Append(FanyImeSentenceTranslation::ParseFrame(frames.front())));
    CHECK(!state.Schedule(session, 401));
    CHECK(state.Accept(72, L"new source"));
    const std::wstring maximum(FanyImeSentenceTranslation::MaxTextUnits, L'x');
    const auto split = FanyImeSentenceTranslation::EncodeResult(72, maximum);
    CHECK(split.size() > 1);
    CHECK(!state.Append(FanyImeSentenceTranslation::ParseFrame(split.back()))); // Missing prefix.
    for (const auto &payload : split)
    {
        CHECK(payload.size() < FanyImePipeLimits::CandidateTextCapacity);
        state.Append(FanyImeSentenceTranslation::ParseFrame(payload));
    }
    CHECK(state.Schedule(session, 500));
    CHECK(state.CanApply(72, session, 501, L"new source"));
    CHECK(state.Translation() == maximum);
    CHECK(FanyImeSentenceTranslation::EncodeResult(72, maximum + L'x').empty());
    CHECK(!FanyImeSentenceTranslation::ParseFrame(L"18446744073709551616\t0:1\tx").valid);
    CHECK(!FanyImeSentenceTranslation::ParseFrame(L"0\t0:1\tx").valid);
    CHECK(!FanyImeSentenceTranslation::ParseFrame(L"1\t0:2\txxx").valid);
    CHECK(!FanyImeSentenceTranslation::ParseFrame(L"1\t0:200\tx").valid);
    CHECK(!FanyImeSentenceTranslation::ParseFrame(L"1\t1:1\tx").valid);
    std::cout << "sentence commit: both arrival orders, one edit, stale sessions, cancellation, timeout, full-capacity "
                 "frames\n";
    return 0;
}
