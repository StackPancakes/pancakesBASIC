module;
#include <stdexcept>
#include <format>
#include <string>
#include <span>
#include <variant>

export module pancakes.basic.parser;

import pancakes.basic.tokentype;
import pancakes.basic.token;
import pancakes.basic.AST;

export struct Parser
{
    std::span<Token const> tokens;
    size_t pos{ 0 };

    explicit Parser(std::span<Token const> const t) : tokens{ t }
    {}

    [[nodiscard]] Token const& peek() const
    {
        if (pos >= tokens.size())
            return tokens.back();
        return tokens[pos];
    }

    Token const& consume()
    {
        if (pos >= tokens.size())
            return tokens.back();
        return tokens[pos++];
    }

    ProgramNode parse()
    {
        ProgramNode program{};

        while (peek().type != TokenType::END_OF_FILE)
        {
            if (peek().type == TokenType::END_OF_LINE)
            {
                consume();
                continue;
            }

            switch (peek().type)
            {
                case TokenType::PRINT:
                {
                    program.statements.emplace_back(parsePrint());
                    break;
                }
                case TokenType::INPUT:
                {
                    program.statements.emplace_back(parseInput());
                    break;
                }
                default:
                {
                    Token const tok{ peek() };
                    throw std::runtime_error{ std::format("Unexpected statement at ({}, {})", tok.position.line, tok.position.column) };
                }
            }
        }

        return program;
    }

private:
    PrintNode parsePrint()
    {
        consume();
        PrintNode node{};

        while (true)
        {
            if (Token const next{ peek() }; next.type == TokenType::END_OF_LINE || next.type == TokenType::COLON || next.type == TokenType::END_OF_FILE)
                break;

            node.items.push_back(parsePrintItem());
        }

        return node;
    }

    PrintItem parsePrintItem()
    {
        auto const [type, name, position]{ peek() };
        PrintItem item{};

        switch (type)
        {
            case TokenType::STRING:
            {
                consume();
                item.value = PrintItem::Expression{ std::string{ name }, true };
                break;
            }
            case TokenType::IDENTIFIER:
            case TokenType::NUMBER:
            {
                consume();
                item.value = PrintItem::Expression{ std::string{ name }, false };
                break;
            }
            case TokenType::TAB:
            {
                PrintItem::Tab tab{};
                parseTab(tab);
                item.value = tab;
                break;
            }
            case TokenType::SPC:
            {
                PrintItem::Spc spc{};
                parseSpc(spc);
                item.value = spc;
                break;
            }
            case TokenType::COMMA:
            case TokenType::SEMICOLON:
            case TokenType::APOSTROPHE:
            {
                consume();
                item.value = PrintItem::Sep{ std::string{ name } };
                break;
            }
            default:
                throw std::runtime_error{ std::format("Unexpected token in PRINT at ({}, {})", position.line, position.column) };
        }

        return item;
    }

    void parseTab(PrintItem::Tab& tab)
    {
        consume();
        if (peek().type == TokenType::LPAREN)
        {
            consume();
            if (peek().type == TokenType::IDENTIFIER || peek().type == TokenType::NUMBER)
            {
                tab.first = std::string{ consume().name };
                if (peek().type == TokenType::COMMA)
                {
                    consume();
                    if (peek().type == TokenType::IDENTIFIER || peek().type == TokenType::NUMBER)
                        tab.second = std::string{ consume().name };
                }
            }
            if (peek().type == TokenType::RPAREN)
                consume();
        }
    }

    void parseSpc(PrintItem::Spc& spc)
    {
        consume();
        if (peek().type == TokenType::LPAREN)
        {
            consume();
            if (peek().type == TokenType::IDENTIFIER || peek().type == TokenType::NUMBER)
                spc.count = std::string{ consume().name };
            if (peek().type == TokenType::RPAREN)
                consume();
        }
    }

    InputNode parseInput()
    {
        consume();
        auto const [type, name, position]{ consume() };
        if (type != TokenType::IDENTIFIER)
            throw std::runtime_error{ std::format("Expected identifier after INPUT at ({}, {})", position.line, position.column) };
        return InputNode{ std::string{ name } };
    }
};