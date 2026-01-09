module;
#include <cstddef>
#include <string_view>
export module pancakes.basic.token;

import pancakes.basic.tokentype;

struct SourceLocation
{
    std::size_t line{};
    std::size_t column{};
};

export struct Token
{
    TokenType type{ TokenType::UNKNOWN };
    std::string_view name{};
    SourceLocation position{};
};
