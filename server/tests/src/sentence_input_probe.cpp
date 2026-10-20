#include "tests/includes/test_framework.h"
#include "cloud/sentence_translation.h"
#include "config/ime_config.h"
#include "global/globals.h"
#include "ipc/event_listener_internal.h"
#include "session/engine_input_session.h"
#include "utils/common_utils.h"

#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

// Only the transport/window boundary is replaced. The engine, selection accumulation,
// confirmation policy and cancellable HTTP worker below are production code.
namespace
{
FanyImeIpc::CandidateUiOwner probe_owner{1, 1};
bool probe_current = true;
std::atomic<unsigned> completed_previews{0};
} // namespace
bool ClientNegotiatedSentenceTranslation(uint64_t)
{
    return true;
}
bool ClientNegotiatedCompositionRestore(uint64_t)
{
    return true;
}
bool IsPipeActivationCurrent(uint64_t client, uint64_t epoch)
{
    return probe_current && client == 1 && epoch == 1;
}
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t, uint64_t, UINT, const std::wstring &)
{
    return true;
}
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t, UINT, const std::wstring &)
{
    return true;
}

namespace event_listener_detail
{
bool g_english_input_mode = false;
bool IsSensitiveInputField()
{
    return false;
}
bool IsEnglishOutputEnabled()
{
    return true;
}
bool IsSpecialModeCompositionActive(const std::string &)
{
    return false;
}
std::string CandidateTextForOutput(const std::string &text)
{
    return text;
}
void PostCaretStateBadge(FanyImeUi::CaretStateBadge, int, int)
{
}
void UpdateCloudInput(const std::string &, uint64_t, uint64_t)
{
}
void UpdateEnglishInput(const std::string &, uint64_t, uint64_t, bool)
{
}
void UpdateAiInput(const std::string &, uint64_t, uint64_t)
{
}
void PublishCandidateUiOwner(uint64_t client, uint64_t epoch)
{
    probe_owner = {client, epoch};
}
FanyImeIpc::CandidateUiOwner SnapshotCandidateUiOwner()
{
    return probe_owner;
}
std::wstring BuildCreateWordPipePayload(const std::string &raw, const std::string &word)
{
    return string_to_wstring(raw + "\t" + word + "\t" + word + raw);
}
void RefreshCandidatePageUi(bool)
{
    auto &ui = Global::candidate_ui;
    ui.page_words.clear();
    for (const auto &item : ui.items)
        ui.page_words.push_back(string_to_wstring(item.word));
}
} // namespace event_listener_detail

namespace FanyNamedPipe
{
void EnqueueRefreshCandidatePageTask()
{
    ++completed_previews;
}
bool SendCurrentDataToClient(uint64_t, uint64_t, uint64_t)
{
    return true;
}
void PrepareCandidateList(uint64_t, uint64_t)
{
    Global::candidate_ui.set_items(g_inputSession->get_candidates());
    event_listener_detail::RefreshCandidatePageUi(false);
}
bool ResolveCandidateItem(int one_based, WordItem &item)
{
    const auto &items = Global::candidate_ui.items;
    if (one_based < 1 || static_cast<std::size_t>(one_based) > items.size())
        return false;
    item = items[static_cast<std::size_t>(one_based - 1)];
    return true;
}
} // namespace FanyNamedPipe

namespace
{
void TypeAndSelect(const std::string &pinyin, const std::string &word)
{
    for (char ch : pinyin)
        g_inputSession->handle_key(static_cast<UINT>(ch - 'a' + 'A'), 0, static_cast<WCHAR>(ch));
    const auto &items = g_inputSession->get_candidates();
    auto match = std::find_if(items.begin(), items.end(), [&](const WordItem &item) { return item.word == word; });
    REQUIRE(match != items.end());
    const WordItem chosen = *match;
    REQUIRE(FanyNamedPipe::AccumulateSentenceCandidate(chosen, 1, 1));
    REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::NeedToCreateWord);
}
bool WaitFor(SentenceTranslation::Status status, std::chrono::seconds budget = std::chrono::seconds(12))
{
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (SentenceTranslation::Current().status == status)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}
} // namespace

