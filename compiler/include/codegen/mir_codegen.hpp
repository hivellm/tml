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

#pragma once

#include "common.hpp"
#include "mir/mir.hpp"

#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::codegen {

/// Options for MIR-to-LLVM code generation.
struct MirCodegenOptions {
    bool emit_comments = true;                            ///< Include source comments in IR.
    bool dll_export = false;                              ///< Add dllexport for Windows DLLs.
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

private:
    MirCodegenOptions options_;
    std::stringstream output_;
    int temp_counter_ = 0;

    // Current function context
    std::string current_func_;

    // Value ID to LLVM register mapping
    std::unordered_map<mir::ValueId, std::string> value_regs_;

    // Block index to LLVM label mapping
    std::unordered_map<uint32_t, std::string> block_labels_;

    // Type definitions emitted (to avoid duplicates)
    std::set<std::string> emitted_types_;

    // Generate helpers
    void emit_preamble();
    void emit_type_defs(const mir::Module& module);
    void emit_struct_def(const mir::StructDef& s);
    void emit_enum_def(const mir::EnumDef& e);
    void emit_function(const mir::Function& func);
    void emit_block(const mir::BasicBlock& block);
    void emit_instruction(const mir::InstructionData& inst);
    void emit_terminator(const mir::Terminator& term);

    // Type conversion
    auto mir_type_to_llvm(const mir::MirTypePtr& type) -> std::string;
    auto mir_primitive_to_llvm(mir::PrimitiveType kind) -> std::string;

    // Value lookup
    auto get_value_reg(const mir::Value& val) -> std::string;
    auto new_temp() -> std::string;

    // Binary operation helpers
    auto get_binop_name(mir::BinOp op, bool is_float, bool is_signed) -> std::string;
    auto get_cmp_predicate(mir::BinOp op, bool is_float, bool is_signed) -> std::string;

    // Emit helpers
    void emit(const std::string& s);
    void emitln(const std::string& s = "");
    void emit_comment(const std::string& s);
};

} // namespace tml::codegen
