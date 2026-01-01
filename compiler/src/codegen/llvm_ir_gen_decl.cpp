// LLVM IR generator - Declaration generation
// Handles: struct, enum, function declarations

#include "codegen/llvm_ir_gen.hpp"
#include "types/type.hpp"

#include <functional>
#include <sstream>

namespace tml::codegen {

void LLVMIRGen::gen_struct_decl(const parser::StructDecl& s) {
    // If struct has generic parameters, defer generation until instantiated
    if (!s.generics.empty()) {
        pending_generic_structs_[s.name] = &s;
        return;
    }

    // Skip builtin types that are already declared in the runtime
    if (s.name == "File" || s.name == "Path" || s.name == "Ordering") {
        // Register field info for builtin structs but don't emit type definition
        std::string type_name = "%struct." + s.name;
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < s.fields.size(); ++i) {
            std::string ft = llvm_type_ptr(s.fields[i].type);
            fields.push_back({s.fields[i].name, static_cast<int>(i), ft});
        }
        struct_types_[s.name] = type_name;
        struct_fields_[s.name] = fields;
        return;
    }

    // Non-generic struct: generate immediately
    std::string type_name = "%struct." + s.name;

    // Collect field types and register field info
    std::vector<std::string> field_types;
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < s.fields.size(); ++i) {
        std::string ft = llvm_type_ptr(s.fields[i].type);
        field_types.push_back(ft);
        fields.push_back({s.fields[i].name, static_cast<int>(i), ft});
    }

    // Emit struct type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    emit_line(def);

    // Register for later use
    struct_types_[s.name] = type_name;
    struct_fields_[s.name] = fields;
}

// Generate a specialized version of a generic struct
void LLVMIRGen::gen_struct_instantiation(const parser::StructDecl& decl,
                                         const std::vector<types::TypePtr>& type_args) {
    // 1. Create substitution map: T -> I32, K -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < decl.generics.size() && i < type_args.size(); ++i) {
        subs[decl.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled name: Pair[I32] -> Pair__I32
    std::string mangled = mangle_struct_name(decl.name, type_args);
    std::string type_name = "%struct." + mangled;

    // 3. Collect field types with substitution and register field info
    std::vector<std::string> field_types;
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < decl.fields.size(); ++i) {
        // Resolve field type and apply substitution
        types::TypePtr field_type = resolve_parser_type_with_subs(*decl.fields[i].type, subs);
        std::string ft = llvm_type_from_semantic(field_type);
        field_types.push_back(ft);
        fields.push_back({decl.fields[i].name, static_cast<int>(i), ft});
    }

    // 4. Emit struct type definition to type_defs_buffer_ (ensures types before functions)
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    type_defs_buffer_ << def << "\n";

    // 5. Register for later use
    struct_types_[mangled] = type_name;
    struct_fields_[mangled] = fields;
}

// Request instantiation of a generic struct - returns mangled name
// Immediately generates the type definition to type_defs_buffer_ if not already generated
auto LLVMIRGen::require_struct_instantiation(const std::string& base_name,
                                             const std::vector<types::TypePtr>& type_args)
    -> std::string {
    // Generate mangled name
    std::string mangled = mangle_struct_name(base_name, type_args);

    // Check if already registered
    auto it = struct_instantiations_.find(mangled);
    if (it != struct_instantiations_.end()) {
        return mangled; // Already queued or generated
    }

    // Register new instantiation (mark as generated since we'll generate immediately)
    struct_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        true // Mark as generated since we'll generate it immediately
    };

    // Register field info and generate type definition immediately
    auto decl_it = pending_generic_structs_.find(base_name);
    if (decl_it != pending_generic_structs_.end()) {
        const parser::StructDecl* decl = decl_it->second;

        // Create substitution map
        std::unordered_map<std::string, types::TypePtr> subs;
        for (size_t i = 0; i < decl->generics.size() && i < type_args.size(); ++i) {
            subs[decl->generics[i].name] = type_args[i];
        }

        // Register field info
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            types::TypePtr field_type = resolve_parser_type_with_subs(*decl->fields[i].type, subs);
            std::string ft = llvm_type_from_semantic(field_type);
            fields.push_back({decl->fields[i].name, static_cast<int>(i), ft});
        }
        struct_fields_[mangled] = fields;

        // Generate type definition immediately to type_defs_buffer_
        gen_struct_instantiation(*decl, type_args);
    }

    return mangled;
}

