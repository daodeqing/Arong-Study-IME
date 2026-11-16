#include "../src/Key/CapsLockPunctuationPolicy.h"
#include <atomic>
#include <cstdint>
#include <deque>
#include <iostream>

using UINT = unsigned int;
using BOOL = int;
using HWND = int;
constexpr BOOL TRUE = 1;
constexpr BOOL FALSE = 0;
constexpr int WM_RefreshLanguageBarTheme = 1;
constexpr int WM_CancelSentenceTranslationCommit = 2;
constexpr int WM_ApplySentenceTranslation = 3;
constexpr int WM_SyncOutputLanguagePunctuation = 5;
constexpr int STATUS_SNAPSHOT_ECHO = 4;

bool SUCCEEDED(int result)
{
    return result >= 0;
}

namespace Global
{
std::atomic<bool> JapaneseInputModeEnabled{false};
std::atomic<bool> OutputLanguageEnglish{true};
std::atomic<bool> SentenceTranslationEnabled{true};
inline BOOL ResolveFollowedPunctuationOpen(BOOL imeOpen, bool englishOutput)
{
    return FollowedPunctuationOpen(imeOpen != FALSE, false, SentenceTranslationEnabled.load(), englishOutput);
}

inline BOOL ResolveFollowedPunctuationOpen(BOOL imeOpen)
{
    return ResolveFollowedPunctuationOpen(imeOpen, OutputLanguageEnglish.load());
}
} // namespace Global

struct ThreadManager
{
    BOOL focused = TRUE;
    int IsThreadFocus(BOOL *result)
    {
        *result = focused;
        return 0;
    }
};

struct CompositionEngine
{
    BOOL imeOpen = FALSE;
    BOOL punctuationOpen = TRUE;
    int setModeCalls = 0;
    bool acceptModeChange = true;
    BOOL GetIMEMode(ThreadManager *, int)
    {
        return imeOpen;
    }
    void SetIMEMode(ThreadManager *, int, BOOL open)
    {
        ++setModeCalls;
        if (acceptModeChange)
            imeOpen = open;
    }
    BOOL GetPunctuationMode(ThreadManager *, int)
    {
        return punctuationOpen;
    }
    void SetPunctuationMode(ThreadManager *, int, BOOL open)
    {
        punctuationOpen = open;
    }
    void SyncPunctuationWithImeMode(ThreadManager *, int, BOOL open)
    {
        punctuationOpen = open;
    }
};

class CMetasequoiaIME
{
  public:
    HWND _msgWndHandle = 1;
    ThreadManager manager;
    CompositionEngine engine;
    ThreadManager *_pThreadMgr = &manager;
    CompositionEngine *_pCompositionProcessorEngine = &engine;
    bool disabled = false;
    bool _sentencePunctuationKnown = false;
    bool _sentencePunctuationEnabled = false;
    bool _sentencePunctuationEnglish = false;
    bool _optimisticOutputLanguagePending = false;
    bool _optimisticOutputLanguageEnglish = false;
    bool _deferredProjectedImeOpen = false;
    bool _deferredKeyProjectionValid = false;
    bool _deferredProjectedOutputLanguageEnglish = false;
    bool _deferredProjectedPunctuationOpen = false;

    ThreadManager *_GetThreadMgr()
    {
        return _pThreadMgr;
    }
    int _GetClientId()
    {
        return 1;
    }
    bool _IsKeyboardDisabled()
    {
        return disabled;
    }
};

struct Frame
{
    wchar_t data[2];
};

class WorkerSnapshots
{
  public:
    UINT inputStateSnapshotGeneration = 0;
    int lastInputMode = -1;
    int lastOutputLanguageEnglish = -1;
    int lastSentenceTranslationEnabled = -1;

    void ResetGeneration(UINT workerGeneration);
    void InputMode(CMetasequoiaIME *pIME, const Frame &buf);
    void OutputLanguage(CMetasequoiaIME *pIME, const Frame &buf);
    void SentenceTranslation(CMetasequoiaIME *pIME, const Frame &buf);
};

namespace
{
struct PendingMessage
{
    int message;
    int wParam;
    int lParam;

    PendingMessage(int message, int wParam = 0, int lParam = 0) : message(message), wParam(wParam), lParam(lParam)
    {
    }
};

std::deque<PendingMessage> messages;
int statusSnapshots = 0;
int refreshes = 0;
int cancellations = 0;
bool confirmationPending = false;

bool IsWindow(HWND window)
{
    return window != 0;
}

void PostMessage(HWND, int message, int wParam, int lParam)
{
    messages.emplace_back(message, wParam, lParam);
}

bool IsSentenceTranslationEnabled()
{
    return Global::SentenceTranslationEnabled.load();
}

void SendCurrentImeStatusSnapshot(CMetasequoiaIME *)
{
    ++statusSnapshots;
    // Deliberately emulate an older Server that answers every status snapshot
    // with all input-state frames. The DLL must stop that feedback by itself.
    messages.emplace_back(STATUS_SNAPSHOT_ECHO);
}

Frame Value(bool value)
{
    return Frame{{value ? L'1' : L'0', L'\0'}};
}
} // namespace

#include "input_state_source.h"

