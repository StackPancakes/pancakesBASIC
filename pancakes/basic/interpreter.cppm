module;
#include <format>
#include <windows.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
export module pancakes.basic.interpreter;

import pancakes.basic.AST;

export struct Interpreter final : ASTVisitor
{
    HANDLE hConsole;
    HANDLE hStdin;
    Interpreter() : hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) },
                    hStdin{ GetStdHandle(STD_INPUT_HANDLE) }
    {
        if (hStdin == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Failed to get stdin handle");
    }

    void visit(PrintNode* node) override
    {
        DWORD written;

        if (auto const it{ variables.find(node->text) }; it != variables.end())
        {
            std::string const output{ std::format("{}", it->second) };
            WriteConsoleA(hConsole, output.data(), output.size(), &written, nullptr);
        }
        else
            WriteConsoleA(hConsole, node->text.data(), node->text.size(), &written, nullptr);

        constexpr std::string_view newLine{ "\r\n" };
        WriteConsoleA(hConsole, newLine.data(), newLine.length(), &written, nullptr);
    }

    void visit(InputNode* node) override
    {
        char buffer[256];
        DWORD charsRead{};
        if (!ReadConsoleA(hStdin, buffer, std::size(buffer) - 1, &charsRead, nullptr))
            throw std::runtime_error("Failed to read from console");

        buffer[charsRead] = '\0';

        if (charsRead >= 2 && buffer[charsRead - 2] == '\r' && buffer[charsRead - 1] == '\n')
            buffer[charsRead - 2] = '\0';
        else if (charsRead >= 1 && (buffer[charsRead - 1] == '\r' || buffer[charsRead - 1] == '\n'))
            buffer[charsRead - 1] = '\0';

        variables[node->variable] = std::stof(buffer);
    }

private:
    std::unordered_map<std::string, float> variables;
};
