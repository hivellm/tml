TML_MODULE("codegen_x86")

//! MIR Codegen Terminator Emission
//!
//! This file contains terminator emission for the MIR-based code generator:
//! - emit_terminator: Emits LLVM IR for basic block terminators
//!
//! Supported terminators:
//! - ReturnTerm: Function return
//! - BranchTerm: Unconditional branch
//! - CondBranchTerm: Conditional branch
//! - SwitchTerm: Switch/match statement
//! - UnreachableTerm: Unreachable code marker

#include "codegen/mir_codegen.hpp"

namespace tml::codegen {

void MirCodegen::emit_terminator(const mir::Terminator& term) {
    std::visit(
        [this](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, mir::ReturnTerm>) {
                if (t.value.has_value()) {
                    std::string val = get_value_reg(*t.value);
                    // Get type from the value itself, with fallback to value_types_ map
                    std::string type_str;
                    if (t.value->type) {
                        type_str = mir_type_to_llvm(t.value->type);
                    }
                    // Check value_types_ for actual type (important for intrinsic calls)
                    if (type_str.empty() || type_str == "i32") {
                        auto it = value_types_.find(t.value->id);
                        if (it != value_types_.end() && !it->second.empty()) {
                            type_str = it->second;
                        }
                    }
                    if (type_str.empty() || type_str == "void") {
                        // Having a return value with void type means type info was lost;
                        // fall back to i32 rather than emitting invalid "ret void %vN"
                        type_str = "i32";
                    }
                    emitln("    ret " + type_str + " " + val);
                } else {
                    emitln("    ret void");
                }

            } else if constexpr (std::is_same_v<T, mir::BranchTerm>) {
                auto it = block_labels_.find(t.target);
                if (it != block_labels_.end() && !it->second.empty()) {
                    emitln("    br label %" + it->second);
                } else {
                    // Target block doesn't exist - fall through to next block
                    // or branch to function exit if no blocks remain
                    if (!fallback_label_.empty()) {
                        emitln("    br label %" + fallback_label_);
                    } else {
                        emitln("    unreachable ; missing target block");
                    }
                }

            } else if constexpr (std::is_same_v<T, mir::CondBranchTerm>) {
                std::string cond = get_value_reg(t.condition);
                auto true_it = block_labels_.find(t.true_block);
                auto false_it = block_labels_.find(t.false_block);
                std::string true_label = (true_it != block_labels_.end()) ? true_it->second : "";
                std::string false_label = (false_it != block_labels_.end()) ? false_it->second : "";
                // Use fallback for missing labels
                if (true_label.empty())
                    true_label = fallback_label_.empty() ? false_label : fallback_label_;
                if (false_label.empty())
                    false_label = fallback_label_.empty() ? true_label : fallback_label_;
                if (true_label.empty() || false_label.empty()) {
                    emitln("    unreachable ; missing branch target");
                } else {
                    emitln("    br i1 " + cond + ", label %" + true_label + ", label %" +
                           false_label);
                }

            } else if constexpr (std::is_same_v<T, mir::SwitchTerm>) {
                std::string disc = get_value_reg(t.discriminant);
                auto def_it = block_labels_.find(t.default_block);
                std::string default_label =
                    (def_it != block_labels_.end()) ? def_it->second : "unreachable";
                emit("    switch i32 " + disc + ", label %" + default_label + " [");
                for (const auto& [val, block] : t.cases) {
                    auto case_it = block_labels_.find(block);
                    std::string label = (case_it != block_labels_.end()) ? case_it->second : "";
                    if (!label.empty()) {
                        emit(" i32 " + std::to_string(val) + ", label %" + label);
                    }
                }
                emitln(" ]");

            } else if constexpr (std::is_same_v<T, mir::UnreachableTerm>) {
                emitln("    unreachable");
            }
        },
        term);
}

} // namespace tml::codegen
