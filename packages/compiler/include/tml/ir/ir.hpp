#ifndef TML_IR_IR_HPP
#define TML_IR_IR_HPP

#include "tml/common.hpp"
#include "tml/parser/ast.hpp"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tml::ir {

// Forward declarations
struct IRModule;
struct IRFunc;
struct IRType;
struct IRBehavior;
struct IRImpl;
struct IRConst;
struct IRExpr;
struct IRStmt;
struct IRPattern;

using IRExprPtr = Box<IRExpr>;
using IRStmtPtr = Box<IRStmt>;
using IRPatternPtr = Box<IRPattern>;

// Stable ID (8-character hex hash)
using StableId = std::string;

// Visibility
enum class Visibility {
    Private,
    Public,
};

// ============================================================================
// Types in IR
// ============================================================================

struct IRTypeRef {
    std::string name;
    std::vector<Box<IRTypeRef>> type_args;
};

struct IRRefType {
    bool is_mut;
    Box<IRTypeRef> inner;
};

struct IRSliceType {
    Box<IRTypeRef> element;
};

struct IRArrayType {
    Box<IRTypeRef> element;
    size_t size;
};

struct IRTupleType {
    std::vector<Box<IRTypeRef>> elements;
};

struct IRFuncType {
    std::vector<Box<IRTypeRef>> params;
    Box<IRTypeRef> ret;
};

using IRTypeKind =
    std::variant<IRTypeRef, IRRefType, IRSliceType, IRArrayType, IRTupleType, IRFuncType>;

struct IRTypeExpr {
    IRTypeKind kind;
};

// ============================================================================
// Patterns
// ============================================================================

struct IRPatternLit {
    std::string value;
    std::string type_name;
};

struct IRPatternBind {
    std::string name;
    bool is_mut;
};

struct IRPatternWild {};

struct IRPatternTuple {
    std::vector<IRPatternPtr> elements;
};

struct IRPatternStruct {
    std::string type_name;
    std::vector<std::pair<std::string, IRPatternPtr>> fields;
};

struct IRPatternVariant {
    std::string variant_name;
    std::vector<IRPatternPtr> fields;
};

using IRPatternKind = std::variant<IRPatternLit, IRPatternBind, IRPatternWild, IRPatternTuple,
                                   IRPatternStruct, IRPatternVariant>;

struct IRPattern {
    IRPatternKind kind;
};

// ============================================================================
// Expressions
// ============================================================================

struct IRLiteral {
    std::string value;
    std::string type_name;
};

struct IRVar {
    std::string name;
};

struct IRBinaryOp {
    std::string op; // "+", "-", "*", "/", "==", etc.
    IRExprPtr left;
    IRExprPtr right;
};

struct IRUnaryOp {
    std::string op; // "-", "not", "ref", "deref"
    IRExprPtr operand;
};

struct IRCall {
    std::string func_name;
    std::vector<IRExprPtr> args;
};

struct IRMethodCall {
    IRExprPtr receiver;
    std::string method_name;
    std::vector<IRExprPtr> args;
};

struct IRFieldGet {
    IRExprPtr object;
    std::string field_name;
};

struct IRFieldSet {
    IRExprPtr object;
    std::string field_name;
    IRExprPtr value;
};

struct IRIndex {
    IRExprPtr object;
    IRExprPtr index;
};

struct IRStructExpr {
    std::string type_name;
    std::vector<std::pair<std::string, IRExprPtr>> fields; // sorted by name
};

struct IRVariantExpr {
    std::string variant_name;
    std::vector<IRExprPtr> fields;
};

struct IRTupleExpr {
    std::vector<IRExprPtr> elements;
};

struct IRArrayExpr {
    std::vector<IRExprPtr> elements;
};

struct IRArrayRepeat {
    IRExprPtr value;
    IRExprPtr count;
};

struct IRIf {
    IRExprPtr condition;
    IRExprPtr then_branch;
    std::optional<IRExprPtr> else_branch;
};

