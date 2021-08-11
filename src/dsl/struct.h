//
// Created by Mike Smith on 2021/3/5.
//

#pragma once

#include <dsl/var.h>
#include <dsl/func.h>
#include <runtime/shader.h>

namespace luisa::compute::detail {

template<typename T>
struct c_array_to_std_array {
    using type = T;
};

template<typename T, size_t N>
struct c_array_to_std_array<T[N]> {
    using type = std::array<T, N>;
};

template<typename T>
using c_array_to_std_array_t = typename c_array_to_std_array<T>::type;

};// namespace luisa::compute::detail

#define LUISA_STRUCT_MAKE_MEMBER_TYPE(m)             \
    using Type_##m = detail::c_array_to_std_array_t< \
        std::remove_cvref_t<                         \
            decltype(std::declval<This>().m)>>;

#define LUISA_STRUCT_MAKE_MEMBER_INIT(m)          \
    m(detail::FunctionBuilder::current()->member( \
        Type::of<Type_##m>(),                     \
        this->_expression,                        \
        _member_index(#m)))

#define LUISA_STRUCT_MAKE_MEMBER_REF_DECL(m) \
    Ref<Type_##m> m;

#define LUISA_STRUCT_MAKE_MEMBER_EXPR_DECL(m) \
    Ref<Type_##m> m;

#define LUISA_STRUCT(S, ...)                                                                    \
    LUISA_STRUCT_REFLECT(S, __VA_ARGS__)                                                        \
    namespace luisa::compute {                                                                  \
    template<>                                                                                  \
    struct Expr<S> {                                                                            \
    private:                                                                                    \
        const Expression *_expression;                                                          \
        using This = S;                                                                         \
        LUISA_MAP(LUISA_STRUCT_MAKE_MEMBER_TYPE, __VA_ARGS__)                                   \
        [[nodiscard]] static constexpr size_t _member_index(std::string_view name) noexcept {   \
            constexpr const std::string_view member_names[]{                                    \
                LUISA_MAP_LIST(LUISA_STRINGIFY, __VA_ARGS__)};                                  \
            return std::find(std::begin(member_names), std::end(member_names), name)            \
                   - std::begin(member_names);                                                  \
        }                                                                                       \
                                                                                                \
    public:                                                                                     \
        LUISA_MAP(LUISA_STRUCT_MAKE_MEMBER_EXPR_DECL, __VA_ARGS__)                              \
        explicit Expr(const Expression *e) noexcept                                             \
            : _expression{e},                                                                   \
              LUISA_MAP_LIST(LUISA_STRUCT_MAKE_MEMBER_INIT, __VA_ARGS__) {}                     \
        [[nodiscard]] auto expression() const noexcept { return this->_expression; }            \
        Expr(Expr &&another) noexcept = default;                                                \
        Expr(const Expr &another) noexcept = default;                                           \
        Expr &operator=(Expr) noexcept = delete;                                                \
    };                                                                                          \
    template<>                                                                                  \
    struct Ref<S> {                                                                             \
    private:                                                                                    \
        const Expression *_expression;                                                          \
        using This = S;                                                                         \
        LUISA_MAP(LUISA_STRUCT_MAKE_MEMBER_TYPE, __VA_ARGS__)                                   \
        [[nodiscard]] static constexpr size_t _member_index(std::string_view name) noexcept {   \
            constexpr const std::string_view member_names[]{                                    \
                LUISA_MAP_LIST(LUISA_STRINGIFY, __VA_ARGS__)};                                  \
            return std::find(std::begin(member_names), std::end(member_names), name)            \
                   - std::begin(member_names);                                                  \
        }                                                                                       \
                                                                                                \
    public:                                                                                     \
        LUISA_MAP(LUISA_STRUCT_MAKE_MEMBER_REF_DECL, __VA_ARGS__)                               \
        explicit Ref(const Expression *e) noexcept                                              \
            : _expression{e},                                                                   \
              LUISA_MAP_LIST(LUISA_STRUCT_MAKE_MEMBER_INIT, __VA_ARGS__) {}                     \
        explicit Ref(detail::ArgumentCreation) noexcept                                         \
            : Ref{detail::FunctionBuilder::current()->reference(Type::of<S>())} {}              \
        [[nodiscard]] auto                                                                      \
        expression() const noexcept { return this->_expression; }                               \
        Ref(Ref &&another) noexcept = default;                                                  \
        Ref(const Ref &another) noexcept = default;                                             \
        [[nodiscard]] operator Expr<S>() const noexcept { return Expr<S>{this->expression()}; } \
        void operator=(Expr<S> rhs) const noexcept {                                            \
            detail::FunctionBuilder::current()->assign(                                         \
                AssignOp::ASSIGN,                                                               \
                this->expression(),                                                             \
                rhs.expression());                                                              \
        }                                                                                       \
        void operator=(Ref rhs) const noexcept { (*this) = Expr{rhs}; }                         \
        template<typename Rhs>                                                                  \
        void assign(Rhs &&v) const noexcept { (*this) = std::forward<Rhs>(v); }                 \
    };                                                                                          \
    }

#define LUISA_BINDING_GROUP_MAKE_MEMBER_VAR_DECL(m) \
    Var<Type_##m> m;

#define LUISA_BINDING_GROUP_MAKE_MEMBER_EXPR_DECL(m) \
    Expr<Type_##m> m;

#define LUISA_BINDING_GROUP_MAKE_MEMBER_VAR_INIT(m) \
    m(detail::ArgumentCreation{})

#define LUISA_BINDING_GROUP_MAKE_MEMBER_EXPR_INIT(m) \
    m(s.m)

#define LUISA_BINDING_GROUP_MAKE_INVOKE(m) \
    invoke << s.m;

#define LUISA_BINDING_GROUP(S, ...)                                                     \
    namespace luisa::compute {                                                          \
    template<>                                                                          \
    struct Var<S> {                                                                     \
        using This = S;                                                                 \
        LUISA_MAP(LUISA_STRUCT_MAKE_MEMBER_TYPE, __VA_ARGS__)                           \
        LUISA_MAP(LUISA_BINDING_GROUP_MAKE_MEMBER_VAR_DECL, __VA_ARGS__)                \
        explicit Var(detail::ArgumentCreation) noexcept                                 \
            : LUISA_MAP_LIST(LUISA_BINDING_GROUP_MAKE_MEMBER_VAR_INIT, __VA_ARGS__) {}  \
        Var(Var &&) noexcept = default;                                                 \
        Var(const Var &) noexcept = delete;                                             \
        Var &operator=(Var &&) noexcept = delete;                                       \
        Var &operator=(const Var &) noexcept = delete;                                  \
    };                                                                                  \
    template<>                                                                          \
    struct Expr<S> {                                                                    \
        using This = S;                                                                 \
        LUISA_MAP(LUISA_STRUCT_MAKE_MEMBER_TYPE, __VA_ARGS__)                           \
        LUISA_MAP(LUISA_BINDING_GROUP_MAKE_MEMBER_EXPR_DECL, __VA_ARGS__)               \
        Expr(const Var<S> &s) noexcept                                                  \
            : LUISA_MAP_LIST(LUISA_BINDING_GROUP_MAKE_MEMBER_EXPR_INIT, __VA_ARGS__) {} \
        Expr(Expr &&another) noexcept = default;                                        \
        Expr(const Expr &another) noexcept = default;                                   \
        Expr &operator=(Expr) noexcept = delete;                                        \
    };                                                                                  \
    namespace detail {                                                                  \
    CallableInvoke &operator<<(CallableInvoke &invoke, Expr<S> s) noexcept {            \
        LUISA_MAP(LUISA_BINDING_GROUP_MAKE_INVOKE, __VA_ARGS__)                         \
        return invoke;                                                                  \
    }                                                                                   \
    ShaderInvokeBase &operator<<(ShaderInvokeBase &invoke, const S &s) noexcept {       \
        LUISA_MAP(LUISA_BINDING_GROUP_MAKE_INVOKE, __VA_ARGS__)                         \
        return invoke;                                                                  \
    }                                                                                   \
    }                                                                                   \
    }
