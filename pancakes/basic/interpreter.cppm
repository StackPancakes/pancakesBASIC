module;
#include <windows.h>
#include <memory>
export module pancakes.basic.interpreter;

import pancakes.basic.AST;

export struct Interpreter final : ASTVisitor
{
    HANDLE hConsole;
    Interpreter() : hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) } {}

    void visit(PrintNode* node) override
    {
        DWORD written;
        WriteConsoleA(hConsole, node->text.data(), static_cast<DWORD>(node->text.size()), &written, nullptr);
        WriteConsoleA(hConsole, "\n", 1, &written, nullptr);
    }
};
