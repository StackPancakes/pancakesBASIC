module;
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <format>
// ReSharper disable once CppUnusedIncludeDirective
#include <iostream>
export module pancakes.basic.lexer;

import pancakes.basic.token;
import pancakes.basic.tokentype;
import pancakes.basic.keywords;
import pancakes.basic.symbols;


export class Lexer
{
public:
    explicit Lexer(std::string_view const source) : input{ source } {}

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

    friend std::ostream& operator<<(std::ostream& out, Lexer lexer)
    {
        for (auto const& [type, name, pos] : lexer.tokenize())
        {
            if (type == TokenType::END_OF_FILE)
                break;
            out << std::format("Token type: [{}], Positions: {{{}, {}}}, Value: {}\n",
                TokenTypeName(type), pos.line, pos.column, name);
        }
        return out;
    }

private:
    std::string input;
    std::size_t position{};
    std::size_t line{ 1 };
    std::size_t column{ 1 };

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
        char c{ peek() };
        ++position;
        if (c == '\n')
        {
            ++line;
            column = 1;
        }
        else
            ++column;
        return c;
    }

    void skipWhitespace()
    {
        while (!eof())
        {
            char const c{ peek() };
            if (c == ' ' || c == '\t' || static_cast<unsigned char>(c) == 0xA0)
                advance();
            else
                break;
        }
    }

    Token next()
    {
        while (true)
        {
            skipWhitespace();

            std::size_t const ln{ line }, col{ column };

            if (eof())
                return { TokenType::END_OF_FILE, "", { ln, col } };

            char const c{ advance() };

            if (c == '\n')
                return { TokenType::END_OF_LINE, "\n", { ln, col } };

            if (c == '\r')
            {
                if (!eof() && peek() == '\n')
                    advance();
                return { TokenType::END_OF_LINE, "\r\n", { ln, col } };
            }

            if (std::isalpha(static_cast<unsigned char>(c)))
                return readIdentifierOrKeyword(ln, col);

            if (std::isdigit(static_cast<unsigned char>(c)))
                return readNumber(ln, col);

            if (c == '"')
                return readString(ln, col);

            return readSingleOrUnknown(c, ln, col);
        }
    }

    Token readIdentifierOrKeyword(size_t const ln, size_t const col)
    {
        std::size_t const start{ position - 1 };
        while (!eof())
        {
            if (char const c{ peek() }; std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                advance();
            else
                break;
        }

        std::string_view const token{ input.data() + start, position - start };
        auto const it{ keywords.find(token) };

        // Handle REM comment
        if (it != keywords.end() && it->second == TokenType::REM)
        {
            while (!eof() && peek() != '\n') advance();
            if (!eof() && peek() == '\n') advance();
            return next(); // skip comment, read next token
        }

        return { it != keywords.end() ? it->second : TokenType::IDENTIFIER, token, { ln, col } };
    }

    Token readNumber(size_t const ln, size_t const col)
    {
        std::size_t const start{ position - 1 };
        while (!eof())
        {
            if (char const c{ peek() }; std::isdigit(static_cast<unsigned char>(c)) || c == '.')
                advance();
            else
                break;
        }

        return { TokenType::NUMBER, { input.data() + start, position - start }, { ln, col } };
    }

    Token readString(std::size_t const ln, std::size_t const col)
    {
        std::size_t const start{ position };
        while (!eof() && peek() != '"') advance();
        if (eof())
            throw std::runtime_error{ "Unterminated string" };
        advance(); // consume closing quote
        return { TokenType::STRING, { input.data() + start, position - start - 1 }, { ln, col } };
    }

    [[nodiscard]] Token readSingleOrUnknown(char const c, std::size_t const ln, std::size_t const col) const
    {
        if (auto const it{ symbols.find(c) }; it != symbols.end())
            return { it->second, std::string_view{ &input[position - 1], 1 }, { ln, col } };

        return { TokenType::UNKNOWN, std::string_view{ &input[position - 1], 1 }, { ln, col } };
    }
};