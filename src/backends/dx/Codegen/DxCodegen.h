#pragma once

#include <vstl/Common.h>
#include <vstl/functional.h>
#include <ast/function.h>
#include <ast/expression.h>
#include <ast/statement.h>

using namespace luisa;
using namespace luisa::compute;
namespace toolhub::directx {
class StringStateVisitor;
class CodegenUtility {
public:
    static constexpr uint64 INLINE_STMT_LIMIT = 5;
    static uint IsBool(Type const &type);
    static void GetConstName(ConstantData const &data, vstd::string &str);
    static void GetVariableName(Variable const &type, vstd::string &str);
    static void GetVariableName(Variable::Tag type, uint id, vstd::string &str);
    static void GetVariableName(Type::Tag type, uint id, vstd::string &str);
    static void GetTypeName(Type const &type, vstd::string &str);
    static void GetBasicTypeName(uint64 typeIndex, vstd::string &str);
    static void GetConstantStruct(ConstantData const &data, vstd::string &str);
    //static void
    static void GetConstantData(ConstantData const &data, vstd::string &str);
    static size_t GetTypeSize(Type const &t);
    static size_t GetTypeAlign(Type const &t);
    static vstd::string GetBasicTypeName(uint64 typeIndex) {
        vstd::string s;
        GetBasicTypeName(typeIndex, s);
        return s;
    }
    static void GetFunctionDecl(Function func, vstd::string &str);
    static void GetFunctionName(CallExpr const *expr, vstd::string &result, StringStateVisitor &visitor);
    static void ClearStructType();
    static void RegistStructType(Type const *type);

    static void CodegenFunction(
        Function func,
        vstd::string &result);
    static void GenerateCBuffer(
        std::span<const Variable> vars,
        vstd::string &result);
    static vstd::optional<vstd::string> Codegen(Function kernel);
};
class StringStateVisitor final : public StmtVisitor, public ExprVisitor {
public:
    void visit(const UnaryExpr *expr) override;
    void visit(const BinaryExpr *expr) override;
    void visit(const MemberExpr *expr) override;
    void visit(const AccessExpr *expr) override;
    void visit(const LiteralExpr *expr) override;
    void visit(const RefExpr *expr) override;
    void visit(const CallExpr *expr) override;
    void visit(const CastExpr *expr) override;
    void visit(const ConstantExpr *expr) override;

    void visit(const BreakStmt *) override;
    void visit(const ContinueStmt *) override;
    void visit(const ReturnStmt *) override;
    void visit(const ScopeStmt *) override;
    void visit(const IfStmt *) override;
    void visit(const LoopStmt *) override;
    void visit(const ExprStmt *) override;
    void visit(const SwitchStmt *) override;
    void visit(const SwitchCaseStmt *) override;
    void visit(const SwitchDefaultStmt *) override;
    void visit(const AssignStmt *) override;
    void visit(const ForStmt *) override;
    void visit(const MetaStmt *stmt) override;
    void visit(const CommentStmt *) override;
    StringStateVisitor(
        vstd::string &str);
    ~StringStateVisitor();
    uint64 StmtCount() const { return stmtCount; }

protected:
    vstd::string &str;
    uint64 stmtCount = 0;
};
template<typename T>
struct PrintValue;
template<>
struct PrintValue<float> {
    void operator()(float const &v, vstd::string &str) {
        if (std::isnan(v)) [[unlikely]] {
            LUISA_ERROR_WITH_LOCATION("Encountered with NaN.");
        }
        if (std::isinf(v)) {
            str.append(v < 0.0f ? "(-INFINITY_f)" : "(+INFINITY_f)");
        } else {
            auto s = fmt::format("{}", v);
            str.append(s);
            if (s.find('.') == std::string_view::npos &&
                s.find('e') == std::string_view::npos) {
                str.append(".0");
            }
            str.append("f");
        }
    }
};
template<>
struct PrintValue<int> {
    void operator()(int const &v, vstd::string &str) {
        vstd::to_string(v, str);
    }
};
template<>
struct PrintValue<uint> {
    void operator()(uint const &v, vstd::string &str) {
        vstd::to_string(v, str);
    }
};

template<>
struct PrintValue<bool> {
    void operator()(bool const &v, vstd::string &str) {
        if (v)
            str << "true";
        else
            str << "false";
    }
};
template<typename EleType, uint64 N>
struct PrintValue<Vector<EleType, N>> {
    using T = Vector<EleType, N>;
    void PureRun(T const &v, vstd::string &varName) {
        for (uint64 i = 0; i < N; ++i) {
            vstd::to_string(v[i], varName);
            varName += ',';
        }
        auto &&last = varName.end() - 1;
        if (*last == ',')
            varName.erase(last);
    }
    void operator()(T const &v, vstd::string &varName) {
        if constexpr (N > 1) {
            if constexpr (std::is_same_v<EleType, float>) {
                varName << "_float";
            } else if constexpr (std::is_same_v<EleType, uint>) {
                varName << "_uint";
            } else if constexpr (std::is_same_v<EleType, int>) {
                varName << "_int";
            } else if constexpr (std::is_same_v<EleType, bool>) {
                varName << "_bool";
            }
            vstd::to_string(N, varName);
            varName << '(';
            PureRun(v, varName);
            varName << ')';
        } else {
            PureRun(v, varName);
        }
    }
};

template<uint64 N>
struct PrintValue<Matrix<N>> {
    using T = Matrix<N>;
    using EleType = float;
    void operator()(T const &v, vstd::string &varName) {
        varName << "_float";
        auto ss = vstd::to_string(N);
        varName << ss << 'x' << ss << '(';
        PrintValue<Vector<EleType, N>> vecPrinter;
        for (uint64 i = 0; i < N; ++i) {
            vecPrinter.PureRun(v[i], varName);
            varName += ',';
        }
        auto &&last = varName.end() - 1;
        if (*last == ',')
            varName.erase(last);
        varName << ')';
    }
};

template<>
struct PrintValue<LiteralExpr::MetaValue> {
    void operator()(const LiteralExpr::MetaValue &s, vstd::string &varName) const noexcept {
        // TODO...
    }
};

}// namespace toolhub::directx