struct IRWhenArm {
    IRPatternPtr pattern;
    std::optional<IRExprPtr> guard;
    IRExprPtr body;
};

struct IRWhen {
    IRExprPtr scrutinee;
    std::vector<IRWhenArm> arms;
};

struct IRLoop {
    IRExprPtr body;
};

struct IRLoopIn {
    std::string binding;
    IRExprPtr iter;
    IRExprPtr body;
};

struct IRLoopWhile {
    IRExprPtr condition;
    IRExprPtr body;
};

struct IRBlock {
    std::vector<IRStmtPtr> stmts;
    std::optional<IRExprPtr> expr;
};

struct IRClosure {
    std::vector<std::pair<std::string, std::optional<IRTypeExpr>>> params;
    std::optional<IRTypeExpr> return_type;
    IRExprPtr body;
};

struct IRTry {
    IRExprPtr expr;
};

struct IRReturn {
    std::optional<IRExprPtr> value;
};

struct IRBreak {
    std::optional<IRExprPtr> value;
};

struct IRContinue {};

struct IRRange {
    IRExprPtr start;
    IRExprPtr end;
    bool inclusive;
};

using IRExprKind =
    std::variant<IRLiteral, IRVar, IRBinaryOp, IRUnaryOp, IRCall, IRMethodCall, IRFieldGet,
                 IRFieldSet, IRIndex, IRStructExpr, IRVariantExpr, IRTupleExpr, IRArrayExpr,
                 IRArrayRepeat, IRIf, IRWhen, IRLoop, IRLoopIn, IRLoopWhile, IRBlock, IRClosure,
                 IRTry, IRReturn, IRBreak, IRContinue, IRRange>;

struct IRExpr {
    IRExprKind kind;
};

// ============================================================================
// Statements
// ============================================================================

struct IRLet {
    IRPatternPtr pattern;
    std::optional<IRTypeExpr> type_annotation;
    IRExprPtr init;
};

struct IRVarMut {
    std::string name;
    std::optional<IRTypeExpr> type_annotation;
    IRExprPtr init;
};

struct IRAssign {
    IRExprPtr target;
    IRExprPtr value;
};

struct IRExprStmt {
    IRExprPtr expr;
};

using IRStmtKind = std::variant<IRLet, IRVarMut, IRAssign, IRExprStmt>;

struct IRStmt {
    IRStmtKind kind;
};

// ============================================================================
// Declarations
// ============================================================================

struct IRGenericParam {
    std::string name;
    std::vector<std::string> bounds;
};

struct IRParam {
    std::string name;
    IRTypeExpr type;
};

struct IRFunc {
    StableId id;
    std::string name;
    Visibility vis;
    std::vector<IRGenericParam> generics;
    std::vector<IRParam> params;
    std::optional<IRTypeExpr> return_type;
    std::vector<std::string> effects;
    std::optional<IRBlock> body;
    std::optional<std::string> ai_context;
};

struct IRField {
    std::string name;
    IRTypeExpr type;
    Visibility vis;
};

struct IRStructType {
    std::vector<IRField> fields; // sorted alphabetically
};

struct IREnumVariant {
    std::string name;
    std::vector<IRTypeExpr> fields; // tuple variant fields
};

struct IREnumType {
    std::vector<IREnumVariant> variants; // sorted alphabetically
};

struct IRAliasType {
    IRTypeExpr target;
};

using IRTypeDefKind = std::variant<IRStructType, IREnumType, IRAliasType>;

struct IRType {
    StableId id;
    std::string name;
    Visibility vis;
    std::vector<IRGenericParam> generics;
    IRTypeDefKind kind;
};

struct IRBehaviorMethod {
    std::string name;
    std::vector<IRParam> params;
    std::optional<IRTypeExpr> return_type;
    std::optional<IRBlock> default_impl;
};

struct IRBehavior {
    StableId id;
    std::string name;
    Visibility vis;
    std::vector<IRGenericParam> generics;
    std::vector<std::string> super_behaviors;
    std::vector<IRBehaviorMethod> methods; // sorted alphabetically
};

