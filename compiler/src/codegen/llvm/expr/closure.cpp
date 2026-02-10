//! # LLVM IR Generator - Closures
//!
//! This file implements closure expression code generation.
//!
//! ## Closure Syntax
//!
//! TML uses `do(params) expr` syntax for closures:
//! ```tml
//! let add = do(a: I32, b: I32) a + b
//! ```
//!
//! ## Implementation
//!
//! Closures are lowered to:
//! 1. A generated function `@tml_closure_N`
//! 2. A capture struct if variables are captured
//! 3. A function pointer (or fat pointer for captures)
//!
//! ## Captured Variables
//!
//! Captured variables are stored in a heap-allocated struct.
//! The closure function receives this struct as an implicit first parameter.
//!
//! ## Non-capturing Closures
//!
//! Closures without captures can be used as plain function pointers.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <sstream>

namespace tml::codegen {

auto LLVMIRGen::gen_closure(const parser::ClosureExpr& closure) -> std::string {
    // Clear previous closure capture info
    last_closure_captures_ = std::nullopt;

    // Generate a unique function name
    // In suite mode, add prefix to avoid symbol collisions when linking multiple test files
    // Only for test-local closures (not library closures)
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }
    std::string closure_name =
        "tml_" + suite_prefix + "closure_" + std::to_string(closure_counter_++);

    // Collect capture info if there are captured variables
    if (!closure.captured_vars.empty()) {
        ClosureCaptureInfo capture_info;
        for (const auto& captured_name : closure.captured_vars) {
            auto it = locals_.find(captured_name);
            std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";
            capture_info.captured_names.push_back(captured_name);
            capture_info.captured_types.push_back(captured_type);
        }
        last_closure_captures_ = capture_info;
    }

    // Build parameter types string, including captured variables as first parameters
    std::string param_types_str;
    std::vector<std::string> param_names;

    // Add captured variables as parameters
    for (const auto& captured_name : closure.captured_vars) {
        if (!param_types_str.empty())
            param_types_str += ", ";

        // Look up the type from locals
        auto it = locals_.find(captured_name);
        std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";

        param_types_str += captured_type + " %" + captured_name + "_captured";
        param_names.push_back(captured_name);
    }

    // Add closure parameters
    for (size_t i = 0; i < closure.params.size(); ++i) {
        if (!param_types_str.empty())
            param_types_str += ", ";

        // For now, assume i32 parameters (simplified)
        param_types_str += "i32";

        // Get parameter name from pattern
        if (closure.params[i].first->is<parser::IdentPattern>()) {
            const auto& ident = closure.params[i].first->as<parser::IdentPattern>();
            param_names.push_back(ident.name);
        } else {
            param_names.push_back("_p" + std::to_string(i));
        }

        param_types_str += " %" + param_names.back();
    }

    // Determine return type from closure annotation or infer from body
    std::string ret_type = "i32";
    if (closure.return_type.has_value()) {
        ret_type = llvm_type(*closure.return_type.value());
    } else if (closure.body) {
        // Infer return type from closure body expression
        types::TypePtr inferred = infer_expr_type(*closure.body);
        if (inferred) {
            ret_type = llvm_type_from_semantic(inferred);
        }
    }

    // Save information about captured variables before clearing locals
    std::vector<std::pair<std::string, std::string>> captured_info; // (name, type)
    for (const auto& captured_name : closure.captured_vars) {
        auto it = locals_.find(captured_name);
        std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";
        captured_info.push_back({captured_name, captured_type});
    }

    // Save current function state
    std::stringstream saved_output;
    saved_output << output_.str();
    output_.str(""); // Clear for closure generation
    auto saved_locals = locals_;
    auto saved_ret_type = current_ret_type_;
    bool saved_terminated = block_terminated_;

    // Start new function
    locals_.clear();
    current_ret_type_ = ret_type;
    block_terminated_ = false;

    // Emit function header
    emit_line("define internal " + ret_type + " @" + closure_name + "(" + param_types_str +
              ") #0 {");
    emit_line("entry:");

    // Bind captured variables to local scope
    for (size_t i = 0; i < captured_info.size(); ++i) {
        const auto& [captured_name, captured_type] = captured_info[i];

        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + captured_type);
        emit_line("  store " + captured_type + " %" + captured_name + "_captured, ptr " +
                  alloca_reg);
        locals_[captured_name] = VarInfo{alloca_reg, captured_type, nullptr, std::nullopt};
    }

    // Bind closure parameters to local scope
    for (size_t i = closure.captured_vars.size(); i < param_names.size(); ++i) {
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca i32");
        emit_line("  store i32 %" + param_names[i] + ", ptr " + alloca_reg);
        locals_[param_names[i]] = VarInfo{alloca_reg, "i32", nullptr, std::nullopt};
    }

    // Generate body
    std::string body_val = gen_expr(*closure.body);

    // Return the result
    if (!block_terminated_) {
        emit_line("  ret " + ret_type + " " + body_val);
    }

    emit_line("}");
    emit_line("");

    // Store generated closure function
    std::string closure_code = output_.str();

    // Restore original function state
    output_.str(saved_output.str());
    output_.seekp(0, std::ios_base::end); // Restore position to end
    locals_ = saved_locals;
    current_ret_type_ = saved_ret_type;
    block_terminated_ = saved_terminated;

    // Add closure function to module-level code
    module_functions_.push_back(closure_code);

    // Return function pointer
    // For now, return the function name as a "function pointer"
    // This is simplified - full support would need actual function pointers
    last_expr_type_ = "ptr"; // Closures are function pointers
    return "@" + closure_name;
}

} // namespace tml::codegen
