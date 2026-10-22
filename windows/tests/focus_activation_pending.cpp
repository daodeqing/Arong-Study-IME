#include <atomic>
#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

using UINT = unsigned int;
using DWORD = unsigned long;
using ULONGLONG = unsigned long long;
constexpr DWORD WAIT_FAILED = static_cast<DWORD>(-1);

namespace
{
ULONGLONG clockMs = 1;
bool transportReady = true;
bool flushReady = true;
bool activationWriteReady = true;
bool activationRequired = true;
std::atomic<bool> workerReady{false};
std::atomic<bool> localResetPending{false};
std::atomic<uint64_t> expectedToken{0};
std::atomic<uint64_t> acknowledgedToken{0};
bool *boundActivationRequired = &activationRequired;
bool *boundFocusResetPending = nullptr;
std::atomic<bool> *boundWorkerCommitReady = &workerReady;
std::atomic<bool> *boundLocalSessionResetPending = &localResetPending;
std::atomic<uint64_t> *boundExpectedWorkerFocusToken = &expectedToken;
std::atomic<uint64_t> *boundAcknowledgedWorkerFocusToken = &acknowledgedToken;
int boundWorkerAckEvent = 1;
uint64_t nextFocusSessionToken = 20;
int activationWrites = 0;
int resetCalls = 0;
int waits = 0;
int signaledAcks = 0;
int postedReconnects = 0;
constexpr UINT WM_IpcReconnect = 1;

struct FocusAckService
{
    std::atomic<uint64_t> _expectedWorkerFocusToken{42};
    std::atomic<uint64_t> _acknowledgedWorkerFocusToken{0};
    std::atomic<bool> _workerCommitReady{false};
    int _workerAckEvent = 1;
    int _msgWndHandle = 1;
};

void SetEvent(int)
{
    ++signaledAcks;
}

void PostMessage(int, UINT, int, int)
{
    ++postedReconnects;
}
std::deque<int> unsentKeys;
std::vector<int> deliveredKeys;
std::vector<uint64_t> writtenActivationTokens;

ULONGLONG GetTickCount64()
{
    return clockMs;
}

uint64_t NextProtocolId(uint64_t &counter)
{
    return ++counter;
}

bool ConnectToAllNamedpipe()
{
    return transportReady;
}

bool FlushNamedpipeFocusSessionReset()
{
    return flushReady;
}

int SendClientActivatedEventToServerViaNamedPipe(uint64_t token)
{
    ++activationWrites;
    writtenActivationTokens.push_back(token);
    return activationWriteReady ? 0 : -1;
}

void MarkNamedpipeSessionDirty()
{
    ++resetCalls;
    localResetPending.store(true);
    unsentKeys.clear();
}

DWORD WaitForSingleObject(int, DWORD duration)
{
    ++waits;
    clockMs += duration;
    return 0;
}

void Sleep(DWORD duration)
{
    ++waits;
    clockMs += duration;
}
} // namespace

#include "focus_activation_source.h"

namespace
{
void ResetFixture()
{
    clockMs = 1;
    transportReady = flushReady = activationWriteReady = true;
    activationRequired = true;
    workerReady.store(false);
    localResetPending.store(false);
    expectedToken.store(0);
    acknowledgedToken.store(0);
    activationWrites = resetCalls = waits = 0;
    unsentKeys = {1, 2, 3};
    deliveredKeys.clear();
    writtenActivationTokens.clear();
    RequireNamedpipeFocusActivation();
}

// The FIFO dispatches only when the production activation gate opens; it does
// not resend, manufacture local candidates, or consume a pending entry.
void DrainReadyKeys()
{
    bool pending = false;
    if (!EnsureNamedpipeFocusSessionActivated(false, &pending))
        return;
    while (!unsentKeys.empty())
    {
        deliveredKeys.push_back(unsentKeys.front());
        unsentKeys.pop_front();
    }
}
} // namespace

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << "Failed at line " << __LINE__ << ": " << #condition << '\n';                                  \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)

