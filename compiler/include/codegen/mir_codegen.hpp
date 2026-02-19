//! # MIR-based LLVM IR Code Generator
//!
//! This module generates LLVM IR from MIR (Mid-level IR). Unlike the
//! AST-based generator, this works with SSA form which enables:
//!
//! - Easier optimization passes
//! - More precise register allocation
//! - Cleaner control flow handling
//!
//! ## MIR Advantages
//!
//! The MIR is already in SSA form with explicit phi nodes, so we can
//! generate LLVM IR more directly without tracking variable assignments.
//!
//! ## Pipeline
//!
//! ```
//! TML Source -> AST -> MIR -> LLVM IR -> Object Code
//! ```
//!
//! ## SROA (Scalar Replacement of Aggregates)
//!
//! This generator produces LLVM IR optimized for SROA, which breaks up
//! stack-allocated structs into individual registers. This is critical
//! for OOP performance - stack-promoted objects become zero-cost:
//!
//! 1. `is_stack_eligible` constructor calls use `alloca` instead of heap
//! 2. Function attributes (`nounwind`, `willreturn`) enable aggressive opts
//! 3. Proper alignment (8-byte) for SROA eligibility
//! 4. No escaping pointers from stack allocations
//!
//! After LLVM's SROA pass runs:
//! - Stack-allocated Point(x, y) becomes two registers (%x, %y)
//! - No memory operations for field access
//! - Virtual dispatch inlined where possible

#pragma once

#include "common.hpp"
#include "mir/mir.hpp"

#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::codegen {

/// Options for MIR-to-LLVM code generation.
struct MirCodegenOptions {
    bool emit_comments = true;                            ///< Include source comments in IR.
    bool dll_export = false;                              ///< Add dllexport for Windows DLLs.
    bool coverage_enabled = false;                        ///< Disable inlining for coverage builds.
    std::string target_triple = "x86_64-pc-windows-msvc"; ///< LLVM target triple.
};

/// MIR-to-LLVM IR code generator.
///
/// Translates MIR (already in SSA form) to LLVM IR text format.
/// This is an alternative to the AST-based LLVMIRGen that may produce
/// better optimized code for certain patterns.
class MirCodegen {
public:
    /// Creates a MIR code generator with the given options.
    explicit MirCodegen(MirCodegenOptions options = {});

    /// Generates LLVM IR from a MIR module.
    auto generate(const mir::Module& module) -> std::string;

    /// Generates LLVM IR for a subset of functions (CGU mode).
    /// Functions at the given indices are emitted as `define` (full body).
    /// All other functions are emitted as `declare` (external stub).
    auto generate_cgu(const mir::Module& module, const std::vector<size_t>& function_indices)
        -> std::string;

private:
    MirCodegenOptions options_;
    std::stringstream output_;
    int temp_counter_ = 0;
    int spill_counter_ = 0; // Counter for struct-to-ptr spill allocas

    // Current function context
    std::string current_func_;

    // Value ID to LLVM register mapping
    std::unordered_map<mir::ValueId, std::string> value_regs_;

    // Value ID to LLVM type string mapping (for type coercion)
    std::unordered_map<mir::ValueId, std::string> value_types_;

    // Struct name to field types mapping (for type coercion in struct init)
    std::unordered_map<std::string, std::vector<std::string>> struct_field_types_;

    // Block index to LLVM label mapping
    std::unordered_map<uint32_t, std::string> block_labels_;

    // Fallback label for missing block targets (set to first return block)
    std::string fallback_label_;

    // Type definitions emitted (to avoid duplicates)
    std::set<std::string> emitted_types_;

    // Enum types used (collected from EnumInitInst, for imported enums)
    std::set<std::string> used_enum_types_;

    // String constants (value -> global name)
    std::unordered_map<std::string, std::string> string_constants_;

    // ValueId -> string content (for compile-time constant string length optimization)
    std::unordered_map<mir::ValueId, std::string> value_string_contents_;

    // ValueId -> integer constant value (for zero-initialization detection)
    std::unordered_map<mir::ValueId, int64_t> value_int_constants_;

    // sret function tracking (func_name -> original return type as LLVM string)
    std::unordered_map<std::string, std::string> sret_functions_;

    // Parameter name to (value_id, type) mapping for indirect calls
    std::unordered_map<std::string, std::pair<mir::ValueId, mir::MirTypePtr>> param_info_;

