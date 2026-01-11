module;
#include <string_view>
#include <string>
#include <memory>
#include <vector>
#include <optional>
export module pancakes.basic.AST;

export struct ASTVisitor;

export struct ASTNode
{
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

export struct ProgramNode : ASTNode
{
    std::vector<std::unique_ptr<ASTNode>> statements;
    void accept(ASTVisitor& visitor) override;
};

export struct PrintItem
{
    enum class Kind
    {
        Expression,
        Tab,
        Spc,
        Sep
    };

    Kind kind{};
    std::string text{};
    std::optional<std::string> second{};
    bool isStringLiteral{};
};

export struct PrintNode final : ASTNode
{
    std::vector<PrintItem> items;
    void accept(ASTVisitor& visitor) override;
};

export struct InputNode final : ASTNode
{
    std::string variable;
    explicit InputNode(std::string_view const v) : variable{ v } {}
    void accept(ASTVisitor& visitor) override;
};

export struct ASTVisitor
{
    virtual ~ASTVisitor() = default;
    virtual void visit(PrintNode* node) = 0;
    virtual void visit(InputNode* node) = 0;
};

inline void ProgramNode::accept(ASTVisitor& visitor)
{
    for (auto const& stmt : statements)
        stmt->accept(visitor);
}

inline void PrintNode::accept(ASTVisitor& visitor)
{
    visitor.visit(this);
}

inline void InputNode::accept(ASTVisitor& visitor)
{
    visitor.visit(this);
}
