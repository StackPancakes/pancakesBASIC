module;
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>

export module pancakes.basic.AST;

export struct Expr
{
    struct Number { double value; };
    struct Variable { std::string name; };
    struct BinaryOp
    {
        char op; // '+', '-', '*', '/'
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
    };

    std::variant<Number, Variable, BinaryOp> value;

    [[maybe_unused]] [[nodiscard]] bool isCompileTime() const
    {
        return std::visit([]<typename T0>(T0 const& node) -> bool
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, Number>)
                return true;
            else if constexpr (std::is_same_v<T, Variable>)
                return false;
            else if constexpr (std::is_same_v<T, BinaryOp>)
                return node.left->isCompileTime() && node.right->isCompileTime();
            return false;
        }, value);
    }
};

export struct PrintItem
{
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

    std::variant<Expr, Tab, Spc, Sep> value;

    [[maybe_unused]] [[nodiscard]] bool isCompileTime() const
    {
        return std::visit([]<typename T0>(T0 const& node)
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, Expr>)
                return node.isCompileTime();
            else if constexpr (std::is_same_v<T, Tab>)
                return false;
            else
                return true;
        }, value);
    }
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
