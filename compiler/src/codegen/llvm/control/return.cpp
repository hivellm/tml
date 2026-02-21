//! # LLVM IR Generator - Return/Throw Control Flow
//!
//! This file implements return and throw expression code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Helper to parse tuple type string into element types
static std::vector<std::string> parse_tuple_types_for_coercion(const std::string& tuple_type) {
    std::vector<std::string> element_types;
    if (tuple_type.size() > 2 && tuple_type.front() == '{' && tuple_type.back() == '}') {
        // Parse "{ i32, i64, ptr }" -> ["i32", "i64", "ptr"]
        std::string inner = tuple_type.substr(2, tuple_type.size() - 4);
        int brace_depth = 0;
        int bracket_depth = 0;
        std::string current;

        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '{') {
                brace_depth++;
                current += c;
            } else if (c == '}') {
                brace_depth--;
                current += c;
            } else if (c == '[') {
                bracket_depth++;
                current += c;
            } else if (c == ']') {
                bracket_depth--;
                current += c;
            } else if (c == ',' && brace_depth == 0 && bracket_depth == 0) {
                size_t start = current.find_first_not_of(" ");
                size_t end = current.find_last_not_of(" ");
                if (start != std::string::npos) {
                    element_types.push_back(current.substr(start, end - start + 1));
                }
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            size_t start = current.find_first_not_of(" ");
            size_t end = current.find_last_not_of(" ");
            if (start != std::string::npos) {
                element_types.push_back(current.substr(start, end - start + 1));
            }
        }
    }
    return element_types;
}

