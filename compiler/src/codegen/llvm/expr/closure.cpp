TML_MODULE("codegen_x86")

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
#include <unordered_set>

namespace tml::codegen {

// Codegen-time capture analysis: walk closure body AST to find identifiers
// referencing variables from the enclosing scope. This is needed because library
// methods skip the type checker, so closure.captured_vars may be empty.
static void
collect_codegen_captures(const parser::Expr& expr,
                         const std::unordered_set<std::string>& param_names,
                         const std::unordered_map<std::string, LLVMIRGen::VarInfo>& locals,
                         std::vector<std::string>& captures) {
    if (expr.is<parser::IdentExpr>()) {
        const auto& e = expr.as<parser::IdentExpr>();
        if (param_names.count(e.name))
            return;
        for (const auto& cap : captures) {
            if (cap == e.name)
                return;
        }
        if (locals.count(e.name)) {
            captures.push_back(e.name);
        }
    } else if (expr.is<parser::BinaryExpr>()) {
        const auto& e = expr.as<parser::BinaryExpr>();
        collect_codegen_captures(*e.left, param_names, locals, captures);
        collect_codegen_captures(*e.right, param_names, locals, captures);
    } else if (expr.is<parser::UnaryExpr>()) {
        const auto& e = expr.as<parser::UnaryExpr>();
        collect_codegen_captures(*e.operand, param_names, locals, captures);
    } else if (expr.is<parser::CallExpr>()) {
        const auto& e = expr.as<parser::CallExpr>();
        collect_codegen_captures(*e.callee, param_names, locals, captures);
        for (const auto& arg : e.args) {
            collect_codegen_captures(*arg, param_names, locals, captures);
        }
    } else if (expr.is<parser::BlockExpr>()) {
        const auto& e = expr.as<parser::BlockExpr>();
        for (const auto& stmt : e.stmts) {
            if (auto* expr_stmt = std::get_if<parser::ExprStmt>(&stmt->kind)) {
                collect_codegen_captures(*expr_stmt->expr, param_names, locals, captures);
            } else if (auto* let_stmt = std::get_if<parser::LetStmt>(&stmt->kind)) {
                if (let_stmt->init) {
                    collect_codegen_captures(**let_stmt->init, param_names, locals, captures);
                }
            }
        }
        if (e.expr) {
            collect_codegen_captures(**e.expr, param_names, locals, captures);
        }
    } else if (expr.is<parser::IfExpr>()) {
        const auto& e = expr.as<parser::IfExpr>();
        collect_codegen_captures(*e.condition, param_names, locals, captures);
        collect_codegen_captures(*e.then_branch, param_names, locals, captures);
        if (e.else_branch) {
            collect_codegen_captures(**e.else_branch, param_names, locals, captures);
        }
    } else if (expr.is<parser::ReturnExpr>()) {
        const auto& e = expr.as<parser::ReturnExpr>();
        if (e.value) {
            collect_codegen_captures(**e.value, param_names, locals, captures);
        }
    } else if (expr.is<parser::FieldExpr>()) {
        const auto& e = expr.as<parser::FieldExpr>();
        collect_codegen_captures(*e.object, param_names, locals, captures);
    } else if (expr.is<parser::MethodCallExpr>()) {
        const auto& e = expr.as<parser::MethodCallExpr>();
        collect_codegen_captures(*e.receiver, param_names, locals, captures);
        for (const auto& arg : e.args) {
            collect_codegen_captures(*arg, param_names, locals, captures);
        }
    } else if (expr.is<parser::WhenExpr>()) {
        const auto& e = expr.as<parser::WhenExpr>();
        collect_codegen_captures(*e.scrutinee, param_names, locals, captures);
        for (const auto& arm : e.arms) {
            collect_codegen_captures(*arm.body, param_names, locals, captures);
        }
    }
    // ClosureExpr: don't recurse into nested closures
    // LiteralExpr, PathExpr, etc.: no variables to capture
}

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

    // Determine parameter types and names first (needed for capture analysis)
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

    // Codegen-time capture analysis: if the type checker didn't populate captured_vars
    // (happens for library methods that are re-parsed without type checking), walk the
    // closure body to find references to enclosing scope variables.
    std::vector<std::string> effective_captures = closure.captured_vars;
    if (effective_captures.empty() && closure.body && !locals_.empty()) {
        std::unordered_set<std::string> param_set(param_names.begin(), param_names.end());
        collect_codegen_captures(*closure.body, param_set, locals_, effective_captures);
    }

    bool has_captures = !effective_captures.empty();

