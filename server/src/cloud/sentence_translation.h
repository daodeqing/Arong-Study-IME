#pragma once
#include "cloud/ai_translation.h"
#include <cstdint>
#include <string>

namespace SentenceTranslation
{
enum class Status
{
    Idle,
    Pending,
    Ready,
    Failed,
    Unconfigured
};
struct Snapshot
{
    std::string source;
    std::string translation;
    Status status = Status::Idle;
    std::uint64_t revision = 0;
    long elapsed_ms = 0;
};
void Start();
void Stop();
void Cancel();
void Request(std::string source);
Snapshot Current();
namespace detail
{
nlohmann::json BuildRequest(const AiTranslation::Config &config, const std::string &source);
std::string ParseResult(const std::string &content);
} // namespace detail
} // namespace SentenceTranslation
