// TML Mid-level IR (MIR) - SSA Form Intermediate Representation
//
// MIR sits between the type-checked AST and LLVM IR generation.
// It provides a clean, optimizable representation in SSA form.
//
// Design goals:
// 1. SSA form - each variable defined exactly once
// 2. Explicit control flow with basic blocks
// 3. Type-annotated for easy optimization
// 4. Close enough to LLVM IR for easy lowering
// 5. High-level enough for TML-specific optimizations

#pragma once

#include "common.hpp"
#include "types/type.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tml::mir {

// Forward declarations
struct BasicBlock;
struct Function;
struct Module;

// ============================================================================
// MIR Types - Simplified type representation for codegen
// ============================================================================

// Primitive types known at MIR level
enum class PrimitiveType {
    Unit,
    Bool,
    I8,
    I16,
    I32,
    I64,
    I128,
    U8,
    U16,
    U32,
    U64,
    U128,
    F32,
    F64,
    Ptr, // Raw pointer (void*)
    Str, // String pointer
};

// MIR type representation
struct MirType;
using MirTypePtr = std::shared_ptr<MirType>;

struct MirPrimitiveType {
    PrimitiveType kind;
};

struct MirPointerType {
    MirTypePtr pointee;
    bool is_mut;
};

struct MirArrayType {
    MirTypePtr element;
    size_t size;
};

struct MirSliceType {
    MirTypePtr element;
};

struct MirTupleType {
    std::vector<MirTypePtr> elements;
};

struct MirStructType {
    std::string name;
    std::vector<MirTypePtr> type_args; // For generic instantiations
};

struct MirEnumType {
    std::string name;
    std::vector<MirTypePtr> type_args;
};

struct MirFunctionType {
    std::vector<MirTypePtr> params;
    MirTypePtr return_type;
};

struct MirType {
    std::variant<MirPrimitiveType, MirPointerType, MirArrayType, MirSliceType, MirTupleType,
                 MirStructType, MirEnumType, MirFunctionType>
        kind;

    // Helper methods
    [[nodiscard]] auto is_primitive() const -> bool {
        return std::holds_alternative<MirPrimitiveType>(kind);
    }
    [[nodiscard]] auto is_unit() const -> bool {
        if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
            return p->kind == PrimitiveType::Unit;
        }
        return false;
    }
    [[nodiscard]] auto is_bool() const -> bool {
        if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
            return p->kind == PrimitiveType::Bool;
        }
        return false;
    }
    [[nodiscard]] auto is_integer() const -> bool;
    [[nodiscard]] auto is_float() const -> bool;
    [[nodiscard]] auto is_signed() const -> bool;

    // Get size in bits (for integers)
    [[nodiscard]] auto bit_width() const -> int;
};

// Type constructors
auto make_unit_type() -> MirTypePtr;
auto make_bool_type() -> MirTypePtr;
auto make_i8_type() -> MirTypePtr;
auto make_i16_type() -> MirTypePtr;
auto make_i32_type() -> MirTypePtr;
auto make_i64_type() -> MirTypePtr;
auto make_f32_type() -> MirTypePtr;
auto make_f64_type() -> MirTypePtr;
auto make_ptr_type() -> MirTypePtr;
auto make_str_type() -> MirTypePtr;
auto make_pointer_type(MirTypePtr pointee, bool is_mut = false) -> MirTypePtr;
auto make_array_type(MirTypePtr element, size_t size) -> MirTypePtr;
auto make_tuple_type(std::vector<MirTypePtr> elements) -> MirTypePtr;
auto make_struct_type(const std::string& name, std::vector<MirTypePtr> type_args = {})
    -> MirTypePtr;
auto make_enum_type(const std::string& name, std::vector<MirTypePtr> type_args = {}) -> MirTypePtr;

// ============================================================================
// MIR Values - SSA Values
// ============================================================================

// Each value in MIR has a unique ID
using ValueId = uint32_t;
constexpr ValueId INVALID_VALUE = UINT32_MAX;

