#include "chat_completions.h"

#include "ai/bearer_auth_header.h"
#include "ai/openai_compatible_url.h"
#include "utils/network_proxy.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <exception>

namespace
{
constexpr size_t kMaxResponseBytes = 1024 * 1024;

struct ResponseBuffer
{
    std::string text;
    bool overflow = false;
};

size_t WriteResponse(char *data, size_t size, size_t count, void *user)
{
    const size_t bytes = size * count;
    auto *buffer = static_cast<ResponseBuffer *>(user);
    if (bytes > kMaxResponseBytes - (std::min)(buffer->text.size(), kMaxResponseBytes))
    {
        buffer->overflow = true;
        return 0;
    }
    buffer->text.append(data, bytes);
    return bytes;
}
} // namespace

namespace ChatCompletions
{
std::optional<std::string> FetchContent(const std::string &endpoint, const std::string &token,
                                        const std::string &payload, long timeout_ms,
                                        const std::function<bool()> &cancelled)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return std::nullopt;

    // Deliberately exclude the API token and the request body from logs.
    ResponseBuffer response;
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    const auto authorization = BearerAuthHeader::ForToken(token);
    if (authorization)
        headers = curl_slist_append(headers, authorization->c_str());

    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    if (OpenAiCompatibleUrl::IsLoopbackUrl(endpoint))
        curl_easy_setopt(curl, CURLOPT_PROXY, "");
    else
        NetworkProxy::ApplyToCurl(curl);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (cancelled)
    {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelled);
        curl_easy_setopt(
            curl, CURLOPT_XFERINFOFUNCTION, +[](void *context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                return (*static_cast<const std::function<bool()> *>(context))() ? 1 : 0;
            });
    }

    CURLcode performed = CURLE_FAILED_INIT;
    if (!cancelled)
        performed = curl_easy_perform(curl);
    else
    {
        // A progress callback alone can sleep for a second on an idle socket.
        // Drive cancellable requests in small waits so a new sentence starts promptly.
        CURLM *multi = curl_multi_init();
        if (multi)
        {
            if (curl_multi_add_handle(multi, curl) == CURLM_OK)
            {
                int running = 0;
                CURLMcode status = CURLM_OK;
                do
                {
                    if (cancelled())
                    {
                        performed = CURLE_ABORTED_BY_CALLBACK;
                        break;
                    }
                    status = curl_multi_perform(multi, &running);
                    if (status != CURLM_OK || !running)
                        break;
                    status = curl_multi_poll(multi, nullptr, 0, 25, nullptr);
                } while (status == CURLM_OK);
                if (performed != CURLE_ABORTED_BY_CALLBACK && status == CURLM_OK)
                {
                    int remaining = 0;
                    while (auto *message = curl_multi_info_read(multi, &remaining))
                        if (message->msg == CURLMSG_DONE)
                            performed = message->data.result;
                }
                curl_multi_remove_handle(multi, curl);
            }
            curl_multi_cleanup(multi);
        }
    }
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (performed != CURLE_OK || response.overflow || status < 200 || status >= 300)
        return std::nullopt;

    try
    {
        const auto outer = nlohmann::json::parse(response.text);
        return outer.at("choices").at(0).at("message").at("content").get<std::string>();
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}
} // namespace ChatCompletions