    // Generate helpers
    void emit_preamble();
    void emit_type_defs(const mir::Module& module);
    void emit_struct_def(const mir::StructDef& s);
    void emit_enum_def(const mir::EnumDef& e);
    void emit_function(const mir::Function& func);
    void emit_function_declaration(const mir::Function& func);
    void emit_block(const mir::BasicBlock& block);
    void emit_instruction(const mir::InstructionData& inst);
    void emit_terminator(const mir::Terminator& term);

    // Type conversion (implemented in mir/types.cpp)
    auto mir_type_to_llvm(const mir::MirTypePtr& type) -> std::string;
    auto mir_primitive_to_llvm(mir::PrimitiveType kind) -> std::string;

    // Value lookup (implemented in mir/helpers.cpp)
    auto get_value_reg(const mir::Value& val) -> std::string;
    auto new_temp() -> std::string;

    // Binary operation helpers (implemented in mir/helpers.cpp)
    auto get_binop_name(mir::BinOp op, bool is_float, bool is_signed) -> std::string;
    auto get_cmp_predicate(mir::BinOp op, bool is_float, bool is_signed) -> std::string;

    // Atomic operation helpers (implemented in mir/helpers.cpp)
    auto atomic_ordering_to_llvm(mir::AtomicOrdering ordering) -> std::string;
    auto atomic_rmw_op_to_llvm(mir::AtomicRMWOp op) -> std::string;
    auto get_type_alignment(const mir::MirTypePtr& type) -> size_t;

    // Emit helpers
    void emit(const std::string& s);
    void emitln(const std::string& s = "");
    void emit_comment(const std::string& s);

    // Instruction emission helpers (implemented in mir/instructions.cpp)
    void emit_binary_inst(const mir::BinaryInst& i, const std::string& result_reg,
                          const mir::MirTypePtr& result_type, const mir::InstructionData& inst);
    void emit_unary_inst(const mir::UnaryInst& i, const std::string& result_reg);
    void emit_extract_value_inst(const mir::ExtractValueInst& i, const std::string& result_reg,
                                 const mir::InstructionData& inst);
    void emit_insert_value_inst(const mir::InsertValueInst& i, const std::string& result_reg);
    void emit_call_inst(const mir::CallInst& i, const std::string& result_reg,
                        const mir::InstructionData& inst);
    void emit_indirect_call(const mir::CallInst& i, const std::string& param_name,
                            mir::ValueId value_id, const mir::MirTypePtr& func_type,
                            const std::string& result_reg, const mir::InstructionData& inst);
    void emit_method_call_inst(const mir::MethodCallInst& i, const std::string& result_reg,
                               const mir::InstructionData& inst);
    void emit_cast_inst(const mir::CastInst& i, const std::string& result_reg,
                        const mir::InstructionData& inst);
    void emit_phi_inst(const mir::PhiInst& i, const std::string& result_reg,
                       const mir::InstructionData& inst);
    void emit_constant_inst(const mir::ConstantInst& i, const std::string& result_reg,
                            const mir::InstructionData& inst);
    void emit_struct_init_inst(const mir::StructInitInst& i, const std::string& result_reg,
                               const mir::MirTypePtr& result_type,
                               const mir::InstructionData& inst);
    void emit_tuple_init_inst(const mir::TupleInitInst& i, const std::string& result_reg);
    void emit_array_init_inst(const mir::ArrayInitInst& i, const std::string& result_reg);
    void emit_atomic_load_inst(const mir::AtomicLoadInst& i, const std::string& result_reg,
                               const mir::InstructionData& inst);
    void emit_atomic_store_inst(const mir::AtomicStoreInst& i);
    void emit_atomic_rmw_inst(const mir::AtomicRMWInst& i, const std::string& result_reg,
                              const mir::InstructionData& inst);
    void emit_atomic_cmpxchg_inst(const mir::AtomicCmpXchgInst& i, const std::string& result_reg,
                                  const mir::InstructionData& inst);

    // Call emission helpers (implemented in mir/instructions.cpp)
    void emit_llvm_intrinsic_call(const mir::CallInst& i, const std::string& base_name,
                                  const std::string& result_reg, const mir::InstructionData& inst);
    void emit_sret_call(const std::string& func_name, const std::string& orig_ret_type,
                        const std::vector<std::string>& processed_args,
                        const std::string& result_reg, const mir::InstructionData& inst);
    void emit_normal_call(const mir::CallInst& i, const std::string& func_name,
                          const std::vector<std::string>& processed_args,
                          const std::string& result_reg, const mir::InstructionData& inst);
};

} // namespace tml::codegen
