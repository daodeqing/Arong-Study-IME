#include "tests/includes/test_framework.h"

#include "ai/bearer_auth_header.h"
#include "ai/openai_compatible_url.h"

TEST_CASE(OpenAiCompatibleBaseUrlBuildsModelAndChatEndpoints)
{
    const std::string base = " http://127.0.0.1:8317/v1/ ";
    REQUIRE_EQ(OpenAiCompatibleUrl::NormalizeBaseUrl(base), std::string("http://127.0.0.1:8317/v1"));
    REQUIRE_EQ(OpenAiCompatibleUrl::ModelsEndpoint(base), std::string("http://127.0.0.1:8317/v1/models"));
    REQUIRE_EQ(OpenAiCompatibleUrl::ChatCompletionsEndpoint(base),
               std::string("http://127.0.0.1:8317/v1/chat/completions"));
}

TEST_CASE(OpenAiCompatibleBaseUrlAcceptsEndpointSuffixAndQuery)
{
    const std::string configured = "http://localhost:8317/v1/chat/completions?api-version=1";
    REQUIRE_EQ(OpenAiCompatibleUrl::NormalizeBaseUrl(configured),
               std::string("http://localhost:8317/v1?api-version=1"));
    REQUIRE_EQ(OpenAiCompatibleUrl::ModelsEndpoint(configured),
               std::string("http://localhost:8317/v1/models?api-version=1"));
    REQUIRE_EQ(OpenAiCompatibleUrl::ChatCompletionsEndpoint(configured), configured);
}

TEST_CASE(OpenAiCompatibleProviderRequiresLoopbackAndOmitsEmptyAuthorization)
{
    REQUIRE(OpenAiCompatibleUrl::IsLoopbackUrl("http://127.0.0.1:8317/v1/chat/completions"));
    REQUIRE(OpenAiCompatibleUrl::IsLoopbackUrl("http://localhost:8317/v1/models"));
    REQUIRE(OpenAiCompatibleUrl::IsLoopbackUrl("http://[::1]:8317/v1/models"));
    REQUIRE(!OpenAiCompatibleUrl::IsLoopbackUrl("https://api.example.com/v1/models"));
    REQUIRE(!OpenAiCompatibleUrl::IsLoopbackUrl("http://127.0.0.1.example.com/v1/models"));
    REQUIRE(!OpenAiCompatibleUrl::IsLoopbackUrl("http://user@127.0.0.1/v1/models"));
    REQUIRE(!BearerAuthHeader::ForToken(""));
    const auto authorization = BearerAuthHeader::ForToken("local-secret");
    REQUIRE(authorization.has_value());
    REQUIRE_EQ(*authorization, std::string("Authorization: Bearer local-secret"));
}
