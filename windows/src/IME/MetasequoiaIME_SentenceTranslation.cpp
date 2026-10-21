#include "Private.h"
#include "Globals.h"
#include "MetasequoiaIME.h"
#include "CompositionProcessorEngine.h"
#include "Global/FanyDefines.h"
#include "Ipc.h"
#include "FanyLog.h"
#include "MetasequoiaIMEInternal.h"

using namespace metasequoia_ime_detail;

namespace
{
class CSentenceTranslationEditSession : public CEditSessionBase
{
  public:
    CSentenceTranslationEditSession(CMetasequoiaIME *service, ITfContext *context, uint64_t request)
        : CEditSessionBase(service, context), request_(request)
    {
    }
    STDMETHODIMP DoEditSession(TfEditCookie cookie) override
    {
        return _pTextService->_ApplySentenceTranslationCommit(cookie, _pContext, request_);
    }

  private:
    uint64_t request_;
};
} // namespace

SentenceAutoCommitState::Session CMetasequoiaIME::_SentenceSession() const
{
    return {_CaptureFocusSessionToken(), _CaptureCompositionEpoch(), _deferredKeyFocusGeneration,
            _localSessionResetToken.load(std::memory_order_acquire)};
}
void CMetasequoiaIME::_CancelSentenceTranslationCommit()
{
    if (_sentenceCommit.Request() == 0)
        return;
    _sentenceCommit.Cancel();
    if (_msgWndHandle)
        KillTimer(_msgWndHandle, TIMER_SENTENCE_TRANSLATION);
}
void CMetasequoiaIME::_BeginSentenceTranslationCommit(uint64_t request)
{
    _CancelSentenceTranslationCommit();
    // Keys already queued after Enter are edits too. Do not re-arm an intent
    // that their real key callbacks already cancelled.
    if (!SupportsSentenceAutoCommit() || !IsSentenceTranslationEnabled() || !_deferredKeyDowns.empty() ||
        !_msgWndHandle || request == 0 || request == FANY_IME_NO_REQUEST_ID)
        return;
    _sentenceCommit.Begin(request, _SentenceSession(), GetTickCount64());
    if (!SetTimer(_msgWndHandle, TIMER_SENTENCE_TRANSLATION, 100, nullptr))
        _sentenceCommit.Cancel();
}
void CMetasequoiaIME::_AcceptSentenceTranslationCommit(uint64_t request)
{
    if (_sentenceCommit.Current(_SentenceSession(), GetTickCount64()) &&
        _sentenceCommit.Accept(request, GlobalIme::word_for_creating_word))
        PostMessage(_msgWndHandle, WM_TrySentenceTranslationCommit, 0, 0);
}
void CMetasequoiaIME::_ReceiveSentenceTranslationResult(UINT token, bool failed)
{
    WorkerCandidateCommit delivery;
    if (!_TakeServerCandidateCommit(token, delivery))
        return;
    if (!_sentenceCommit.Current(_SentenceSession(), GetTickCount64()) ||
        !_IsFocusSessionCurrent(delivery.focusToken) || !_IsCompositionEpochCurrent(delivery.compositionEpoch))
        return;
    if (failed)
    {
        std::uint64_t request = 0;
        if (FanyImeSentenceTranslation::ParseNumber(delivery.text, request) && request == _sentenceCommit.Request())
            _CancelSentenceTranslationCommit();
        return;
    }
    if (_sentenceCommit.Append(FanyImeSentenceTranslation::ParseFrame(delivery.text)))
        _TrySentenceTranslationCommit();
}
void CMetasequoiaIME::_TrySentenceTranslationCommit()
{
    if (!_pThreadMgr || !_pCompositionProcessorEngine || _localSessionResetPending.load(std::memory_order_acquire) ||
        !IsSentenceTranslationEnabled() || !Global::OutputLanguageEnglish.load(std::memory_order_relaxed) ||
        !_IsComposing() || _pCompositionProcessorEngine->GetVirtualKeyLength() != 0 ||
        GlobalIme::word_for_creating_word != _sentenceCommit.Source() ||
        !_sentenceCommit.Schedule(_SentenceSession(), GetTickCount64()))
        return;
    ITfDocumentMgr *document = nullptr;
    ITfContext *context = nullptr;
    HRESULT result = E_FAIL;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&document)) && document)
    {
        if (SUCCEEDED(document->GetTop(&context)) && context &&
            _IsFocusSessionCurrent(_CaptureFocusSessionToken(), context))
        {
            auto *session =
                new (std::nothrow) CSentenceTranslationEditSession(this, context, _sentenceCommit.Request());
            if (session)
            {
                HRESULT editResult = E_FAIL;
                result = context->RequestEditSession(_tfClientId, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
                                                     &editResult);
                session->Release();
                if (SUCCEEDED(result) && FAILED(editResult))
                    result = editResult;
            }
        }
    }
    if (context)
        context->Release();
    if (document)
        document->Release();
    if (FAILED(result))
        _CancelSentenceTranslationCommit();
}
HRESULT CMetasequoiaIME::_ApplySentenceTranslationCommit(TfEditCookie cookie, ITfContext *context, uint64_t request)
{
    // Revalidate inside the granted edit session: it can run after another key,
    // a reset, focus change, or the five-second waiting window.
    if (_localSessionResetPending.load(std::memory_order_acquire) || !IsSentenceTranslationEnabled() ||
        !Global::OutputLanguageEnglish.load(std::memory_order_relaxed) ||
        !_IsFocusSessionCurrent(_CaptureFocusSessionToken(), context) || !_IsComposing() ||
        !_pCompositionProcessorEngine || _pCompositionProcessorEngine->GetVirtualKeyLength() != 0 ||
        !_sentenceCommit.CanApply(request, _SentenceSession(), GetTickCount64(), GlobalIme::word_for_creating_word))
        return S_FALSE;
    TF_SELECTION selection = {};
    ULONG fetched = 0;
    ITfRange *compositionRange = nullptr;
    LONG start = 1, end = 1;
    bool atEnd = false;
    if (SUCCEEDED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) && fetched == 1 &&
        selection.range && _pComposition && SUCCEEDED(_pComposition->GetRange(&compositionRange)) && compositionRange)
        atEnd = SUCCEEDED(selection.range->CompareStart(cookie, compositionRange, TF_ANCHOR_END, &start)) &&
                SUCCEEDED(selection.range->CompareEnd(cookie, compositionRange, TF_ANCHOR_END, &end)) && start == 0 &&
                end == 0;
    if (selection.range)
        selection.range->Release();
    if (compositionRange)
        compositionRange->Release();
    if (!atEnd)
    {
        _CancelSentenceTranslationCommit();
        return S_FALSE;
    }
    const std::wstring translation = _sentenceCommit.Translation();
    if (translation.empty() || translation.size() >= FanyImePipeLimits::CandidateTextCapacity)
        return S_FALSE;
    CStringRange text;
    text.Set(translation.c_str(), translation.size());
    // Require the existing composition. Never create a range at a new caret
    // for a late model answer, and never synthesize an application Enter.
    const HRESULT result = _InsertTextToComposition(cookie, context, &text);
    _CancelSentenceTranslationCommit();
    if (SUCCEEDED(result))
        _HandleCompleteCommitFirst(cookie, context);
    DebugTsfKeyLatency(L"sentence-auto-commit", request, 0, result);
    return result;
}
