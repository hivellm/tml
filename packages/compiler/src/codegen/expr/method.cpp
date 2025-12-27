// LLVM IR generator - Method calls
// Handles: gen_method_call for all method dispatch (static, instance, ptr, dyn)

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

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
        bool is_type_name = struct_types_.count(type_name) > 0 || type_name == "List" ||
                            type_name == "HashMap" || type_name == "Buffer" ||
                            type_name == "File" || type_name == "Path";

        // Also check it's not a local variable
        if (is_type_name && locals_.count(type_name) == 0) {
            // This is a static method call on a type

            // List static methods
            if (type_name == "List") {
                if (method == "new") {
                    std::string cap = call.args.empty() ? "8" : gen_expr(*call.args[0]);
                    // Convert i32 to i64 if needed
                    std::string cap_i64 = fresh_reg();
                    emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @list_create(i64 " + cap_i64 + ")");
                    last_expr_type_ = "ptr";
                    return result;
                }
                if (method == "default") {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @list_create(i64 8)");
                    last_expr_type_ = "ptr";
                    return result;
                }
            }

            // HashMap static methods
            if (type_name == "HashMap") {
                if (method == "new") {
                    std::string cap = call.args.empty() ? "16" : gen_expr(*call.args[0]);
                    std::string cap_i64 = fresh_reg();
                    emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @hashmap_create(i64 " + cap_i64 + ")");
                    last_expr_type_ = "ptr";
                    return result;
                }
                if (method == "default") {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @hashmap_create(i64 16)");
                    last_expr_type_ = "ptr";
                    return result;
                }
            }

            // Buffer static methods
            if (type_name == "Buffer") {
                if (method == "new") {
                    std::string cap = call.args.empty() ? "64" : gen_expr(*call.args[0]);
                    std::string cap_i64 = fresh_reg();
                    emit_line("  " + cap_i64 + " = sext i32 " + cap + " to i64");
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @buffer_create(i64 " + cap_i64 + ")");
                    last_expr_type_ = "ptr";
                    return result;
                }
                if (method == "default") {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call ptr @buffer_create(i64 64)");
                    last_expr_type_ = "ptr";
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

    // Determine receiver type name for type-aware method dispatch
    std::string receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        receiver_type_name = receiver_type->as<types::NamedType>().name;
    }

    // Type-aware len method - calls different runtime functions based on type
    if (method == "len" || method == "length") {
        if (receiver_type_name == "HashMap") {
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @hashmap_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        if (receiver_type_name == "Buffer") {
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @buffer_len(ptr " + receiver + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        // Default: List
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_len(ptr " + receiver + ")");
        return result;
    }

    // Check if there's a user-defined impl method BEFORE falling through to built-ins
    // This allows user types to define methods like get(), push(), etc.
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        std::string qualified_name = named.name + "::" + method;
        auto func_sig = env_.lookup_func(qualified_name);
        if (func_sig) {
            // Generate call to impl method: @tml_TypeName_MethodName(this_ptr, args...)
            std::string fn_name = "@tml_" + named.name + "_" + method;

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

            for (const auto& arg : call.args) {
                std::string val = gen_expr(*arg);
                std::string arg_type = "i32"; // default
                typed_args.push_back({arg_type, val});
            }

            std::string ret_type = llvm_type_from_semantic(func_sig->return_type);

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
                emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str +
                          ")");
                last_expr_type_ = ret_type;
                return result;
            }
        }
    }

    // List methods (only for List type)
    if (receiver_type_name == "List" && method == "push") {
        if (call.args.empty()) {
            report_error("push requires an argument", call.span);
            return "0";
        }
        std::string val = gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_push(ptr " + receiver + ", i32 " + val + ")");
        return result;
    }
    if (receiver_type_name == "List" && method == "pop") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_pop(ptr " + receiver + ")");
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
            emit_line("  " + result_i64 + " = call i64 @hashmap_get(ptr " + receiver + ", i64 " +
                      key_i64 + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        // Default: List - get(index: I32) -> I32
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_get(ptr " + receiver + ", i32 " + arg + ")");
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
            emit_line("  call void @hashmap_set(ptr " + receiver + ", i64 " + key_i64 + ", i64 " +
                      val_i64 + ")");
            return "void";
        }
        // Default: List - set(index: I32, value: I32) -> Unit
        emit_line("  call void @list_set(ptr " + receiver + ", i32 " + arg1 + ", i32 " + arg2 +
                  ")");
        return "void";
    }
    if (method == "clear") {
        emit_line("  call void @list_clear(ptr " + receiver + ")");
        return "void";
    }
    if (method == "is_empty" || method == "isEmpty") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i1 @list_is_empty(ptr " + receiver + ")");
        return result;
    }
    if (method == "capacity") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @list_capacity(ptr " + receiver + ")");
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
        emit_line("  " + result + " = call i1 @hashmap_has(ptr " + receiver + ", i64 " + key_i64 +
                  ")");
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
        emit_line("  " + result + " = call i1 @hashmap_remove(ptr " + receiver + ", i64 " +
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
        emit_line("  call void @buffer_write_byte(ptr " + receiver + ", i32 " + val + ")");
        return "void";
    }
    if (method == "read_byte") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + receiver + ")");
        return result;
    }
    if (method == "remaining") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_remaining(ptr " + receiver + ")");
        return result;
    }
    if (method == "write_i32") {
        if (call.args.empty()) {
            report_error("write_i32 requires a value argument", call.span);
            return "void";
        }
        std::string val = gen_expr(*call.args[0]);
        emit_line("  call void @buffer_write_i32(ptr " + receiver + ", i32 " + val + ")");
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
        emit_line("  call void @buffer_write_i64(ptr " + receiver + ", i64 " + val_i64 + ")");
        return "void";
    }
    if (method == "read_i32") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + receiver + ")");
        return result;
    }
    if (method == "read_i64") {
        std::string result_i64 = fresh_reg();
        emit_line("  " + result_i64 + " = call i64 @buffer_read_i64(ptr " + receiver + ")");
        // Truncate to i32 for now
        std::string result = fresh_reg();
        emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
        return result;
    }
    if (method == "reset_read") {
        emit_line("  call void @buffer_reset_read(ptr " + receiver + ")");
        return "void";
    }
    if (method == "destroy") {
        // Type-aware destroy - different for List, HashMap, Buffer
        if (receiver_type_name == "HashMap") {
            emit_line("  call void @hashmap_destroy(ptr " + receiver + ")");
        } else if (receiver_type_name == "Buffer") {
            emit_line("  call void @buffer_destroy(ptr " + receiver + ")");
        } else {
            // Default: List
            emit_line("  call void @list_destroy(ptr " + receiver + ")");
        }
        return "void";
    }

    // Try to find impl method using type inference
    // receiver_type already computed above for Ptr handling
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        std::string qualified_name = named.name + "::" + method;
        auto func_sig = env_.lookup_func(qualified_name);

        // If not found locally, try looking in the module the type is from
        if (!func_sig) {
            std::string module_path = named.module_path;

            // If module_path is empty, try to resolve via imported symbol
            if (module_path.empty()) {
                auto import_path = env_.resolve_imported_symbol(named.name);
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
        }
        if (func_sig) {
            // Generate call to impl method: @tml_TypeName_MethodName(this_ptr, args...)
            std::string fn_name = "@tml_" + named.name + "_" + method;

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

            for (size_t i = 0; i < call.args.size(); ++i) {
                std::string val = gen_expr(*call.args[i]);
                // Get arg type from func signature (skip 'this' at index 0)
                std::string arg_type = "i32"; // default
                if (i + 1 < func_sig->params.size()) {
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
                emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str +
                          ")");
                last_expr_type_ = ret_type; // Set expression type for when/match expressions
                return result;
            }
        }
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
