module;
#include <unordered_map>
#include <string_view>
export module pancakes.basic.keywords;

import pancakes.basic.tokentype;

struct CaseInsensitiveHash
{
    size_t operator()(std::string_view s) const
    {
        size_t h{};
        for (char c : s)
        {
            c &= ~0x20;
            h = h * 131 + static_cast<unsigned char>(c);
        }
        return h;
    }
};

struct CaseInsensitiveEqual
{
    bool operator()(std::string_view a, std::string_view b) const
    {
        if (a.length() != b.length())
            return false;
        for (size_t i{}; i < a.length(); ++i)
            if ((a[i] & ~0x20) != (b[i] & ~0x20))
                return false;
        return true;
    }
};

export std::unordered_map<std::string_view, TokenType, CaseInsensitiveHash, CaseInsensitiveEqual> keywords
{
    { "PRINT", TokenType::PRINT },
        { "?", TokenType::PRINT },
        { "REM", TokenType::REM }
};