int main()
{
    ResetFixture();
    const uint64_t token = expectedToken.load();
    // UI/control senders use the public default, which must yield as well.
    CHECK(!EnsureNamedpipeFocusSessionActivated());
    CHECK(waits == 0 && resetCalls == 0 && unsentKeys.size() == 3);
    bool pending = false;
    CHECK(!EnsureNamedpipeFocusSessionActivated(false, &pending) && pending);
    CHECK(activationWrites == 1 && resetCalls == 0 && waits == 0);
    int expectedWrites = 1;
    ULONGLONG nextRetry = 251;
    ULONGLONG retryDelay = 500;
    for (ULONGLONG elapsed : {149ULL, 150ULL, 500ULL, 1500ULL, 5000ULL})
    {
        clockMs = elapsed + 1;
        pending = false;
        if (clockMs >= nextRetry)
        {
            ++expectedWrites;
            nextRetry = clockMs + retryDelay;
            retryDelay = retryDelay < 1000 ? retryDelay * 2 : 2000;
        }
        CHECK(!EnsureNamedpipeFocusSessionActivated(false, &pending) && pending);
        DrainReadyKeys();
        CHECK(expectedToken.load() == token && activationWrites == expectedWrites);
        for (const uint64_t sentToken : writtenActivationTokens)
            CHECK(sentToken == token);
        CHECK(resetCalls == 0 && waits == 0 && !localResetPending.load());
        CHECK(unsentKeys.size() == 3 && deliveredKeys.empty());
    }

    // A ready bit from a different focus epoch cannot release queued input.
    workerReady.store(true);
    acknowledgedToken.store(token - 1);
    DrainReadyKeys();
    CHECK(unsentKeys.size() == 3 && deliveredKeys.empty());
    acknowledgedToken.store(token);
    DrainReadyKeys();
    DrainReadyKeys();
    CHECK(unsentKeys.empty() && deliveredKeys == std::vector<int>({1, 2, 3}));
    CHECK(activationWrites == expectedWrites && resetCalls == 0 && waits == 0);
    pending = true;
    CHECK(EnsureNamedpipeFocusSessionActivated(false, &pending) && !pending);

    // Repeated probes must retain the next session's exact token. An old echo
    // after another profile switch is insufficient, even with workerReady set.
    RequireNamedpipeFocusActivation();
    const uint64_t nextToken = expectedToken.load();
    CHECK(nextToken != token);
    unsentKeys = {4, 5};
    acknowledgedToken.store(token);
    workerReady.store(true);
    DrainReadyKeys();
    CHECK(deliveredKeys.size() == 3 && unsentKeys.size() == 2);
    acknowledgedToken.store(nextToken);
    DrainReadyKeys();
    CHECK(deliveredKeys == std::vector<int>({1, 2, 3, 4, 5}));
    CHECK(activationWrites == expectedWrites + 1 && resetCalls == 0);

    // The first activation can be written successfully but ignored while the
    // reverse route registers. Repeated probes stay bounded, then retransmit
    // only the same activation token and recover the FIFO after its exact echo.
    ResetFixture();
    const uint64_t ignoredToken = expectedToken.load();
    DrainReadyKeys();
    CHECK(activationWrites == 1 && unsentKeys.size() == 3);
    for (int probe = 0; probe < 1000; ++probe)
        DrainReadyKeys();
    CHECK(activationWrites == 1 && resetCalls == 0);
    clockMs = 250;
    DrainReadyKeys();
    CHECK(activationWrites == 1);
    clockMs = 251;
    DrainReadyKeys();
    CHECK(activationWrites == 2 && writtenActivationTokens.back() == ignoredToken);
    acknowledgedToken.store(ignoredToken);
    workerReady.store(true);
    DrainReadyKeys();
    clockMs = 10000;
    DrainReadyKeys();
    CHECK(activationWrites == 2 && resetCalls == 0 && waits == 0);
    CHECK(deliveredKeys == std::vector<int>({1, 2, 3}));

    // An explicit legacy wait remains bounded. Its timeout ends the probe,
    // without turning slow but healthy activation into destructive recovery.
    ResetFixture();
    pending = false;
    CHECK(!EnsureNamedpipeFocusSessionActivated(true, &pending) && pending);
    CHECK(clockMs == 151 && waits == 1 && resetCalls == 0 && unsentKeys.size() == 3);

    // Real connection/write failures and an already-pending reset remain
    // unavailable, rather than being mislabeled as a healthy pending echo.
    for (int failure = 0; failure < 4; ++failure)
    {
        ResetFixture();
        if (failure == 0)
            transportReady = false;
        else if (failure == 1)
            flushReady = false;
        else if (failure == 2)
            activationWriteReady = false;
        else
            localResetPending.store(true);
        pending = true;
        CHECK(!EnsureNamedpipeFocusSessionActivated(false, &pending) && !pending);
        CHECK(deliveredKeys.empty());
    }

    // A Server may repeat FocusSessionReady on each key. The production worker
    // must wake the owner once, rather than emit a reconnect/status cycle for
    // every key. A real reactivation (ready cleared) must still wake it again.
    FocusAckService worker;
    ApplyFocusReadyAck(&worker, 41);
    CHECK(signaledAcks == 0 && postedReconnects == 0 && !worker._workerCommitReady.load());
    for (int duplicate = 0; duplicate < 1000; ++duplicate)
        ApplyFocusReadyAck(&worker, 42);
    CHECK(signaledAcks == 1 && postedReconnects == 1);
    CHECK(worker._workerCommitReady.load() && worker._acknowledgedWorkerFocusToken.load() == 42);
    worker._workerCommitReady.store(false);
    ApplyFocusReadyAck(&worker, 42);
    CHECK(signaledAcks == 2 && postedReconnects == 2);
    worker._expectedWorkerFocusToken.store(43);
    ApplyFocusReadyAck(&worker, 42);
    CHECK(signaledAcks == 2 && postedReconnects == 2);
    ApplyFocusReadyAck(&worker, 43);
    CHECK(signaledAcks == 3 && postedReconnects == 3);

    std::cout << "Delayed focus activation preserves unsent keys and exact-token ordering\n";
    return 0;
}
