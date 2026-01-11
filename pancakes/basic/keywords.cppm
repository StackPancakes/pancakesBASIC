module;
#include <unordered_map>
#include <string_view>
export module pancakes.basic.keywords;

import pancakes.basic.setting.settings;
import pancakes.basic.tokentype;

export std::unordered_map<std::string_view, TokenType, CaseInsensitiveHash, CaseInsensitiveEqual> const keywords
{
    { "PRINT", TokenType::PRINT },
        { "REM", TokenType::REM },
    { "INPUT", TokenType::INPUT },
    { "TAB", TokenType::TAB },
    { "SPC", TokenType::SPC }
};
