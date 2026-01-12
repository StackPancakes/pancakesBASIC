module;
#include <variant>
#include <type_traits>
export module pancakes.basic.visitor;

export template <typename T>
struct ConstexprVisitor
{
    T& self;

    constexpr explicit ConstexprVisitor(T& s)
        : self{ s }
    {
    }

    template <typename Node>
    constexpr void operator()(Node& node) const
    {
        self.visit(node);
    }
};

template <typename T>
inline constexpr bool is_std_variant_v{ false };

template <typename... Ts>
inline constexpr bool is_std_variant_v<std::variant<Ts...>>{ true };

template <typename, typename = void>
struct has_value_member_with_variant : std::false_type
{
};

template <typename T>
struct has_value_member_with_variant<T, std::void_t<decltype(std::declval<T>().value)>>
{
private:
    using val_t = std::decay_t<decltype(std::declval<T>().value)>;
public:
    static constexpr bool value{ is_std_variant_v<val_t> };
};

template <typename T>
inline constexpr bool has_value_member_with_variant_v = has_value_member_with_variant<T>::value;

export template <typename Visitor, typename Variant>
constexpr void visitVariant(Variant& var, Visitor&& visitor)
{
    using VarT = std::decay_t<Variant>;

    if constexpr (is_std_variant_v<VarT>)
        std::visit(std::forward<Visitor>(visitor), var);
    else if constexpr (has_value_member_with_variant_v<VarT>)
        visitVariant(var.value, std::forward<Visitor>(visitor));
    else
        std::forward<Visitor>(visitor)(var);
}

export template <typename Visitor, typename NodeContainer>
constexpr void visitAll(NodeContainer& nodes, Visitor&& visitor)
{
    for (auto& node : nodes)
        visitVariant(node, std::forward<Visitor>(visitor));
}
