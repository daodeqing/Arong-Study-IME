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
    int setModeCalls = 0;
    BOOL GetIMEMode(ThreadManager *, int)
    {
        return imeOpen;
    }
    void SetIMEMode(ThreadManager *, int, BOOL open)
    {
        ++setModeCalls;
        imeOpen = open;
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
std::deque<int> messages;
int statusSnapshots = 0;
int refreshes = 0;
int cancellations = 0;
bool confirmationPending = false;

bool IsWindow(HWND window)
{
    return window != 0;
}

void PostMessage(HWND, int message, int, int)
{
    messages.push_back(message);
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
    messages.push_back(STATUS_SNAPSHOT_ECHO);
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
        const int message = messages.front();
        messages.pop_front();
        if (message == WM_ApplySentenceTranslation)
            ApplySentenceTranslation(&service);
        else if (message == WM_CancelSentenceTranslationCommit)
        {
            ++cancellations;
            confirmationPending = false;
        }
        else if (message == WM_RefreshLanguageBarTheme)
            ++refreshes;
        else if (message == STATUS_SNAPSHOT_ECHO)
        {
            worker.InputMode(&service, Value(false));
            worker.OutputLanguage(&service, Value(true));
            worker.SentenceTranslation(&service, Value(true));
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
    CHECK(service.engine.imeOpen && service.engine.setModeCalls == 1);
    CHECK(statusSnapshots == 1 && cancellations == 0);

    // Repeated enabled snapshots may validate the owner mode, but cannot write
    // the compartment or emit more status snapshots once it already matches.
    confirmationPending = true;
    const int oldRefreshes = refreshes;
    for (int repeat = 0; repeat < 1000; ++repeat)
    {
        worker.InputMode(&service, Value(false));
        worker.OutputLanguage(&service, Value(true));
        worker.SentenceTranslation(&service, Value(true));
    }
    CHECK(PumpMessages(service, worker));
    CHECK(confirmationPending && cancellations == 0);
    CHECK(statusSnapshots == 1 && service.engine.setModeCalls == 1 && refreshes == oldRefreshes);

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
    backgroundWorker.ResetGeneration(1);
    background.manager.focused = FALSE;
    backgroundWorker.SentenceTranslation(&background, Value(true));
    CHECK(PumpMessages(background, backgroundWorker));
    CHECK(!background.engine.imeOpen && background.engine.setModeCalls == 0);
    background.manager.focused = TRUE;
    backgroundWorker.SentenceTranslation(&background, Value(true));
    CHECK(PumpMessages(background, backgroundWorker));
    CHECK(background.engine.imeOpen && background.engine.setModeCalls == 1);
    CHECK(statusSnapshots == 2);

    // Disabled state cancels once, while repeated disabled snapshots stay inert.
    worker.SentenceTranslation(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    const int disabledCancellations = cancellations;
    for (int repeat = 0; repeat < 1000; ++repeat)
        worker.SentenceTranslation(&service, Value(false));
    CHECK(PumpMessages(service, worker));
    CHECK(cancellations == disabledCancellations && statusSnapshots == 2);

    std::cout << "Input-state echoes terminate and unchanged language preserves confirmation\n";
    return 0;
}
