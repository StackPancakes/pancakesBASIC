module;
#include <string_view>
#include <charconv>
export module pancakes.basic.util;

export bool isNumericLiteral(std::string_view const s)
{
    float dummy;
    auto [ptr, ec]{ std::from_chars(s.data(), s.data() + s.size(), dummy) };
    return ec == std::errc{} && ptr == s.data() + s.size();
}