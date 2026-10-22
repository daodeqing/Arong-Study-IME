#include "tests/includes/test_framework.h"
#include "cloud/sentence_translation.h"
#include "config/ime_config.h"
#include "global/globals.h"
#include "ipc/event_listener_internal.h"
#include "session/engine_input_session.h"
#include "utils/common_utils.h"
#include "engine/contracts/sentence_translation.h"

#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

// Only window/pipe boundaries are replaced. Selection, retained composition,
// confirmation, request-bound delivery and the HTTP worker are production code.
namespace
{
FanyImeIpc::CandidateUiOwner probe_owner{1, 1};
bool probe_current = true;
std::atomic<unsigned> completed_previews{0};
struct Delivery
{
    UINT type;
    std::wstring data;
};
std::vector<Delivery> deliveries;
std::vector<double> key_times;
} // namespace
bool ClientNegotiatedSentenceTranslation(uint64_t)
{
    return true;
}
bool ClientNegotiatedSentenceAutoCommit(uint64_t)
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
bool SendToTsfWorkerThreadClientViaNamedpipe(uint64_t, uint64_t, UINT type, const std::wstring &data)
{
    if (type == FanyImeWorkerReplyType::SentenceTranslationReady ||
        type == FanyImeWorkerReplyType::SentenceTranslationFailed)
        deliveries.push_back({type, data});
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
void EnqueueSentenceTranslationResultTask()
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
    {
        const auto started = std::chrono::steady_clock::now();
        g_inputSession->handle_key(static_cast<UINT>(ch - 'a' + 'A'), 0, static_cast<WCHAR>(ch));
        (void)g_inputSession->get_candidates(); // Visible candidate query on each key.
        key_times.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
    }
    const auto &items = g_inputSession->get_candidates();
    auto match = std::find_if(items.begin(), items.end(), [&](const WordItem &item) { return item.word == word; });
    REQUIRE(match != items.end());
    const WordItem chosen = *match;
    REQUIRE(FanyNamedPipe::AccumulateSentenceCandidate(chosen, 1, 1));
    REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::NeedToCreateWord);
}
bool WaitFor(SentenceTranslation::Status status, std::chrono::seconds budget = std::chrono::seconds(8))
{
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (SentenceTranslation::Current().status == status)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
std::wstring DeliveredText(uint64_t request)
{
    FanyImeSentenceTranslation::ResultAssembler assembler;
    bool complete = false;
    for (const auto &delivery : deliveries)
        if (delivery.type == FanyImeWorkerReplyType::SentenceTranslationReady)
            complete = assembler.Append(FanyImeSentenceTranslation::ParseFrame(delivery.data), request);
    REQUIRE(complete);
    return assembler.Text();
}
void ResetSentence()
{
    FanyNamedPipe::CancelSentenceConfirmation();
    SentenceTranslation::Cancel();
    GlobalIme::composition.clear();
    g_inputSession->reset_state();
    Global::candidate_ui.set_items({});
    deliveries.clear();
}
} // namespace

