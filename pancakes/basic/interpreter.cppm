module;
#include <format>
#include <string>
#include <unordered_map>
export module pancakes.basic.interpreter;

import pancakes.basic.AST;

extern "C"
{
    float pancakes_parse_float(char const* str, int len);
    void pancakes_print_float(float val);
    void pancakes_print_string(char const* str, int len);
    int pancakes_input(char* buffer, int bufferLen);
}

export struct Interpreter final : ASTVisitor
{
    Interpreter() = default;

    void visit(PrintNode* node) override
    {
        if (node->isStringLiteral)
            pancakes_print_string(node->text.data(), static_cast<int>(node->text.size()));
        else
        {
            if (auto const it{ variables.find(node->text) }; it != variables.end())
                pancakes_print_float(it->second);
            else
                pancakes_print_string(node->text.data(), static_cast<int>(node->text.size()));
        }
    }

    void visit(InputNode* node) override
    {
        char buffer[256]{};
        int const charsRead{ pancakes_input(buffer, sizeof(buffer)) };
        variables[node->variable] = pancakes_parse_float(buffer, charsRead);
    }

private:
    std::unordered_map<std::string, float> variables;
};
