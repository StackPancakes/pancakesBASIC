module;
#include <unordered_map>
#include <string>
#include <variant>
#include <optional>
#include <format>
#include <algorithm>

export module pancakes.basic.interpreter;

import pancakes.basic.AST;
import pancakes.basic.settings;
import pancakes.basic.visitor;
import pancakes.basic.util;

extern "C"
{
    float pancakes_parse_float(char const* str, int len);
    void pancakes_print_string(char const* str, int len);
    void pancakes_print_number(float val);
    int pancakes_input(char* buffer, int bufferLen);
    struct WindowsSize { int width; int height; };
    WindowsSize pancakes_get_windows_size();
    void pancakes_move_cursor_to(int col, int row);
    void pancakes_get_cursor_pos(int* col, int* row);
}

export struct Interpreter final
{
    std::unordered_map<std::string, float, CaseInsensitiveHash, CaseInsensitiveEqual> variables{};
    int fieldWidth{ 10 };
    int screenWidth{ 80 };
    int screenHeight{ 25 };

    Interpreter()
    {
        if (auto const [width, height]{ pancakes_get_windows_size() }; width > 0 && height > 0)
        {
            screenWidth = width;
            screenHeight = height;
        }
    }

    void run(ProgramNode& program)
    {
        visitAll(program.statements, ConstexprVisitor{ *this });
    }

    void visit(PrintNode& node)
    {
        for (auto const&[value] : node.items)
            std::visit(ConstexprVisitor{ *this }, value);

        if (node.items.empty())
        {
            printNewline();
            return;
        }

        bool const suppressNewline{ std::visit([]<typename T0>(T0 const& x) -> bool
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, PrintItem::Sep>)
                return x.symbol == ";" || x.symbol == "," || x.symbol == "'";
            return false;
        }, node.items.back().value) };

        if (!suppressNewline)
            printNewline();
    }

    void visit(InputNode const& node)
    {
        char buffer[256]{};
        int const count{ pancakes_input(buffer, 256) };
        variables[node.variable] = pancakes_parse_float(buffer, count);
    }

    void visit(PrintItem::Expression const& x)
    {
        if (x.isStringLiteral)
            printRaw(x.text);
        else if (isNumericLiteral(x.text))
        {
            float const val{ pancakes_parse_float(x.text.data(), static_cast<int>(x.text.size())) };
            pancakes_print_number(val);
        }
        else
            printValue(x.text);
    }

    void visit(PrintItem::Tab const& x) const
    {
        if (x.first.empty())
            return;

        int const destX{ std::clamp(std::stoi(x.first), 0, screenWidth - 1) };
        int col{ 0 }, row{ 0 };
        pancakes_get_cursor_pos(&col, &row);
        int destY{ row };

        if (x.second.has_value())
            destY = std::clamp(std::stoi(*x.second), 0, screenHeight - 1);

        pancakes_move_cursor_to(destX, destY);
    }

    static void visit(PrintItem::Spc const& x)
    {
        if (!x.count.empty())
            printSpaces(std::stoi(x.count));
    }

    void visit(PrintItem::Sep const& x) const
    {
        if (x.symbol == "'")
            printNewline();
        else if (x.symbol == ",")
        {
            int col{ 0 }, row{ 0 };
            pancakes_get_cursor_pos(&col, &row);
            printSpaces(fieldWidth - col % fieldWidth);
        }
    }

private:
    void printValue(std::string const& varName)
    {
        float val{ 0.0f };
        if (auto const it{ variables.find(varName) }; it != variables.end())
            val = it->second;

        pancakes_print_number(val);
    }

    static void printRaw(std::string_view const str)
    {
        pancakes_print_string(str.data(), static_cast<int>(str.size()));
    }

    static void printNewline()
    {
        printRaw("\n");
    }

    static void printSpaces(int n)
    {
        if (n > 0)
        {
            std::string const s(static_cast<size_t>(n), ' ');
            printRaw(s);
        }
    }
};