struct IRImplMethod {
    StableId id;
    std::string name;
    std::vector<IRParam> params;
    std::optional<IRTypeExpr> return_type;
    IRBlock body;
};

struct IRImpl {
    StableId id;
    std::vector<IRGenericParam> generics;
    std::string target_type;
    std::optional<std::string> behavior; // None for inherent impl
    std::vector<IRImplMethod> methods;   // sorted alphabetically
};

struct IRConst {
    StableId id;
    std::string name;
    Visibility vis;
    IRTypeExpr type;
    IRExprPtr value;
};

struct IRImport {
    std::string path;
    std::optional<std::string> alias;
};

using IRItem = std::variant<IRConst, IRType, IRBehavior, IRImpl, IRFunc>;

struct IRModule {
    StableId id;
    std::string name;
    std::vector<std::string> caps;
    std::vector<IRImport> imports; // sorted by path
    std::vector<IRItem> items;     // sorted by kind, then name
};

// ============================================================================
// IR Builder
// ============================================================================

class IRBuilder {
public:
    IRBuilder();

    // Convert AST to IR
    auto build_module(const parser::Module& module, const std::string& module_name) -> IRModule;

private:
    size_t next_seq_ = 0;
    std::string current_module_;

    // Generate stable ID
    auto generate_id(const std::string& name, const std::string& signature) -> StableId;

    // Convert declarations
    auto build_func(const parser::FuncDecl& func) -> IRFunc;
    auto build_struct(const parser::StructDecl& type) -> IRType;
    auto build_enum(const parser::EnumDecl& en) -> IRType;
    auto build_trait(const parser::TraitDecl& trait) -> IRBehavior;
    auto build_impl(const parser::ImplDecl& impl) -> IRImpl;
    auto build_const(const parser::ConstDecl& cst) -> IRConst;

    // Convert expressions
    auto build_expr(const parser::Expr& expr) -> IRExprPtr;
    auto build_block(const parser::BlockExpr& block) -> IRBlock;

    // Convert statements
    auto build_stmt(const parser::Stmt& stmt) -> IRStmtPtr;

    // Convert patterns
    auto build_pattern(const parser::Pattern& pattern) -> IRPatternPtr;

    // Convert types
    auto build_type_expr(const parser::Type& type) -> IRTypeExpr;

    // Helpers
    auto visibility_from_ast(parser::Visibility vis) -> Visibility;
    auto binary_op_to_string(parser::BinaryOp op) -> std::string;
    auto unary_op_to_string(parser::UnaryOp op) -> std::string;
};

// ============================================================================
// IR Emitter (S-expression format)
// ============================================================================

class IREmitter {
public:
    struct Options {
        int indent_size = 2;
        bool compact = false;
    };

    explicit IREmitter(Options opts = {2, false});

    auto emit_module(const IRModule& module) -> std::string;

private:
    Options opts_;
    int indent_level_ = 0;

    void emit_item(std::ostringstream& out, const IRItem& item);
    void emit_func(std::ostringstream& out, const IRFunc& func);
    void emit_type(std::ostringstream& out, const IRType& type);
    void emit_behavior(std::ostringstream& out, const IRBehavior& behavior);
    void emit_impl(std::ostringstream& out, const IRImpl& impl);
    void emit_const(std::ostringstream& out, const IRConst& cst);

    void emit_expr(std::ostringstream& out, const IRExpr& expr);
    void emit_stmt(std::ostringstream& out, const IRStmt& stmt);
    void emit_pattern(std::ostringstream& out, const IRPattern& pattern);
    void emit_type_expr(std::ostringstream& out, const IRTypeExpr& type);
    void emit_block(std::ostringstream& out, const IRBlock& block);

    void emit_indent(std::ostringstream& out);
    void emit_newline(std::ostringstream& out);
};

} // namespace tml::ir

#endif // TML_IR_IR_HPP
