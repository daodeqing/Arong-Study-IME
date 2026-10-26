#pragma once

#include "../../../engine/contracts/sentence_translation.h"

// Owner-thread state. A worker result can be assembled before the Enter ACK,
// but neither arrival order is sufficient to authorize a text edit on its own.
class SentenceAutoCommitState
{
  public:
    struct Session
    {
        std::uint64_t focus = 0;
        std::uint64_t composition = 0;
        std::uint64_t generation = 0;
        unsigned reset = 0;
    };

    void Begin(std::uint64_t request, Session session, std::uint64_t now)
    {
        *this = {};
        request_ = request;
        session_ = session;
        deadline_ = now + FanyImeSentenceTranslation::WaitMs;
    }
    void Cancel()
    {
        *this = {};
    }
    bool Current(Session session, std::uint64_t now) const
    {
        return request_ != 0 && session.focus != 0 && session.composition != 0 && now < deadline_ &&
               session.focus == session_.focus && session.composition == session_.composition &&
               session.generation == session_.generation && session.reset == session_.reset;
    }
    bool Accept(std::uint64_t request, const std::wstring &source)
    {
        if (request == 0 || request != request_ || source.empty())
            return false;
        accepted_ = true;
        source_ = source;
        return true;
    }
    bool Append(const FanyImeSentenceTranslation::Frame &frame)
    {
        if (!frame.valid || frame.request != request_ || ready_)
            return false;
        ready_ = result_.Append(frame, request_);
        return ready_;
    }
    bool Schedule(Session session, std::uint64_t now)
    {
        if (!accepted_ || !ready_ || scheduled_ || !Current(session, now))
            return false;
        scheduled_ = true;
        return true;
    }
    bool CanApply(std::uint64_t request, Session session, std::uint64_t now, const std::wstring &source) const
    {
        return scheduled_ && accepted_ && ready_ && request == request_ && source == source_ && Current(session, now);
    }
    std::uint64_t Request() const
    {
        return request_;
    }
    const std::wstring &Source() const
    {
        return source_;
    }
    const std::wstring &Translation() const
    {
        return result_.Text();
    }

  private:
    std::uint64_t request_ = 0;
    Session session_;
    std::uint64_t deadline_ = 0;
    bool accepted_ = false;
    bool ready_ = false;
    bool scheduled_ = false;
    std::wstring source_;
    FanyImeSentenceTranslation::ResultAssembler result_;
};
