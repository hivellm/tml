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
//! ## Implementation — Fat Pointer Architecture
//!
//! Closures are represented as fat pointers: { func_ptr, env_ptr }
//!
//! - func_ptr: pointer to the generated closure function @tml_closure_N
//! - env_ptr: pointer to heap-allocated capture struct (null if no captures)
//!
//! **Capturing closures**: function receives `ptr %env` as first parameter.
//! %env points to a malloc'd struct containing captured values, accessed via GEP.
//!
//! **Non-capturing closures**: function has NO %env parameter — the function
//! signature matches the user-visible type, making it compatible with thin
//! pointer (func(...)) call sites. env_ptr in the fat pointer is null.
//!
//! This design allows closures to be stored in struct fields, passed
//! through function boundaries, and returned from functions — the capture
//! environment travels with the closure value as runtime data.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <sstream>

namespace tml::codegen {

auto LLVMIRGen::gen_closure(const parser::ClosureExpr& closure) -> std::string {
    // Generate a unique function name
    // In suite mode, add prefix to avoid symbol collisions when linking multiple test files
    std::string suite_prefix = "";
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        suite_prefix = "s" + std::to_string(options_.suite_test_index) + "_";
    }
    std::string closure_name =
        "tml_" + suite_prefix + "closure_" + std::to_string(closure_counter_++);

    bool has_captures = !closure.captured_vars.empty();

    // Collect capture info: (name, llvm_type) for each captured variable
    std::vector<std::pair<std::string, std::string>> captured_info;
    for (const auto& captured_name : closure.captured_vars) {
        auto it = locals_.find(captured_name);
        std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";
        captured_info.push_back({captured_name, captured_type});
    }

    // Determine parameter types from type annotations or inference
    std::vector<std::string> param_llvm_types;
    std::vector<std::string> param_names;
    for (size_t i = 0; i < closure.params.size(); ++i) {
        // Get parameter type
        std::string param_type = "i32"; // default
        if (closure.params[i].second.has_value()) {
            param_type = llvm_type(*closure.params[i].second.value());
        }
        param_llvm_types.push_back(param_type);

        // Get parameter name from pattern
        if (closure.params[i].first->is<parser::IdentPattern>()) {
            const auto& ident = closure.params[i].first->as<parser::IdentPattern>();
            param_names.push_back(ident.name);
        } else {
            param_names.push_back("_p" + std::to_string(i));
        }
    }

    // Determine return type from closure annotation or infer from body
    std::string ret_type = "i32";
    if (closure.return_type.has_value()) {
        ret_type = llvm_type(*closure.return_type.value());
    } else if (closure.body) {
        types::TypePtr inferred = infer_expr_type(*closure.body);
        if (inferred) {
            ret_type = llvm_type_from_semantic(inferred);
        }
    }

    // ================================================================
    // Generate the closure function
    // Capturing: ptr %env as first param, then user params
    // Non-capturing: user params only (compatible with thin func pointers)
    // ================================================================

    // Build parameter string
    std::string param_types_str;
    if (has_captures) {
        param_types_str = "ptr %env";
        for (size_t i = 0; i < param_names.size(); ++i) {
            param_types_str += ", " + param_llvm_types[i] + " %" + param_names[i];
        }
    } else {
        for (size_t i = 0; i < param_names.size(); ++i) {
            if (i > 0)
                param_types_str += ", ";
            param_types_str += param_llvm_types[i] + " %" + param_names[i];
        }
    }

    // Save current function state
    std::stringstream saved_output;
    saved_output << output_.str();
    output_.str("");
    auto saved_locals = locals_;
    auto saved_ret_type = current_ret_type_;
    bool saved_terminated = block_terminated_;

    // Start new function
    locals_.clear();
    current_ret_type_ = ret_type;
    block_terminated_ = false;

    emit_line("define internal " + ret_type + " @" + closure_name + "(" + param_types_str +
              ") #0 {");
    emit_line("entry:");

