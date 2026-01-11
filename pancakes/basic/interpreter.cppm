module;
#include <format>
#include <string>
#include <unordered_map>
#include <algorithm>

export module pancakes.basic.interpreter;

import pancakes.basic.AST;

extern "C"
{
    float pancakes_parse_float(char const* str, int len);
    void pancakes_print_string(char const* str, int len);
    int pancakes_input(char* buffer, int bufferLen);

    struct WindowsSize { int width; int height; };

    WindowsSize pancakes_get_windows_size();
    void pancakes_move_cursor_to(int col, int row);
    void pancakes_get_cursor_pos(int* col, int* row);
}

export struct Interpreter final : ASTVisitor
{
    Interpreter()
        : fieldWidth{ 10 }, screenWidth{ 80 }, screenHeight{ 25 }
    {
        auto const [w, h]{ pancakes_get_windows_size() };
        if (w > 0 && h > 0)
        {
            screenWidth = w;
            screenHeight = h;
        }
    }

    void visit(PrintNode* node) override
    {
        if (node->items.empty())
            return printNewline();

        for (auto const& item : node->items)
            emitPrintItem(item);

        if (bool const suppressNewline{ node->items.back().kind == PrintItem::Kind::Sep }; !suppressNewline)
            printNewline();
    }

    void visit(InputNode* node) override
    {
        char buffer[256]{};
        int const count{ pancakes_input(buffer, static_cast<int>(std::size(buffer))) };
        variables[node->variable] = pancakes_parse_float(buffer, count);
    }

private:
    std::unordered_map<std::string, float> variables;
    int const fieldWidth;
    int screenWidth;
    int screenHeight;

    static std::pair<int, int> getCursorPos()
    {
        int col, row;
        pancakes_get_cursor_pos(&col, &row);
        return { col, row };
    }

    static void printNewline()
    {
        pancakes_print_string("\n", 1);
    }

    void emitPrintItem(PrintItem const& item)
    {
        switch (item.kind)
        {
            case PrintItem::Kind::Expression:
                if (item.isStringLiteral) printRaw(item.text);
                else printValue(item.text);
                break;
            case PrintItem::Kind::Tab: handleTab(item); break;
            case PrintItem::Kind::Spc: handleSpc(item); break;
            case PrintItem::Kind::Sep: handleSep(item.text); break;
        }
    }

    void handleSep(std::string_view const sep) const
    {
        if (sep == "'")
            printNewline();
        else if (sep == ",")
        {
            int const currentX{ getCursorPos().first };
            int const spaces{ fieldWidth - (currentX % fieldWidth) };
            printSpaces(spaces);
        }
    }

    void handleTab(PrintItem const& item) const
    {
        if (item.text.empty())
            return;

        int const x{ std::stoi(item.text) };
        auto const [curCol, curRow]{ getCursorPos() };

        int destX{ std::clamp(x, 0, screenWidth - 1) };
        int destY{ curRow };

        if (item.second.has_value())
            destY = std::clamp(std::stoi(item.second.value()), 0, screenHeight - 1);

        pancakes_move_cursor_to(destX, destY);
    }

    static void handleSpc(PrintItem const& item)
    {
        if (!item.text.empty())
            printSpaces(std::stoi(item.text));
    }

    void printValue(std::string const& varName)
    {
        if (auto it{ variables.find(varName) }; it != variables.end())
        {
            std::string const s{ std::format("{}", it->second) };
            pancakes_print_string(s.data(), static_cast<int>(s.size()));
        }
        else
            printRaw(varName);
    }

    static void printRaw(std::string_view const str)
    {
        if (!str.empty())
            pancakes_print_string(str.data(), static_cast<int>(str.size()));
    }

    static void printSpaces(int const n)
    {
        if (n <= 0)
            return;
        std::string const spaces(static_cast<std::size_t>(n), ' ');
        pancakes_print_string(spaces.data(), n);
    }
};