namespace
{
bool PumpMessages(CMetasequoiaIME &service, WorkerSnapshots &worker)
{
    int remaining = 10000;
    while (!messages.empty() && --remaining > 0)
    {
        const PendingMessage pending = messages.front();
        messages.pop_front();
        const int message = pending.message;
        if (message == WM_ApplySentenceTranslation)
            ApplySentenceTranslation(&service, pending.message, pending.wParam, pending.lParam);
        else if (message == WM_CancelSentenceTranslationCommit)
        {
            ++cancellations;
            confirmationPending = false;
        }
        else if (message == WM_RefreshLanguageBarTheme)
            ++refreshes;
        else if (message == WM_SyncOutputLanguagePunctuation)
            ApplySentenceTranslation(&service, pending.message, pending.wParam, pending.lParam);
        else if (message == STATUS_SNAPSHOT_ECHO)
        {
            worker.InputMode(&service, Value(false));
            worker.OutputLanguage(&service, Value(Global::OutputLanguageEnglish.load()));
            worker.SentenceTranslation(&service, Value(Global::SentenceTranslationEnabled.load()));
        }
    }
    return messages.empty();
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
    CMetasequoiaIME service;
    WorkerSnapshots worker;
    worker.ResetGeneration(1);
    // A process-global value already set by another TSF thread must not prevent
    // this owner's first synchronization from entering Chinese composition.
    CHECK(Global::SentenceTranslationEnabled.load());
    worker.SentenceTranslation(&service, Value(true));
    CHECK(PumpMessages(service, worker));
    CHECK(service.engine.imeOpen && !service.engine.punctuationOpen && service.engine.setModeCalls == 1);
    CHECK(statusSnapshots >= 1 && cancellations == 0);

    // An optimistic Shift projection must be corrected when the authoritative
    // Server echo arrives with the opposite language.
    service.engine.punctuationOpen = TRUE;
    service._deferredKeyProjectionValid = true;
    service._deferredProjectedImeOpen = true;
    service._optimisticOutputLanguagePending = true;
    service._optimisticOutputLanguageEnglish = true;
    ApplySentenceTranslation(&service, WM_SyncOutputLanguagePunctuation, 1, 2);
    CHECK(!service.engine.punctuationOpen && service._optimisticOutputLanguagePending);
    ApplySentenceTranslation(&service, WM_SyncOutputLanguagePunctuation, 0, 1);
    CHECK(service.engine.punctuationOpen && !service._optimisticOutputLanguagePending);

    worker.OutputLanguage(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(service.engine.punctuationOpen);
    worker.OutputLanguage(&service, Value(true));
    CHECK(PumpMessages(service, worker));
    CHECK(!service.engine.punctuationOpen);

    // Repeated enabled snapshots may validate the owner mode, but cannot write
    // the compartment or emit more status snapshots once it already matches.
    cancellations = 0;
    confirmationPending = true;
    const int oldRefreshes = refreshes;
    const int oldStatusSnapshots = statusSnapshots;
    for (int repeat = 0; repeat < 1000; ++repeat)
    {
        worker.InputMode(&service, Value(false));
        worker.OutputLanguage(&service, Value(true));
        worker.SentenceTranslation(&service, Value(true));
    }
    CHECK(PumpMessages(service, worker));
    CHECK(confirmationPending && cancellations == 0);
    CHECK(statusSnapshots == oldStatusSnapshots && service.engine.setModeCalls == 1 && refreshes == oldRefreshes);

    // A manual punctuation toggle survives repeated status snapshots. Only
    // an actual output-language change applies the following rule again.
    service.engine.punctuationOpen = TRUE;
    worker.SentenceTranslation(&service, Value(true));
    CHECK(PumpMessages(service, worker));
    CHECK(service.engine.punctuationOpen);

    // Only a real output-language edge cancels confirmation.
    worker.OutputLanguage(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(!confirmationPending && cancellations == 1);
    confirmationPending = true;
    worker.OutputLanguage(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(confirmationPending && cancellations == 1);

    // A new physical endpoint synchronizes its icons without manufacturing an
    // output-language change; normal disconnect cleanup owns prior sessions.
    worker.ResetGeneration(2);
    worker.OutputLanguage(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(confirmationPending && cancellations == 1);

    // A background initial sync cannot touch COM compartments. A later enabled
    // activation snapshot must still apply, although its boolean is unchanged.
    CMetasequoiaIME background;
    WorkerSnapshots backgroundWorker;
    const int statusBeforeBackground = statusSnapshots;
    backgroundWorker.ResetGeneration(1);
    background.manager.focused = FALSE;
    backgroundWorker.SentenceTranslation(&background, Value(true));
    CHECK(PumpMessages(background, backgroundWorker));
    CHECK(!background.engine.imeOpen && background.engine.setModeCalls == 0);
    background.manager.focused = TRUE;
    backgroundWorker.SentenceTranslation(&background, Value(true));
    CHECK(PumpMessages(background, backgroundWorker));
    CHECK(background.engine.imeOpen && background.engine.setModeCalls == 1);
    CHECK(background.engine.punctuationOpen);
    CHECK(statusSnapshots == statusBeforeBackground + 1);

    // A host may reject the compartment write. Do not publish a false change
    // that an older Server would answer with another identical apply request.
    CMetasequoiaIME rejecting;
    WorkerSnapshots rejectingWorker;
    rejectingWorker.ResetGeneration(1);
    rejecting.engine.acceptModeChange = false;
    rejectingWorker.SentenceTranslation(&rejecting, Value(true));
    CHECK(PumpMessages(rejecting, rejectingWorker));
    CHECK(!rejecting.engine.imeOpen && rejecting.engine.setModeCalls == 1);
    CHECK(statusSnapshots == statusBeforeBackground + 1);

    // Disabled state cancels once, while repeated disabled snapshots stay inert.
    worker.SentenceTranslation(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(service.engine.punctuationOpen == service.engine.imeOpen);
    const int disabledCancellations = cancellations;
    for (int repeat = 0; repeat < 1000; ++repeat)
        worker.SentenceTranslation(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(cancellations == disabledCancellations && statusSnapshots == statusBeforeBackground + 1);

    std::cout << "Input-state echoes terminate and unchanged language preserves confirmation\n";
    return 0;
}