void LLVMIRGen::gen_enum_decl(const parser::EnumDecl& e) {
    // If enum has generic parameters, defer generation until instantiated
    if (!e.generics.empty()) {
        pending_generic_enums_[e.name] = &e;
        return;
    }

    // TML enums are represented as tagged unions
    // For now, simple enums are just integers (tag only)
    // Complex enums with data would need { i32, union_data }

    bool has_data = false;
    for (const auto& variant : e.variants) {
        if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        // Simple enum - represented as struct with single i32 tag field
        std::string type_name = "%struct." + e.name;
        emit_line(type_name + " = type { i32 }");
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    } else {
        // Complex enum with data - create tagged union
        // Find the largest variant
        size_t max_size = 0;
        for (const auto& variant : e.variants) {
            size_t size = 0;
            if (variant.tuple_fields.has_value()) {
                for (const auto& field_type : *variant.tuple_fields) {
                    // Approximate size
                    std::string ty = llvm_type_ptr(field_type);
                    if (ty == "i8")
                        size += 1;
                    else if (ty == "i16")
                        size += 2;
                    else if (ty == "i32" || ty == "float")
                        size += 4;
                    else if (ty == "i64" || ty == "double" || ty == "ptr")
                        size += 8;
                    else
                        size += 8; // Default
                }
            }
            if (variant.struct_fields.has_value()) {
                for (const auto& field : *variant.struct_fields) {
                    std::string ty = llvm_type_ptr(field.type);
                    if (ty == "i8")
                        size += 1;
                    else if (ty == "i16")
                        size += 2;
                    else if (ty == "i32" || ty == "float")
                        size += 4;
                    else if (ty == "i64" || ty == "double" || ty == "ptr")
                        size += 8;
                    else
                        size += 8; // Default
                }
            }
            max_size = std::max(max_size, size);
        }

        // Emit the enum type
        std::string type_name = "%struct." + e.name;
        // { i32 tag, [N x i8] data }
        emit_line(type_name + " = type { i32, [" + std::to_string(max_size) + " x i8] }");
        struct_types_[e.name] = type_name;

        // Register variant values
        int tag = 0;
        for (const auto& variant : e.variants) {
            std::string key = e.name + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }
}

// Generate a specialized version of a generic enum
void LLVMIRGen::gen_enum_instantiation(const parser::EnumDecl& decl,
                                       const std::vector<types::TypePtr>& type_args) {
    // 1. Create substitution map: T -> I32, K -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < decl.generics.size() && i < type_args.size(); ++i) {
        subs[decl.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled name: Maybe[I32] -> Maybe__I32
    std::string mangled = mangle_struct_name(decl.name, type_args);
    std::string type_name = "%struct." + mangled;

    // Check if this type has already been emitted
    if (struct_types_.find(mangled) != struct_types_.end()) {
        // Type already emitted, skip
        return;
    }

    // Check if any variant has data
    bool has_data = false;
    for (const auto& variant : decl.variants) {
        if (variant.tuple_fields.has_value() || variant.struct_fields.has_value()) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        // Simple enum - just a tag
        // Emit to type_defs_buffer_ instead of output_ to ensure type is defined before use
        type_defs_buffer_ << type_name << " = type { i32 }\n";
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    } else {
        // Complex enum with data
        // Calculate max size with substituted types
        // Use for_data=true since these are data fields (Unit -> "{}" not "void")

        // Helper lambda to calculate size of an LLVM type
        std::function<size_t(const std::string&)> calc_type_size;
        calc_type_size = [this, &calc_type_size](const std::string& ty) -> size_t {
            if (ty == "{}" || ty == "void")
                return 0; // Unit type has zero size
            if (ty == "i8")
                return 1;
            if (ty == "i16")
                return 2;
            if (ty == "i32" || ty == "float" || ty == "i1")
                return 4;
            if (ty == "i64" || ty == "double" || ty == "ptr")
                return 8;
            if (ty == "i128")
                return 16;
            // Check if it's an anonymous struct/tuple type like "{ %struct.Layout, i64 }"
            if (ty.starts_with("{ ") && ty.ends_with(" }")) {
                std::string inner = ty.substr(2, ty.size() - 4); // Remove "{ " and " }"
                size_t tuple_size = 0;
                size_t pos = 0;
                while (pos < inner.size()) {
                    // Find the next element (separated by ", ")
                    size_t next_comma = inner.find(", ", pos);
                    std::string elem = (next_comma == std::string::npos)
                                           ? inner.substr(pos)
                                           : inner.substr(pos, next_comma - pos);
                    tuple_size += calc_type_size(elem);
                    if (next_comma == std::string::npos)
                        break;
                    pos = next_comma + 2;
                }
                return tuple_size > 0 ? tuple_size : 8;
            }
            // Check if it's a struct type
            if (ty.starts_with("%struct.")) {
                std::string struct_name = ty.substr(8); // Remove "%struct."
                auto it = struct_fields_.find(struct_name);
                if (it != struct_fields_.end()) {
                    size_t struct_size = 0;
                    for (const auto& field : it->second) {
                        struct_size += calc_type_size(field.llvm_type);
                    }
                    return struct_size > 0 ? struct_size : 8;
                }
            }
            return 8; // Default size
        };

        size_t max_size = 0;
        for (const auto& variant : decl.variants) {
            size_t size = 0;
            if (variant.tuple_fields.has_value()) {
                for (const auto& field_type : *variant.tuple_fields) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*field_type, subs);
                    std::string ty = llvm_type_from_semantic(resolved, true);
                    size += calc_type_size(ty);
                }
            }
            if (variant.struct_fields.has_value()) {
                for (const auto& field : *variant.struct_fields) {
                    types::TypePtr resolved = resolve_parser_type_with_subs(*field.type, subs);
                    std::string ty = llvm_type_from_semantic(resolved, true);
                    size += calc_type_size(ty);
                }
            }
            max_size = std::max(max_size, size);
        }

        // Ensure at least 8 bytes for data
        if (max_size == 0)
            max_size = 8;

        // Calculate alignment needed for payload (max alignment of any variant)
        // Use [N x i64] instead of [N x i8] to ensure 8-byte alignment of the payload data.
        // This is necessary because payloads may contain i64/double/structs that require
        // 8-byte alignment. Using i8 array would place data at offset 4 after the i32 tag,
        // causing alignment issues.
        //
        // We round up max_size to the nearest multiple of 8, then divide by 8 to get
        // the number of i64 elements needed.
        size_t num_i64 = (max_size + 7) / 8;
        type_defs_buffer_ << type_name << " = type { i32, [" << std::to_string(num_i64)
                          << " x i64] }\n";
        struct_types_[mangled] = type_name;

        int tag = 0;
        for (const auto& variant : decl.variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }
    }
}

