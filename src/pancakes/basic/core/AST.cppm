module;
#include <string>
#include <vector>
#include <optional>
#include <variant>

export module pancakes.basic.AST;

export struct PrintItem
{
    struct Expression
    {
        std::string text;
        bool isStringLiteral{};
    };

    struct Tab
    {
        std::string first;
        std::optional<std::string> second;
    };

    struct Spc
    {
        std::string count;
    };

    struct Sep
    {
        std::string symbol;
    };

    std::variant<Expression, Tab, Spc, Sep> value;
};

export struct PrintNode
{
    std::vector<PrintItem> items;
};

export struct InputNode
{
    std::string variable;
};

export using Statement = std::variant<PrintNode, InputNode>;

export struct ProgramNode
{
    std::vector<Statement> statements;
};

export template <typename Derived>
struct ASTVisitor
{
    void run(ProgramNode& program)
    {
        for (auto& stmt : program.statements)
            dispatch(stmt);
    }

    void dispatch(Statement& stmt)
    {
        std::visit([this](auto& node)
        {
            static_cast<Derived*>(this)->visit(node);
        }, stmt);
    }

    void dispatch(PrintItem& item)
    {
        std::visit([this](auto& node)
        {
            static_cast<Derived*>(this)->visit(node);
        }, item.value);
    }
};
