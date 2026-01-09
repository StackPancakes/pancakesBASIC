module;
#include <memory>
#include <vector>
#include <stdexcept>
#include <format>
export module pancakes.basic.parser;

import pancakes.basic.tokentype;
import pancakes.basic.token;
import pancakes.basic.AST;

export struct Parser
{
    std::vector<Token> const& tokens;
    size_t pos;

    explicit Parser(std::vector<Token>& t) : tokens{ t }, pos{ 0 } {}

    Token peek() const { return tokens[pos]; }
    Token consume() { return tokens[pos++]; }

    std::unique_ptr<ProgramNode> parse()
    {
        auto program{ std::make_unique<ProgramNode>() };

        while (peek().type != TokenType::END_OF_FILE)
        {
            switch (Token tok{ peek() }; tok.type)
            {
                case TokenType::PRINT:
                    {
                        consume();
                        Token t{ consume() };
                        if (t.type == TokenType::STRING)
                            program->statements.push_back(std::make_unique<PrintNode>(t.name));
                        else
                            throw std::runtime_error{
                                std::format("Expected string after {} at ({}, {})",
                                        t.name,
                                        t.position.line,
                                        t.position.column)
                            };
                        break;
                    }
                default:
                    throw std::runtime_error{
                        std::format("Unknown statement \"{}\" at ({}, {})",
                                tok.name,
                                tok.position.line,
                                tok.position.column)
                    };
            }
        }

        return program;
    }
};
