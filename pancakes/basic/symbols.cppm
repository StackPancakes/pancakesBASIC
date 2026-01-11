module;
#include <unordered_map>
export module pancakes.basic.symbols;

import pancakes.basic.tokentype;

export std::unordered_map<char, TokenType> const symbols
{
        { ',', TokenType::COMMA },
        { ';', TokenType::SEMICOLON },
        { '(', TokenType::LPAREN },
        { ')', TokenType::RPAREN },
        { '\'', TokenType::APOSTROPHE }
};
