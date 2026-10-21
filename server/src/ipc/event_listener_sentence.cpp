// 中文选词只维护组合；整句译文由 Enter 确认，与空格选词分开。
#include "ipc/event_listener_internal.h"
#include "cloud/sentence_translation.h"
#include "config/ime_config.h"
#include "global/globals.h"
#include "ipc/output_language_policy.h"
#include "utils/common_utils.h"
#include "engine/contracts/sentence_translation.h"

#include <algorithm>

namespace
{
constexpr std::size_t kMaxSentenceUnits = 80;
struct PendingConfirmation
{
    uint64_t client = 0;
    uint64_t activation = 0;
    uint64_t request = 0;
    uint64_t revision = 0;
    ULONGLONG deadline = 0;
    std::string source;
} pending;

bool HasSentenceText()
{
    return g_inputSession && !GlobalIme::composition.creating_word.word.empty();
}
void SentenceNotice(const wchar_t *text)
{
    event_listener_detail::PostCaretStateBadge({text}, Global::Point[0], Global::Point[1]);
}
void HoldSentence()
{
    const auto raw = string_to_wstring(g_inputSession->get_pinyin_sequence_with_cases());
    const auto word = string_to_wstring(GlobalIme::composition.creating_word.word);
    const auto payload = raw + L'\t' + word + L'\t' + word + raw;
    Global::MsgTypeToTsf = payload.size() < FanyImePipeLimits::CandidateTextCapacity
                               ? Global::DataFromServerMsgType::NeedToCreateWord
                               : Global::DataFromServerMsgType::NavigationIgnored;
    Global::candidate_ui.selected_text =
        payload.size() < FanyImePipeLimits::CandidateTextCapacity ? payload : std::wstring{};
}
bool SelectRemainingSentence(uint64_t client_id, uint64_t activation_epoch)
{
    for (unsigned step = 0; step < 128 && !g_inputSession->get_pinyin_sequence_with_cases().empty(); ++step)
    {
        const auto before = g_inputSession->get_pinyin_sequence_with_cases();
        if (Global::candidate_ui.items.empty())
            FanyNamedPipe::PrepareCandidateList(client_id, activation_epoch);
        WordItem item;
        if (!FanyNamedPipe::ResolveCandidateItem(Global::candidate_ui.selected_index_in_page + 1, item) ||
            !FanyNamedPipe::AccumulateSentenceCandidate(item, client_id, activation_epoch) ||
            g_inputSession->get_pinyin_sequence_with_cases() == before)
            return false;
    }
    return g_inputSession->get_pinyin_sequence_with_cases().empty();
}
std::string SentenceStatus(const SentenceTranslation::Snapshot &snapshot, const std::string &source)
{
    if (!event_listener_detail::IsEnglishOutputEnabled())
        return "中文输出：Enter 或 Ctrl+Enter 提交；Shift 切换英文";
    if (pending.request)
        return "正在翻译，完成后自动上屏；继续编辑可撤销，Ctrl+Enter 提交中文";
    if (snapshot.source != source || snapshot.status == SentenceTranslation::Status::Idle)
        return "空格只选中文；Enter 翻译整句，Ctrl+Enter 提交中文";
    switch (snapshot.status)
    {
    case SentenceTranslation::Status::Pending:
        return "正在预览整句；空格继续选词，Enter 确认英文";
    case SentenceTranslation::Status::Ready:
        return string_to_wstring(snapshot.translation).size() < FanyImePipeLimits::CandidateTextCapacity
                   ? "Enter 确认英文；Ctrl+Enter 提交中文；Shift 切换输出"
                   : "英文译文过长，请分句输入；Ctrl+Enter 可提交中文";
    case SentenceTranslation::Status::Failed:
        return "翻译超时或连接失败：Enter 重试，Ctrl+Enter 提交中文";
    case SentenceTranslation::Status::Unconfigured:
        return "请在设置中配置接口与模型并测试连接；Ctrl+Enter 可提交中文";
    default:
        return "空格只选中文；Enter 确认英文，Ctrl+Enter 提交中文";
    }
}
} // namespace