    // Collect capture info: (name, llvm_type) for each captured variable
    // All captures are by-reference: we store a ptr in the env struct
    std::vector<std::pair<std::string, std::string>> captured_info;
    for (const auto& captured_name : effective_captures) {
        auto it = locals_.find(captured_name);
        std::string captured_type = (it != locals_.end()) ? it->second.type : "i32";
        captured_info.push_back({captured_name, captured_type});
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
    auto saved_expected_enum_type = expected_enum_type_;

    // Start new function — closure is an independent scope
    locals_.clear();
    current_ret_type_ = ret_type;
    block_terminated_ = false;
    expected_enum_type_.clear();

    emit_line("define internal " + ret_type + " @" + closure_name + "(" + param_types_str +
              ") #0 {");
    emit_line("entry:");

    // Load captured variables from env struct via GEP
    // Captures are by-reference: env contains pointers to original variables
    if (has_captures) {
        // Build the env struct type for GEP: all captures are ptr (by-reference)
        std::string env_struct_type = "{ ";
        for (size_t i = 0; i < captured_info.size(); ++i) {
            if (i > 0)
                env_struct_type += ", ";
            env_struct_type += "ptr";
        }
        env_struct_type += " }";

        for (size_t i = 0; i < captured_info.size(); ++i) {
            const auto& [cap_name, cap_type] = captured_info[i];
            std::string gep_reg = fresh_reg();

            // GEP into the env struct to get pointer to the pointer slot
            emit_line("  " + gep_reg + " = getelementptr inbounds " + env_struct_type +
                      ", ptr %env, i32 0, i32 " + std::to_string(i));
            // Load the pointer to the original variable
            std::string ptr_reg = fresh_reg();
            emit_line("  " + ptr_reg + " = load ptr, ptr " + gep_reg);
            // Use the loaded pointer as the local — reads/writes go to the original variable
            // This enables mutable capture: mutations persist in the caller's scope
            locals_[cap_name] = VarInfo{ptr_reg, cap_type, nullptr, std::nullopt};
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

    // After generating the body, check if the actual body type differs from inferred ret_type.
    // This handles closures like `do() { this.value = Just(f()) }` where the body is
    // a block with only statements (assignment) — should return void, not the RHS type.
    if (!block_terminated_) {
        if (last_expr_type_ == "void" || ret_type == "void") {
            ret_type = "void";
            emit_line("  ret void");
        } else {
            emit_line("  ret " + ret_type + " " + body_val);
        }
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
    expected_enum_type_ = saved_expected_enum_type;

    // Add closure function to module-level code
    module_functions_.push_back(closure_code);

    // ================================================================
    // At the closure creation site: build the { fn_ptr, env_ptr } value
    // ================================================================

    if (has_captures) {
        // Build the env struct type: all captures are ptr (by-reference)
        std::string env_struct_type = "{ ";
        for (size_t i = 0; i < captured_info.size(); ++i) {
            if (i > 0)
                env_struct_type += ", ";
            env_struct_type += "ptr";
        }
        env_struct_type += " }";

        // Calculate env struct size using LLVM's getelementptr trick
        std::string size_reg = fresh_reg();
        std::string size_int_reg = fresh_reg();
        emit_line("  " + size_reg + " = getelementptr inbounds " + env_struct_type +
                  ", ptr null, i32 1");
        emit_line("  " + size_int_reg + " = ptrtoint ptr " + size_reg + " to i64");

        // Allocate env struct on heap via malloc
        std::string env_ptr = fresh_reg();
        emit_line("  " + env_ptr + " = call ptr @malloc(i64 " + size_int_reg + ")");

        // Store pointers to captured variables into the env struct (capture by reference)
        for (size_t i = 0; i < captured_info.size(); ++i) {
            const auto& [cap_name, cap_type] = captured_info[i];

            // Get the pointer to the captured variable from current scope
            auto cap_it = locals_.find(cap_name);
            std::string cap_ptr;
            if (cap_it != locals_.end()) {
                const auto& var = cap_it->second;
                // Check if this is an alloca'd variable (has memory address)
                // vs a direct SSA parameter (like %this, %f)
                bool is_alloca = var.reg.size() > 2 && var.reg[0] == '%' && var.reg[1] == 't' &&
                                 std::isdigit(static_cast<unsigned char>(var.reg[2]));
                if (is_alloca) {
                    // Alloca — use it directly as the capture pointer
                    cap_ptr = var.reg;
                } else {
                    // Direct SSA parameter — create an alloca to hold it
                    // so we can capture its address
                    cap_ptr = fresh_reg();
                    emit_line("  " + cap_ptr + " = alloca " + cap_type);
                    emit_line("  store " + cap_type + " " + var.reg + ", ptr " + cap_ptr);
                    // Update the local to point to the alloca so subsequent
                    // code in the same function also uses the alloca
                    locals_[cap_name] = VarInfo{cap_ptr, cap_type, var.semantic_type, std::nullopt};
                }
            } else {
                // Should not happen — create a dummy alloca
                cap_ptr = fresh_reg();
                emit_line("  " + cap_ptr + " = alloca " + cap_type);
            }

            // GEP to the field in env struct and store the pointer
            std::string field_ptr = fresh_reg();
            emit_line("  " + field_ptr + " = getelementptr inbounds " + env_struct_type + ", ptr " +
                      env_ptr + ", i32 0, i32 " + std::to_string(i));
            emit_line("  store ptr " + cap_ptr + ", ptr " + field_ptr);
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
