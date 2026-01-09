module;
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <format>
#include <iostream>
export module pancakes.basic.lexer;

import pancakes.basic.token;
import pancakes.basic.tokentype;
import pancakes.basic.keywords;

export class Lexer
{
    std::string input;
    std::size_t position{};
    std::size_t line{ 1 };
    std::size_t column{ 1 };

    public:
    explicit Lexer(std::string_view source) : input{ source } {}
    std::vector<Token> tokenize()
    {
        std::vector<Token> tokens;
        while (true)
        {
            Token t{ next() };
            tokens.push_back(t);
            if (t.type == TokenType::END_OF_FILE)
                break;
        }
        return tokens;
    }

    private:

    [[nodiscard]] bool eof() const
    {
        return position >= input.size() || input[position] == '\0';
    }

    [[nodiscard]] char peek() const
    {
        return eof() ? '\0' : input[position];
    }

    char advance()
    {
        char const c{ peek() };
        ++position;
        if (c == '\n') { ++line; column = 1; }
        else ++column;
        return c;
    }

    void skip()
    {
        while (!eof())
        {
            if (char const c{ peek() }; std::isspace(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) == 0xA0)
                advance();
            else break;
        }
    }

    Token next()
    {
        while (true)
        {
            skip();

            std::size_t const ln{ line }, col{ column };

            if (eof())
                return { TokenType::END_OF_FILE, "", { ln, col } };

            char const c{ advance() };

            if (std::isalpha(static_cast<unsigned char>(c)))
            {
                std::size_t start{ position - 1 };
                while (!eof() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
                    advance();

                std::string_view token{ input.data() + start, position - start };

                auto it{ keywords.find(token) };

                if (it != keywords.end() && it->second == TokenType::REM)
                {
                    while (!eof() && peek() != '\n')
                        advance();
                    if (!eof() && peek() == '\n')
                        advance();
                    continue;
                }

                return { it != keywords.end() ? it->second : TokenType::IDENTIFIER, token, { ln, col } };
            }

            if (std::isdigit(static_cast<unsigned char>(c)))
            {
                std::size_t start{ position - 1 };
                while (!eof() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.'))
                    advance();

                return { TokenType::NUMBER, { input.data() + start, position - start }, { ln, col } };
            }


            if (c == '"')
            {
                std::size_t start{ position };
                while (!eof() && peek() != '"')
                    advance();
                if (eof())
                    throw std::runtime_error{ "Unterminated string" };

                advance();
                return { TokenType::STRING, { input.data() + start, position - start - 1 }, { ln, col } };
            }

            std::string_view single{ input.data() + position - 1, 1 };
            auto sit{ keywords.find(single) };
            if (sit != keywords.end())
                return { sit->second, single, { ln, col } };

            return { TokenType::UNKNOWN, { input.data() + position - 1, 1 }, { ln, col } };
        }
    }

    public:
    friend std::ostream& operator<<(std::ostream& out, Lexer lexer)
    {
        for (auto const& tokens : lexer.tokenize())
        {
            if (tokens.type == TokenType::END_OF_FILE)
                break;
            out << std::format("Token type: [{}], Positions: {{{}, {}}}, Value: {}\n", TokenTypeName(tokens.type),
                    tokens.position.line, tokens.position.column, tokens.name);
        }
        return out;
    }
};