// Helper to extract name from FuncParam pattern
static std::string get_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

// Pre-register function signature without generating code
// This ensures intra-module calls resolve correctly before any code is generated
void LLVMIRGen::pre_register_func(const parser::FuncDecl& func) {
    // Skip generic functions - they are instantiated on demand
    if (!func.generics.empty()) {
        return;
    }

    // Skip @extern functions - they're handled in gen_func_decl
    if (func.extern_abi.has_value()) {
        return;
    }

    // Build return type
    std::string ret_type = "i32"; // Default
    if (func.return_type.has_value()) {
        std::string inner_ret_type = llvm_type_ptr(*func.return_type);
        if (func.is_async && inner_ret_type != "void") {
            // Async functions return Poll[T]
            auto semantic_ret = resolve_parser_type_with_subs(**func.return_type, {});
            std::vector<types::TypePtr> poll_type_args = {semantic_ret};
            std::string poll_mangled = require_enum_instantiation("Poll", poll_type_args);
            ret_type = "%struct." + poll_mangled;
        } else {
            ret_type = inner_ret_type;
        }
    } else if (func.is_async) {
        // async func with no return type -> Poll[Unit]
        std::vector<types::TypePtr> poll_type_args = {
            std::make_shared<types::Type>(types::PrimitiveType{types::PrimitiveKind::Unit})};
        std::string poll_mangled = require_enum_instantiation("Poll", poll_type_args);
        ret_type = "%struct." + poll_mangled;
    }

    // Build parameter type list
    std::string param_types;
    std::vector<std::string> param_types_vec;
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(func.params[i].type);
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Build function name with module prefix
    std::string full_func_name = func.name;
    if (!current_module_prefix_.empty()) {
        full_func_name = current_module_prefix_ + "_" + func.name;
    }

    // Register function in functions_ map
    std::string func_type = ret_type + " (" + param_types + ")";
    FuncInfo func_info{"@tml_" + full_func_name, func_type, ret_type, param_types_vec};
    functions_[func.name] = func_info;

    // Register with module-qualified name for cross-module calls
    if (!current_module_prefix_.empty()) {
        // Convert prefix to :: format (core_unicode -> core::unicode)
        std::string qualified_name = current_module_prefix_;
        size_t pos = 0;
        while ((pos = qualified_name.find("_", pos)) != std::string::npos) {
            qualified_name.replace(pos, 1, "::");
            pos += 2;
        }
        qualified_name += "::" + func.name;
        functions_[qualified_name] = func_info;

        // Also register with short key (e.g., "unicode::is_alphabetic")
        size_t last_sep = qualified_name.rfind("::");
        if (last_sep != std::string::npos) {
            std::string without_func = qualified_name.substr(0, last_sep);
            size_t second_last_sep = without_func.rfind("::");
            if (second_last_sep != std::string::npos) {
                std::string short_key = qualified_name.substr(second_last_sep + 2);
                functions_[short_key] = func_info;
            }
        }

        // Register with submodule name (e.g., "unicode_data::is_alphabetic_nonascii")
        if (!current_submodule_name_.empty() && current_submodule_name_ != "mod") {
            std::string submod_key = current_submodule_name_ + "::" + func.name;
            functions_[submod_key] = func_info;
        }
    }
}