auto LLVMIRGen::gen_return(const parser::ReturnExpr& ret) -> std::string {
    // Check if we're inside an inlined closure body (e.g., Maybe::map, and_then, etc.).
    // In that case, `return` should store the value and branch to the merge label
    // instead of emitting a function-level `ret`.
    if (!closure_return_alloca_.empty()) {
        if (ret.value.has_value()) {
            std::string val = gen_expr(*ret.value.value());
            emit_line("  store " + closure_return_type_ + " " + val + ", ptr " +
                      closure_return_alloca_);
            // Remove Str temp if closure returns a Str — ownership transfers to caller
            if (last_expr_type_ == "ptr" && !temp_drops_.empty() &&
                temp_drops_.back().is_heap_str) {
                temp_drops_.pop_back();
            }
        }
        emit_line("  br label %" + closure_return_label_);
        block_terminated_ = true;
        return "void";
    }

    // IMPORTANT: Generate return expression FIRST, before dropping
    // This allows mark_var_consumed to be called for variables used in the return value
    std::string val;
    std::string val_type;
    if (ret.value.has_value()) {
        val = gen_expr(*ret.value.value());
        val_type = last_expr_type_;

        // Mark the returned variable as consumed (moved) so it won't be dropped.
        // This prevents double-free / use-after-free for types with Drop (like Mutex).
        // When returning a variable by value, the value is moved out and the local
        // should NOT have its destructor called.
        if (ret.value.value()->is<parser::IdentExpr>()) {
            const auto& ident = ret.value.value()->as<parser::IdentExpr>();
            mark_var_consumed(ident.name);
        }

        // If the return value is a Str temp from a call, remove it from temp_drops_.
        // The caller takes ownership; freeing it here would cause use-after-free.
        if (val_type == "ptr" && !temp_drops_.empty() && temp_drops_.back().is_heap_str) {
            temp_drops_.pop_back();
        }
    }

    // Emit lifetime.end for all allocas before returning
    emit_all_lifetime_ends();

    // Emit drops for all variables in all scopes before returning
    // (consumed variables from return expression will be skipped)
    emit_all_drops();

    if (ret.value.has_value()) {

        // For async functions, wrap the return value in Poll.Ready
        if (current_func_is_async_ && !current_poll_type_.empty()) {
            std::string wrapped = wrap_in_poll_ready(val, val_type);
            emit_line("  ret " + current_poll_type_ + " " + wrapped);
        } else {
            // Check if we need tuple coercion (e.g., { i32, i32 } -> { i32, i64 })
            if (val_type != current_ret_type_ && val_type.front() == '{' &&
                current_ret_type_.front() == '{') {
                // Parse element types for both
                auto actual_elems = parse_tuple_types_for_coercion(val_type);
                auto expected_elems = parse_tuple_types_for_coercion(current_ret_type_);

                if (actual_elems.size() == expected_elems.size()) {
                    bool needs_conversion = false;
                    for (size_t i = 0; i < actual_elems.size(); ++i) {
                        if (actual_elems[i] != expected_elems[i]) {
                            needs_conversion = true;
                            break;
                        }
                    }

                    if (needs_conversion) {
                        // Store original tuple to memory
                        std::string src_ptr = fresh_reg();
                        emit_line("  " + src_ptr + " = alloca " + val_type);
                        emit_line("  store " + val_type + " " + val + ", ptr " + src_ptr);

                        // Allocate destination tuple
                        std::string dst_ptr = fresh_reg();
                        emit_line("  " + dst_ptr + " = alloca " + current_ret_type_);

                        // Convert each element
                        for (size_t i = 0; i < actual_elems.size(); ++i) {
                            std::string elem_ptr = fresh_reg();
                            emit_line("  " + elem_ptr + " = getelementptr inbounds " + val_type +
                                      ", ptr " + src_ptr + ", i32 0, i32 " + std::to_string(i));
                            std::string elem_val = fresh_reg();
                            emit_line("  " + elem_val + " = load " + actual_elems[i] + ", ptr " +
                                      elem_ptr);

                            std::string conv_val = elem_val;
                            if (actual_elems[i] != expected_elems[i]) {
                                conv_val = fresh_reg();
                                // Integer extension/truncation
                                if (expected_elems[i] == "i64" && actual_elems[i] == "i32") {
                                    emit_line("  " + conv_val + " = sext i32 " + elem_val +
                                              " to i64");
                                } else if (expected_elems[i] == "i32" && actual_elems[i] == "i64") {
                                    emit_line("  " + conv_val + " = trunc i64 " + elem_val +
                                              " to i32");
                                } else if (expected_elems[i] == "i64" && actual_elems[i] == "i16") {
                                    emit_line("  " + conv_val + " = sext i16 " + elem_val +
                                              " to i64");
                                } else if (expected_elems[i] == "i64" && actual_elems[i] == "i8") {
                                    emit_line("  " + conv_val + " = sext i8 " + elem_val +
                                              " to i64");
                                } else if (expected_elems[i] == "i32" && actual_elems[i] == "i16") {
                                    emit_line("  " + conv_val + " = sext i16 " + elem_val +
                                              " to i32");
                                } else if (expected_elems[i] == "i32" && actual_elems[i] == "i8") {
                                    emit_line("  " + conv_val + " = sext i8 " + elem_val +
                                              " to i32");
                                } else {
                                    // Same type or unhandled - just use original
                                    conv_val = elem_val;
                                }
                            }

                            std::string dst_elem_ptr = fresh_reg();
                            emit_line("  " + dst_elem_ptr + " = getelementptr inbounds " +
                                      current_ret_type_ + ", ptr " + dst_ptr + ", i32 0, i32 " +
                                      std::to_string(i));
                            emit_line("  store " + expected_elems[i] + " " + conv_val + ", ptr " +
                                      dst_elem_ptr);
                        }

                        // Load converted tuple
                        std::string result = fresh_reg();
                        emit_line("  " + result + " = load " + current_ret_type_ + ", ptr " +
                                  dst_ptr);
                        emit_line("  ret " + current_ret_type_ + " " + result);
                        block_terminated_ = true;
                        return "void";
                    }
                }
            }

            // Handle FFI wrapper struct return: if returning a ptr but expecting a struct
            // type that is a single-ptr wrapper (e.g., List[Str] = { ptr }, File = { ptr }),
            // wrap the pointer using insertvalue. For other structs (multi-field or non-ptr
            // first field), load from the pointer instead.
            if (val_type == "ptr" && (current_ret_type_.starts_with("%class.") ||
                                      current_ret_type_.starts_with("%struct."))) {
                // Extract struct name to check if it's a single-ptr wrapper
                std::string struct_name;
                if (current_ret_type_.starts_with("%struct.")) {
                    struct_name = current_ret_type_.substr(8); // skip "%struct."
                } else {
                    struct_name = current_ret_type_.substr(7); // skip "%class."
                }

                // Check if struct is a single-field { ptr } wrapper
                bool is_ptr_wrapper = false;
                auto fields_it = struct_fields_.find(struct_name);
                if (fields_it != struct_fields_.end() && fields_it->second.size() == 1 &&
                    fields_it->second[0].llvm_type == "ptr") {
                    is_ptr_wrapper = true;
                }

                if (is_ptr_wrapper) {
                    // Wrap the ptr as field 0 of the wrapper struct
                    std::string wrapped = fresh_reg();
                    emit_line("  " + wrapped + " = insertvalue " + current_ret_type_ +
                              " undef, ptr " + val + ", 0");
                    emit_line("  ret " + current_ret_type_ + " " + wrapped);
                } else {
                    // Load the struct from the pointer (ptr points to struct memory)
                    std::string loaded_struct = fresh_reg();
                    emit_line("  " + loaded_struct + " = load " + current_ret_type_ + ", ptr " +
                              val);
                    emit_line("  ret " + current_ret_type_ + " " + loaded_struct);
                }
                block_terminated_ = true;
                return "void";
            }

            // Handle dyn coercion: return a concrete type as dyn Behavior
            // This converts a concrete struct to a fat pointer {data_ptr, vtable_ptr}
            if (current_ret_type_.starts_with("%dyn.") &&
                (val_type.starts_with("%struct.") || val_type.starts_with("%class."))) {
                // Extract behavior name from %dyn.Counter -> Counter
                std::string behavior_name = current_ret_type_.substr(5);

                // Extract concrete type name from %struct.SimpleCounter -> SimpleCounter
                std::string concrete_type;
                if (val_type.starts_with("%struct.")) {
                    concrete_type = val_type.substr(8);
                } else if (val_type.starts_with("%class.")) {
                    concrete_type = val_type.substr(7);
                }

                // Look up the vtable
                std::string vtable = get_vtable(concrete_type, behavior_name);
                if (!vtable.empty()) {
                    // Allocate space for the concrete value
                    std::string data_alloca = fresh_reg();
                    emit_line("  " + data_alloca + " = alloca " + val_type);
                    emit_line("  store " + val_type + " " + val + ", ptr " + data_alloca);

                    // Allocate the fat pointer struct
                    std::string dyn_alloca = fresh_reg();
                    emit_line("  " + dyn_alloca + " = alloca " + current_ret_type_);

                    // Store data pointer (field 0)
                    std::string data_field = fresh_reg();
                    emit_line("  " + data_field + " = getelementptr " + current_ret_type_ +
                              ", ptr " + dyn_alloca + ", i32 0, i32 0");
                    emit_line("  store ptr " + data_alloca + ", ptr " + data_field);

                    // Store vtable pointer (field 1)
                    std::string vtable_field = fresh_reg();
                    emit_line("  " + vtable_field + " = getelementptr " + current_ret_type_ +
                              ", ptr " + dyn_alloca + ", i32 0, i32 1");
                    emit_line("  store ptr " + vtable + ", ptr " + vtable_field);

                    // Load the fat pointer and return it
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + current_ret_type_ + ", ptr " +
                              dyn_alloca);
                    emit_line("  ret " + current_ret_type_ + " " + result);
                    block_terminated_ = true;
                    return "void";
                }
            }

            // Handle integer type extension when actual differs from expected
            std::string final_val = val;
            if (val_type != current_ret_type_) {
                // Integer extension: i32 -> i64, i16 -> i64, i8 -> i64
                if (current_ret_type_ == "i64" &&
                    (val_type == "i32" || val_type == "i16" || val_type == "i8")) {
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext " + val_type + " " + val + " to i64");
                    final_val = ext_reg;
                } else if (current_ret_type_ == "i32" && (val_type == "i16" || val_type == "i8")) {
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext " + val_type + " " + val + " to i32");
                    final_val = ext_reg;
                }
                // Bool truncation: i32 -> i1 (C functions return int for bool)
                else if (current_ret_type_ == "i1" && (val_type == "i32" || val_type == "i64" ||
                                                       val_type == "i8" || val_type == "i16")) {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i1");
                    final_val = trunc_reg;
                }
                // Integer truncation: larger -> smaller (for negative literals)
                else if (current_ret_type_ == "i8" && (val_type == "i32" || val_type == "i64")) {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i8");
                    final_val = trunc_reg;
                } else if (current_ret_type_ == "i16" && (val_type == "i32" || val_type == "i64")) {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i16");
                    final_val = trunc_reg;
                } else if (current_ret_type_ == "i32" && val_type == "i64") {
                    std::string trunc_reg = fresh_reg();
                    emit_line("  " + trunc_reg + " = trunc " + val_type + " " + val + " to i32");
                    final_val = trunc_reg;
                }
            }
            emit_line("  ret " + current_ret_type_ + " " + final_val);
        }
    } else {
        emit_line("  ret void");
    }
    block_terminated_ = true;
    return "void";
}

