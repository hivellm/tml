//! # TML Mid-level IR (MIR)
//!
//! MIR is a Static Single Assignment (SSA) form intermediate representation
//! that sits between the type-checked AST and LLVM IR generation. It provides
//! a clean, optimizable representation for TML programs.
//!
//! ## Design Goals
//!
//! 1. **SSA Form** - Each variable is defined exactly once
//! 2. **Explicit Control Flow** - Basic blocks with explicit terminators
//! 3. **Type Annotations** - All values have known types
//! 4. **LLVM Compatible** - Easy lowering to LLVM IR
//! 5. **TML Aware** - High-level enough for TML-specific optimizations
//!
//! ## Structure
//!
//! - **Module**: Top-level container with structs, enums, and functions
//! - **Function**: Contains basic blocks in CFG form
//! - **BasicBlock**: Sequence of instructions ending in a terminator
//! - **Instruction**: SSA operations (binary, call, load, store, etc.)
//! - **Terminator**: Control flow (return, branch, switch)
//!
//! ## Value System
//!
//! Every value has a unique `ValueId` and associated `MirTypePtr`. Values are
//! immutable once created (SSA property). Phi nodes are used at control flow
//! merge points.

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

/// Primitive types known at MIR level.
enum class PrimitiveType {
    Unit, ///< Unit type (void).
    Bool, ///< Boolean.
    I8,   ///< 8-bit signed integer.
    I16,  ///< 16-bit signed integer.
    I32,  ///< 32-bit signed integer.
    I64,  ///< 64-bit signed integer.
    I128, ///< 128-bit signed integer.
    U8,   ///< 8-bit unsigned integer.
    U16,  ///< 16-bit unsigned integer.
    U32,  ///< 32-bit unsigned integer.
    U64,  ///< 64-bit unsigned integer.
    U128, ///< 128-bit unsigned integer.
    F32,  ///< 32-bit floating point.
    F64,  ///< 64-bit floating point.
    Ptr,  ///< Raw pointer (void*).
    Str,  ///< String pointer.
};

/// MIR type representation.
struct MirType;
/// Shared pointer to MIR type.
using MirTypePtr = std::shared_ptr<MirType>;

/// Primitive type variant.
struct MirPrimitiveType {
    PrimitiveType kind; ///< The primitive type kind.
};

/// Pointer type variant.
struct MirPointerType {
    MirTypePtr pointee; ///< Type being pointed to.
    bool is_mut;        ///< True for mutable pointers.
};

/// Fixed-size array type variant.
struct MirArrayType {
    MirTypePtr element; ///< Element type.
    size_t size;        ///< Number of elements.
};

/// Slice type variant (fat pointer).
struct MirSliceType {
    MirTypePtr element; ///< Element type.
};

/// Tuple type variant.
struct MirTupleType {
    std::vector<MirTypePtr> elements; ///< Element types.
};

/// Struct type variant.
struct MirStructType {
    std::string name;                  ///< Struct name.
    std::vector<MirTypePtr> type_args; ///< Generic type arguments.
};

/// Enum type variant.
struct MirEnumType {
    std::string name;                  ///< Enum name.
    std::vector<MirTypePtr> type_args; ///< Generic type arguments.
};

/// Function type variant.
struct MirFunctionType {
    std::vector<MirTypePtr> params; ///< Parameter types.
    MirTypePtr return_type;         ///< Return type.
};

/// SIMD vector type variant.
struct MirVectorType {
    MirTypePtr element; ///< Element type (must be primitive).
    size_t width;       ///< Number of elements (e.g., 4 for <4 x i32>).
};

/// MIR type - a tagged union of all type variants.
struct MirType {
    /// The type variant data.
    std::variant<MirPrimitiveType, MirPointerType, MirArrayType, MirSliceType, MirTupleType,
                 MirStructType, MirEnumType, MirFunctionType, MirVectorType>
        kind;

