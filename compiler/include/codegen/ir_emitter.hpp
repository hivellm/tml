//! # Typed IR Emission Helper
//!
//! Wraps raw LLVM IR string concatenation with type-validated methods
//! that assert correctness at emit time. This prevents common mistakes
//! like loading void types, storing to void pointers, or allocating
//! zero-sized types without explicit intent.
//!
//! ## Usage
//!
//! ```cpp
//! IREmitter emitter(output_stream, temp_counter);
//! std::string reg = emitter.emit_load("i64", "%ptr.1");
//! emitter.emit_store("i64", "%val", "%ptr.2");
//! ```
//!
//! ## Design
//!
//! The IREmitter does NOT own the output stream or counter; it holds
//! references to MirCodegen's existing members. This allows gradual
//! adoption: individual instruction handlers can migrate to IREmitter
//! while others continue using raw emitln().

#pragma once

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

namespace tml::codegen {

/// Typed IR emission helper. Wraps raw string concatenation with
/// type-validated methods that assert correctness at emit time.
class IREmitter {
public:
    /// Constructs an IREmitter referencing MirCodegen's output stream and
    /// temp counter. Both must outlive this IREmitter.
    IREmitter(std::stringstream& output, int& temp_counter)
        : output_(output), counter_(temp_counter) {}

    /// Emit: <result_reg> = load [volatile] <type>, ptr <ptr_reg> [, align <align>]
    /// Uses the provided result_reg (for MIR value ID registers like %v42).
    /// Asserts type is not "void" or "{}" (can't load void/unit).
    void emit_load_to(const std::string& result_reg, const std::string& type,
                      const std::string& ptr_reg, bool is_volatile = false, int align = 0);

    /// Emit: %tN = load [volatile] <type>, ptr <ptr_reg> [, align <align>]
    /// Allocates a fresh temp register and returns it.
    /// Asserts type is not "void" or "{}" (can't load void/unit).
    auto emit_load(const std::string& type, const std::string& ptr_reg, bool is_volatile = false,
                   int align = 0) -> std::string;

    /// Emit: store [volatile] <type> <value>, ptr <ptr_reg> [, align <align>]
    /// Asserts type is not "void" (can't store void).
    /// Note: "{}" (unit) stores are intentionally skipped with a comment.
    void emit_store(const std::string& type, const std::string& value, const std::string& ptr_reg,
                    bool is_volatile = false, int align = 0);

    /// Emit: <result_reg> = alloca <type> [, align <align>]
    /// Uses the provided result_reg (for MIR value ID registers like %v42).
    /// Asserts type is not "void" (can't alloca void).
    void emit_alloca_to(const std::string& result_reg, const std::string& type, int align = 0);

    /// Emit: %tN = alloca <type> [, align <align>]
    /// Allocates a fresh temp register and returns it.
    /// Asserts type is not "void" (can't alloca void).
    auto emit_alloca(const std::string& type, int align = 0) -> std::string;

    /// Emit: <result_reg> = getelementptr [inbounds] <base_type>, ptr <ptr_reg>, <indices...>
    /// Uses the provided result_reg (for MIR value ID registers like %v42).
    /// Each index is a pair of (type, value), e.g., ("i64", "0"), ("i32", "%idx").
    void emit_gep_to(const std::string& result_reg, const std::string& base_type,
                     const std::string& ptr_reg,
                     const std::vector<std::pair<std::string, std::string>>& typed_indices,
                     bool inbounds = true);

    /// Emit: %tN = getelementptr [inbounds] <base_type>, ptr <ptr_reg>, <indices...>
    /// Allocates a fresh temp register and returns it.
    /// Each index is a pair of (type, value), e.g., ("i64", "0"), ("i32", "%idx").
    auto emit_gep(const std::string& base_type, const std::string& ptr_reg,
                  const std::vector<std::pair<std::string, std::string>>& typed_indices,
                  bool inbounds = true) -> std::string;

    /// Emit: %reg = call <ret_type> @<func>(<args_str>)
    /// For void returns, emits "call void @<func>(...)" and returns "".
    /// Returns the new register name (or "" for void).
    auto emit_call(const std::string& ret_type, const std::string& func_name,
                   const std::string& args_str) -> std::string;

    /// Emit: call void @llvm.memcpy.p0.p0.i64(ptr <dst>, ptr <src>, i64 <size>, i1 <volatile>)
    void emit_memcpy(const std::string& dst, const std::string& src, const std::string& size,
                     bool is_volatile = false);

    /// Emit: call void @llvm.memset.p0.i64(ptr <dst>, i8 <val>, i64 <size>, i1 <volatile>)
    void emit_memset(const std::string& dst, const std::string& val, const std::string& size,
                     bool is_volatile = false);

    /// Emit: call void @llvm.memmove.p0.p0.i64(ptr <dst>, ptr <src>, i64 <size>, i1 <volatile>)
    void emit_memmove(const std::string& dst, const std::string& src, const std::string& size,
                      bool is_volatile = false);

private:
    std::stringstream& output_;
    int& counter_;

    /// Allocate a fresh temporary register name (%tN).
    auto fresh_reg() -> std::string {
        return "%t" + std::to_string(counter_++);
    }

    /// Emit an indented line (4 spaces + content + newline).
    void emit_line(const std::string& line) {
        output_ << "    " << line << "\n";
    }
};

} // namespace tml::codegen