// Value reference (used in operands)
struct Value {
    ValueId id;
    MirTypePtr type;

    [[nodiscard]] auto is_valid() const -> bool {
        return id != INVALID_VALUE;
    }
};

// ============================================================================
// MIR Constants
// ============================================================================

struct ConstInt {
    int64_t value;
    bool is_signed;
    int bit_width;
};

struct ConstFloat {
    double value;
    bool is_f64;
};

struct ConstBool {
    bool value;
};

struct ConstString {
    std::string value;
};

struct ConstUnit {};

using Constant = std::variant<ConstInt, ConstFloat, ConstBool, ConstString, ConstUnit>;

// ============================================================================
// MIR Instructions (SSA Form)
// ============================================================================

// Each instruction produces at most one value (SSA property)
// The result ValueId is stored separately in the basic block

// Binary operations
enum class BinOp {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    // Comparison
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    // Logical (bool only)
    And,
    Or,
    // Bitwise
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
};

// Unary operations
enum class UnaryOp {
    Neg,    // Arithmetic negation
    Not,    // Logical not
    BitNot, // Bitwise not
};

// Binary instruction: result = left op right
struct BinaryInst {
    BinOp op;
    Value left;
    Value right;
    MirTypePtr result_type; // Type of the result (for codegen)
};

// Unary instruction: result = op operand
struct UnaryInst {
    UnaryOp op;
    Value operand;
    MirTypePtr result_type; // Type of the result (for codegen)
};

// Load from memory: result = *ptr
struct LoadInst {
    Value ptr;
    MirTypePtr result_type; // Type being loaded
};

// Store to memory: *ptr = value (no result)
struct StoreInst {
    Value ptr;
    Value value;
    MirTypePtr value_type; // Type being stored
};

// Allocate stack memory: result = alloca type
struct AllocaInst {
    MirTypePtr alloc_type;
    std::string name; // Original variable name (for debugging)
};

// Get element pointer: result = &aggregate[index]
struct GetElementPtrInst {
    Value base;
    std::vector<Value> indices;
    MirTypePtr base_type;   // Type of base pointer
    MirTypePtr result_type; // Type of result pointer
};

// Extract value from aggregate: result = aggregate.index
struct ExtractValueInst {
    Value aggregate;
    std::vector<uint32_t> indices;
    MirTypePtr aggregate_type; // Type of aggregate
    MirTypePtr result_type;    // Type of extracted value
};

// Insert value into aggregate: result = aggregate with [index] = value
struct InsertValueInst {
    Value aggregate;
    Value value;
    std::vector<uint32_t> indices;
    MirTypePtr aggregate_type; // Type of aggregate
    MirTypePtr value_type;     // Type of value being inserted
};

// Function call: result = func(args...)
struct CallInst {
    std::string func_name;
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types; // Types of arguments
    MirTypePtr return_type;
};

// Method call (resolved to function call with self)
struct MethodCallInst {
    Value receiver;
    std::string receiver_type; // Type name of receiver
    std::string method_name;
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types; // Types of arguments
    MirTypePtr return_type;
};

// Cast instruction: result = cast value to target_type
enum class CastKind {
    Bitcast,  // Reinterpret bits
    Trunc,    // Truncate integer
    ZExt,     // Zero extend
    SExt,     // Sign extend
    FPTrunc,  // Float truncate
    FPExt,    // Float extend
    FPToSI,   // Float to signed int
    FPToUI,   // Float to unsigned int
    SIToFP,   // Signed int to float
    UIToFP,   // Unsigned int to float
    PtrToInt, // Pointer to integer
    IntToPtr, // Integer to pointer
};

struct CastInst {
    CastKind kind;
    Value operand;
    MirTypePtr source_type; // Source type
    MirTypePtr target_type; // Target type
};

// Phi node: result = phi [val1, block1], [val2, block2], ...
struct PhiInst {
    std::vector<std::pair<Value, uint32_t>> incoming; // (value, block_id)
    MirTypePtr result_type;                           // Type of incoming values
};