    /// Returns true if this is a primitive type.
    [[nodiscard]] auto is_primitive() const -> bool {
        return std::holds_alternative<MirPrimitiveType>(kind);
    }
    /// Returns true if this is the unit type.
    [[nodiscard]] auto is_unit() const -> bool {
        if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
            return p->kind == PrimitiveType::Unit;
        }
        return false;
    }
    /// Returns true if this is the bool type.
    [[nodiscard]] auto is_bool() const -> bool {
        if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
            return p->kind == PrimitiveType::Bool;
        }
        return false;
    }
    /// Returns true if this is an integer type.
    [[nodiscard]] auto is_integer() const -> bool;
    /// Returns true if this is a float type.
    [[nodiscard]] auto is_float() const -> bool;
    /// Returns true if this is a signed integer type.
    [[nodiscard]] auto is_signed() const -> bool;
    /// Returns the bit width for integer types.
    [[nodiscard]] auto bit_width() const -> int;
    /// Returns true if this is a pointer type.
    [[nodiscard]] auto is_pointer() const -> bool {
        return std::holds_alternative<MirPointerType>(kind);
    }
    /// Returns true if this is a struct type.
    [[nodiscard]] auto is_struct() const -> bool {
        return std::holds_alternative<MirStructType>(kind);
    }
    /// Returns true if this is an enum type.
    [[nodiscard]] auto is_enum() const -> bool {
        return std::holds_alternative<MirEnumType>(kind);
    }
    /// Returns true if this is a tuple type.
    [[nodiscard]] auto is_tuple() const -> bool {
        return std::holds_alternative<MirTupleType>(kind);
    }
    /// Returns true if this is an array type.
    [[nodiscard]] auto is_array() const -> bool {
        return std::holds_alternative<MirArrayType>(kind);
    }
    /// Returns true if this is a SIMD vector type.
    [[nodiscard]] auto is_vector() const -> bool {
        return std::holds_alternative<MirVectorType>(kind);
    }
    /// Returns true if this is an aggregate type (struct, enum, tuple, array).
    /// Aggregate types benefit from alloca+store+load instead of phi nodes.
    [[nodiscard]] auto is_aggregate() const -> bool {
        return is_struct() || is_enum() || is_tuple() || is_array();
    }
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
auto make_vector_type(MirTypePtr element, size_t width) -> MirTypePtr;

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
    MirTypePtr result_type = nullptr; // Type being loaded
    bool is_volatile = false;         // Volatile load (prevents optimization)
};

// Store to memory: *ptr = value (no result)
struct StoreInst {
    Value ptr;
    Value value;
    MirTypePtr value_type = nullptr; // Type being stored
    bool is_volatile = false;        // Volatile store (prevents optimization)
};

// Allocate stack memory: result = alloca type
struct AllocaInst {
    MirTypePtr alloc_type;
    std::string name;              // Original variable name (for debugging)
    bool is_stack_eligible = true; // Always stack-eligible since it's already alloca
    bool is_volatile = false;      // Volatile variable (prevents optimization)
};

// Get element pointer: result = &aggregate[index]
struct GetElementPtrInst {
    Value base;
    std::vector<Value> indices;
    MirTypePtr base_type;           // Type of base pointer
    MirTypePtr result_type;         // Type of result pointer
    bool needs_bounds_check = true; // Whether bounds check is needed (false if proven safe)
    int64_t known_array_size = -1;  // Array size for bounds check (-1 if unknown)
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

/// Devirtualization info for calls that were converted from virtual to direct.
struct DevirtInfo {
    std::string original_class; ///< Original receiver class type.
    std::string method_name;    ///< Original method name.
    bool from_sealed_class;     ///< Was devirtualized due to sealed class.
    bool from_exact_type;       ///< Was devirtualized due to exact type known.
    bool from_single_impl;      ///< Was devirtualized due to single implementation.
    bool from_final_method;     ///< Was devirtualized due to final method.
};

// Function call: result = func(args...)
struct CallInst {
    std::string func_name;
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types; // Types of arguments
    MirTypePtr return_type;
    std::optional<DevirtInfo> devirt_info; ///< Set if this was a devirtualized call.
    bool is_stack_eligible = false;        ///< True if result can be stack-allocated (for allocs).

    /// Returns true if this call was devirtualized from a virtual method call.
    [[nodiscard]] auto is_devirtualized() const -> bool {
        return devirt_info.has_value();
    }

    /// Returns true if this is a heap allocation that can be stack-promoted.
    [[nodiscard]] auto can_stack_promote() const -> bool {
        return is_stack_eligible &&
               (func_name == "alloc" || func_name == "heap_alloc" || func_name == "tml_alloc" ||
                func_name == "malloc" || func_name == "Heap::new");
    }
};

// Method call (resolved to function call with self)
struct MethodCallInst {
    Value receiver;
    std::string receiver_type; // Type name of receiver
    std::string method_name;
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types; // Types of arguments
    MirTypePtr return_type;
    std::optional<DevirtInfo> devirt_info; ///< Set if this was a devirtualized call.

