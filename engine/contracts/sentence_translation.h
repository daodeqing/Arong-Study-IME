#pragma once

#include "ipc_protocol_limits.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

// Sentence results are tied to the Enter request, not to the focus at delivery time.
// Ordered worker frames preserve the existing 199 UTF-16 unit text limit in full.
namespace FanyImeSentenceTranslation
{
constexpr std::uint64_t WaitMs = 125000;
constexpr std::size_t MaxSentenceTranslationUnits = 4096;
constexpr std::size_t MaxFrameUnits = FanyImePipeLimits::CandidateTextCapacity - 1;
constexpr std::size_t MaxTextUnits = MaxSentenceTranslationUnits;

inline bool ParseNumber(std::wstring_view value, std::uint64_t &number)
{
    number = 0;
    if (value.empty())
        return false;
    for (const auto ch : value)
    {
        if (ch < L'0' || ch > L'9' || number > ((std::numeric_limits<std::uint64_t>::max)() - (ch - L'0')) / 10)
            return false;
        number = number * 10 + (ch - L'0');
    }
    return true;
}

struct Frame
{
    bool valid = false;
    std::uint64_t request = 0;
    std::size_t offset = 0;
    std::size_t total = 0;
    std::wstring text;
};

inline Frame ParseFrame(std::wstring_view payload)
{
    Frame frame;
    if (payload.size() >= FanyImePipeLimits::CandidateTextCapacity)
        return frame;
    const auto first = payload.find(L'\t');
    const auto colon = payload.find(L':', first == payload.npos ? 0 : first + 1);
    const auto second = payload.find(L'\t', colon == payload.npos ? 0 : colon + 1);
    std::uint64_t offset = 0, total = 0;
    if (first == payload.npos || colon == payload.npos || second == payload.npos ||
        !ParseNumber(payload.substr(0, first), frame.request) || frame.request == 0 ||
        frame.request == (std::numeric_limits<std::uint64_t>::max)() ||
        !ParseNumber(payload.substr(first + 1, colon - first - 1), offset) ||
        !ParseNumber(payload.substr(colon + 1, second - colon - 1), total) || total == 0 || total > MaxSentenceTranslationUnits ||
        offset >= total || payload.size() - second - 1 > total - offset || payload.size() == second + 1 ||
        payload.find(L'\0') != payload.npos)
        return {};
    frame.offset = static_cast<std::size_t>(offset);
    frame.total = static_cast<std::size_t>(total);
    frame.text = payload.substr(second + 1);
    frame.valid = true;
    return frame;
}

inline std::vector<std::wstring> EncodeResult(std::uint64_t request, const std::wstring &text)
{
    std::vector<std::wstring> frames;
    if (request == 0 || request == (std::numeric_limits<std::uint64_t>::max)() || text.empty() ||
        text.size() > MaxSentenceTranslationUnits || text.find(L'\0') != text.npos)
        return frames;
    for (std::size_t offset = 0; offset < text.size();)
    {
        auto header =
            std::to_wstring(request) + L'\t' + std::to_wstring(offset) + L':' + std::to_wstring(text.size()) + L'\t';
        if (header.size() >= MaxFrameUnits)
            return {};
        const auto count = (std::min)(MaxFrameUnits - header.size(), text.size() - offset);
        frames.push_back(header + text.substr(offset, count));
        offset += count;
    }
    return frames;
}

class ResultAssembler
{
  public:
    bool Append(const Frame &frame, std::uint64_t expected)
    {
        if (!frame.valid || frame.request != expected)
            return false;
        if (frame.offset == 0)
        {
            text_.clear();
            total_ = frame.total;
        }
        if (frame.total != total_ || frame.offset != text_.size())
        {
            text_.clear();
            total_ = 0;
            return false;
        }
        text_ += frame.text;
        return text_.size() == total_;
    }
    const std::wstring &Text() const
    {
        return text_;
    }

  private:
    std::size_t total_ = 0;
    std::wstring text_;
};
} // namespace FanyImeSentenceTranslation
