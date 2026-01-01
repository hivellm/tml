// LLVM IR generator - Method calls dispatcher
// Main entry point for method dispatch. Delegates to specialized handlers:
// - method_static.cpp: Type::method() static calls
// - method_primitive.cpp: Integer, Float, Bool methods
// - method_collection.cpp: List, HashMap, Buffer methods
// - method_slice.cpp: Slice, MutSlice methods
// - method_maybe.cpp: Maybe[T] methods
// - method_outcome.cpp: Outcome[T,E] methods
// - method_array.cpp: Array[T; N] methods

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>

namespace tml::codegen {

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    const std::string& method = call.method;

    // =========================================================================
    // 1. Check for static method calls (Type::method)
    // =========================================================================
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
        bool is_type_name =
            struct_types_.count(type_name) > 0 || type_name == "List" || type_name == "HashMap" ||
            type_name == "Buffer" || type_name == "File" || type_name == "Path" ||
            type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128" || type_name == "F32" || type_name == "F64" ||
            type_name == "Bool" || type_name == "Str";

        if (is_type_name && locals_.count(type_name) == 0) {
            auto result = gen_static_method_call(call, type_name);
            if (result) {
                return *result;
            }
            report_error("Unknown static method: " + type_name + "." + method, call.span);
            return "0";
        }
    }

    // =========================================================================
    // 2. Check for array methods first (before generating receiver)
    // =========================================================================
    auto array_result = gen_array_method(call, method);
    if (array_result.has_value()) {
        return *array_result;
    }

    // =========================================================================
    // 3. Generate receiver and get receiver pointer
    // =========================================================================
    std::string receiver = gen_expr(*call.receiver);
    std::string receiver_ptr;
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            receiver_ptr = it->second.reg;
        }
    }

    // =========================================================================
    // 4. Get receiver type info
    // =========================================================================
    types::TypePtr receiver_type = infer_expr_type(*call.receiver);
    std::string receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        receiver_type_name = receiver_type->as<types::NamedType>().name;
    }

    // =========================================================================
    // 5. Handle Ptr[T] methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::PtrType>()) {
        const auto& ptr_type = receiver_type->as<types::PtrType>();
        std::string inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);

        if (method == "read") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + receiver);
            last_expr_type_ = inner_llvm_type;
            return result;
        }
        if (method == "write") {
            if (call.args.empty()) {
                report_error("Ptr.write() requires a value argument", call.span);
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  store " + inner_llvm_type + " " + val + ", ptr " + receiver);
            return "void";
        }
        if (method == "offset") {
            if (call.args.empty()) {
                report_error("Ptr.offset() requires an offset argument", call.span);
                return receiver;
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_i64 = fresh_reg();
            emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + inner_llvm_type + ", ptr " + receiver +
                      ", i64 " + offset_i64);
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "is_null") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq ptr " + receiver + ", null");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 6. Handle primitive type methods
    // =========================================================================
    auto prim_result = gen_primitive_method(call, receiver, receiver_ptr, receiver_type);
    if (prim_result) {
        return *prim_result;
    }

    // =========================================================================
    // 7. Handle Ordering enum methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (named.name == "Ordering") {
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue %struct.Ordering " + receiver + ", 0");

            if (method == "is_less") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_equal") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_greater") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 2");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "reverse") {
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_greater = fresh_reg();
                emit_line("  " + is_greater + " = icmp eq i32 " + tag_val + ", 2");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_greater + ", i32 0, i32 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_less + ", i32 2, i32 " + sel1);
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }
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
            if (method == "to_string") {
                std::string less_str = add_string_literal("Less");
                std::string equal_str = add_string_literal("Equal");
                std::string greater_str = add_string_literal("Greater");
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
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + receiver +
                      ", 0");
            auto result = gen_maybe_method(call, receiver, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + receiver +
                      ", 0");
            auto result = gen_outcome_method(call, receiver, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }
    }

    // =========================================================================
    // 8. Handle Slice/MutSlice methods
    // =========================================================================
    auto slice_result = gen_slice_method(call, receiver, receiver_type_name, receiver_type);
    if (slice_result) {
        return *slice_result;
    }

    // =========================================================================
    // 9. Handle collection methods (List, HashMap, Buffer)
    // =========================================================================
    auto coll_result = gen_collection_method(call, receiver, receiver_type_name, receiver_type);
    if (coll_result) {
        return *coll_result;
    }

    // =========================================================================
    // 10. Check for user-defined impl methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        bool is_builtin_type =
            (named.name == "List" || named.name == "HashMap" || named.name == "Buffer" ||
             named.name == "File" || named.name == "Path");
        bool is_slice_inlined = (named.name == "Slice" || named.name == "MutSlice") &&
                                (method == "len" || method == "is_empty");

        if (!is_builtin_type && !is_slice_inlined) {
            std::string qualified_name = named.name + "::" + method;
            auto func_sig = env_.lookup_func(qualified_name);
            if (func_sig) {
                std::string mangled_type_name = named.name;
                std::unordered_map<std::string, types::TypePtr> type_subs;

                if (!named.type_args.empty()) {
                    mangled_type_name = mangle_struct_name(named.name, named.type_args);
                    std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method;

                    auto impl_it = pending_generic_impls_.find(named.name);
                    if (impl_it != pending_generic_impls_.end()) {
                        const auto& impl = *impl_it->second;
                        for (size_t i = 0; i < impl.generics.size() && i < named.type_args.size();
                             ++i) {
                            type_subs[impl.generics[i].name] = named.type_args[i];
                        }
                    }

                    if (generated_impl_methods_.find(mangled_method_name) ==
                        generated_impl_methods_.end()) {
                        if (impl_it != pending_generic_impls_.end()) {
                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, type_subs, named.name});
                            generated_impl_methods_.insert(mangled_method_name);
                        }
                    }
                }

                std::string fn_name = "@tml_" + mangled_type_name + "_" + method;
                std::string impl_receiver_ptr;

                if (call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        impl_receiver_ptr = (it->second.type == "ptr") ? receiver : it->second.reg;
                    } else {
                        impl_receiver_ptr = receiver;
                    }
                } else if (last_expr_type_.starts_with("%struct.")) {
                    std::string tmp = fresh_reg();
                    emit_line("  " + tmp + " = alloca " + last_expr_type_);
                    emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " + tmp);
                    impl_receiver_ptr = tmp;
                } else {
                    impl_receiver_ptr = receiver;
                }

                std::vector<std::pair<std::string, std::string>> typed_args;
                typed_args.push_back({"ptr", impl_receiver_ptr});

                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32";
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        auto param_type = func_sig->params[i + 1];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        arg_type = llvm_type_from_semantic(param_type);
                    }
                    typed_args.push_back({arg_type, val});
                }

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
        }
    }

    // =========================================================================
    // 11. Try module lookup for impl methods
    // =========================================================================
    TML_DEBUG_LN("[METHOD] receiver_type for method '"
                 << method << "': " << (receiver_type ? "exists" : "null"));
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named2 = receiver_type->as<types::NamedType>();
        bool is_builtin_type2 =
            (named2.name == "List" || named2.name == "HashMap" || named2.name == "Buffer" ||
             named2.name == "File" || named2.name == "Path");
        if (!is_builtin_type2) {
            std::string qualified_name = named2.name + "::" + method;
            TML_DEBUG_LN("[METHOD] Looking for impl method: " << qualified_name);
            auto func_sig = env_.lookup_func(qualified_name);

            if (!func_sig) {
                std::string module_path = named2.module_path;
                if (module_path.empty()) {
                    auto import_path = env_.resolve_imported_symbol(named2.name);
                    if (import_path) {
                        auto pos = import_path->rfind("::");
                        if (pos != std::string::npos) {
                            module_path = import_path->substr(0, pos);
                        }
                    }
                }
                if (!module_path.empty()) {
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto func_it = module->functions.find(qualified_name);
                        if (func_it != module->functions.end()) {
                            func_sig = func_it->second;
                        }
                    }
                }
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
                std::string fn_name = "@tml_" + named2.name + "_" + method;
                std::string impl_receiver_ptr;

                if (call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        impl_receiver_ptr = (it->second.type == "ptr") ? receiver : it->second.reg;
                    } else {
                        impl_receiver_ptr = receiver;
                    }
                } else if (last_expr_type_.starts_with("%struct.")) {
                    std::string tmp = fresh_reg();
                    emit_line("  " + tmp + " = alloca " + last_expr_type_);
                    emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " + tmp);
                    impl_receiver_ptr = tmp;
                } else {
                    impl_receiver_ptr = receiver;
                }

                std::vector<std::pair<std::string, std::string>> typed_args;
                typed_args.push_back({"ptr", impl_receiver_ptr});

                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32";
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                    }
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
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }
    }

    // =========================================================================
    // 12. Handle dyn dispatch
    // =========================================================================
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end() && it->second.type.starts_with("%dyn.")) {
            std::string dyn_type = it->second.type;
            std::string behavior_name = dyn_type.substr(5);
            std::string dyn_ptr = it->second.reg;

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
                    std::string data_field = fresh_reg();
                    emit_line("  " + data_field + " = getelementptr " + dyn_type + ", ptr " +
                              dyn_ptr + ", i32 0, i32 0");
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = load ptr, ptr " + data_field);

                    std::string vtable_field = fresh_reg();
                    emit_line("  " + vtable_field + " = getelementptr " + dyn_type + ", ptr " +
                              dyn_ptr + ", i32 0, i32 1");
                    std::string vtable_ptr = fresh_reg();
                    emit_line("  " + vtable_ptr + " = load ptr, ptr " + vtable_field);

                    // Build vtable struct type based on number of methods
                    std::string vtable_type = "{ ";
                    for (size_t i = 0; i < methods.size(); ++i) {
                        if (i > 0)
                            vtable_type += ", ";
                        vtable_type += "ptr";
                    }
                    vtable_type += " }";

                    std::string fn_ptr_loc = fresh_reg();
                    emit_line("  " + fn_ptr_loc + " = getelementptr " + vtable_type + ", ptr " +
                              vtable_ptr + ", i32 0, i32 " + std::to_string(method_idx));
                    std::string fn_ptr = fresh_reg();
                    emit_line("  " + fn_ptr + " = load ptr, ptr " + fn_ptr_loc);

                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call i32 " + fn_ptr + "(ptr " + data_ptr + ")");
                    last_expr_type_ = "i32";
                    return result;
                }
            }
        }
    }

    // =========================================================================
    // 13. Handle File instance methods
    // =========================================================================
    if (method == "is_open" || method == "read_line" || method == "write_str" || method == "size" ||
        method == "close") {
        std::string file_ptr = receiver_ptr;
        if (file_ptr.empty()) {
            file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            emit_line("  store %struct.File " + receiver + ", ptr " + file_ptr);
        }

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
