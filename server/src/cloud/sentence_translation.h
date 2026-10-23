#pragma once
#include "cloud/ai_translation.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SentenceTranslation
{
struct PromptPreset
{
    std::string_view id;
    std::string_view name;
    std::string_view prompt;
};
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
    bool cache_hit = false;
};
const std::vector<PromptPreset> &BuiltinPromptPresets();
bool IsValidPromptId(std::string_view id);
bool IsBuiltinPromptId(std::string_view id);
std::string ResolveConfiguredPrompt(const AiAssistantConfig &config);
void Start();
void Stop();
void Cancel();
void Request(std::string source, bool immediate = false, bool retry = false);
Snapshot Current();
namespace detail
{
std::string ResolvePrompt(const AiTranslation::Config &config);
nlohmann::json BuildRequest(const AiTranslation::Config &config, const std::string &source);
std::string ParseResult(const std::string &content);
} // namespace detail
} // namespace SentenceTranslation
