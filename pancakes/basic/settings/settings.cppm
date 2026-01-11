module;
#include <string_view>
export module pancakes.basic.setting.settings;

export struct CaseInsensitiveHash
{
    size_t operator()(std::string_view const s) const
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

export struct CaseInsensitiveEqual
{
    bool operator()(std::string_view const a, std::string_view const b) const
    {
        if (a.length() != b.length())
            return false;
        for (size_t i{}; i < a.length(); ++i)
            if ((a[i] & ~0x20) != (b[i] & ~0x20))
                return false;
        return true;
    }
};