void LLVMIRGen::gen_func_decl(const parser::FuncDecl& func) {
    // Defer generic functions - they will be instantiated when called
    if (!func.generics.empty()) {
        pending_generic_funcs_[func.name] = &func;
        return;
    }

    // Determine return type
    std::string ret_type = "void";
    std::string inner_ret_type = "void"; // For async functions, the unwrapped return type
    types::TypePtr semantic_ret = nullptr;
    if (func.return_type.has_value()) {
        inner_ret_type = llvm_type_ptr(*func.return_type);
        semantic_ret = resolve_parser_type_with_subs(**func.return_type, {});
    }

    // Async functions return Poll[T] instead of T
    // Poll[T] = { i32 tag, T data } where tag 0 = Ready, tag 1 = Pending
    if (func.is_async && inner_ret_type != "void") {
        // Use the semantic return type to create Poll[T]
        if (!semantic_ret) {
            semantic_ret =
                std::make_shared<types::Type>(types::PrimitiveType{types::PrimitiveKind::Unit});
        }
        std::vector<types::TypePtr> poll_type_args = {semantic_ret};
        std::string poll_mangled = require_enum_instantiation("Poll", poll_type_args);
        ret_type = "%struct." + poll_mangled;
        current_poll_type_ = ret_type;
        current_poll_inner_type_ = inner_ret_type; // Store inner type for wrap_in_poll_ready

        // For async functions, record Poll[T] as the return type for type inference
        auto poll_type =
            std::make_shared<types::Type>(types::NamedType{"Poll", "", poll_type_args});
        func_return_types_[func.name] = poll_type;
    } else {
        ret_type = inner_ret_type;
        current_poll_type_.clear();
        current_poll_inner_type_.clear();

        // Record semantic return type for use in infer_expr_type
        if (semantic_ret) {
            func_return_types_[func.name] = semantic_ret;
        }
    }

    // Build parameter list and type list for function signature
    std::string params;
    std::string param_types;
    std::vector<std::string> param_types_vec;
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(func.params[i].type);
        std::string param_name = get_param_name(func.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
        param_types_vec.push_back(param_type);
    }

    // Handle @extern functions - emit declare instead of define
    if (func.extern_abi.has_value()) {
        // Get the actual symbol name (extern_name or func.name)
        std::string symbol_name = func.extern_name.value_or(func.name);

        // Determine calling convention based on ABI
        std::string call_conv = "";
        const std::string& abi = *func.extern_abi;
        if (abi == "stdcall") {
            call_conv = "x86_stdcallcc ";
        } else if (abi == "fastcall") {
            call_conv = "x86_fastcallcc ";
        } else if (abi == "thiscall") {
            call_conv = "x86_thiscallcc ";
        }
        // "c" and "c++" use default calling convention (no prefix)

        // Emit external declaration
        emit_line("");
        emit_line("; @extern(\"" + abi + "\") " + func.name);
        emit_line("declare " + call_conv + ret_type + " @" + symbol_name + "(" + param_types + ")");

        // Register function - map TML name to external symbol
        std::string func_type = ret_type + " (" + param_types + ")";
        functions_[func.name] = FuncInfo{"@" + symbol_name, func_type, ret_type, param_types_vec};

        // Store link libraries for later (linker phase)
        for (const auto& lib : func.link_libs) {
            extern_link_libs_.insert(lib);
        }

        return; // Don't generate function body for extern functions
    }

    current_func_ = func.name;
    locals_.clear();
    block_terminated_ = false;

    // Store the return type for use in gen_return
    current_ret_type_ = ret_type;
    current_func_is_async_ = func.is_async;

    // Build function name with module prefix if generating code for an imported module
    std::string full_func_name = func.name;
    if (!current_module_prefix_.empty()) {
        full_func_name = current_module_prefix_ + "_" + func.name;
    }

    // Skip if this function was already generated (handles duplicates in directory modules)
    std::string llvm_name = "@tml_" + full_func_name;
    if (generated_functions_.count(llvm_name)) {
        return;
    }
    generated_functions_.insert(llvm_name);

    // Register function for first-class function support
    std::string func_type = ret_type + " (" + param_types + ")";
    FuncInfo func_info{"@tml_" + full_func_name, func_type, ret_type, param_types_vec};
    functions_[func.name] = func_info;

    // Also register with module-qualified name for cross-module calls
    // When module A calls module B's function, the lookup uses "B::func" or "B_func"
    if (!current_module_prefix_.empty()) {
        // Register with :: separator (e.g., "core::unicode::is_grapheme_extend_nonascii")
        std::string qualified_name = current_module_prefix_;
        // Replace _ back to :: for the lookup key
        size_t pos = 0;
        while ((pos = qualified_name.find("_", pos)) != std::string::npos) {
            qualified_name.replace(pos, 1, "::");
            pos += 2;
        }
        qualified_name += "::" + func.name;
        functions_[qualified_name] = func_info;

        // Also register with just the last segment of module path
        // This allows `use core::unicode` to enable calls like `unicode::is_alphabetic`
        // From qualified_name like "core::unicode::is_alphabetic", extract "unicode::is_alphabetic"
        size_t last_sep = qualified_name.rfind("::");
        if (last_sep != std::string::npos) {
            // Find the second-to-last :: (the one before the module name)
            std::string without_func = qualified_name.substr(0, last_sep);
            size_t second_last_sep = without_func.rfind("::");
            if (second_last_sep != std::string::npos) {
                // Extract "module::func" pattern (e.g., "unicode::is_alphabetic")
                std::string short_key = qualified_name.substr(second_last_sep + 2);
                functions_[short_key] = func_info;
            }
        }

        // Also register with submodule name for `submodule::func` style calls
        // e.g., when mod.tml calls unicode_data::is_grapheme_extend, register as
        // "unicode_data::is_grapheme_extend"
        if (!current_submodule_name_.empty() && current_submodule_name_ != "mod") {
            std::string submod_key = current_submodule_name_ + "::" + func.name;
            functions_[submod_key] = func_info;
        }
    }

    // Function signature with optimization attributes
    // All user-defined functions get tml_ prefix (main becomes tml_main, wrapper @main calls it)
    std::string func_llvm_name = "tml_" + full_func_name;
    // Public functions, main, and @should_panic tests get external linkage
    // @should_panic tests need external linkage because they're called via function pointer
    bool has_should_panic = false;
    for (const auto& decorator : func.decorators) {
        if (decorator.name == "should_panic") {
            has_should_panic = true;
            break;
        }
    }
    // In suite mode (force_internal_linkage), all functions including main get internal linkage
    // to avoid duplicate symbols when linking multiple test objects into one DLL.
    // Only @should_panic tests need external linkage (called via function pointer).
    std::string linkage =
        ((!options_.force_internal_linkage && func.name == "main") ||
         (func.vis == parser::Visibility::Public && !options_.force_internal_linkage) ||
         has_should_panic)
            ? ""
            : "internal ";
    // Windows DLL export for public functions (disabled in suite mode)
    std::string dll_linkage = "";
    if (options_.dll_export && func.vis == parser::Visibility::Public && func.name != "main" &&
        !options_.force_internal_linkage) {
        dll_linkage = "dllexport ";
    }
    // Optimization attributes:
    // - nounwind: function doesn't throw exceptions
    // - mustprogress: function will eventually return (enables loop optimizations)
    // - willreturn: function will return (helps with dead code elimination)
    std::string attrs = " #0";
    emit_line("");

    // Create debug scope for function (if debug info enabled)
    int func_scope_id = 0;
    if (options_.emit_debug_info) {
        func_scope_id = create_function_debug_scope(func_llvm_name, func.span.start.line,
                                                    func.span.start.column);
        // Create a default debug location for instructions in this function
        create_debug_location(func.span.start.line, func.span.start.column);
    }

    // Add debug info as function attribute if we have a scope
    std::string dbg_attr = "";
    if (func_scope_id != 0) {
        dbg_attr = " !dbg !" + std::to_string(func_scope_id);
    }

    emit_line("define " + dll_linkage + linkage + ret_type + " @" + func_llvm_name + "(" + params +
              ")" + attrs + dbg_attr + " {");
    emit_line("entry:");

    // Register function parameters in locals_ by creating allocas
    for (size_t i = 0; i < func.params.size(); ++i) {
        std::string param_type = llvm_type_ptr(func.params[i].type);
        std::string param_name = get_param_name(func.params[i]);
        // Resolve semantic type for the parameter
        types::TypePtr semantic_type = resolve_parser_type_with_subs(*func.params[i].type, {});
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};

        // Emit debug info for parameters (if enabled and debug level >= 2)
        if (options_.emit_debug_info && options_.debug_level >= 2 && current_scope_id_ != 0) {
            uint32_t line = func.params[i].span.start.line;
            uint32_t column = func.params[i].span.start.column;

            // Create debug info for parameter (arg_no is 1-based)
            int param_debug_id = create_local_variable_debug_info(param_name, param_type, line,
                                                                  static_cast<uint32_t>(i + 1));

            // Create debug location
            int loc_id = fresh_debug_id();
            std::ostringstream meta;
            meta << "!" << loc_id << " = !DILocation("
                 << "line: " << line << ", "
                 << "column: " << column << ", "
                 << "scope: !" << current_scope_id_ << ")\n";
            debug_metadata_.push_back(meta.str());

            // Emit llvm.dbg.declare intrinsic
            emit_debug_declare(alloca_reg, param_debug_id, loc_id);
        }
    }

    // Coverage instrumentation - inject call at function entry
    if (options_.coverage_enabled) {
        std::string func_name_str = add_string_literal(func.name);
        emit_line("  call void @tml_cover_func(ptr " + func_name_str + ")");
    }

    // Generate function body
    if (func.body) {
        for (const auto& stmt : func.body->stmts) {
            if (block_terminated_) {
                // Block already terminated, skip remaining statements
                break;
            }
            gen_stmt(*stmt);
        }

        // Handle trailing expression (return value)
        if (func.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*func.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                // For async functions, wrap result in Poll.Ready
                if (current_func_is_async_ && !current_poll_type_.empty()) {
                    std::string wrapped = wrap_in_poll_ready(result, last_expr_type_);
                    emit_line("  ret " + current_poll_type_ + " " + wrapped);
                } else {
                    emit_line("  ret " + ret_type + " " + result);
                }
                block_terminated_ = true;
            }
        }
    }

    // Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else {
            // For other types, return zeroinitializer
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");
    current_func_.clear();
    current_ret_type_.clear();
    current_func_is_async_ = false;
    current_poll_type_.clear();
    current_poll_inner_type_.clear();
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

