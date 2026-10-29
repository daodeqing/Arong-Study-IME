#pragma once
#include "windows_ipc.h"
#include <algorithm>
#include <string>
#include <vector>

// Additive, request-correlated exact commits; legacy wire packet sizes stay unchanged.
namespace FanyImeLongCommit
{
using Text = std::basic_string<FanyImeWireChar>;
constexpr std::size_t MaxUnits = 4096;
constexpr std::size_t HeaderUnits = 3;
constexpr std::size_t ChunkUnits = FanyImePipeLimits::CandidateTextCapacity - HeaderUnits - 1;

inline bool ValidText(const Text &text)
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto unit = static_cast<unsigned>(text[i]);
        if (unit == 0 || (unit >= 0xDC00 && unit <= 0xDFFF))
            return false;
        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            if (++i >= text.size() || text[i] < 0xDC00 || text[i] > 0xDFFF)
                return false;
        }
    }
    return true;
}

inline std::vector<FanyImeNamedpipeDataToTsf> Encode(const Text &text, std::uint64_t request)
{
    std::vector<FanyImeNamedpipeDataToTsf> frames;
    if (request == 0 || request == FANY_IME_NO_REQUEST_ID || text.empty() || text.size() > MaxUnits || !ValidText(text))
        return frames;
    for (std::size_t offset = 0; offset < text.size();)
    {
        auto count = (std::min)(ChunkUnits, text.size() - offset);
        if (offset + count < text.size() && text[offset + count - 1] >= 0xD800 && text[offset + count - 1] <= 0xDBFF)
            --count;
        FanyImeNamedpipeDataToTsf packet{};
        packet.msg_type = FanyImeReplyType::CommitExactTextFrame;
        packet.request_id = request;
        packet.candidate_string[0] = static_cast<FanyImeWireChar>(offset);
        packet.candidate_string[1] = static_cast<FanyImeWireChar>(text.size());
        packet.candidate_string[2] = static_cast<FanyImeWireChar>(count);
        std::copy_n(text.data() + offset, count, packet.candidate_string + HeaderUnits);
        frames.push_back(packet);
        offset += count;
    }
    return frames;
}

class Assembler
{
  public:
    enum class Result
    {
        Invalid,
        Pending,
        Complete
    };
    Result Append(const FanyImeNamedpipeDataToTsf &frame)
    {
        const auto offset = static_cast<std::size_t>(frame.candidate_string[0]);
        const auto total = static_cast<std::size_t>(frame.candidate_string[1]);
        const auto count = static_cast<std::size_t>(frame.candidate_string[2]);
        if (frame.msg_type != FanyImeReplyType::CommitExactTextFrame || frame.request_id == 0 ||
            frame.request_id == FANY_IME_NO_REQUEST_ID || total == 0 || total > MaxUnits || count == 0 ||
            count > ChunkUnits || offset != text_.size() || offset + count > total ||
            frame.candidate_string[HeaderUnits + count] != 0)
            return Result::Invalid;
        if (offset == 0)
        {
            request_ = frame.request_id;
            total_ = total;
        }
        if (frame.request_id != request_ || total != total_)
            return Result::Invalid;
        Text chunk(frame.candidate_string + HeaderUnits, count);
        if (!ValidText(chunk))
            return Result::Invalid;
        text_ += chunk;
        return text_.size() == total_ ? Result::Complete : Result::Pending;
    }
    const Text &text() const
    {
        return text_;
    }

  private:
    std::uint64_t request_ = 0;
    std::size_t total_ = 0;
    Text text_;
};
} // namespace FanyImeLongCommit
