// LLVM IR generator - Method calls
// Handles: gen_method_call for all method dispatch (static, instance, ptr, dyn)

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>

namespace tml::codegen {

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    const std::string& method = call.method;

    // Check for static method calls on type names (e.g., List.new(16), HashMap.default())
    // These occur when receiver is an IdentExpr or PathExpr that names a type, not a variable

    // Extract type name from either IdentExpr or PathExpr (for generic types like List[I32])
    std::string type_name;
    bool has_type_name = false;

    if (call.receiver->is<parser::IdentExpr>()) {
        type_name = call.receiver->as<parser::IdentExpr>().name;
        has_type_name = true;
    } else if (call.receiver->is<parser::PathExpr>()) {
        const auto& path_expr = call.receiver->as<parser::PathExpr>();
        if (path_expr.path.segments.size() == 1) {
            type_name = path_expr.path.segments[0];
            has_type_name = true;
        }
    }

    if (has_type_name) {
        // Check if this is a known struct type (not a variable)
        bool is_type_name =
            struct_types_.count(type_name) > 0 || type_name == "List" || type_name == "HashMap" ||
            type_name == "Buffer" || type_name == "File" || type_name == "Path" ||
            // Primitive types for static methods like I32::default()
            type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128" || type_name == "F32" || type_name == "F64" ||
            type_name == "Bool" || type_name == "Str";

        // Also check it's not a local variable
        if (is_type_name && locals_.count(type_name) == 0) {
            // This is a static method call on a type

            // List static methods
            if (type_name == "List") {
                // Build the full struct type name including generics (e.g., List__I32)
                std::string struct_name = "List";
                if (call.receiver->is<parser::PathExpr>()) {
                    const auto& pe = call.receiver->as<parser::PathExpr>();
                    if (pe.generics.has_value() && !pe.generics->args.empty()) {
                        for (const auto& arg : pe.generics->args) {
                            // Extract type name from parser type
                            if (arg->is<parser::NamedType>()) {
                                const auto& named = arg->as<parser::NamedType>();
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
                    // Construct the List struct with the handle
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
                    // Construct the List struct with the handle
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
                // Build struct type name from generics (HashMap[K, V] -> %struct.HashMap__K__V)
                std::string struct_name = "HashMap";
                if (call.receiver->is<parser::PathExpr>()) {
                    const auto& pe = call.receiver->as<parser::PathExpr>();
                    if (pe.generics.has_value() && !pe.generics->args.empty()) {
                        for (const auto& arg : pe.generics->args) {
                            if (arg->is<parser::NamedType>()) {
                                const auto& named = arg->as<parser::NamedType>();
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
                    // Construct the HashMap struct with the handle
                    std::string map_ptr = fresh_reg();
                    emit_line("  " + map_ptr + " = alloca " + struct_type);
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " +
                              map_ptr + ", i32 0, i32 0");
                    emit_line("  store ptr " + handle + ", ptr " + handle_field);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + struct_type + ", ptr " + map_ptr);
                    last_expr_type_ = struct_type;
                    return result;
                }
                if (method == "default") {
                    std::string handle = fresh_reg();
                    emit_line("  " + handle + " = call ptr @hashmap_create(i64 16)");
                    // Construct the HashMap struct with the handle
                    std::string map_ptr = fresh_reg();
                    emit_line("  " + map_ptr + " = alloca " + struct_type);
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " +
                              map_ptr + ", i32 0, i32 0");
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
                    // Construct the Buffer struct with the handle
                    std::string buf_ptr = fresh_reg();
                    emit_line("  " + buf_ptr + " = alloca " + struct_type);
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " +
                              buf_ptr + ", i32 0, i32 0");
                    emit_line("  store ptr " + handle + ", ptr " + handle_field);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + struct_type + ", ptr " + buf_ptr);
                    last_expr_type_ = struct_type;
                    return result;
                }
                if (method == "default") {
                    std::string handle = fresh_reg();
                    emit_line("  " + handle + " = call ptr @buffer_create(i64 64)");
                    // Construct the Buffer struct with the handle
                    std::string buf_ptr = fresh_reg();
                    emit_line("  " + buf_ptr + " = alloca " + struct_type);
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr " + struct_type + ", ptr " +
                              buf_ptr + ", i32 0, i32 0");
                    emit_line("  store ptr " + handle + ", ptr " + handle_field);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + struct_type + ", ptr " + buf_ptr);
                    last_expr_type_ = struct_type;
                    return result;
                }
            }

            // File static methods - methods that return File need to construct %struct.File
            if (type_name == "File") {
                if (method == "open_read") {
                    std::string path_arg = gen_expr(*call.args[0]);
                    // Call runtime to get handle
                    std::string handle = fresh_reg();
                    emit_line("  " + handle + " = call ptr @file_open_read(ptr " + path_arg + ")");
                    // Construct File struct with the handle
                    std::string file_ptr = fresh_reg();
                    emit_line("  " + file_ptr + " = alloca %struct.File");
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr %struct.File, ptr " +
                              file_ptr + ", i32 0, i32 0");
                    emit_line("  store ptr " + handle + ", ptr " + handle_field);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load %struct.File, ptr " + file_ptr);
                    last_expr_type_ = "%struct.File";
                    return result;
                }
                if (method == "open_write") {
                    std::string path_arg = gen_expr(*call.args[0]);
                    // Call runtime to get handle
                    std::string handle = fresh_reg();
                    emit_line("  " + handle + " = call ptr @file_open_write(ptr " + path_arg + ")");
                    // Construct File struct with the handle
                    std::string file_ptr = fresh_reg();
                    emit_line("  " + file_ptr + " = alloca %struct.File");
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr %struct.File, ptr " +
                              file_ptr + ", i32 0, i32 0");
                    emit_line("  store ptr " + handle + ", ptr " + handle_field);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load %struct.File, ptr " + file_ptr);
                    last_expr_type_ = "%struct.File";
                    return result;
                }
                if (method == "open_append") {
                    std::string path_arg = gen_expr(*call.args[0]);
                    // Call runtime to get handle
                    std::string handle = fresh_reg();
                    emit_line("  " + handle + " = call ptr @file_open_append(ptr " + path_arg +
                              ")");
                    // Construct File struct with the handle
                    std::string file_ptr = fresh_reg();
                    emit_line("  " + file_ptr + " = alloca %struct.File");
                    std::string handle_field = fresh_reg();
                    emit_line("  " + handle_field + " = getelementptr %struct.File, ptr " +
                              file_ptr + ", i32 0, i32 0");
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
                    emit_line("  " + result + " = call i1 @file_write_all(ptr " + path_arg +
                              ", ptr " + content_arg + ")");
                    last_expr_type_ = "i1";
                    return result;
                }
                if (method == "append_all") {
                    std::string path_arg = gen_expr(*call.args[0]);
                    std::string content_arg = gen_expr(*call.args[1]);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call i1 @file_append_all(ptr " + path_arg +
                              ", ptr " + content_arg + ")");
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
                    emit_line("  " + result + " = call i1 @path_create_dir_all(ptr " + path_arg +
                              ")");
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
                    emit_line("  " + result + " = call i1 @path_copy(ptr " + from_arg + ", ptr " +
                              to_arg + ")");
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
                if (type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                    type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                    type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                    type_name == "U128") {
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

            // Unknown static method - report error
            report_error("Unknown static method: " + type_name + "." + method, call.span);
            return "0";
        }
    }

    // Generate receiver (the object the method is called on)
    std::string receiver = gen_expr(*call.receiver);

    // For struct instance methods (like File), we also need the pointer to the receiver
    // gen_expr loads the value, but getelementptr needs a pointer
    std::string receiver_ptr;
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            receiver_ptr = it->second.reg; // Use alloca pointer directly
        }
    }

    // Handle Ptr[T] methods
    types::TypePtr receiver_type = infer_expr_type(*call.receiver);
    if (receiver_type && receiver_type->is<types::PtrType>()) {
        const auto& ptr_type = receiver_type->as<types::PtrType>();
        std::string inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);

        // .read() -> T - dereference pointer
        if (method == "read") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + receiver);
            last_expr_type_ = inner_llvm_type;
            return result;
        }

        // .write(value: T) -> Unit - write value to pointer
        if (method == "write") {
            if (call.args.empty()) {
                report_error("Ptr.write() requires a value argument", call.span);
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  store " + inner_llvm_type + " " + val + ", ptr " + receiver);
            return "void";
        }

        // .offset(n: I64) -> Ptr[T] - pointer arithmetic
        if (method == "offset") {
            if (call.args.empty()) {
                report_error("Ptr.offset() requires an offset argument", call.span);
                return receiver;
            }
            std::string offset = gen_expr(*call.args[0]);
            // Convert offset to i64 (getelementptr requires i64 index)
            std::string offset_i64 = fresh_reg();
            emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + inner_llvm_type + ", ptr " + receiver +
                      ", i64 " + offset_i64);
            last_expr_type_ = "ptr";
            return result;
        }

        // .is_null() -> Bool - check if pointer is null
        if (method == "is_null") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq ptr " + receiver + ", null");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // Handle primitive type methods (core::ops)
    if (receiver_type && receiver_type->is<types::PrimitiveType>()) {
        const auto& prim = receiver_type->as<types::PrimitiveType>();
        auto kind = prim.kind;

        // Integer operations (I8, I16, I32, I64, I128, U8, U16, U32, U64, U128)
        bool is_integer = (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                           kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                           kind == types::PrimitiveKind::I128 || kind == types::PrimitiveKind::U8 ||
                           kind == types::PrimitiveKind::U16 || kind == types::PrimitiveKind::U32 ||
                           kind == types::PrimitiveKind::U64 || kind == types::PrimitiveKind::U128);
        bool is_signed = (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                          kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                          kind == types::PrimitiveKind::I128);
        bool is_float = (kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64);

        std::string llvm_ty = llvm_type_from_semantic(receiver_type);

        if (is_integer || is_float) {
            // add(other) -> Self
            if (method == "add") {
                if (call.args.empty()) {
                    report_error("add() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string result = fresh_reg();
                if (is_float) {
                    emit_line("  " + result + " = fadd " + llvm_ty + " " + receiver + ", " + other);
                } else {
                    emit_line("  " + result + " = add " + llvm_ty + " " + receiver + ", " + other);
                }
                last_expr_type_ = llvm_ty;
                return result;
            }

            // sub(other) -> Self
            if (method == "sub") {
                if (call.args.empty()) {
                    report_error("sub() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string result = fresh_reg();
                if (is_float) {
                    emit_line("  " + result + " = fsub " + llvm_ty + " " + receiver + ", " + other);
                } else {
                    emit_line("  " + result + " = sub " + llvm_ty + " " + receiver + ", " + other);
                }
                last_expr_type_ = llvm_ty;
                return result;
            }

            // mul(other) -> Self
            if (method == "mul") {
                if (call.args.empty()) {
                    report_error("mul() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string result = fresh_reg();
                if (is_float) {
                    emit_line("  " + result + " = fmul " + llvm_ty + " " + receiver + ", " + other);
                } else {
                    emit_line("  " + result + " = mul " + llvm_ty + " " + receiver + ", " + other);
                }
                last_expr_type_ = llvm_ty;
                return result;
            }

            // div(other) -> Self
            if (method == "div") {
                if (call.args.empty()) {
                    report_error("div() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string result = fresh_reg();
                if (is_float) {
                    emit_line("  " + result + " = fdiv " + llvm_ty + " " + receiver + ", " + other);
                } else if (is_signed) {
                    emit_line("  " + result + " = sdiv " + llvm_ty + " " + receiver + ", " + other);
                } else {
                    emit_line("  " + result + " = udiv " + llvm_ty + " " + receiver + ", " + other);
                }
                last_expr_type_ = llvm_ty;
                return result;
            }

            // rem(other) -> Self (integers only)
            if (method == "rem" && is_integer) {
                if (call.args.empty()) {
                    report_error("rem() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string result = fresh_reg();
                if (is_signed) {
                    emit_line("  " + result + " = srem " + llvm_ty + " " + receiver + ", " + other);
                } else {
                    emit_line("  " + result + " = urem " + llvm_ty + " " + receiver + ", " + other);
                }
                last_expr_type_ = llvm_ty;
                return result;
            }

            // neg() -> Self (unary negation)
            if (method == "neg") {
                std::string result = fresh_reg();
                if (is_float) {
                    emit_line("  " + result + " = fneg " + llvm_ty + " " + receiver);
                } else {
                    emit_line("  " + result + " = sub " + llvm_ty + " 0, " + receiver);
                }
                last_expr_type_ = llvm_ty;
                return result;
            }
        }

        // Bool operations
        if (kind == types::PrimitiveKind::Bool) {
            // negate() -> Bool
            if (method == "negate") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = xor i1 " + receiver + ", true");
                last_expr_type_ = "i1";
                return result;
            }
        }

        // duplicate() -> Self (for all primitive types)
        // For primitives, duplicate is just returning the value (copy semantics)
        if (method == "duplicate") {
            last_expr_type_ = llvm_ty;
            return receiver;
        }

        // to_owned() -> Self (for ToOwned behavior)
        // For primitives, to_owned is just returning the value (copy semantics)
        if (method == "to_owned") {
            last_expr_type_ = llvm_ty;
            return receiver;
        }

        // borrow() -> ref Self (for Borrow behavior)
        // Returns a reference to the value
        if (method == "borrow") {
            // Need to use receiver_ptr (alloca) not receiver (loaded value)
            if (!receiver_ptr.empty()) {
                last_expr_type_ = "ptr";
                return receiver_ptr;
            }
            // Fallback: store value to temp and return pointer
            std::string tmp = fresh_reg();
            emit_line("  " + tmp + " = alloca " + llvm_ty);
            emit_line("  store " + llvm_ty + " " + receiver + ", ptr " + tmp);
            last_expr_type_ = "ptr";
            return tmp;
        }

        // borrow_mut() -> mut ref Self (for BorrowMut behavior)
        // Returns a mutable reference to the value
        if (method == "borrow_mut") {
            // Need to use receiver_ptr (alloca) not receiver (loaded value)
            if (!receiver_ptr.empty()) {
                last_expr_type_ = "ptr";
                return receiver_ptr;
            }
            // Fallback: store value to temp and return pointer
            std::string tmp = fresh_reg();
            emit_line("  " + tmp + " = alloca " + llvm_ty);
            emit_line("  store " + llvm_ty + " " + receiver + ", ptr " + tmp);
            last_expr_type_ = "ptr";
            return tmp;
        }

        // to_string() -> Str (for Display behavior)
        if (method == "to_string") {
            std::string result = fresh_reg();
            if (kind == types::PrimitiveKind::Bool) {
                // Bool uses zext to convert i1 to i32 for the runtime call
                std::string ext = fresh_reg();
                emit_line("  " + ext + " = zext i1 " + receiver + " to i32");
                emit_line("  " + result + " = call ptr @bool_to_string(i32 " + ext + ")");
            } else if (kind == types::PrimitiveKind::I32) {
                emit_line("  " + result + " = call ptr @i32_to_string(i32 " + receiver + ")");
            } else if (kind == types::PrimitiveKind::I64) {
                emit_line("  " + result + " = call ptr @i64_to_string(i64 " + receiver + ")");
            } else if (kind == types::PrimitiveKind::F64) {
                emit_line("  " + result + " = call ptr @float_to_string(double " + receiver + ")");
            } else if (kind == types::PrimitiveKind::Str) {
                // String's to_string just returns itself
                last_expr_type_ = "ptr";
                return receiver;
            } else {
                // For other types, convert to i64 first
                std::string ext = fresh_reg();
                emit_line("  " + ext + " = sext " + llvm_ty + " " + receiver + " to i64");
                emit_line("  " + result + " = call ptr @i64_to_string(i64 " + ext + ")");
            }
            last_expr_type_ = "ptr";
            return result;
        }

        // hash() -> I64 (for Hash behavior)
        // Uses FNV-1a hash algorithm
        if (method == "hash") {
            std::string result = fresh_reg();
            if (kind == types::PrimitiveKind::Bool) {
                // Bool: true -> 1, false -> 0
                emit_line("  " + result + " = zext i1 " + receiver + " to i64");
            } else if (kind == types::PrimitiveKind::Str) {
                // String: call str_hash and extend to i64
                std::string hash32 = fresh_reg();
                emit_line("  " + hash32 + " = call i32 @str_hash(ptr " + receiver + ")");
                emit_line("  " + result + " = sext i32 " + hash32 + " to i64");
            } else if (is_integer) {
                // Integer types: FNV-1a-like hash
                // For simplicity, XOR with FNV offset and multiply by FNV prime
                std::string val64 = receiver;
                if (llvm_ty != "i64") {
                    val64 = fresh_reg();
                    if (is_signed) {
                        emit_line("  " + val64 + " = sext " + llvm_ty + " " + receiver + " to i64");
                    } else {
                        emit_line("  " + val64 + " = zext " + llvm_ty + " " + receiver + " to i64");
                    }
                }
                // FNV-1a offset basis: 14695981039346656037
                // FNV-1a prime: 1099511628211
                std::string xor_result = fresh_reg();
                emit_line("  " + xor_result + " = xor i64 " + val64 + ", 14695981039346656037");
                emit_line("  " + result + " = mul i64 " + xor_result + ", 1099511628211");
            } else if (is_float) {
                // Float: bitcast to i64 and hash
                std::string bits = fresh_reg();
                if (kind == types::PrimitiveKind::F32) {
                    std::string bits32 = fresh_reg();
                    emit_line("  " + bits32 + " = bitcast float " + receiver + " to i32");
                    emit_line("  " + bits + " = zext i32 " + bits32 + " to i64");
                } else {
                    emit_line("  " + bits + " = bitcast double " + receiver + " to i64");
                }
                std::string xor_result = fresh_reg();
                emit_line("  " + xor_result + " = xor i64 " + bits + ", 14695981039346656037");
                emit_line("  " + result + " = mul i64 " + xor_result + ", 1099511628211");
            } else {
                // Unknown type - return 0
                result = "0";
            }
            last_expr_type_ = "i64";
            return result;
        }

        // Comparison methods for numeric types
        if (is_integer || is_float) {
            // cmp(ref other) -> Ordering
            // Returns: Ordering { Less=0, Equal=1, Greater=2 }
            if (method == "cmp") {
                if (call.args.empty()) {
                    report_error("cmp() requires an argument", call.span);
                    return "0";
                }
                // The argument is a reference, need to load it
                std::string other_ptr = gen_expr(*call.args[0]);
                std::string other = fresh_reg();
                emit_line("  " + other + " = load " + llvm_ty + ", ptr " + other_ptr);

                // Compare: if a < b => Less(0), if a == b => Equal(1), else Greater(2)
                std::string cmp_lt = fresh_reg();
                std::string cmp_eq = fresh_reg();
                if (is_float) {
                    emit_line("  " + cmp_lt + " = fcmp olt " + llvm_ty + " " + receiver + ", " +
                              other);
                    emit_line("  " + cmp_eq + " = fcmp oeq " + llvm_ty + " " + receiver + ", " +
                              other);
                } else if (is_signed) {
                    emit_line("  " + cmp_lt + " = icmp slt " + llvm_ty + " " + receiver + ", " +
                              other);
                    emit_line("  " + cmp_eq + " = icmp eq " + llvm_ty + " " + receiver + ", " +
                              other);
                } else {
                    emit_line("  " + cmp_lt + " = icmp ult " + llvm_ty + " " + receiver + ", " +
                              other);
                    emit_line("  " + cmp_eq + " = icmp eq " + llvm_ty + " " + receiver + ", " +
                              other);
                }
                // Less = 0, Equal = 1, Greater = 2
                // if lt: 0, else (if eq: 1, else 2)
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + cmp_eq + ", i32 1, i32 2");
                std::string tag = fresh_reg();
                emit_line("  " + tag + " = select i1 " + cmp_lt + ", i32 0, i32 " + sel1);
                // Create Ordering struct with the tag
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }

            // max(other) -> Self
            if (method == "max") {
                if (call.args.empty()) {
                    report_error("max() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string cmp = fresh_reg();
                if (is_float) {
                    emit_line("  " + cmp + " = fcmp ogt " + llvm_ty + " " + receiver + ", " +
                              other);
                } else if (is_signed) {
                    emit_line("  " + cmp + " = icmp sgt " + llvm_ty + " " + receiver + ", " +
                              other);
                } else {
                    emit_line("  " + cmp + " = icmp ugt " + llvm_ty + " " + receiver + ", " +
                              other);
                }
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + cmp + ", " + llvm_ty + " " + receiver +
                          ", " + llvm_ty + " " + other);
                last_expr_type_ = llvm_ty;
                return result;
            }

            // min(other) -> Self
            if (method == "min") {
                if (call.args.empty()) {
                    report_error("min() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string cmp = fresh_reg();
                if (is_float) {
                    emit_line("  " + cmp + " = fcmp olt " + llvm_ty + " " + receiver + ", " +
                              other);
                } else if (is_signed) {
                    emit_line("  " + cmp + " = icmp slt " + llvm_ty + " " + receiver + ", " +
                              other);
                } else {
                    emit_line("  " + cmp + " = icmp ult " + llvm_ty + " " + receiver + ", " +
                              other);
                }
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + cmp + ", " + llvm_ty + " " + receiver +
                          ", " + llvm_ty + " " + other);
                last_expr_type_ = llvm_ty;
                return result;
            }
        }
    }

    // Handle Ordering enum methods
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (named.name == "Ordering") {
            // Ordering is a struct { i32 } - extract the tag value first
            // The receiver is a %struct.Ordering value, we need to extract the i32 tag
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue %struct.Ordering " + receiver + ", 0");

            // is_less() -> Bool (tag == 0)
            if (method == "is_less") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
                last_expr_type_ = "i1";
                return result;
            }

            // is_equal() -> Bool (tag == 1)
            if (method == "is_equal") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
                last_expr_type_ = "i1";
                return result;
            }

            // is_greater() -> Bool (tag == 2)
            if (method == "is_greater") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 2");
                last_expr_type_ = "i1";
                return result;
            }

            // reverse() -> Ordering
            // Less(0) -> Greater(2), Equal(1) -> Equal(1), Greater(2) -> Less(0)
            if (method == "reverse") {
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_greater = fresh_reg();
                emit_line("  " + is_greater + " = icmp eq i32 " + tag_val + ", 2");
                // if is_less: 2, else (if is_greater: 0, else 1)
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_greater + ", i32 0, i32 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_less + ", i32 2, i32 " + sel1);
                // Create new Ordering struct with the new tag
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }

            // then_cmp(other: Ordering) -> Ordering
            // Returns this if not Equal, otherwise returns other
            if (method == "then_cmp") {
                if (call.args.empty()) {
                    report_error("then_cmp() requires an argument", call.span);
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string other_tag = fresh_reg();
                emit_line("  " + other_tag + " = extractvalue %struct.Ordering " + other + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_equal + ", i32 " + other_tag +
                          ", i32 " + tag_val);
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }

            // to_string() -> Str
            // Returns "Less", "Equal", or "Greater"
            if (method == "to_string") {
                // Create string constants if not already created
                std::string less_str = add_string_literal("Less");
                std::string equal_str = add_string_literal("Equal");
                std::string greater_str = add_string_literal("Greater");

                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                // if is_greater: "Greater", else "Equal"
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_equal + ", ptr " + equal_str +
                          ", ptr " + greater_str);
                // if is_less: "Less", else sel1
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + is_less + ", ptr " + less_str +
                          ", ptr " + sel1);
                last_expr_type_ = "ptr";
                return result;
            }

            // debug_string() -> Str
            // Returns "Ordering::Less", "Ordering::Equal", or "Ordering::Greater"
            if (method == "debug_string") {
                std::string less_str = add_string_literal("Ordering::Less");
                std::string equal_str = add_string_literal("Ordering::Equal");
                std::string greater_str = add_string_literal("Ordering::Greater");

                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_equal + ", ptr " + equal_str +
                          ", ptr " + greater_str);
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + is_less + ", ptr " + less_str +
                          ", ptr " + sel1);
                last_expr_type_ = "ptr";
                return result;
            }
        }

        // Handle Maybe[T] methods
        if (named.name == "Maybe") {
            // Maybe is a tagged union: { i32 tag, [8 x i8] data }
            // Just(value) has tag=0, Nothing has tag=1
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);

            // Extract the tag first
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + receiver +
                      ", 0");

            // Dispatch to helper function
            auto result = gen_maybe_method(call, receiver, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            // Outcome is a tagged union: { i32 tag, [8 x i8] data }
            // Ok(value) has tag=0, Err(error) has tag=1
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);

            // Extract the tag first
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + receiver +
                      ", 0");

            // Dispatch to helper function
            auto result = gen_outcome_method(call, receiver, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
            // Note: Remaining Outcome methods moved to method_outcome.cpp
        }
    }

    // Determine receiver type name for type-aware method dispatch
    std::string receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        receiver_type_name = receiver_type->as<types::NamedType>().name;
    }

    // Type-aware len method - calls different runtime functions based on type
    // Returns i64 to match the TML std library definition (len() -> I64)
    if (method == "len" || method == "length") {
        // For List/HashMap/Buffer, receiver is a struct with handle field - extract the handle
        std::string handle = receiver;
        if (receiver_type_name == "List" || receiver_type_name == "HashMap" ||
            receiver_type_name == "Buffer") {
            // Store struct value to temp, then extract handle field
            std::string struct_type = "%struct." + receiver_type_name;
            // For generic types, need the full mangled name
            if (receiver_type && receiver_type->is<types::NamedType>()) {
                const auto& named = receiver_type->as<types::NamedType>();
                if (!named.type_args.empty()) {
                    struct_type = "%struct." + named.name;
                    for (const auto& arg : named.type_args) {
                        struct_type += "__" + mangle_type(arg);
                    }
                }
            }
            std::string tmp = fresh_reg();
            emit_line("  " + tmp + " = alloca " + struct_type);
            emit_line("  store " + struct_type + " " + receiver + ", ptr " + tmp);
            std::string handle_ptr = fresh_reg();
            emit_line("  " + handle_ptr + " = getelementptr " + struct_type + ", ptr " + tmp +
                      ", i32 0, i32 0");
            handle = fresh_reg();
            emit_line("  " + handle + " = load ptr, ptr " + handle_ptr);
        }

        if (receiver_type_name == "HashMap") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @hashmap_len(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (receiver_type_name == "Buffer") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_len(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        // Str type - use str_len (returns I32, extend to I64)
        if (receiver_type_name == "Str" || last_expr_type_ == "ptr") {
            // For Str (ptr type), call str_len and sign-extend to i64
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @str_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext i32 " + i32_result + " to i64");
            last_expr_type_ = "i64";
            return result;
        }
        // Default: List
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @list_len(ptr " + handle + ")");
        last_expr_type_ = "i64";
        return result;
    }

    // Check if there's a user-defined impl method BEFORE falling through to built-ins
    // This allows user types to define methods like get(), push(), etc.
    // Skip builtin types (List, HashMap, Buffer) - their methods are handled explicitly below
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        // Skip builtin types - their impl methods are not generated
        bool is_builtin_type =
            (named.name == "List" || named.name == "HashMap" || named.name == "Buffer" ||
             named.name == "File" || named.name == "Path");
        if (!is_builtin_type) {
            // Use base type name for method lookup (Maybe, not Maybe__I32)
            // Impl methods are generated for the generic type, not each instantiation
            std::string qualified_name = named.name + "::" + method;
            auto func_sig = env_.lookup_func(qualified_name);
            if (func_sig) {
                // For generic impl blocks, queue specialized method for later generation
                std::string mangled_type_name = named.name;
                std::unordered_map<std::string, types::TypePtr> type_subs;

                if (!named.type_args.empty()) {
                    mangled_type_name = mangle_struct_name(named.name, named.type_args);
                    std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method;

                    // Build substitution map: T -> I32, etc. (used for both queueing and call gen)
                    auto impl_it = pending_generic_impls_.find(named.name);
                    if (impl_it != pending_generic_impls_.end()) {
                        const auto& impl = *impl_it->second;
                        for (size_t i = 0; i < impl.generics.size() && i < named.type_args.size();
                             ++i) {
                            type_subs[impl.generics[i].name] = named.type_args[i];
                        }
                    }

                    // Queue the specialized method for later generation if not already done
                    if (generated_impl_methods_.find(mangled_method_name) ==
                        generated_impl_methods_.end()) {
                        if (impl_it != pending_generic_impls_.end()) {
                            // Queue for later generation (after current function completes)
                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, type_subs, named.name});
                            generated_impl_methods_.insert(mangled_method_name);
                        }
                    }
                }

                // Generate call to impl method: @tml_TypeName_MethodName(this_ptr, args...)
                std::string fn_name = "@tml_" + mangled_type_name + "_" + method;

                // Get receiver pointer
                std::string impl_receiver_ptr;
                if (call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        if (it->second.type == "ptr") {
                            impl_receiver_ptr = receiver;
                        } else {
                            impl_receiver_ptr = it->second.reg;
                        }
                    } else {
                        impl_receiver_ptr = receiver;
                    }
                } else {
                    impl_receiver_ptr = receiver;
                }

                // Build argument list: self (receiver ptr) + args
                std::vector<std::pair<std::string, std::string>> typed_args;
                typed_args.push_back({"ptr", impl_receiver_ptr});

                // Get parameter types from function signature with type substitution
                // params[0] is 'this', so real args start at index 1
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    // Get type from func_sig if available (params[i+1] because params[0] is 'this')
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        auto param_type = func_sig->params[i + 1];
                        // Apply type substitution for generic parameters
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        arg_type = llvm_type_from_semantic(param_type);
                    }
                    typed_args.push_back({arg_type, val});
                }

                // Calculate return type with substitution
                auto return_type = func_sig->return_type;
                if (!type_subs.empty()) {
                    return_type = types::substitute_type(return_type, type_subs);
                }
                std::string ret_type = llvm_type_from_semantic(return_type);

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        } // if (!is_builtin_type)
    }

    // Extract handle from collection struct types (List, HashMap, Buffer)
    // These types have { ptr } layout where the ptr is the runtime handle
    std::string collection_handle = receiver;
    if (receiver_type_name == "List" || receiver_type_name == "HashMap" ||
        receiver_type_name == "Buffer") {
        // Store struct value to temp, then extract handle field
        std::string struct_type = "%struct." + receiver_type_name;
        // For generic types, need the full mangled name
        if (receiver_type && receiver_type->is<types::NamedType>()) {
            const auto& named = receiver_type->as<types::NamedType>();
            if (!named.type_args.empty()) {
                struct_type = "%struct." + named.name;
                for (const auto& arg : named.type_args) {
                    struct_type += "__" + mangle_type(arg);
                }
            }
        }
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + struct_type);
        emit_line("  store " + struct_type + " " + receiver + ", ptr " + tmp);
        std::string handle_ptr = fresh_reg();
        emit_line("  " + handle_ptr + " = getelementptr " + struct_type + ", ptr " + tmp +
                  ", i32 0, i32 0");
        collection_handle = fresh_reg();
        emit_line("  " + collection_handle + " = load ptr, ptr " + handle_ptr);
    }

    // List methods (only for List type)
    if (receiver_type_name == "List" && method == "push") {
        if (call.args.empty()) {
            report_error("push requires an argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_push(ptr " + collection_handle + ", i32 " +
                  val + ")");
        return result;
    }
    if (receiver_type_name == "List" && method == "pop") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_pop(ptr " + collection_handle + ")");
        return result;
    }
    // Type-aware get method - only for List/HashMap collection types
    if ((receiver_type_name == "List" || receiver_type_name == "HashMap") && method == "get") {
        if (call.args.empty()) {
            report_error("get requires an argument", call.span);
            return "0";
        }
        std::string arg = gen_expr(*call.args[0]);

        if (receiver_type_name == "HashMap") {
            // HashMap: get(key: I64) -> I64
            std::string key_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + arg + " to i64");
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @hashmap_get(ptr " + collection_handle +
                      ", i64 " + key_i64 + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        // Default: List - get(index: I32) -> I32
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_get(ptr " + collection_handle + ", i32 " +
                  arg + ")");
        return result;
    }
    // Type-aware set method (only for List/HashMap collection types)
    if ((receiver_type_name == "List" || receiver_type_name == "HashMap") && method == "set") {
        if (call.args.size() < 2) {
            report_error("set requires two arguments", call.span);
            return "void";
        }
        std::string arg1 = gen_expr(*call.args[0]);
        std::string arg2 = gen_expr(*call.args[1]);

        if (receiver_type_name == "HashMap") {
            // HashMap: set(key: I64, value: I64) -> Unit
            std::string key_i64 = fresh_reg();
            std::string val_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + arg1 + " to i64");
            emit_line("  " + val_i64 + " = sext i32 " + arg2 + " to i64");
            emit_line("  call void @hashmap_set(ptr " + collection_handle + ", i64 " + key_i64 +
                      ", i64 " + val_i64 + ")");
            return "void";
        }
        // Default: List - set(index: I32, value: I32) -> Unit
        emit_line("  call void @list_set(ptr " + collection_handle + ", i32 " + arg1 + ", i32 " +
                  arg2 + ")");
        return "void";
    }
    if (method == "clear") {
        emit_line("  call void @list_clear(ptr " + collection_handle + ")");
        return "void";
    }
    if (method == "is_empty" || method == "isEmpty") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @list_is_empty(ptr " + collection_handle + ")");
        return result;
    }
    if (method == "capacity") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @list_capacity(ptr " + collection_handle + ")");
        last_expr_type_ = "i64";
        return result;
    }

    // HashMap-specific methods (has, remove)
    // NOTE: get/set are handled above with type-aware dispatching
    if (method == "has" || method == "contains") {
        if (call.args.empty()) {
            report_error("has requires a key argument", call.span);
            return "0";
        }
        std::string key = gen_expr(*call.args[0]);
        std::string key_i64 = fresh_reg();
        emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @hashmap_has(ptr " + collection_handle + ", i64 " +
                  key_i64 + ")");
        return result;
    }
    if (method == "remove") {
        if (call.args.empty()) {
            report_error("remove requires a key argument", call.span);
            return "0";
        }
        std::string key = gen_expr(*call.args[0]);
        std::string key_i64 = fresh_reg();
        emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @hashmap_remove(ptr " + collection_handle + ", i64 " +
                  key_i64 + ")");
        return result;
    }

    // Buffer methods
    if (method == "write_byte") {
        if (call.args.empty()) {
            report_error("write_byte requires a value argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        emit_line("  call void @buffer_write_byte(ptr " + collection_handle + ", i32 " + val + ")");
        return "void";
    }
    if (method == "read_byte") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + collection_handle + ")");
        return result;
    }
    if (method == "remaining") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @buffer_remaining(ptr " + collection_handle + ")");
        last_expr_type_ = "i64";
        return result;
    }
    if (method == "write_i32") {
        if (call.args.empty()) {
            report_error("write_i32 requires a value argument", call.span);
            return "void";
        }
        std::string val = gen_expr(*call.args[0]);
        emit_line("  call void @buffer_write_i32(ptr " + collection_handle + ", i32 " + val + ")");
        return "void";
    }
    if (method == "write_i64") {
        if (call.args.empty()) {
            report_error("write_i64 requires a value argument", call.span);
            return "void";
        }
        std::string val = gen_expr(*call.args[0]);
        // Convert i32 to i64 if needed
        std::string val_i64 = fresh_reg();
        emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
        emit_line("  call void @buffer_write_i64(ptr " + collection_handle + ", i64 " + val_i64 +
                  ")");
        return "void";
    }
    if (method == "read_i32") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + collection_handle + ")");
        return result;
    }
    if (method == "read_i64") {
        std::string result_i64 = fresh_reg();
        emit_line("  " + result_i64 + " = call i64 @buffer_read_i64(ptr " + collection_handle +
                  ")");
        // Truncate to i32 for now
        std::string result = fresh_reg();
        emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
        return result;
    }
    if (method == "reset_read") {
        emit_line("  call void @buffer_reset_read(ptr " + collection_handle + ")");
        return "void";
    }
    if (method == "destroy") {
        // Type-aware destroy - different for List, HashMap, Buffer
        // collection_handle already extracted above for collection types
        if (receiver_type_name == "HashMap") {
            emit_line("  call void @hashmap_destroy(ptr " + collection_handle + ")");
        } else if (receiver_type_name == "Buffer") {
            emit_line("  call void @buffer_destroy(ptr " + collection_handle + ")");
        } else {
            // Default: List
            emit_line("  call void @list_destroy(ptr " + collection_handle + ")");
        }
        return "void";
    }

    // Try to find impl method using type inference
    // receiver_type already computed above for Ptr handling
    // Skip builtin types (List, HashMap, Buffer) - their methods are handled explicitly above
    TML_DEBUG_LN("[METHOD] receiver_type for method '"
                 << method << "': " << (receiver_type ? "exists" : "null"));
    if (receiver_type) {
        TML_DEBUG_LN("[METHOD] receiver_type kind index: " << receiver_type->kind.index());
    }
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named2 = receiver_type->as<types::NamedType>();
        bool is_builtin_type2 =
            (named2.name == "List" || named2.name == "HashMap" || named2.name == "Buffer" ||
             named2.name == "File" || named2.name == "Path");
        if (is_builtin_type2) {
            // Fall through to report unknown method error
        } else {
            std::string qualified_name = named2.name + "::" + method;
            TML_DEBUG_LN("[METHOD] Looking for impl method: " << qualified_name << " on type "
                                                              << named2.name);
            auto func_sig = env_.lookup_func(qualified_name);

            // If not found locally, try looking in the module the type is from
            if (!func_sig) {
                std::string module_path = named2.module_path;

                // If module_path is empty, try to resolve via imported symbol
                if (module_path.empty()) {
                    auto import_path = env_.resolve_imported_symbol(named2.name);
                    if (import_path) {
                        auto pos = import_path->rfind("::");
                        if (pos != std::string::npos) {
                            module_path = import_path->substr(0, pos);
                        }
                    }
                }

                // Look up in the module
                if (!module_path.empty()) {
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto func_it = module->functions.find(qualified_name);
                        if (func_it != module->functions.end()) {
                            func_sig = func_it->second;
                        }
                    }
                }

                // If still not found and we're generating module code, search all loaded modules
                // (for module-internal calls where the type isn't "imported")
                if (!func_sig && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            func_sig = func_it->second;
                            break;
                        }
                    }
                }
            }
            if (func_sig) {
                // Generate call to impl method: @tml_TypeName_MethodName(this_ptr, args...)
                std::string fn_name = "@tml_" + named2.name + "_" + method;

                // Get receiver pointer (not the loaded value)
                // For identifiers, use the alloca directly
                std::string impl_receiver_ptr;
                if (call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        // If the variable stores a pointer (like 'this' parameter),
                        // we need to load it to get the actual pointer value
                        if (it->second.type == "ptr") {
                            impl_receiver_ptr = receiver; // Use the loaded value
                        } else {
                            impl_receiver_ptr = it->second.reg; // Use alloca pointer directly
                        }
                    } else {
                        impl_receiver_ptr = receiver; // Fall back to generated value
                    }
                } else {
                    // For other expressions, receiver is already a pointer
                    impl_receiver_ptr = receiver;
                }

                // Build argument list: self (receiver ptr) + args
                std::vector<std::pair<std::string, std::string>> typed_args;
                typed_args.push_back({"ptr", impl_receiver_ptr}); // self reference

                // Get parameter types from function signature
                // params[0] is 'this', so real args start at index 1
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    // Get type from func_sig if available (params[i+1] because params[0] is 'this')
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                    }
                    typed_args.push_back({arg_type, val});
                }

                // Get return type
                std::string ret_type = llvm_type_from_semantic(func_sig->return_type);

                // Build call
                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type; // Set expression type for when/match expressions
                    return result;
                }
            }
        } // close else block for builtin type check
    }

    // Handle dyn dispatch: call method through vtable
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end() && it->second.type.starts_with("%dyn.")) {
            std::string dyn_type = it->second.type;
            std::string behavior_name = dyn_type.substr(5); // Skip "%dyn."
            std::string dyn_ptr = it->second.reg;

            // Get method index in vtable
            auto behavior_methods_it = behavior_method_order_.find(behavior_name);
            if (behavior_methods_it != behavior_method_order_.end()) {
                const auto& methods = behavior_methods_it->second;
                int method_idx = -1;
                for (size_t i = 0; i < methods.size(); ++i) {
                    if (methods[i] == method) {
                        method_idx = static_cast<int>(i);
                        break;
                    }
                }

                if (method_idx >= 0) {
                    // Load data pointer from fat pointer (field 0)
                    std::string data_field = fresh_reg();
                    emit_line("  " + data_field + " = getelementptr " + dyn_type + ", ptr " +
                              dyn_ptr + ", i32 0, i32 0");
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = load ptr, ptr " + data_field);

                    // Load vtable pointer from fat pointer (field 1)
                    std::string vtable_field = fresh_reg();
                    emit_line("  " + vtable_field + " = getelementptr " + dyn_type + ", ptr " +
                              dyn_ptr + ", i32 0, i32 1");
                    std::string vtable_ptr = fresh_reg();
                    emit_line("  " + vtable_ptr + " = load ptr, ptr " + vtable_field);

                    // Get function pointer from vtable
                    std::string fn_ptr_loc = fresh_reg();
                    emit_line("  " + fn_ptr_loc + " = getelementptr { ptr }, ptr " + vtable_ptr +
                              ", i32 0, i32 " + std::to_string(method_idx));
                    std::string fn_ptr = fresh_reg();
                    emit_line("  " + fn_ptr + " = load ptr, ptr " + fn_ptr_loc);

                    // Call through function pointer (assumes method takes self and returns i32)
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call i32 " + fn_ptr + "(ptr " + data_ptr + ")");
                    return result;
                }
            }
        }
    }

    // File instance methods - use receiver_ptr (alloca) instead of receiver (loaded value)
    // For getelementptr, we need a pointer, not a loaded struct value
    if (method == "is_open" || method == "read_line" || method == "write_str" || method == "size" ||
        method == "close") {
        // Get pointer to the File struct (either from alloca or create temp)
        std::string file_ptr = receiver_ptr;
        if (file_ptr.empty()) {
            // Receiver is a complex expression - store to temp alloca
            file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            emit_line("  store %struct.File " + receiver + ", ptr " + file_ptr);
        }

        // Get handle field from File struct
        std::string handle_field_ptr = fresh_reg();
        emit_line("  " + handle_field_ptr + " = getelementptr %struct.File, ptr " + file_ptr +
                  ", i32 0, i32 0");
        std::string handle = fresh_reg();
        emit_line("  " + handle + " = load ptr, ptr " + handle_field_ptr);

        if (method == "is_open") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_is_open(ptr " + handle + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "read_line") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @file_read_line(ptr " + handle + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "write_str") {
            if (call.args.empty()) {
                report_error("write_str requires a content argument", call.span);
                return "0";
            }
            std::string content_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_write_str(ptr " + handle + ", ptr " +
                      content_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "size") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @file_size(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "close") {
            emit_line("  call void @file_close(ptr " + handle + ")");
            return "void";
        }
    }

    report_error("Unknown method: " + method, call.span);
    return "0";
}

} // namespace tml::codegen