    /// Returns true if this call was devirtualized.
    [[nodiscard]] bool is_devirtualized() const {
        return devirt_info.has_value();
    }
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
    bool is_stack_eligible = false;      ///< True if instance can be stack-allocated.
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

// Closure construction: result = closure { func_ptr, captures... }
// A closure is represented as a struct containing a function pointer and captured values.
struct ClosureInitInst {
    std::string func_name;                                     // Generated closure function name
    std::vector<std::pair<std::string, Value>> captures;       // Captured variables (name, value)
    std::vector<std::pair<std::string, MirTypePtr>> cap_types; // Types of captures
    MirTypePtr func_type;                                      // Function type of the closure
    MirTypePtr result_type;                                    // Closure struct type
};

// ============================================================================
// Atomic Instructions
// ============================================================================

/// Memory ordering for atomic operations.
/// Maps directly to LLVM atomic orderings.
enum class AtomicOrdering {
    Monotonic, ///< Relaxed - no synchronization
    Acquire,   ///< Acquire - prevents reordering after
    Release,   ///< Release - prevents reordering before
    AcqRel,    ///< Both acquire and release
    SeqCst,    ///< Sequentially consistent
};

/// Atomic RMW (read-modify-write) operation types.
enum class AtomicRMWOp {
    Xchg, ///< Exchange (swap)
    Add,  ///< Addition
    Sub,  ///< Subtraction
    And,  ///< Bitwise AND
    Nand, ///< Bitwise NAND
    Or,   ///< Bitwise OR
    Xor,  ///< Bitwise XOR
    Max,  ///< Signed maximum
    Min,  ///< Signed minimum
    UMax, ///< Unsigned maximum
    UMin, ///< Unsigned minimum
};

/// Atomic load instruction: result = atomic_load(ptr, ordering)
struct AtomicLoadInst {
    Value ptr;               ///< Pointer to load from
    AtomicOrdering ordering; ///< Memory ordering
    MirTypePtr result_type;  ///< Type being loaded
};

/// Atomic store instruction: atomic_store(ptr, value, ordering)
struct AtomicStoreInst {
    Value ptr;               ///< Pointer to store to
    Value value;             ///< Value to store
    AtomicOrdering ordering; ///< Memory ordering
    MirTypePtr value_type;   ///< Type being stored
};

/// Atomic read-modify-write instruction: result = atomicrmw op ptr, val, ordering
struct AtomicRMWInst {
    AtomicRMWOp op;          ///< RMW operation type
    Value ptr;               ///< Pointer to operate on
    Value value;             ///< Value operand
    AtomicOrdering ordering; ///< Memory ordering
    MirTypePtr value_type;   ///< Type of value
};

/// Atomic compare-and-exchange instruction:
/// result = cmpxchg ptr, expected, desired, success_ordering, failure_ordering
/// Returns a struct { T value; bool success; }
struct AtomicCmpXchgInst {
    Value ptr;                       ///< Pointer to operate on
    Value expected;                  ///< Expected value
    Value desired;                   ///< Desired new value
    AtomicOrdering success_ordering; ///< Ordering on success
    AtomicOrdering failure_ordering; ///< Ordering on failure
    bool weak = false;               ///< If true, may spuriously fail
    MirTypePtr value_type;           ///< Type of value
};

/// Memory fence instruction: fence ordering
struct FenceInst {
    AtomicOrdering ordering;    ///< Memory ordering
    bool single_thread = false; ///< If true, compiler fence only (signal fence)
};

// ============================================================================
// SIMD Vector Instructions
// ============================================================================

/// Vector load: result = vector_load(ptr, width)
/// Loads 'width' consecutive elements starting at ptr into a vector.
struct VectorLoadInst {
    Value ptr;               ///< Base pointer
    size_t width;            ///< Vector width (number of elements)
    MirTypePtr element_type; ///< Scalar element type
    MirTypePtr result_type;  ///< Vector type
};

/// Vector store: vector_store(ptr, vec_value, width)
/// Stores vector elements to consecutive memory locations.
struct VectorStoreInst {
    Value ptr;               ///< Base pointer
    Value value;             ///< Vector value to store
    size_t width;            ///< Vector width
    MirTypePtr element_type; ///< Scalar element type
};

/// Vector binary operation: result = vec_op(lhs, rhs)
struct VectorBinaryInst {
    BinOp op;                ///< Binary operation (Add, Sub, Mul, etc.)
    Value left;              ///< Left operand (vector)
    Value right;             ///< Right operand (vector)
    size_t width;            ///< Vector width
    MirTypePtr element_type; ///< Scalar element type
    MirTypePtr result_type;  ///< Vector type
};

/// Horizontal reduction operation type.
enum class ReductionOp {
    Add, ///< Sum all elements
    Mul, ///< Multiply all elements
    Min, ///< Minimum element
    Max, ///< Maximum element
    And, ///< Bitwise AND all elements
    Or,  ///< Bitwise OR all elements
    Xor, ///< Bitwise XOR all elements
};

/// Vector reduction: result = reduce_op(vector)
/// Reduces a vector to a scalar by applying an associative operation.
struct VectorReductionInst {
    ReductionOp op;          ///< Reduction operation
    Value vector;            ///< Vector operand
    size_t width;            ///< Vector width
    MirTypePtr element_type; ///< Scalar element type (also result type)
};

/// Vector splat: result = splat(scalar, width)
/// Creates a vector with all elements set to the scalar value.
struct VectorSplatInst {
    Value scalar;            ///< Scalar value to broadcast
    size_t width;            ///< Vector width
    MirTypePtr element_type; ///< Scalar element type
    MirTypePtr result_type;  ///< Vector type
};

/// Vector extract: result = extract(vector, index)
/// Extracts a single scalar element from a vector.
struct VectorExtractInst {
    Value vector;            ///< Vector operand
    uint32_t index;          ///< Index to extract (constant)
    MirTypePtr element_type; ///< Scalar element type (result type)
};

/// Vector insert: result = insert(vector, scalar, index)
/// Inserts a scalar into a vector at the specified index.
struct VectorInsertInst {
    Value vector;           ///< Vector operand
    Value scalar;           ///< Scalar to insert
    uint32_t index;         ///< Index to insert at (constant)
    MirTypePtr result_type; ///< Vector type
};

// All instruction types
using Instruction =
    std::variant<BinaryInst, UnaryInst, LoadInst, StoreInst, AllocaInst, GetElementPtrInst,
                 ExtractValueInst, InsertValueInst, CallInst, MethodCallInst, CastInst, PhiInst,
                 ConstantInst, SelectInst, StructInitInst, EnumInitInst, TupleInitInst,
                 ArrayInitInst, AwaitInst, ClosureInitInst,
                 // Atomic instructions
                 AtomicLoadInst, AtomicStoreInst, AtomicRMWInst, AtomicCmpXchgInst, FenceInst,
                 // SIMD vector instructions
                 VectorLoadInst, VectorStoreInst, VectorBinaryInst, VectorReductionInst,
                 VectorSplatInst, VectorExtractInst, VectorInsertInst>;

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

