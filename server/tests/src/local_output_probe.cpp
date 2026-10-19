#include "tests/includes/test_framework.h"
#include "ai/openai_compatible_url.h"
#include "cloud/output_translation.h"
#include "config/ime_config.h"
#include "ipc/output_language_policy.h"
#include "session/engine_input_session.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

// A separate process isolates config and worker lifetime from the unit-test registry.
int RunLocalOutputProbe(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try
    {
        InitImeConfig();
        EngineInputSession session(SchemeType::Quanpin);
        for (const char ch : std::string("nihao"))
            session.handle_key(static_cast<UINT>(ch - 'a' + 'A'), 0, static_cast<WCHAR>(ch));
        const auto &candidates = session.get_candidates();
        REQUIRE(std::any_of(candidates.begin(), candidates.end(),
                            [](const WordItem &item) { return item.word == "你好"; }));

        const auto config = GetConfiguredTranslation();
        const std::string models_url = OpenAiCompatibleUrl::ModelsEndpoint(config->ai.endpoint);
        const bool online = std::string(argv[1]) == "online";
        if (online)
        {
            std::string response;
            CURL *curl = curl_easy_init();
            REQUIRE(curl != nullptr);
            curl_easy_setopt(curl, CURLOPT_URL, models_url.c_str());
            curl_easy_setopt(curl, CURLOPT_PROXY, "");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000L);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(
                curl, CURLOPT_WRITEFUNCTION, +[](char *data, size_t size, size_t count, void *user) -> size_t {
                    static_cast<std::string *>(user)->append(data, size * count);
                    return size * count;
                });
            const auto result = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            REQUIRE(result == CURLE_OK);
            REQUIRE_EQ(nlohmann::json::parse(response)["data"][0]["id"].get<std::string>(),
                       std::string("local-test-model"));
        }

        OutputTranslation::Start();
        OutputTranslation::Request({"你好"});
        if (online)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!OutputTranslation::Lookup("你好") && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const auto commit_started = std::chrono::steady_clock::now();
        const auto translation = OutputTranslation::Lookup("你好");
        const auto kind = OutputLanguagePolicy::ResolveCommit(true, true, false, translation.has_value());
        const std::string output = kind == OutputLanguagePolicy::CommitKind::EnglishTranslation ? *translation : "你好";
        REQUIRE(std::chrono::steady_clock::now() - commit_started < std::chrono::milliseconds(100));
        REQUIRE_EQ(output, online ? std::string("Hello") : std::string("你好"));
        REQUIRE(OutputLanguagePolicy::ResolveCommit(OutputLanguagePolicy::Toggle(true), true, false, true) ==
                OutputLanguagePolicy::CommitKind::ExistingText);
        OutputTranslation::Stop();
        REQUIRE(!OutputTranslation::Lookup("你好"));
        std::cout << "local output probe passed: " << (online ? "English, Shift Chinese" : "immediate Chinese fallback")
                  << '\n';
        curl_global_cleanup();
        return 0;
    }
    catch (const std::exception &error)
    {
        OutputTranslation::Stop();
        curl_global_cleanup();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
