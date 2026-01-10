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

    [[nodiscard]] Token peek() const { return tokens[pos]; }
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

                    switch (auto [type, name, position]{ consume() }; type)
                    {
                        case TokenType::STRING:
                            program->statements.push_back(std::make_unique<PrintNode>(name, true));
                            break;

                        case TokenType::IDENTIFIER:
                            program->statements.push_back(std::make_unique<PrintNode>(name, false));
                            break;

                        default:
                            throw std::runtime_error{
                                std::format("Expected string or identifier after PRINT at ({}, {})",
                                            position.line,
                                            position.column)
                            };
                    }
                    break;
                }
                case TokenType::INPUT:
                {
                    consume();
                    if (auto [type, name, position]{ consume() };
                        type == TokenType::IDENTIFIER)
                        program->statements.push_back(std::make_unique<InputNode>(name));
                    else
                        throw std::runtime_error{
                            std::format("Expected identifier after {} at ({}, {})",
                                    name,
                                    position.line,
                                    position.column)
                            };
                    break;
                }
                default:
                    throw std::runtime_error{
                        std::format("Expected identifier after {} at ({}, {})",
                                tok.name,
                                tok.position.line,
                                tok.position.column)
                    };
            }
        }

        return program;
    }
};
