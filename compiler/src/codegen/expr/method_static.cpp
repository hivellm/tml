//! # LLVM IR Generator - Static Method Calls
//!
//! This file implements `Type::method()` static method calls.
//!
//! ## Supported Types
//!
//! | Type    | Static Methods            |
//! |---------|---------------------------|
//! | List    | `new()`, `with_capacity()`|
//! | HashMap | `new()`, `with_capacity()`|
//! | Buffer  | `new()`, `with_capacity()`|
//! | File    | `open()`, `create()`      |
//! | Path    | `new()`                   |
//! | I32, etc| `default()`, `max()`, `min()`|
//!
//! ## Generic Handling
//!
//! Generic static methods like `List[I32]::new()` extract the type
//! argument and call the appropriate runtime function.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_static_method_call(const parser::MethodCallExpr& call,
                                       const std::string& type_name) -> std::optional<std::string> {
    const std::string& method = call.method;

    // List static methods
    if (type_name == "List") {
        std::string struct_name = "List";
        if (call.receiver->is<parser::PathExpr>()) {
            const auto& pe = call.receiver->as<parser::PathExpr>();
            if (pe.generics.has_value() && !pe.generics->args.empty()) {
                for (const auto& arg : pe.generics->args) {
                    if (arg.is_type() && arg.as_type()->is<parser::NamedType>()) {
                        const auto& named = arg.as_type()->as<parser::NamedType>();
                        if (!named.path.segments.empty()) {
                            struct_name += "__" + named.path.segments.back();
                        }
                    }
                }
            }
        }
        std::string struct_type = "%struct." + struct_name;

        if (method == "new") {
            std::string cap = call.args.empty() ? "8" : gen_expr(*call.args[0]);
            std::string cap_type = last_expr_type_;
            std::string cap_i64;
            if (cap_type == "i64") {
                cap_i64 = cap;
            } else {
                cap_i64 = fresh_reg();
                emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
            }
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @list_create(i64 " + cap_i64 + ")");
            std::string list_ptr = fresh_reg();
            emit_line("  " + list_ptr + " = alloca " + struct_type);
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " +
                      list_ptr + ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + struct_type + ", ptr " + list_ptr);
            last_expr_type_ = struct_type;
            return result;
        }
        if (method == "default") {
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @list_create(i64 8)");
            std::string list_ptr = fresh_reg();
            emit_line("  " + list_ptr + " = alloca " + struct_type);
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " +
                      list_ptr + ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + struct_type + ", ptr " + list_ptr);
            last_expr_type_ = struct_type;
            return result;
        }
    }

    // HashMap static methods
    if (type_name == "HashMap") {
        std::string struct_name = "HashMap";
        if (call.receiver->is<parser::PathExpr>()) {
            const auto& pe = call.receiver->as<parser::PathExpr>();
            if (pe.generics.has_value() && !pe.generics->args.empty()) {
                for (const auto& arg : pe.generics->args) {
                    if (arg.is_type() && arg.as_type()->is<parser::NamedType>()) {
                        const auto& named = arg.as_type()->as<parser::NamedType>();
                        if (!named.path.segments.empty()) {
                            struct_name += "__" + named.path.segments.back();
                        }
                    }
                }
            }
        }
        std::string struct_type = "%struct." + struct_name;

        if (method == "new") {
            std::string cap = call.args.empty() ? "16" : gen_expr(*call.args[0]);
            std::string cap_i64 = fresh_reg();
            emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @hashmap_create(i64 " + cap_i64 + ")");
            std::string map_ptr = fresh_reg();
            emit_line("  " + map_ptr + " = alloca " + struct_type);
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " + map_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + struct_type + ", ptr " + map_ptr);
            last_expr_type_ = struct_type;
            return result;
        }
        if (method == "default") {
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @hashmap_create(i64 16)");
            std::string map_ptr = fresh_reg();
            emit_line("  " + map_ptr + " = alloca " + struct_type);
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " + map_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + struct_type + ", ptr " + map_ptr);
            last_expr_type_ = struct_type;
            return result;
        }
    }

    // Buffer static methods
    if (type_name == "Buffer") {
        std::string struct_type = "%struct.Buffer";

        if (method == "new") {
            std::string cap = call.args.empty() ? "64" : gen_expr(*call.args[0]);
            std::string cap_i64 = fresh_reg();
            emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @buffer_create(i64 " + cap_i64 + ")");
            std::string buf_ptr = fresh_reg();
            emit_line("  " + buf_ptr + " = alloca " + struct_type);
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " + buf_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + struct_type + ", ptr " + buf_ptr);
            last_expr_type_ = struct_type;
            return result;
        }
        if (method == "default") {
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @buffer_create(i64 64)");
            std::string buf_ptr = fresh_reg();
            emit_line("  " + buf_ptr + " = alloca " + struct_type);
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " + buf_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + struct_type + ", ptr " + buf_ptr);
            last_expr_type_ = struct_type;
            return result;
        }
    }

    // File static methods
    if (type_name == "File") {
        if (method == "open_read") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @file_open_read(ptr " + path_arg + ")");
            std::string file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr %struct.File, ptr " + file_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load %struct.File, ptr " + file_ptr);
            last_expr_type_ = "%struct.File";
            return result;
        }
        if (method == "open_write") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @file_open_write(ptr " + path_arg + ")");
            std::string file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr %struct.File, ptr " + file_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load %struct.File, ptr " + file_ptr);
            last_expr_type_ = "%struct.File";
            return result;
        }
        if (method == "open_append") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string handle = fresh_reg();
            emit_line("  " + handle + " = call ptr @file_open_append(ptr " + path_arg + ")");
            std::string file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            std::string handle_field = fresh_reg();
            emit_line("  " + handle_field + " = getelementptr %struct.File, ptr " + file_ptr +
                      ", i32 0, i32 0");
            emit_line("  store ptr " + handle + ", ptr " + handle_field);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load %struct.File, ptr " + file_ptr);
            last_expr_type_ = "%struct.File";
            return result;
        }
        if (method == "read_all") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @file_read_all(ptr " + path_arg + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "write_all") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string content_arg = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_write_all(ptr " + path_arg + ", ptr " +
                      content_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "append_all") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string content_arg = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_append_all(ptr " + path_arg + ", ptr " +
                      content_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // Path static methods
    if (type_name == "Path") {
        if (method == "exists") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_exists(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "is_file") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_is_file(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "is_dir") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_is_dir(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "create_dir") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_create_dir(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "create_dir_all") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_create_dir_all(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "remove") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_remove(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "remove_dir") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_remove_dir(ptr " + path_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "rename") {
            std::string from_arg = gen_expr(*call.args[0]);
            std::string to_arg = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_rename(ptr " + from_arg + ", ptr " +
                      to_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "copy") {
            std::string from_arg = gen_expr(*call.args[0]);
            std::string to_arg = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @path_copy(ptr " + from_arg + ", ptr " + to_arg +
                      ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "join") {
            std::string base_arg = gen_expr(*call.args[0]);
            std::string child_arg = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @path_join(ptr " + base_arg + ", ptr " +
                      child_arg + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "parent") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @path_parent(ptr " + path_arg + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "filename") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @path_filename(ptr " + path_arg + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "extension") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @path_extension(ptr " + path_arg + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "absolute") {
            std::string path_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @path_absolute(ptr " + path_arg + ")");
            last_expr_type_ = "ptr";
            return result;
        }
    }

    // Primitive type static methods (default)
    if (method == "default") {
        // Integer types: default is 0
        if (type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128") {
            std::string llvm_ty;
            if (type_name == "I8" || type_name == "U8")
                llvm_ty = "i8";
            else if (type_name == "I16" || type_name == "U16")
                llvm_ty = "i16";
            else if (type_name == "I32" || type_name == "U32")
                llvm_ty = "i32";
            else if (type_name == "I64" || type_name == "U64")
                llvm_ty = "i64";
            else
                llvm_ty = "i128";
            last_expr_type_ = llvm_ty;
            return "0";
        }
        // Float types: default is 0.0
        if (type_name == "F32") {
            last_expr_type_ = "float";
            return "0.0";
        }
        if (type_name == "F64") {
            last_expr_type_ = "double";
            return "0.0";
        }
        // Bool: default is false
        if (type_name == "Bool") {
            last_expr_type_ = "i1";
            return "false";
        }
        // Str: default is empty string
        if (type_name == "Str") {
            std::string empty_str = add_string_literal("");
            last_expr_type_ = "ptr";
            return empty_str;
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
