// 可选整句翻译：中文只留在组合区，后台预览，用户明确确认后才提交。
#include "ipc/event_listener_internal.h"
#include "cloud/sentence_translation.h"
#include "config/ime_config.h"
#include "global/globals.h"
#include "ipc/output_language_policy.h"
#include "utils/common_utils.h"

#include <algorithm>

namespace
{
constexpr std::size_t kMaxSentenceUnits = 80;

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
    // 旧协议长度固定，绝不把截断的中文/拼音送给 DLL 覆盖现有组合。
    Global::MsgTypeToTsf = payload.size() < FanyImePipeLimits::CandidateTextCapacity
                               ? Global::DataFromServerMsgType::NeedToCreateWord
                               : Global::DataFromServerMsgType::NavigationIgnored;
    Global::candidate_ui.selected_text =
        payload.size() < FanyImePipeLimits::CandidateTextCapacity ? payload : std::wstring{};
}

bool SelectRemainingSentence(uint64_t client_id, uint64_t activation_epoch)
{
    // 一次确认可以选完本次拼音中的多个词；每次必须实际消耗拼音，不能循环卡住按键线程。
    for (unsigned step = 0; step < 128 && !g_inputSession->get_pinyin_sequence_with_cases().empty(); ++step)
    {
        const auto before = g_inputSession->get_pinyin_sequence_with_cases();
        if (Global::candidate_ui.items.empty())
            FanyNamedPipe::PrepareCandidateList(client_id, activation_epoch);
        WordItem item;
        if (!FanyNamedPipe::ResolveCandidateItem(Global::candidate_ui.selected_index_in_page + 1, item) ||
            !FanyNamedPipe::AccumulateSentenceCandidate(item, client_id, activation_epoch))
            return false;
        if (g_inputSession->get_pinyin_sequence_with_cases() == before)
            return false;
    }
    return g_inputSession->get_pinyin_sequence_with_cases().empty();
}
} // namespace

namespace FanyNamedPipe
{
using namespace event_listener_detail;
void HoldSentenceComposition()
{
    HoldSentence();
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
    SentenceTranslation::Request(
        event_listener_detail::CandidateTextForOutput(GlobalIme::composition.creating_word.word));
}

bool RefreshSentenceTranslationPreview()
{
    const auto owner = event_listener_detail::SnapshotCandidateUiOwner();
    if (!owner || !IsPipeActivationCurrent(owner.client_id, owner.activation_epoch) ||
        !SentenceModeForClient(owner.client_id) || !HasSentenceText() ||
        !g_inputSession->get_pinyin_sequence_with_cases().empty())
        return false;
    const auto snapshot = SentenceTranslation::Current();
    const auto source = event_listener_detail::CandidateTextForOutput(GlobalIme::composition.creating_word.word);
    std::string label;
    if (!event_listener_detail::IsEnglishOutputEnabled())
        label = "中文待提交：Enter 确认，Shift 切换英文";
    else if (snapshot.source != source)
        label = "整句已保留：Ctrl+Enter 请求翻译，Enter 提交中文";
    else
    {
        switch (snapshot.status)
        {
        case SentenceTranslation::Status::Ready:
            label = string_to_wstring(snapshot.translation).size() < FanyImePipeLimits::CandidateTextCapacity
                        ? snapshot.translation
                        : "英文译文过长，请分句输入；Enter 可提交中文";
            break;
        case SentenceTranslation::Status::Pending:
            label = "正在翻译整句…完成后 Ctrl+Enter 或空格确认";
            break;
        case SentenceTranslation::Status::Failed:
            label = "翻译失败：Ctrl+Enter 重试，Enter 提交中文";
            break;
        case SentenceTranslation::Status::Unconfigured:
            label = "请先在 AI 与云联想中配置模型并测试连接";
            break;
        default:
            label = "Ctrl+Enter 翻译整句，Enter 提交中文";
            break;
        }
    }
    auto &ui = Global::candidate_ui;
    ui.set_items({WordItem{std::string{}, label, 0, CandidateSource::Fallback}});
    ui.page_index = 0;
    ui.selected_index_in_page = 0;
    event_listener_detail::RefreshCandidatePageUi(true);
    return true;
}

bool AccumulateSentenceCandidate(const WordItem &item, uint64_t client_id, uint64_t activation_epoch)
{
    if (!SentenceModeForClient(client_id) || !g_inputSession ||
        g_inputSession->get_pinyin_sequence_with_cases().empty() || item.word.empty() ||
        item.scheme == SchemeType::JapaneseRomaji)
        return false;
    auto &composition = GlobalIme::composition;
    const std::string word = event_listener_detail::CandidateTextForOutput(item.word);
    if (!OutputLanguagePolicy::ContainsHan(word) && item.source != CandidateSource::EnglishDictionary)
    {
        SentenceNotice(L"请先选择中文候选；Enter 可提交已选中文");
        return false;
    }
    const auto selected_units = string_to_wstring(composition.creating_word.word + word).size();
    const auto raw_units = string_to_wstring(g_inputSession->get_pinyin_sequence_with_cases()).size();
    if (selected_units > kMaxSentenceUnits || selected_units + raw_units > 94)
    {
        SentenceNotice(L"句子较长，请先提交或分句输入");
        return false;
    }
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
    event_listener_detail::PublishCandidateUiOwner(client_id, activation_epoch);
    event_listener_detail::UpdateCloudInput("");
    event_listener_detail::UpdateEnglishInput("");
    event_listener_detail::UpdateAiInput("");
    UpdateSentenceTranslationPreview();
    if (composition.raw_input_with_cases.empty())
        RefreshSentenceTranslationPreview();
    else
        PrepareCandidateList(client_id, activation_epoch);
    HoldSentence();
    return true;
}

void PrepareSentenceConfirmation(uint64_t client_id, uint64_t activation_epoch, bool force_chinese)
{
    if (!SelectRemainingSentence(client_id, activation_epoch) || !HasSentenceText())
    {
        SentenceNotice(L"请先用空格选择中文候选");
        HoldSentence();
        return;
    }
    const auto chinese = event_listener_detail::CandidateTextForOutput(GlobalIme::composition.creating_word.word);
    if (force_chinese || !event_listener_detail::IsEnglishOutputEnabled())
    {
        Global::MsgTypeToTsf = Global::DataFromServerMsgType::CommitExactText;
        Global::candidate_ui.selected_text = string_to_wstring(chinese);
        return;
    }
    auto snapshot = SentenceTranslation::Current();
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
    else
    {
        if (snapshot.source == chinese && snapshot.status == SentenceTranslation::Status::Failed)
            SentenceTranslation::Cancel();
        UpdateSentenceTranslationPreview();
        snapshot = SentenceTranslation::Current();
        SentenceNotice(snapshot.status == SentenceTranslation::Status::Unconfigured ? L"请先配置模型并测试连接"
                                                                                    : L"整句翻译中，请稍后再次确认");
    }
    RefreshSentenceTranslationPreview();
    HoldSentence();
}

void HandleSentenceConfirmation(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id, bool force_chinese)
{
    PrepareSentenceConfirmation(client_id, activation_epoch, force_chinese);
    SendCurrentDataToClient(client_id, activation_epoch, request_id);
}

bool HandleSentencePunctuation(uint64_t client_id, uint64_t activation_epoch, uint64_t request_id, WCHAR punctuation)
{
    if (!SentenceModeForClient(client_id) ||
        (!HasSentenceText() && g_inputSession->get_pinyin_sequence_with_cases().empty()))
        return false;
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
