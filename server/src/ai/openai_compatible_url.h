#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>

namespace OpenAiCompatibleUrl
{
inline std::string Trim(std::string value)
{
    const auto non_space = [](unsigned char ch) { return !std::isspace(ch); };
    const auto begin = std::find_if(value.begin(), value.end(), non_space);
    const auto end = std::find_if(value.rbegin(), value.rend(), non_space).base();
    return begin < end ? std::string(begin, end) : std::string();
}

inline bool EndsWith(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string NormalizeBaseUrl(std::string value)
{
    value = Trim(std::move(value));
    const std::size_t scheme_end = value.find("://");
    if (scheme_end != std::string::npos)
    {
        std::transform(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(scheme_end), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    }
    const std::size_t suffix_at = value.find_first_of("?#");
    std::string path = value.substr(0, suffix_at);
    const std::string query = suffix_at == std::string::npos ? std::string() : value.substr(suffix_at);
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    for (const std::string suffix : {"/chat/completions", "/models"})
    {
        if (EndsWith(path, suffix))
        {
            path.resize(path.size() - suffix.size());
            break;
        }
    }
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path + query;
}

inline std::string Endpoint(const std::string &base_url, const std::string &path_suffix)
{
    const std::string base = NormalizeBaseUrl(base_url);
    if (base.empty())
        return {};
    const std::size_t suffix_at = base.find_first_of("?#");
    const std::string path = base.substr(0, suffix_at);
    const std::string query = suffix_at == std::string::npos ? std::string() : base.substr(suffix_at);
    return path + path_suffix + query;
}

inline std::string ChatCompletionsEndpoint(const std::string &base_url)
{
    return Endpoint(base_url, "/chat/completions");
}

inline std::string ModelsEndpoint(const std::string &base_url)
{
    return Endpoint(base_url, "/models");
}

inline bool IsLoopbackUrl(const std::string &url)
{
    const std::size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos)
        return false;
    std::string scheme = url.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (scheme != "http" && scheme != "https")
        return false;

    const std::size_t authority_begin = scheme_end + 3;
    const std::size_t authority_end = url.find_first_of("/?#", authority_begin);
    const std::string authority = url.substr(authority_begin, authority_end - authority_begin);
    if (authority.empty() || authority.find('@') != std::string::npos)
        return false;

    std::string host;
    if (authority.front() == '[')
    {
        const std::size_t close = authority.find(']');
        if (close == std::string::npos || (close + 1 < authority.size() && authority[close + 1] != ':'))
            return false;
        host = authority.substr(0, close + 1);
    }
    else
    {
        const std::size_t colon = authority.find(':');
        if (colon != std::string::npos && authority.find(':', colon + 1) != std::string::npos)
            return false;
        host = authority.substr(0, colon);
    }
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return host == "localhost" || host == "127.0.0.1" || host == "[::1]";
}
} // namespace OpenAiCompatibleUrl
