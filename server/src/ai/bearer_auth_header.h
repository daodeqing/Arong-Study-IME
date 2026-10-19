#pragma once

#include <optional>
#include <string>

namespace BearerAuthHeader
{
inline std::optional<std::string> ForToken(const std::string &token)
{
    if (token.empty())
        return std::nullopt;
    return "Authorization: Bearer " + token;
}
} // namespace BearerAuthHeader