auto LLVMIRGen::gen_throw(const parser::ThrowExpr& thr) -> std::string {
    // Generate the expression being thrown (e.g., new Error("message"))
    std::string thrown_val = gen_expr(*thr.expr);
    std::string thrown_type = last_expr_type_;

    // If the thrown value is a pointer to an Error-like object with a 'message' field,
    // extract the message and pass it to panic
    std::string panic_msg = "null";

    if (thrown_type == "ptr" || thrown_type.starts_with("%class.") ||
        thrown_type.starts_with("%struct.")) {
        // Try to access a 'message' field at index 0 (common convention for Error types)
        // This handles `new Error("message")` where Error has a message field
        std::string msg_ptr = fresh_reg();
        std::string msg_val = fresh_reg();

        // Assume Error-like objects have message as first field (ptr to char)
        emit_line("  ; throw expression - extracting error message");
        emit_line("  " + msg_ptr + " = getelementptr inbounds ptr, ptr " + thrown_val + ", i32 0");
        emit_line("  " + msg_val + " = load ptr, ptr " + msg_ptr);
        panic_msg = msg_val;
    }

    // Call panic to terminate the program (panic is declared by emit_runtime_decls)
    // This integrates with @should_panic test infrastructure
    emit_line("  call void @panic(ptr " + panic_msg + ")");
    emit_line("  unreachable");

    block_terminated_ = true;
    return "void";
}

// Helper: generate comparison for a single pattern against scrutinee
// Returns the comparison result register, or empty string for always-match patterns

} // namespace tml::codegen