    // sret (struct return) calling convention support
    bool uses_sret = false;                ///< True if function uses sret parameter for return
    MirTypePtr original_return_type;       ///< Original return type before sret conversion
    ValueId sret_param_id = INVALID_VALUE; ///< Value ID of the sret parameter

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
// Class Metadata (for OOP escape analysis optimization)
// ============================================================================

/// Metadata about a class for escape analysis and optimization.
///
/// This information enables aggressive optimizations for sealed classes
/// and classes with known allocation characteristics.
struct ClassMetadata {
    std::string name;                         ///< Class name.
    bool is_sealed = false;                   ///< True if class cannot be inherited from.
    bool is_abstract = false;                 ///< True if class cannot be instantiated.
    bool is_value = false;                    ///< True for @value classes (no vtable).
    bool stack_allocatable = false;           ///< True if instances can be stack-allocated.
    size_t estimated_size = 0;                ///< Estimated size in bytes (vtable ptr + fields).
    size_t inheritance_depth = 0;             ///< Depth in inheritance hierarchy.
    std::optional<std::string> base_class;    ///< Parent class name (if any).
    std::vector<std::string> subclasses;      ///< Known subclasses (empty if sealed).
    std::vector<std::string> virtual_methods; ///< Virtual method names.
    std::vector<std::string> final_methods;   ///< Final method names.

    /// Returns true if this class has no virtual methods (pure value type).
    [[nodiscard]] auto is_pure_value() const -> bool {
        return is_value && virtual_methods.empty();
    }

    /// Returns true if all method calls can be devirtualized.
    [[nodiscard]] auto can_devirtualize_all() const -> bool {
        return is_sealed || is_value || subclasses.empty();
    }

    /// Returns true if instances of this class don't escape through method calls.
    /// This is true for sealed classes where we know all possible method implementations.
    [[nodiscard]] auto methods_preserve_noescapse() const -> bool {
        return is_sealed && !is_abstract;
    }
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

    // Class metadata for OOP optimization (keyed by class name)
    std::unordered_map<std::string, ClassMetadata> class_metadata;

    /// Looks up class metadata by name.
    [[nodiscard]] auto get_class_metadata(const std::string& class_name) const
        -> std::optional<ClassMetadata> {
        auto it = class_metadata.find(class_name);
        if (it != class_metadata.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Returns true if class is sealed (cannot be inherited).
    [[nodiscard]] auto is_class_sealed(const std::string& class_name) const -> bool {
        auto it = class_metadata.find(class_name);
        return it != class_metadata.end() && it->second.is_sealed;
    }

    /// Returns true if class instances can be stack-allocated.
    [[nodiscard]] auto can_stack_allocate(const std::string& class_name) const -> bool {
        auto it = class_metadata.find(class_name);
        return it != class_metadata.end() && it->second.stack_allocatable;
    }
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
    [[maybe_unused]] bool use_colors_;
};

// Convenience free function for printing a module
inline auto print_module(const Module& module, bool use_colors = false) -> std::string {
    MirPrinter printer(use_colors);
    return printer.print_module(module);
}

} // namespace tml::mir