void LLVMIRGen::gen_impl_method(const std::string& type_name, const parser::FuncDecl& method) {
    // Skip builtin types that have hard-coded implementations in method.cpp
    // These use lowlevel blocks in TML source but are handled directly by codegen
    if (type_name == "File" || type_name == "Path" || type_name == "List" ||
        type_name == "HashMap" || type_name == "Buffer") {
        return;
    }

    // Skip generic methods for now (they will be instantiated when called)
    if (!method.generics.empty()) {
        return;
    }

    std::string method_name = type_name + "_" + method.name;
    current_func_ = method_name;
    current_impl_type_ = type_name; // Set impl type for 'this' field access
    locals_.clear();
    block_terminated_ = false;

    // Determine return type
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        ret_type = llvm_type_ptr(*method.return_type);
    }
    current_ret_type_ = ret_type;

    // Build parameter list
    std::string params;
    std::string param_types;

    // Check if first param is 'this' or 'mut this' (instance method vs static)
    size_t param_start = 0;
    bool is_instance_method = false;
    if (!method.params.empty()) {
        const auto& first_param = method.params[0];
        std::string first_name = get_param_name(first_param);
        if (first_name == "this") {
            is_instance_method = true;
            param_start = 1; // Skip 'this' in param loop since we handle it specially
        }
    }

    // Add 'this' pointer as first parameter only for instance methods
    if (is_instance_method) {
        params = "ptr %this";
        param_types = "ptr";
    }

    // Add remaining parameters
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (!params.empty()) {
            params += ", ";
            param_types += ", ";
        }
        std::string param_type = llvm_type_ptr(method.params[i].type);
        std::string param_name = get_param_name(method.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
    }

    // Function signature
    std::string func_llvm_name = "tml_" + type_name + "_" + method.name;
    emit_line("");
    emit_line("define internal " + ret_type + " @" + func_llvm_name + "(" + params + ") #0 {");
    emit_line("entry:");

    // Register 'this' in locals only for instance methods
    if (is_instance_method) {
        locals_["this"] = VarInfo{"%this", "ptr", nullptr, std::nullopt};
    }

    // Register other parameters in locals by creating allocas
    for (size_t i = param_start; i < method.params.size(); ++i) {
        std::string param_type = llvm_type_ptr(method.params[i].type);
        std::string param_name = get_param_name(method.params[i]);
        // Resolve semantic type for the parameter
        types::TypePtr semantic_type = resolve_parser_type_with_subs(*method.params[i].type, {});
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, semantic_type, std::nullopt};
    }

    // Generate method body
    if (method.body) {
        for (const auto& stmt : method.body->stmts) {
            if (block_terminated_)
                break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + result);
                block_terminated_ = true;
            }
        }
    }

    // Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else if (ret_type == "i1") {
            emit_line("  ret i1 false");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");
    current_func_.clear();
    current_ret_type_.clear();
    current_impl_type_.clear();
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

