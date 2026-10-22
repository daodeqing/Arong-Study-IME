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