// Constant: result = constant
struct ConstantInst {
    Constant value;
};

// Select: result = cond ? true_val : false_val
struct SelectInst {
    Value condition;
    Value true_val;
    Value false_val;
    MirTypePtr result_type; // Type of true_val/false_val
};

// Struct construction: result = { field1, field2, ... }
struct StructInitInst {
    std::string struct_name;
    std::vector<Value> fields;
    std::vector<MirTypePtr> field_types; // Types of field values
};

// Enum variant construction: result = EnumName::Variant(payload...)
struct EnumInitInst {
    std::string enum_name;
    std::string variant_name;
    int variant_index; // Index of variant in enum
    std::vector<Value> payload;
    std::vector<MirTypePtr> payload_types; // Types of payload values
};

// Tuple construction: result = (elem1, elem2, ...)
struct TupleInitInst {
    std::vector<Value> elements;
    std::vector<MirTypePtr> element_types; // Types of elements
    MirTypePtr result_type;                // Full tuple type
};

// Array construction: result = [elem1, elem2, ...]
struct ArrayInitInst {
    std::vector<Value> elements;
    MirTypePtr element_type;
    MirTypePtr result_type; // Full array type
};

// Await instruction: result = await poll_value (suspension point)
// This instruction marks a potential suspension point in async functions.
// The poll_value is a Poll[T] and result is T (extracted from Ready).
struct AwaitInst {
    Value poll_value;       // The Poll[T] value being awaited
    MirTypePtr poll_type;   // Poll[T] type
    MirTypePtr result_type; // T type (inner type)
    uint32_t suspension_id; // ID of this suspension point (for state machine)
};

// All instruction types
using Instruction =
    std::variant<BinaryInst, UnaryInst, LoadInst, StoreInst, AllocaInst, GetElementPtrInst,
                 ExtractValueInst, InsertValueInst, CallInst, MethodCallInst, CastInst, PhiInst,
                 ConstantInst, SelectInst, StructInitInst, EnumInitInst, TupleInitInst,
                 ArrayInitInst, AwaitInst>;

// Instruction with result
struct InstructionData {
    ValueId result;  // INVALID_VALUE for void instructions (store)
    MirTypePtr type; // Result type
    Instruction inst;
    SourceSpan span; // Source location for debugging
};

// ============================================================================
// MIR Terminators (Control Flow)
// ============================================================================

// Return from function
struct ReturnTerm {
    std::optional<Value> value;
};

// Unconditional branch
struct BranchTerm {
    uint32_t target; // Block ID
};

// Conditional branch
struct CondBranchTerm {
    Value condition;
    uint32_t true_block;
    uint32_t false_block;
};

// Switch on integer value
struct SwitchTerm {
    Value discriminant;
    std::vector<std::pair<int64_t, uint32_t>> cases; // (value, block)
    uint32_t default_block;
};

// Unreachable (after panic, infinite loop, etc.)
struct UnreachableTerm {};

using Terminator =
    std::variant<ReturnTerm, BranchTerm, CondBranchTerm, SwitchTerm, UnreachableTerm>;

// ============================================================================
// Basic Block
// ============================================================================

struct BasicBlock {
    uint32_t id;
    std::string name; // Label name (for debugging)
    std::vector<InstructionData> instructions;
    std::optional<Terminator> terminator;

    // Predecessors/successors (computed during CFG construction)
    std::vector<uint32_t> predecessors;
    std::vector<uint32_t> successors;
};

// ============================================================================
// Function
// ============================================================================

struct FunctionParam {
    std::string name;
    MirTypePtr type;
    ValueId value_id; // SSA value for this parameter
};

// ============================================================================
// Async State Machine
// ============================================================================

