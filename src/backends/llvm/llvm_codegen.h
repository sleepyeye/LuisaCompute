//
// Created by Mike Smith on 2022/5/23.
//

#pragma once

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>

#include <ast/type.h>
#include <ast/expression.h>
#include <ast/statement.h>
#include <ast/function_builder.h>

namespace luisa::compute::llvm {

class LLVMCodegen : public StmtVisitor {

private:
    struct FunctionContext {
        Function function;
        ::llvm::Function *ir;
        ::llvm::Value *ret;
        ::llvm::BasicBlock *exit_block;
        luisa::unique_ptr<::llvm::IRBuilder<>> builder;
        luisa::unordered_map<uint, ::llvm::Value *> variables;
        luisa::vector<::llvm::BasicBlock *> break_targets;
        luisa::vector<::llvm::BasicBlock *> continue_targets;
        luisa::vector<::llvm::SwitchInst *> switch_stack;
        FunctionContext(Function f, ::llvm::Function *ir, ::llvm::Value *ret,
                        ::llvm::BasicBlock *exit_block,
                        luisa::unique_ptr<::llvm::IRBuilder<>> builder,
                        luisa::unordered_map<uint, ::llvm::Value *> variables) noexcept
        : function{f}, ir{ir}, ret{ret}, exit_block{exit_block},
          builder{std::move(builder)}, variables{std::move(variables)} {}
    };

public:
    static constexpr auto buffer_argument_size = 8u;
    static constexpr auto texture_argument_size = 8u;
    static constexpr auto accel_argument_size = 8u;
    static constexpr auto bindless_array_argument_size = 8u;

private:
    struct LLVMStruct {
        ::llvm::StructType *type;
        luisa::vector<uint> member_indices;
    };

private:
    ::llvm::LLVMContext &_context;
    ::llvm::Module *_module{nullptr};
    luisa::unordered_map<uint64_t, LLVMStruct> _struct_types;
    luisa::vector<luisa::unique_ptr<FunctionContext>> _function_stack;

private:
    void _emit_function() noexcept;
    [[nodiscard]] luisa::string _variable_name(Variable v) const noexcept;
    [[nodiscard]] luisa::string _function_name(Function f) const noexcept;
    [[nodiscard]] ::llvm::Function *_create_function(Function f) noexcept;
    [[nodiscard]] luisa::unique_ptr<FunctionContext> _create_kernel_program(Function f) noexcept;
    [[nodiscard]] luisa::unique_ptr<FunctionContext> _create_kernel_context(Function f) noexcept;
    [[nodiscard]] luisa::unique_ptr<FunctionContext> _create_callable_context(Function f) noexcept;
    [[nodiscard]] ::llvm::Type *_create_type(const Type *t) noexcept;
    [[nodiscard]] ::llvm::Value *_create_expr(const Expression *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_unary_expr(const UnaryExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_binary_expr(const BinaryExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_member_expr(const MemberExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_access_expr(const AccessExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_literal_expr(const LiteralExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_ref_expr(const RefExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_constant_expr(const ConstantExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_call_expr(const CallExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_builtin_call_expr(const Type *ret_type, CallOp op, luisa::span<const Expression *const> args) noexcept;
    [[nodiscard]] ::llvm::Value *_create_cast_expr(const CastExpr *expr) noexcept;
    [[nodiscard]] ::llvm::Value *_create_stack_variable(::llvm::Value *v, luisa::string_view name = "") noexcept;
    [[nodiscard]] FunctionContext *_current_context() noexcept;
    [[nodiscard]] ::llvm::Value *_convert(const Type *dst_type, const Type *src_type, ::llvm::Value *p_src) noexcept;
    [[nodiscard]] ::llvm::Value *_scalar_to_bool(const Type *src_type, ::llvm::Value *p_src) noexcept;
    [[nodiscard]] ::llvm::Value *_scalar_to_float(const Type *src_type, ::llvm::Value *p_src) noexcept;
    [[nodiscard]] ::llvm::Value *_scalar_to_int(const Type *src_type, ::llvm::Value *p_src) noexcept;
    [[nodiscard]] ::llvm::Value *_scalar_to_uint(const Type *src_type, ::llvm::Value *p_src) noexcept;
    [[nodiscard]] ::llvm::Value *_scalar_to_vector(const Type *src_type, uint dst_dim, ::llvm::Value *p_src) noexcept;
    void _create_assignment(const Type *dst_type, const Type *src_type, ::llvm::Value *p_dst, ::llvm::Value *p_src) noexcept;

    // built-in make_vector functions
    [[nodiscard]] ::llvm::Value *_make_int2(::llvm::Value *px, ::llvm::Value *py) noexcept;
    [[nodiscard]] ::llvm::Value *_make_int3(::llvm::Value *px, ::llvm::Value *py, ::llvm::Value *pz) noexcept;
    [[nodiscard]] ::llvm::Value *_make_int4(::llvm::Value *px, ::llvm::Value *py, ::llvm::Value *pz, ::llvm::Value *pw) noexcept;
    [[nodiscard]] ::llvm::Value *_make_bool2(::llvm::Value *px, ::llvm::Value *py) noexcept;
    [[nodiscard]] ::llvm::Value *_make_bool3(::llvm::Value *px, ::llvm::Value *py, ::llvm::Value *pz) noexcept;
    [[nodiscard]] ::llvm::Value *_make_bool4(::llvm::Value *px, ::llvm::Value *py, ::llvm::Value *pz, ::llvm::Value *pw) noexcept;
    [[nodiscard]] ::llvm::Value *_make_float2(::llvm::Value *px, ::llvm::Value *py) noexcept;
    [[nodiscard]] ::llvm::Value *_make_float3(::llvm::Value *px, ::llvm::Value *py, ::llvm::Value *pz) noexcept;
    [[nodiscard]] ::llvm::Value *_make_float4(::llvm::Value *px, ::llvm::Value *py, ::llvm::Value *pz, ::llvm::Value *pw) noexcept;
    [[nodiscard]] ::llvm::Value *_make_float2x2(::llvm::Value *p0, ::llvm::Value *p1) noexcept;
    [[nodiscard]] ::llvm::Value *_make_float3x3(::llvm::Value *p0, ::llvm::Value *p1, ::llvm::Value *p2) noexcept;
    [[nodiscard]] ::llvm::Value *_make_float4x4(::llvm::Value *p0, ::llvm::Value *p1, ::llvm::Value *p2, ::llvm::Value *p3) noexcept;

    // built-in short-cut logical operators
    [[nodiscard]] ::llvm::Value *_shortcut_and(const Expression *lhs, const Expression *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_shortcut_or(const Expression *lhs, const Expression *rhs) noexcept;

    // built-in operators
    [[nodiscard]] ::llvm::Value *_builtin_and(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_or(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_xor(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_add(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_sub(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_mul(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_div(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_mod(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_lt(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_le(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_gt(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_ge(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_eq(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_neq(const Type *t, ::llvm::Value *lhs, ::llvm::Value *rhs) noexcept;

    // built-in functions builders
    [[nodiscard]] ::llvm::Value *_builtin_all(const Type *t, ::llvm::Value *v) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_any(const Type *t, ::llvm::Value *v) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_select(const Type *t_pred, const Type *t_value, ::llvm::Value *pred,
                                                 ::llvm::Value *v_true, ::llvm::Value *v_false) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_clamp(const Type *t, ::llvm::Value *v, ::llvm::Value *lo, ::llvm::Value *hi) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_lerp(const Type *t, ::llvm::Value *a, ::llvm::Value *b, ::llvm::Value *x) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_step(const Type *t, ::llvm::Value *edge, ::llvm::Value *x) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_abs(const Type *t, ::llvm::Value *x) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_min(const Type *t, ::llvm::Value *x, ::llvm::Value *y) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_max(const Type *t, ::llvm::Value *x, ::llvm::Value *y) noexcept;
    [[nodiscard]] ::llvm::Value *_builtin_fma(const Type *t, ::llvm::Value *a, ::llvm::Value *b, ::llvm::Value *c) noexcept;


public:
    explicit LLVMCodegen(::llvm::LLVMContext &ctx) noexcept;
    void visit(const BreakStmt *stmt) override;
    void visit(const ContinueStmt *stmt) override;
    void visit(const ReturnStmt *stmt) override;
    void visit(const ScopeStmt *stmt) override;
    void visit(const IfStmt *stmt) override;
    void visit(const LoopStmt *stmt) override;
    void visit(const ExprStmt *stmt) override;
    void visit(const SwitchStmt *stmt) override;
    void visit(const SwitchCaseStmt *stmt) override;
    void visit(const SwitchDefaultStmt *stmt) override;
    void visit(const AssignStmt *stmt) override;
    void visit(const ForStmt *stmt) override;
    void visit(const CommentStmt *stmt) override;
    void visit(const MetaStmt *stmt) override;
    luisa::unique_ptr<::llvm::Module> emit(Function f) noexcept;
};

}// namespace luisa::compute::llvm