namespace FanyNamedPipe
{
using namespace event_listener_detail;

void CancelSentenceConfirmation()
{
    pending = {};
}
void HoldSentenceComposition()
{
    HoldSentence();
}
bool RetainSelectedSentence(uint64_t client_id)
{
    if (!SentenceModeForClient(client_id) || !HasSentenceText() ||
        !g_inputSession->get_pinyin_sequence_with_cases().empty())
        return false;
    HoldSentence();
    return true;
}
bool SentenceModeForClient(uint64_t client_id)
{
    if (!GetConfiguredSentenceTranslationEnabled() || !ClientNegotiatedSentenceTranslation(client_id) ||
        !ClientNegotiatedCompositionRestore(client_id) || !g_inputSession || g_english_input_mode ||
        event_listener_detail::IsSensitiveInputField())
        return false;
    const auto scheme = g_inputSession->current_scheme_type();
    return (scheme == SchemeType::Quanpin || scheme == SchemeType::Shuangpin) &&
           !event_listener_detail::IsSpecialModeCompositionActive(g_inputSession->get_pinyin_sequence_with_cases());
}
void SendSentenceTranslationState(uint64_t client_id, uint64_t activation_epoch)
{
    if (!ClientNegotiatedSentenceTranslation(client_id))
        return;
    const auto scheme = GetConfiguredActiveInputScheme();
    const bool enabled =
        GetConfiguredSentenceTranslationEnabled() && (scheme == SchemeType::Quanpin || scheme == SchemeType::Shuangpin);
    const std::wstring payload = enabled ? L"1" : L"0";
    if (activation_epoch != 0)
        SendToTsfWorkerThreadClientViaNamedpipe(client_id, activation_epoch,
                                                FanyImeWorkerReplyType::SentenceTranslationChanged, payload);
    else
        SendToTsfWorkerThreadClientViaNamedpipe(client_id, FanyImeWorkerReplyType::SentenceTranslationChanged, payload);
}
void UpdateSentenceTranslationPreview()
{
    if (!HasSentenceText())
    {
        SentenceTranslation::Cancel();
        return;
    }
    SentenceTranslation::Request(CandidateTextForOutput(GlobalIme::composition.creating_word.word));
}
void PopulateSentencePreview(Global::CandidatePageSnapshot &page)
{
    const auto owner = SnapshotCandidateUiOwner();
    if (!owner || !SentenceModeForClient(owner.client_id) || !HasSentenceText())
        return;
    const auto source = CandidateTextForOutput(GlobalIme::composition.creating_word.word);
    const auto snapshot = SentenceTranslation::Current();
    page.sentence_source = string_to_wstring(source);
    if (snapshot.source == source && snapshot.status == SentenceTranslation::Status::Ready)
        page.sentence_preview = string_to_wstring(snapshot.translation);
    page.sentence_status = string_to_wstring(SentenceStatus(snapshot, source));
    if (!ClientNegotiatedSentenceAutoCommit(owner.client_id))
        page.sentence_status = L"前端为旧版：Ctrl+Enter 确认英文；请更新输入法以使用整句回车提交";
    page.sentence_buffer_only = g_inputSession->get_pinyin_sequence_with_cases().empty();
}
bool RefreshSentenceTranslationPreview()
{
    const auto owner = SnapshotCandidateUiOwner();
    if (!owner || !IsPipeActivationCurrent(owner.client_id, owner.activation_epoch) ||
        !SentenceModeForClient(owner.client_id) || !HasSentenceText() ||
        !g_inputSession->get_pinyin_sequence_with_cases().empty())
        return false;
    // Keep a live page while only selected Chinese remains. Its row cannot commit:
    // all Space/digit/mouse selection paths return HoldSentenceComposition.
    auto &ui = Global::candidate_ui;
    ui.set_items({WordItem{std::string{}, "整句编辑中", 0, CandidateSource::Fallback}});
    ui.page_index = 0;
    ui.selected_index_in_page = 0;
    RefreshCandidatePageUi(true);
    return true;
}
bool AccumulateSentenceCandidate(const WordItem &item, uint64_t client_id, uint64_t activation_epoch)
{
    if (!SentenceModeForClient(client_id) || !g_inputSession ||
        g_inputSession->get_pinyin_sequence_with_cases().empty() || item.word.empty() ||
        item.scheme == SchemeType::JapaneseRomaji)
        return false;
    auto &composition = GlobalIme::composition;
    const std::string word = CandidateTextForOutput(item.word);
    if (!OutputLanguagePolicy::ContainsHan(word) && item.source != CandidateSource::EnglishDictionary)
    {
        SentenceNotice(L"请先选择中文候选；Ctrl+Enter 可提交已选中文");
        return false;
    }
    const auto selected_units = string_to_wstring(composition.creating_word.word + word).size();
    const auto raw_units = string_to_wstring(g_inputSession->get_pinyin_sequence_with_cases()).size();
    if (selected_units > kMaxSentenceUnits || selected_units + raw_units > 94)
    {
        SentenceNotice(L"句子较长，请先提交或分句输入");
        return false;
    }
    CancelSentenceConfirmation();
    const auto raw = g_inputSession->get_pinyin_sequence_with_cases();
    const auto transition =
        g_inputSession->advance_composition_after_selection(item.pinyin, item.word, item.canonical_pinyin, item.scheme);
    composition.push_selection_snapshot(transition.continues_composition ? transition.consumed_raw_input_with_cases
                                                                         : raw);
    composition.creating_word.active = true;
    composition.creating_word.word += word;
    composition.creating_word.preedit = composition.creating_word.word;
    // 整句组合不属于用户造词；不写词库、候选学习或持久化上下文。
    if (!transition.continues_composition)
        g_inputSession->reset_state();
    composition.raw_input_with_cases = g_inputSession->get_pinyin_sequence_with_cases();
    composition.segmented_pinyin = g_inputSession->get_pinyin_segmentation_with_cases();
    composition.shuangpin_forms_source.clear();
    composition.caret_position = composition.raw_input_with_cases.size();
    PublishCandidateUiOwner(client_id, activation_epoch);
    UpdateCloudInput("");
    UpdateEnglishInput("");
    UpdateAiInput("");
    UpdateSentenceTranslationPreview();
    if (composition.raw_input_with_cases.empty())
        RefreshSentenceTranslationPreview();
    else
        PrepareCandidateList(client_id, activation_epoch);
    HoldSentence();
    return true;
}
void PrepareSentenceConfirmation(uint64_t client_id, uint64_t activation_epoch, bool force_chinese, uint64_t request_id)
{
    CancelSentenceConfirmation();
    if (!SelectRemainingSentence(client_id, activation_epoch) || !HasSentenceText())
    {
        SentenceNotice(L"请先用空格选择中文候选");
        HoldSentence();
        return;
    }
    const auto chinese = CandidateTextForOutput(GlobalIme::composition.creating_word.word);
    if (force_chinese || !IsEnglishOutputEnabled())
    {
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::CommitExactText;
        Global::candidate_ui.selected_text = string_to_wstring(chinese);
        return;
    }
    // Enter flushes the preview debounce; failures can be retried with one Enter.
    SentenceTranslation::Request(chinese, true, true);
    const auto snapshot = SentenceTranslation::Current();
    if (snapshot.source == chinese && snapshot.status == SentenceTranslation::Status::Ready)
    {
        if (string_to_wstring(snapshot.translation).size() < FanyImePipeLimits::CandidateTextCapacity)
        {
            Global::MsgTypeToTsf = Global::DataFromServerMsgType::CommitExactText;
            Global::candidate_ui.selected_text = string_to_wstring(snapshot.translation);
            return;
        }
        SentenceNotice(L"英文译文过长，请分句输入");
    }
    else if (snapshot.status == SentenceTranslation::Status::Pending && request_id != 0 &&
             request_id != FANY_IME_NO_REQUEST_ID && ClientNegotiatedSentenceAutoCommit(client_id))
    {
        pending = {client_id,
                   activation_epoch,
                   request_id,
                   snapshot.revision,
                   GetTickCount64() + FanyImeSentenceTranslation::WaitMs,
                   chinese};
    }
    else
        SentenceNotice(snapshot.status == SentenceTranslation::Status::Unconfigured ? L"请先配置模型并测试连接"
                                                                                    : L"整句翻译中，请稍后按回车确认");
    RefreshSentenceTranslationPreview();
    HoldSentence();
    if (pending.request)
        Global::MsgTypeToTsf = FanyImeReplyType::SentenceTranslationPending;
}
void HandleSentenceConfirmation(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id, bool force_chinese)
{
    PrepareSentenceConfirmation(client_id, activation_epoch, force_chinese, request_id);
    SendCurrentDataToClient(client_id, activation_epoch, request_id);
}
void DeliverSentenceTranslationIfReady()
{
    if (!pending.request)
        return;
    const auto owner = SnapshotCandidateUiOwner();
    const auto snapshot = SentenceTranslation::Current();
    if (!owner || owner.client_id != pending.client || owner.activation_epoch != pending.activation ||
        !IsPipeActivationCurrent(pending.client, pending.activation) || !SentenceModeForClient(pending.client) ||
        !IsEnglishOutputEnabled() || !HasSentenceText() || !g_inputSession->get_pinyin_sequence_with_cases().empty() ||
        CandidateTextForOutput(GlobalIme::composition.creating_word.word) != pending.source ||
        snapshot.revision != pending.revision)
    {
        CancelSentenceConfirmation();
        return;
    }
    const bool expired = GetTickCount64() >= pending.deadline;
    if (!expired && snapshot.status == SentenceTranslation::Status::Pending)
        return;
    const auto confirmation = std::move(pending);
    pending = {};
    const auto frames =
        !expired && snapshot.status == SentenceTranslation::Status::Ready
            ? FanyImeSentenceTranslation::EncodeResult(confirmation.request, string_to_wstring(snapshot.translation))
            : std::vector<std::wstring>{};
    if (frames.empty())
    {
        SendToTsfWorkerThreadClientViaNamedpipe(confirmation.client, confirmation.activation,
                                                FanyImeWorkerReplyType::SentenceTranslationFailed,
                                                std::to_wstring(confirmation.request));
        SentenceNotice(L"本次未提交：Enter 重试，Ctrl+Enter 提交中文");
        return;
    }
    for (const auto &frame : frames)
        if (!SendToTsfWorkerThreadClientViaNamedpipe(confirmation.client, confirmation.activation,
                                                     FanyImeWorkerReplyType::SentenceTranslationReady, frame))
            break;
    // Retain the source until the TSF edit session actually commits. Its ordinary
    // candidate teardown clears it; editing first instead cancels both sides.
}
bool HandleSentencePunctuation(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id, WCHAR punctuation)
{
    if (!SentenceModeForClient(client_id) ||
        (!HasSentenceText() && g_inputSession->get_pinyin_sequence_with_cases().empty()))
        return false;
    CancelSentenceConfirmation();
    if (!SelectRemainingSentence(client_id, activation_epoch))
    {
        HoldSentence();
        SendCurrentDataToClient(client_id, activation_epoch, request_id);
        return true;
    }
    if (punctuation != 0 && string_to_wstring(GlobalIme::composition.creating_word.word).size() < kMaxSentenceUnits)
    {
        auto &composition = GlobalIme::composition;
        composition.selection_history.push_back({"", composition.creating_word, -1, "", false});
        composition.creating_word.active = true;
        composition.creating_word.word += wstring_to_string(std::wstring(1, punctuation));
        composition.creating_word.preedit = composition.creating_word.word;
        UpdateSentenceTranslationPreview();
    }
    RefreshSentenceTranslationPreview();
    HoldSentence();
    SendCurrentDataToClient(client_id, activation_epoch, request_id);
    return true;
}
} // namespace FanyNamedPipe
