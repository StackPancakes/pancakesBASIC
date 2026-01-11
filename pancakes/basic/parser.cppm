module;
#include <memory>
#include <vector>
#include <stdexcept>
#include <format>
#include <string>
export module pancakes.basic.parser;

import pancakes.basic.tokentype;
import pancakes.basic.token;
import pancakes.basic.AST;

export struct Parser
{
    std::vector<Token> const& tokens;
    size_t pos;

    explicit Parser(std::vector<Token>& t) : tokens{ t }, pos{ 0 } {}

    [[nodiscard]] Token peek() const { return tokens[pos]; }
    Token consume() { return tokens[pos++]; }

    std::unique_ptr<ProgramNode> parse()
    {
        auto program{ std::make_unique<ProgramNode>() };

        while (peek().type != TokenType::END_OF_FILE)
        {
            if (peek().type == TokenType::END_OF_LINE)
            {
                consume();
                continue;
            }

            switch (Token tok{ peek() }; tok.type)
            {
                case TokenType::PRINT:
                    program->statements.push_back(parsePrint());
                    break;
                case TokenType::INPUT:
                    program->statements.push_back(parseInput());
                    break;
                default:
                    throw std::runtime_error{
                        std::format("Unexpected statement at ({}, {})",
                                tok.position.line,
                                tok.position.column)
                    };
            }
        }

        return program;
    }

private:

    std::unique_ptr<PrintNode> parsePrint()
    {
        consume(); // consume PRINT
        auto node{ std::make_unique<PrintNode>() };

        while (true)
        {
            if (Token const next{ peek() }; next.type == TokenType::END_OF_LINE
                                      ||  next.type == TokenType::COLON
                                      ||  next.type == TokenType::END_OF_FILE)
                break;

            node->items.push_back(parsePrintItem());
        }

        return node;
    }

    PrintItem parsePrintItem()
    {
        auto [type, name, position]{ peek() };
        PrintItem item{};

        switch (type)
        {
            case TokenType::STRING:
                consume();
                item.kind = PrintItem::Kind::Expression;
                item.text = std::string{ name };
                item.isStringLiteral = true;
                break;

            case TokenType::IDENTIFIER:
            case TokenType::NUMBER:
                consume();
                item.kind = PrintItem::Kind::Expression;
                item.text = std::string{ name };
                item.isStringLiteral = false;
                break;

            case TokenType::TAB:
                parseTab(item);
                break;

            case TokenType::SPC:
                parseSpc(item);
                break;

            case TokenType::COMMA:
            case TokenType::SEMICOLON:
            case TokenType::APOSTROPHE:
                consume();
                item.kind = PrintItem::Kind::Sep;
                item.text = std::string{ name };
                break;

            default:
                throw std::runtime_error{
                    std::format("Unexpected token '{}' in PRINT at ({}, {})",
                            name,
                            position.line,
                            position.column)
                };
        }

        return item;
    }

    void parseTab(PrintItem& item)
    {
        consume(); // consume TAB
        item.kind = PrintItem::Kind::Tab;

        if (peek().type == TokenType::LPAREN)
        {
            consume(); // consume '('

            if (peek().type == TokenType::IDENTIFIER
            ||  peek().type == TokenType::NUMBER
            ||  peek().type == TokenType::STRING)
            {
                Token const arg{ consume() };
                item.text = std::string{ arg.name };

                if (peek().type == TokenType::COMMA)
                {
                    consume();
                    if (peek().type == TokenType::IDENTIFIER
                    ||  peek().type == TokenType::NUMBER
                    ||  peek().type == TokenType::STRING)
                    {
                        Token const arg2{ consume() };
                        item.second = std::string{ arg2.name };
                    }
                    else
                        throw std::runtime_error{
                            std::format("Expected second TAB arg at ({}, {})",
                                    peek().position.line,
                                    peek().position.column)
                        };
                }
            }

            if (peek().type == TokenType::RPAREN)
                consume(); // consume ')'
            else
                throw std::runtime_error{
                    std::format("Expected ')' after TAB args at ({}, {})",
                            peek().position.line,
                            peek().position.column)
                };
        }
    }

    void parseSpc(PrintItem& item)
    {
        consume(); // consume SPC
        item.kind = PrintItem::Kind::Spc;

        if (peek().type == TokenType::LPAREN)
        {
            consume(); // consume '('

            if (peek().type == TokenType::IDENTIFIER
            ||  peek().type == TokenType::NUMBER
            ||  peek().type == TokenType::STRING)
            {
                Token arg{ consume() };
                item.text = std::string{ arg.name };
            }

            if (peek().type == TokenType::RPAREN)
                consume(); // consume ')'
            else
                throw std::runtime_error{
                    std::format("Expected ')' after SPC arg at ({}, {})",
                            peek().position.line,
                            peek().position.column)
                };
        }
    }

    std::unique_ptr<InputNode> parseInput()
    {
        consume(); // consume INPUT
        auto [type, name, position]{ consume() };

        if (type != TokenType::IDENTIFIER)
            throw std::runtime_error{
                std::format("Expected identifier after INPUT at ({}, {})",
                        position.line,
                        position.column)
            };

        return std::make_unique<InputNode>(name);
    }
};