// Generate a specialized version of a generic impl method
// e.g., impl[T] Container[T] { func get() -> T } instantiated for Container[I32]
void LLVMIRGen::gen_impl_method_instantiation(
    const std::string& mangled_type_name, const parser::FuncDecl& method,
    const std::unordered_map<std::string, types::TypePtr>& type_subs,
    [[maybe_unused]] const std::vector<parser::GenericParam>& impl_generics) {
    // Save current context
    std::string saved_func = current_func_;
    std::string saved_ret_type = current_ret_type_;
    std::string saved_impl_type = current_impl_type_;
    bool saved_terminated = block_terminated_;
    auto saved_locals = locals_;

    std::string method_name = mangled_type_name + "_" + method.name;
    current_func_ = method_name;
    current_impl_type_ = mangled_type_name;
    locals_.clear();
    block_terminated_ = false;

    // Determine return type with substitution
    std::string ret_type = "void";
    if (method.return_type.has_value()) {
        auto resolved_ret = resolve_parser_type_with_subs(**method.return_type, type_subs);
        ret_type = llvm_type_from_semantic(resolved_ret);
    }
    current_ret_type_ = ret_type;

    // Build parameter list
    std::string params;
    std::string param_types;

    // Check if first param is 'this'
    size_t param_start = 0;
    bool is_instance_method = false;
    if (!method.params.empty()) {
        const auto& first_param = method.params[0];
        std::string first_name = get_param_name(first_param);
        if (first_name == "this") {
            is_instance_method = true;
            param_start = 1;
        }
    }

    // Add 'this' pointer as first parameter for instance methods
    if (is_instance_method) {
        params = "ptr %this";
        param_types = "ptr";
    }

    // Add remaining parameters with type substitution
    for (size_t i = param_start; i < method.params.size(); ++i) {
        if (!params.empty()) {
            params += ", ";
            param_types += ", ";
        }
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, type_subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        std::string param_name = get_param_name(method.params[i]);
        params += param_type + " %" + param_name;
        param_types += param_type;
    }

    // Function signature
    std::string func_llvm_name = "tml_" + mangled_type_name + "_" + method.name;
    emit_line("");
    emit_line("define internal " + ret_type + " @" + func_llvm_name + "(" + params + ") #0 {");
    emit_line("entry:");

    // Register 'this' in locals
    if (is_instance_method) {
        locals_["this"] = VarInfo{"%this", "ptr", nullptr, std::nullopt};
    }

    // Register other parameters in locals by creating allocas
    for (size_t i = param_start; i < method.params.size(); ++i) {
        std::string param_name = get_param_name(method.params[i]);
        auto resolved_param = resolve_parser_type_with_subs(*method.params[i].type, type_subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + param_type);
        emit_line("  store " + param_type + " %" + param_name + ", ptr " + alloca_reg);
        locals_[param_name] = VarInfo{alloca_reg, param_type, resolved_param, std::nullopt};
    }

    // Generate method body
    if (method.body.has_value()) {
        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }
        if (method.body->expr.has_value()) {
            std::string result = gen_expr(*method.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + result);
                block_terminated_ = true;
            }
        }
    }

    // Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else if (ret_type == "i1") {
            emit_line("  ret i1 false");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");

    // Restore context
    current_func_ = saved_func;
    current_ret_type_ = saved_ret_type;
    current_impl_type_ = saved_impl_type;
    block_terminated_ = saved_terminated;
    locals_ = saved_locals;
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}

