#include "tests/includes/test_framework.h"
#include "cloud/sentence_translation.h"

TEST_CASE(sentence_translation_requests_complete_sentence_instead_of_dictionary_gloss)
{
    const AiTranslation::Config config{"local", "http://127.0.0.1:8317/v1/chat/completions", "", "test"};
    const auto request = SentenceTranslation::detail::BuildRequest(config, "我今天很高兴。");
    REQUIRE_EQ(request["messages"][1]["content"].get<std::string>(), std::string("我今天很高兴。"));
    REQUIRE(request["messages"][0]["content"].get<std::string>().find("complete Chinese sentence") !=
            std::string::npos);
    REQUIRE_EQ(request["stream"].get<bool>(), false);
    REQUIRE(request["max_tokens"].get<int>() == 2048);
}
TEST_CASE(sentence_translation_routed_deepseek_disables_reasoning_and_openai_uses_compatible_parameters)
{
    AiTranslation::Config routed{"local", "http://127.0.0.1:8317/v1/chat/completions", "", "cn/deepseek-v4.1-flash"};
    const auto request = SentenceTranslation::detail::BuildRequest(routed, "明天不去。");
    REQUIRE_EQ(request["thinking"]["type"].get<std::string>(), std::string("disabled"));
    AiTranslation::Config reasoning{"openai", "https://example.invalid/v1/chat/completions", "", "gpt-5"};
    const auto openai = SentenceTranslation::detail::BuildRequest(reasoning, "两杯咖啡。");
    REQUIRE(openai.contains("max_completion_tokens"));
    REQUIRE(!openai.contains("max_tokens"));
    REQUIRE(!openai.contains("temperature"));
    REQUIRE(openai.contains("reasoning_effort"));
}

TEST_CASE(sentence_translation_rejects_empty_chinese_numeric_and_oversized_answers)
{
    REQUIRE_EQ(SentenceTranslation::detail::ParseResult("  I am very happy today.  "),
               std::string("I am very happy today."));
    REQUIRE(SentenceTranslation::detail::ParseResult("我今天很高兴。").empty());
    REQUIRE(SentenceTranslation::detail::ParseResult("1234").empty());
    REQUIRE(SentenceTranslation::detail::ParseResult("").empty());
    REQUIRE(SentenceTranslation::detail::ParseResult(std::string(4097, 'x')).empty());
    REQUIRE_EQ(SentenceTranslation::detail::ParseResult(R"({"translation":"Hello."})"), std::string("Hello."));
}

TEST_CASE(sentence_translation_accepts_complete_answers_around_packet_and_total_limits)
{
    for (std::size_t units : {199u, 200u, 278u, 4096u})
    {
        const std::string translation(units, 'x');
        REQUIRE_EQ(SentenceTranslation::detail::ParseResult(translation), translation);
    }
    REQUIRE(SentenceTranslation::detail::ParseResult(std::string(4097, 'x')).empty());
}

TEST_CASE(sentence_translation_presets_cover_daily_academic_creative_and_custom_slots)
{
    const auto &presets = SentenceTranslation::BuiltinPromptPresets();
    REQUIRE_EQ(presets.size(), std::size_t{4});
    REQUIRE_EQ(std::string(presets[0].id), std::string("daily"));
    REQUIRE_EQ(std::string(presets[0].name), std::string("日常交流"));
    REQUIRE_EQ(std::string(presets[1].id), std::string("casual"));
    REQUIRE_EQ(std::string(presets[1].name), std::string("口语模式"));
    REQUIRE_EQ(std::string(presets[2].id), std::string("academic"));
    REQUIRE_EQ(std::string(presets[2].name), std::string("学术科研"));
    REQUIRE_EQ(std::string(presets[3].id), std::string("creative"));
    REQUIRE_EQ(std::string(presets[3].name), std::string("写作创作"));

    for (const char *valid : {"daily", "casual", "academic", "creative", "custom_1", "custom_2", "custom_3"})
        REQUIRE(SentenceTranslation::IsValidPromptId(valid));
    REQUIRE(!SentenceTranslation::IsValidPromptId("unknown"));

    AiAssistantConfig ai;
    ai.prompt_id = "daily";
    REQUIRE(SentenceTranslation::ResolveConfiguredPrompt(ai).find("natural conversational English") !=
            std::string::npos);
    ai.prompt_id = "casual";
    const auto casual = SentenceTranslation::ResolveConfiguredPrompt(ai);
    REQUIRE(casual.find("use lowercase style") != std::string::npos);
    REQUIRE(casual.find("case of URLs, email addresses, code") != std::string::npos);
    const AiTranslation::Config casual_config{"local", "http://127.0.0.1:8317/v1/chat/completions", "", "test",
                                              "casual", casual};
    const auto casual_request = SentenceTranslation::detail::BuildRequest(casual_config, "Visit Example.com/Case");
    REQUIRE_EQ(casual_request["messages"][0]["content"].get<std::string>(), casual);
    REQUIRE_EQ(casual_request["messages"][1]["content"].get<std::string>(), std::string("Visit Example.com/Case"));
    REQUIRE_EQ(SentenceTranslation::detail::ParseResult("hey, see Example.com/Case"),
               std::string("hey, see Example.com/Case"));
    ai.prompt_id = "academic";
    REQUIRE(SentenceTranslation::ResolveConfiguredPrompt(ai).find("precise academic English") != std::string::npos);
    ai.prompt_id = "creative";
    REQUIRE(SentenceTranslation::ResolveConfiguredPrompt(ai).find("imagery and voice") != std::string::npos);

    // Stock candidate JSON prompt in custom_1 falls back to daily translation prompt.
    ai.prompt_id = "custom_1";
    ai.prompt_custom_1 = AiAssistantConfig{}.prompt;
    REQUIRE(SentenceTranslation::ResolveConfiguredPrompt(ai).find("natural conversational English") !=
            std::string::npos);

    // User-written custom translation prompt is preserved.
    ai.prompt_id = "custom_2";
    ai.prompt_custom_2 = "Translate into concise British English.";
    REQUIRE_EQ(SentenceTranslation::ResolveConfiguredPrompt(ai),
               std::string("Translate into concise British English."));

    TranslationConfig snapshot;
    snapshot.ai = {"local",
                   "http://127.0.0.1:8317/v1/chat/completions",
                   "",
                   "workbuddy-intl/deepseek-v4.1-flash",
                   "academic",
                   SentenceTranslation::ResolveConfiguredPrompt([] {
                       AiAssistantConfig c;
                       c.prompt_id = "academic";
                       return c;
                   }())};
    const auto translated_cfg = AiTranslation::FromConfiguredTranslation(snapshot);
    const auto request = SentenceTranslation::detail::BuildRequest(translated_cfg, "该结果提示两者可能相关。");
    REQUIRE(request["messages"][0]["content"].get<std::string>().find("precise academic English") != std::string::npos);
}