// Represents a suspension point (await expression) in an async function
struct SuspensionPoint {
    uint32_t id;            // Unique ID for this suspension point
    uint32_t block_before;  // Block containing the await
    uint32_t block_after;   // Block to resume after await completes
    ValueId awaited_value;  // The Poll value being awaited
    ValueId result_value;   // Where to store the extracted result
    MirTypePtr result_type; // Type of the awaited value (inner T of Poll[T])
    SourceSpan span;        // Source location of await
};

// Saved local variable that lives across suspension points
struct SavedLocal {
    std::string name;              // Original variable name
    ValueId value_id;              // SSA value ID
    MirTypePtr type;               // Type of the variable
    std::vector<uint32_t> live_at; // Suspension points where this is live
};

// State machine representation for an async function
struct AsyncStateMachine {
    std::string state_struct_name;            // Name of generated state struct
    std::vector<SuspensionPoint> suspensions; // All suspension points
    std::vector<SavedLocal> saved_locals;     // Locals that span suspensions
    MirTypePtr poll_return_type;              // Poll[T] return type
    MirTypePtr inner_return_type;             // T (unwrapped return type)

    // Check if function needs state machine transformation
    [[nodiscard]] auto needs_transformation() const -> bool {
        return !suspensions.empty();
    }

    // Get number of states (entry + one per suspension + done)
    [[nodiscard]] auto state_count() const -> size_t {
        return suspensions.size() + 2; // 0=entry, 1..N=after await, N+1=done
    }
};

struct Function {
    std::string name;
    std::vector<FunctionParam> params;
    MirTypePtr return_type;
    std::vector<BasicBlock> blocks;
    bool is_public = false;
    bool is_async = false;                          // Whether this is an async function
    std::optional<AsyncStateMachine> state_machine; // State machine for async functions
    std::vector<std::string> attributes;            // @inline, @noinline, etc.

    // Entry block is always blocks[0]
    [[nodiscard]] auto entry_block() -> BasicBlock& {
        return blocks[0];
    }
    [[nodiscard]] auto entry_block() const -> const BasicBlock& {
        return blocks[0];
    }

    // Value ID counter for SSA
    ValueId next_value_id = 0;

    // Block ID counter
    uint32_t next_block_id = 0;

    // Create a new value ID
    auto fresh_value() -> ValueId {
        return next_value_id++;
    }

    // Create a new block
    auto create_block(const std::string& label = "") -> uint32_t;

    // Get block by ID
    [[nodiscard]] auto get_block(uint32_t id) -> BasicBlock*;
    [[nodiscard]] auto get_block(uint32_t id) const -> const BasicBlock*;
};

// ============================================================================
// Struct/Enum Definitions
// ============================================================================

struct StructField {
    std::string name;
    MirTypePtr type;
};

struct StructDef {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<StructField> fields;
};

struct EnumVariant {
    std::string name;
    std::vector<MirTypePtr> payload_types;
};

struct EnumDef {
    std::string name;
    std::vector<std::string> type_params;
    std::vector<EnumVariant> variants;
};

// ============================================================================
// Module
// ============================================================================

struct Module {
    std::string name;
    std::vector<StructDef> structs;
    std::vector<EnumDef> enums;
    std::vector<Function> functions;

    // Global constants
    std::unordered_map<std::string, Constant> constants;
};

// ============================================================================
// MIR Pretty Printer
// ============================================================================

class MirPrinter {
public:
    explicit MirPrinter(bool use_colors = false);

    auto print_module(const Module& module) -> std::string;
    auto print_function(const Function& func) -> std::string;
    auto print_block(const BasicBlock& block) -> std::string;
    auto print_instruction(const InstructionData& inst) -> std::string;
    auto print_terminator(const Terminator& term) -> std::string;
    auto print_value(const Value& val) -> std::string;
    auto print_type(const MirTypePtr& type) -> std::string;

private:
    bool use_colors_;
};

// Convenience free function for printing a module
inline auto print_module(const Module& module, bool use_colors = false) -> std::string {
    MirPrinter printer(use_colors);
    return printer.print_module(module);
}

} // namespace tml::mir