// Generate a specialized version of a generic function
void LLVMIRGen::gen_func_instantiation(const parser::FuncDecl& func,
                                       const std::vector<types::TypePtr>& type_args) {
    // 1. Create substitution map: T -> I32, U -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < func.generics.size() && i < type_args.size(); ++i) {
        subs[func.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled function name: identity[I32] -> identity__I32
    std::string mangled = mangle_func_name(func.name, type_args);

    // Save current context
    std::string saved_func = current_func_;
    std::string saved_ret_type = current_ret_type_;
    bool saved_terminated = block_terminated_;
    auto saved_locals = locals_;

    current_func_ = mangled;
    locals_.clear();
    block_terminated_ = false;

    // 3. Determine return type with substitution
    std::string ret_type = "void";
    if (func.return_type.has_value()) {
        types::TypePtr resolved_ret = resolve_parser_type_with_subs(**func.return_type, subs);
        ret_type = llvm_type_from_semantic(resolved_ret);
    }
    current_ret_type_ = ret_type;

    // 4. Build parameter list with substituted types
    std::string params;
    std::string param_types;
    // Store name, llvm_type, and semantic_type for each parameter
    struct ParamInfo {
        std::string name;
        std::string llvm_type;
        types::TypePtr semantic_type;
    };
    std::vector<ParamInfo> param_info;

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            params += ", ";
            param_types += ", ";
        }
        // Resolve param type with substitution
        types::TypePtr resolved_param = resolve_parser_type_with_subs(*func.params[i].type, subs);
        std::string param_type = llvm_type_from_semantic(resolved_param);
        std::string param_name = get_param_name(func.params[i]);

        params += param_type + " %" + param_name;
        param_types += param_type;
        param_info.push_back({param_name, param_type, resolved_param});
    }

    // 5. Register function for first-class function support
    std::string func_type = ret_type + " (" + param_types + ")";
    std::vector<std::string> param_types_vec;
    for (const auto& p : param_info) {
        param_types_vec.push_back(p.llvm_type);
    }
    functions_[mangled] = FuncInfo{"@tml_" + mangled, func_type, ret_type, param_types_vec};

    // 6. Emit function definition
    std::string attrs = " #0";
    // Public functions get external linkage for library export
    // In suite mode (force_internal_linkage), all functions are internal to avoid duplicate symbols
    std::string linkage =
        (func.vis == parser::Visibility::Public && !options_.force_internal_linkage) ? ""
                                                                                     : "internal ";
    // Windows DLL export for public functions (disabled in suite mode)
    std::string dll_linkage = "";
    if (options_.dll_export && func.vis == parser::Visibility::Public &&
        !options_.force_internal_linkage) {
        dll_linkage = "dllexport ";
    }
    emit_line("");

    // Create debug scope for generic function instantiation (if debug info enabled)
    int func_scope_id = 0;
    if (options_.emit_debug_info) {
        func_scope_id = create_function_debug_scope("tml_" + mangled, func.span.start.line,
                                                    func.span.start.column);
        // Create a default debug location for instructions in this function
        create_debug_location(func.span.start.line, func.span.start.column);
    }

    // Add debug info as function attribute if we have a scope
    std::string dbg_attr = "";
    if (func_scope_id != 0) {
        dbg_attr = " !dbg !" + std::to_string(func_scope_id);
    }

    emit_line("define " + dll_linkage + linkage + ret_type + " @tml_" + mangled + "(" + params +
              ")" + attrs + dbg_attr + " {");
    emit_line("entry:");

    // 7. Register parameters in locals_
    for (size_t i = 0; i < param_info.size(); ++i) {
        const auto& p = param_info[i];
        std::string alloca_reg = fresh_reg();
        emit_line("  " + alloca_reg + " = alloca " + p.llvm_type);
        emit_line("  store " + p.llvm_type + " %" + p.name + ", ptr " + alloca_reg);
        locals_[p.name] = VarInfo{alloca_reg, p.llvm_type, p.semantic_type, std::nullopt};

        // Emit debug info for parameters (if enabled and debug level >= 2)
        if (options_.emit_debug_info && options_.debug_level >= 2 && current_scope_id_ != 0) {
            uint32_t line = func.params[i].span.start.line;
            uint32_t column = func.params[i].span.start.column;

            // Create debug info for parameter (arg_no is 1-based)
            int param_debug_id = create_local_variable_debug_info(p.name, p.llvm_type, line,
                                                                  static_cast<uint32_t>(i + 1));

            // Create debug location
            int loc_id = fresh_debug_id();
            std::ostringstream meta;
            meta << "!" << loc_id << " = !DILocation("
                 << "line: " << line << ", "
                 << "column: " << column << ", "
                 << "scope: !" << current_scope_id_ << ")\n";
            debug_metadata_.push_back(meta.str());

            // Emit llvm.dbg.declare intrinsic
            emit_debug_declare(alloca_reg, param_debug_id, loc_id);
        }
    }

    // 8. Generate function body
    if (func.body) {
        for (const auto& stmt : func.body->stmts) {
            if (block_terminated_)
                break;
            gen_stmt(*stmt);
        }

        // Handle trailing expression (return value)
        if (func.body->expr.has_value() && !block_terminated_) {
            std::string result = gen_expr(*func.body->expr.value());
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + result);
                block_terminated_ = true;
            }
        }
    }

    // 9. Add implicit return if needed
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i32") {
            emit_line("  ret i32 0");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }

    emit_line("}");

    // Restore context
    current_func_ = saved_func;
    current_ret_type_ = saved_ret_type;
    block_terminated_ = saved_terminated;
    locals_ = saved_locals;
    current_scope_id_ = 0;
    current_debug_loc_id_ = 0;
}
} // namespace tml::codegen