    // Load captured variables from env struct via GEP
    if (has_captures) {
        // Build the env struct type for GEP: { capture0_type, capture1_type, ... }
        std::string env_struct_type = "{ ";
        for (size_t i = 0; i < captured_info.size(); ++i) {
            if (i > 0)
                env_struct_type += ", ";
            env_struct_type += captured_info[i].second;
        }
        env_struct_type += " }";

        for (size_t i = 0; i < captured_info.size(); ++i) {
            const auto& [cap_name, cap_type] = captured_info[i];
            std::string gep_reg = fresh_reg();
            std::string load_reg = fresh_reg();
            std::string alloca_reg = fresh_reg();

            // GEP into the env struct to get pointer to this capture field
            emit_line("  " + gep_reg + " = getelementptr inbounds " + env_struct_type +
                      ", ptr %env, i32 0, i32 " + std::to_string(i));
            // Load the captured value
            emit_line("  " + load_reg + " = load " + cap_type + ", ptr " + gep_reg);
            // Store to local alloca (consistent with how locals work)
            emit_line("  " + alloca_reg + " = alloca " + cap_type);
            emit_line("  store " + cap_type + " " + load_reg + ", ptr " + alloca_reg);
            locals_[cap_name] = VarInfo{alloca_reg, cap_type, nullptr, std::nullopt};
        }
    }

    // Bind closure parameters to local scope
    for (size_t i = 0; i < param_names.size(); ++i) {
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_llvm_types[i]);
        emit_line("  store " + param_llvm_types[i] + " %" + param_names[i] + ", ptr " + alloca_reg);
        locals_[param_names[i]] = VarInfo{alloca_reg, param_llvm_types[i], nullptr, std::nullopt};
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
    output_.seekp(0, std::ios_base::end);
    locals_ = saved_locals;
    current_ret_type_ = saved_ret_type;
    block_terminated_ = saved_terminated;

    // Add closure function to module-level code
    module_functions_.push_back(closure_code);

    // ================================================================
    // At the closure creation site: build the { fn_ptr, env_ptr } value
    // ================================================================

    if (has_captures) {
        // Build the env struct type
        std::string env_struct_type = "{ ";
        for (size_t i = 0; i < captured_info.size(); ++i) {
            if (i > 0)
                env_struct_type += ", ";
            env_struct_type += captured_info[i].second;
        }
        env_struct_type += " }";

        // Calculate env struct size using LLVM's getelementptr trick
        std::string size_reg = fresh_reg();
        std::string size_int_reg = fresh_reg();
        emit_line("  " + size_reg + " = getelementptr " + env_struct_type + ", ptr null, i32 1");
        emit_line("  " + size_int_reg + " = ptrtoint ptr " + size_reg + " to i64");

        // Allocate env struct on heap via malloc
        std::string env_ptr = fresh_reg();
        emit_line("  " + env_ptr + " = call ptr @malloc(i64 " + size_int_reg + ")");

        // Store captured values into the env struct
        for (size_t i = 0; i < captured_info.size(); ++i) {
            const auto& [cap_name, cap_type] = captured_info[i];

            // Load captured variable value from current scope
            auto cap_it = locals_.find(cap_name);
            std::string cap_val = fresh_reg();
            if (cap_it != locals_.end()) {
                emit_line("  " + cap_val + " = load " + cap_type + ", ptr " + cap_it->second.reg);
            } else {
                // Should not happen, but fallback to zero
                emit_line("  " + cap_val + " = add " + cap_type + " 0, 0");
            }

            // GEP to the field in env struct and store
            std::string field_ptr = fresh_reg();
            emit_line("  " + field_ptr + " = getelementptr inbounds " + env_struct_type + ", ptr " +
                      env_ptr + ", i32 0, i32 " + std::to_string(i));
            emit_line("  store " + cap_type + " " + cap_val + ", ptr " + field_ptr);
        }

        // Build the fat pointer { fn_ptr, env_ptr }
        std::string fat1 = fresh_reg();
        std::string fat2 = fresh_reg();
        emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr @" + closure_name + ", 0");
        emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 + ", ptr " + env_ptr + ", 1");

        last_expr_type_ = "{ ptr, ptr }";
        last_closure_is_capturing_ = true;
        return fat2;
    } else {
        // Non-capturing closure: env_ptr = null
        std::string fat1 = fresh_reg();
        std::string fat2 = fresh_reg();
        emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr @" + closure_name + ", 0");
        emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 + ", ptr null, 1");

        last_expr_type_ = "{ ptr, ptr }";
        last_closure_is_capturing_ = false;
        return fat2;
    }
}

} // namespace tml::codegen