int RunSentenceOutputProbe(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try
    {
        const bool offline = std::string(argv[1]) == "offline";
        const bool incomplete = std::string(argv[1]) == "incomplete";
        const bool configured = std::string(argv[1]) == "configured";
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
        FanyNamedPipe::HandleSentenceConfirmation(1, 1, 41);
        REQUIRE(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100));
        const bool waiting = Global::MsgTypeToTsf == FanyImeReplyType::SentenceTranslationPending;
        REQUIRE(waiting || (offline && Global::MsgTypeToTsf == FanyImeReplyType::NeedToCreateWord));
        REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你好"));

        if (offline || incomplete)
        {
            REQUIRE(WaitFor(SentenceTranslation::Status::Failed));
            FanyNamedPipe::DeliverSentenceTranslationIfReady();
            REQUIRE_EQ(deliveries.size(), static_cast<std::size_t>(waiting ? 1 : 0));
            if (waiting)
                REQUIRE_EQ(deliveries.front().type, FanyImeWorkerReplyType::SentenceTranslationFailed);
            REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你好"));
        }
        else
        {
            REQUIRE(WaitFor(SentenceTranslation::Status::Ready));
            REQUIRE_EQ(SentenceTranslation::Current().source, std::string("你好"));
            FanyNamedPipe::DeliverSentenceTranslationIfReady(); // No second Enter.
            const auto english = DeliveredText(41);
            REQUIRE(!english.empty());
            if (!configured)
                REQUIRE_EQ(english, std::wstring(L"Hello."));
            const auto count = deliveries.size();
            FanyNamedPipe::DeliverSentenceTranslationIfReady();
            REQUIRE_EQ(deliveries.size(), count);
            // Even a completed preview never turns an extra Space/digit/click into a commit.
            REQUIRE(FanyNamedPipe::RetainSelectedSentence(1));
            REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::NeedToCreateWord);
            REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你好"));
            probe_current = false;
            REQUIRE(!FanyNamedPipe::RefreshSentenceTranslationPreview());
            probe_current = true;

            if (!configured)
            {
                // A cancelled slow socket must not hold the latest sentence for six seconds.
                SentenceTranslation::Request("旧句测试", true);
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                const auto newer = std::chrono::steady_clock::now();
                SentenceTranslation::Request("新句测试", true);
                REQUIRE(WaitFor(SentenceTranslation::Status::Ready));
                REQUIRE(std::chrono::steady_clock::now() - newer < std::chrono::seconds(2));
                REQUIRE_EQ(SentenceTranslation::Current().translation, std::string("New sentence."));
            }
            SentenceTranslation::Cancel();
            const auto cached = std::chrono::steady_clock::now();
            SentenceTranslation::Request("你好");
            REQUIRE(SentenceTranslation::Current().cache_hit);
            REQUIRE(SentenceTranslation::Current().status == SentenceTranslation::Status::Ready);
            REQUIRE(std::chrono::steady_clock::now() - cached < std::chrono::milliseconds(100));
        }
        FanyNamedPipe::PrepareSentenceConfirmation(1, 1, true);
        REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::CommitExactText);
        REQUIRE_EQ(Global::candidate_ui.selected_text, std::wstring(L"你好"));
        if (!offline && !incomplete)
        {
            ResetSentence();
            TypeAndSelect("wo", "我");
            TypeAndSelect("jintian", "今天");
            TypeAndSelect("hen", "很");
            TypeAndSelect("gaoxing", "高兴");
            REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("我今天很高兴"));
            Global::CandidatePageSnapshot page;
            FanyNamedPipe::PopulateSentencePreview(page);
            REQUIRE_EQ(page.sentence_source, std::wstring(L"我今天很高兴"));
            REQUIRE(page.sentence_buffer_only);
            FanyNamedPipe::HandleSentenceConfirmation(1, 1, 42);
            REQUIRE(Global::MsgTypeToTsf == FanyImeReplyType::SentenceTranslationPending);
            // An edit/Shift before completion revokes the accepted Enter; source stays intact.
            FanyNamedPipe::CancelSentenceConfirmation();
            REQUIRE(WaitFor(SentenceTranslation::Status::Ready));
            FanyNamedPipe::DeliverSentenceTranslationIfReady();
            REQUIRE(deliveries.empty());
            FanyNamedPipe::PopulateSentencePreview(page);
            REQUIRE(!page.sentence_preview.empty());
            FanyNamedPipe::PrepareSentenceConfirmation(1, 1);
            REQUIRE(Global::MsgTypeToTsf == Global::DataFromServerMsgType::CommitExactText);
            if (!configured)
                REQUIRE_EQ(Global::candidate_ui.selected_text, std::wstring(L"I am very happy today."));
            ResetSentence();
            TypeAndSelect("ni", "你");
            TypeAndSelect("hao", "好");
        }
        REQUIRE(GlobalIme::composition.restore_last_selection());
        REQUIRE_EQ(GlobalIme::composition.creating_word.word, std::string("你"));
        REQUIRE_EQ(GlobalIme::composition.raw_input_with_cases, std::string("hao"));
        SentenceTranslation::Cancel();
        REQUIRE(SentenceTranslation::Current().status != SentenceTranslation::Status::Ready);
        SentenceTranslation::Stop();
        g_inputSession.reset();
        GlobalIme::composition.clear();
        curl_global_cleanup();
        std::sort(key_times.begin(), key_times.end());
        std::cout << "sentence probe passed; engine key+candidate samples=" << key_times.size()
                  << " p95_ms=" << key_times[(key_times.size() - 1) * 95 / 100] << " max_ms=" << key_times.back()
                  << '\n';
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