int RunSentenceOutputProbe(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try
    {
        InitImeConfig();
        REQUIRE(GetConfiguredSentenceTranslationEnabled());
        g_inputSession = std::make_shared<EngineInputSession>(SchemeType::Quanpin);
        SentenceTranslation::Start();
        TypeAndSelect("ni", "你");
        TypeAndSelect("hao", "好");
        REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你好"));
        REQUIRE(g_inputSession->get_pinyin_sequence_with_cases().empty());
        REQUIRE_EQ(GlobalIme::composition.selection_history.size(), std::size_t{2});
        const auto start = std::chrono::steady_clock::now();
        FanyNamedPipe::PrepareSentenceConfirmation(1, 1);
        REQUIRE(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100));
        REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::NeedToCreateWord);
        REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你好"));

        const bool offline = std::string(argv[1]) == "offline";
        if (offline)
        {
            REQUIRE(WaitFor(SentenceTranslation::Status::Failed));
            FanyNamedPipe::PrepareSentenceConfirmation(1, 1);
            REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::NeedToCreateWord);
        }
        else
        {
            REQUIRE(WaitFor(SentenceTranslation::Status::Ready));
            REQUIRE_EQ(SentenceTranslation::Current().source, std::string("你好"));
            FanyNamedPipe::PrepareSentenceConfirmation(1, 1);
            REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::CommitExactText);
            REQUIRE_EQ(Global::candidate_ui.selected_text, std::wstring(L"Hello."));
            // The production callback may refresh only the current owner, never another field.
            probe_current = false;
            REQUIRE(!FanyNamedPipe::RefreshSentenceTranslationPreview());
            probe_current = true;
            // A newer sentence cancels an in-flight slow response instead of queuing behind it.
            SentenceTranslation::Request("旧句测试");
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            const auto newer_started = std::chrono::steady_clock::now();
            SentenceTranslation::Request("你好");
            REQUIRE(WaitFor(SentenceTranslation::Status::Ready));
            REQUIRE(std::chrono::steady_clock::now() - newer_started < std::chrono::seconds(4));
            REQUIRE_EQ(SentenceTranslation::Current().translation, std::string("Hello."));
        }
        FanyNamedPipe::PrepareSentenceConfirmation(1, 1, true);
        REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::CommitExactText);
        REQUIRE_EQ(Global::candidate_ui.selected_text, std::wstring(L"你好"));
        if (!offline)
        {
            GlobalIme::composition.clear();
            g_inputSession->reset_state();
            TypeAndSelect("wo", "我");
            TypeAndSelect("jintian", "今天");
            TypeAndSelect("hen", "很");
            TypeAndSelect("gaoxing", "高兴");
            REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("我今天很高兴"));
            REQUIRE(WaitFor(SentenceTranslation::Status::Ready));
            FanyNamedPipe::PrepareSentenceConfirmation(1, 1);
            REQUIRE_EQ(Global::candidate_ui.selected_text, std::wstring(L"I am very happy today."));
            GlobalIme::composition.clear();
            g_inputSession->reset_state();
            TypeAndSelect("ni", "你");
            TypeAndSelect("hao", "好");
        }
        // Undo a separately selected character and restore exactly its spelling.
        REQUIRE(GlobalIme::composition.restore_last_selection());
        REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你"));
        REQUIRE_EQ(GlobalIme::composition.raw_input_with_cases, std::string("hao"));
        SentenceTranslation::Cancel();
        REQUIRE(SentenceTranslation::Current().status != SentenceTranslation::Status::Ready);
        SentenceTranslation::Stop();
        g_inputSession.reset();
        GlobalIme::composition.clear();
        curl_global_cleanup();
        std::cout << "sentence probe passed: separate Chinese selections, retained composition, "
                     "nonblocking confirmation, English/Chinese commit and owner guard\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        SentenceTranslation::Stop();
        curl_global_cleanup();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
