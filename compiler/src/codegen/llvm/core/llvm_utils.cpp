TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Core Utilities
//!
//! This file implements fundamental codegen utilities.
//!
//! ## Register Allocation
//!
//! | Method        | Returns         | Example        |
//! |---------------|-----------------|----------------|
//! | `fresh_reg`   | Unique register | `%t0`, `%t1`   |
//! | `fresh_label` | Unique label    | `if.then0`     |
//!
//! ## Output Emission
//!
//! | Method      | Description                    |
//! |-------------|--------------------------------|
//! | `emit`      | Emit raw text (no newline)     |
//! | `emit_line` | Emit text with newline         |
//!
//! ## String Literals
//!
//! `add_string_literal()` registers a string constant and returns its
//! global variable name (`@.str.0`, `@.str.1`, etc.). These are emitted
//! in the module preamble.
//!
//! ## Error Reporting
//!
//! `report_error()` collects codegen errors for later reporting.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cstdio>

namespace tml::codegen {

LLVMIRGen::LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options)
    : env_(env), options_(std::move(options)) {}

auto LLVMIRGen::fresh_reg() -> std::string {
    return "%t" + std::to_string(temp_counter_++);
}

auto LLVMIRGen::fresh_label(const std::string& prefix) -> std::string {
    return prefix + std::to_string(label_counter_++);
}

void LLVMIRGen::emit(const std::string& code) {
    output_ << code;
}

void LLVMIRGen::emit_line(const std::string& code) {
    // Auto-detect runtime function references for dead declaration elimination.
    // Scans for @symbol patterns and marks them as needed in the catalog.
    if (!runtime_catalog_index_.empty() && needed_runtime_decls_.size() < runtime_catalog_.size()) {
        size_t pos = code.find('@');
        while (pos != std::string::npos) {
            pos++; // skip '@'
            size_t end = pos;
            while (end < code.size() && (std::isalnum(static_cast<unsigned char>(code[end])) ||
                                         code[end] == '_' || code[end] == '.'))
                end++;
            if (end > pos) {
                auto it = runtime_catalog_index_.find(code.substr(pos, end - pos));
                if (it != runtime_catalog_index_.end())
                    require_runtime_decl(runtime_catalog_[it->second].name);
            }
            pos = code.find('@', end);
        }
    }
    output_ << code << "\n";
}

// ============ Entry-Block Alloca Hoisting ============

auto LLVMIRGen::emit_hoisted_alloca(const std::string& type, const std::string& align)
    -> std::string {
    std::string reg = fresh_reg();
    std::string line = "  " + reg + " = alloca " + type;
    if (!align.empty())
        line += ", align " + align;
    if (alloca_hoisting_active_) {
        entry_allocas_.push_back(line);
    } else {
        // Not inside a function body (e.g., during module-level codegen),
        // emit directly as before
        emit_line(line);
    }
    return reg;
}

void LLVMIRGen::begin_alloca_hoisting() {
    entry_allocas_.clear();
    // Emit a unique marker that we'll replace with hoisted allocas at function end
    alloca_hoisting_marker_ = "; @HOISTED_ALLOCAS_" + std::to_string(temp_counter_) + "@";
    emit_line(alloca_hoisting_marker_);
    alloca_hoisting_active_ = true;
}

void LLVMIRGen::end_alloca_hoisting() {
    if (!alloca_hoisting_active_)
        return;
    alloca_hoisting_active_ = false;

    // Build the alloca block to replace marker with
    std::string alloca_block;
    for (const auto& line : entry_allocas_) {
        alloca_block += line + "\n";
    }
    entry_allocas_.clear();

    // Replace the marker in output_ with the hoisted allocas.
    // Use rfind (reverse search) — the marker is near the end of the stream
    // since it was emitted at the start of the CURRENT function.
    // This avoids O(n) scanning from the beginning of multi-megabyte output.
    std::string full = output_.str();
    auto pos = full.rfind(alloca_hoisting_marker_);
    if (pos != std::string::npos) {
        // Replace marker line (marker + newline) with alloca block
        full.replace(pos, alloca_hoisting_marker_.size() + 1, alloca_block);
        output_.str(full);
        output_.seekp(0, std::ios_base::end);
    }
    alloca_hoisting_marker_.clear();
}

void LLVMIRGen::emit_coverage(const std::string& func_name) {
    if (options_.coverage_enabled) {
        std::string func_name_str = add_string_literal(func_name);
        emit_line("  call void @tml_cover_func(ptr " + func_name_str + ")");
    }
}

void LLVMIRGen::emit_coverage_report_calls(const std::string& coverage_output_str,
                                           bool check_quiet) {
    if (!options_.coverage_enabled) {
        return;
    }
    if (check_quiet && options_.coverage_quiet) {
        return;
    }
    emit_line("  call void @print_coverage_report()");
    if (!coverage_output_str.empty()) {
        emit_line("  call void @write_coverage_html(ptr " + coverage_output_str + ")");
    }
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span) {
    errors_.push_back(LLVMGenError{msg, span, {}, ""});
}

void LLVMIRGen::report_error(const std::string& msg, const SourceSpan& span,
                             const std::string& code) {
    errors_.push_back(LLVMGenError{msg, span, {}, code});
}

auto LLVMIRGen::coerce_closure_to_fn_ptr(const std::string& val) -> std::string {
    if (last_expr_type_ == "{ ptr, ptr }") {
        // Extract fn_ptr (index 0) from the fat pointer
        std::string fn_ptr = fresh_reg();
        emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + val + ", 0");
        last_expr_type_ = "ptr";
        return fn_ptr;
    }
    return val;
}

auto LLVMIRGen::add_string_literal(const std::string& value) -> std::string {
    auto it = string_literal_dedup_.find(value);
    if (it != string_literal_dedup_.end()) {
        return it->second;
    }
    std::string name = "@.str." + std::to_string(string_literals_.size());
    string_literals_.emplace_back(name, value);
    string_literal_dedup_.emplace(value, name);
    return name;
}

auto LLVMIRGen::mangle_tml_symbol(const std::string& module_name,
                                  const std::string& func_name) const -> std::string {
    // Hierarchical Itanium-inspired mangling for TML library functions.
    // Produces N<len><seg>...<len><seg>E encoding that is unambiguous even when
    // module names contain underscores.
    //
    // Examples:
    //   ("core::str",            "to_lowercase") → "N4core3str12to_lowercaseE"
    //   ("core::char::methods",  "to_lowercase") → "N4core4char7methods12to_lowercaseE"
    //   ("core::iter::sources::repeat", "repeat") → "N4core4iter7sources6repeat6repeatE"
    //
    // Local (non-module) functions are not mangled — they use the plain name
    // with suite prefix for disambiguation (handled by get_suite_prefix()).
    if (module_name.empty()) {
        return func_name;
    }
    std::string result = "N";
    // Encode each :: -separated path segment
    size_t pos = 0;
    while (pos <= module_name.size()) {
        size_t sep = module_name.find("::", pos);
        if (sep == std::string::npos)
            sep = module_name.size();
        std::string seg = module_name.substr(pos, sep - pos);
        if (!seg.empty()) {
            result += std::to_string(seg.size()) + seg;
        }
        pos = sep + 2;
    }
    // Encode function name as the last segment
    result += std::to_string(func_name.size()) + func_name;
    result += "E";
    return result;
}

auto LLVMIRGen::mangle_tml_symbol(const std::string& module_name, const std::string& func_name,
                                  const std::vector<types::TypePtr>& param_types) const
    -> std::string {
    // Build the base mangled name (hierarchical path encoding)
    std::string result = mangle_tml_symbol(module_name, func_name);

    // Append parameter type suffix: _<type codes>
    // Only for library functions (non-empty module_name) with parameters
    if (!module_name.empty() && !param_types.empty()) {
        result += "_";
        for (const auto& ty : param_types) {
            result += mangle_type_code(ty);
        }
    }

    return result;
}

auto LLVMIRGen::mangle_type_code(const types::TypePtr& type) -> std::string {
    if (!type)
        return "v"; // void/unknown → unit

    if (type->is<types::PrimitiveType>()) {
        switch (type->as<types::PrimitiveType>().kind) {
        case types::PrimitiveKind::I8:
            return "a";
        case types::PrimitiveKind::I16:
            return "s";
        case types::PrimitiveKind::I32:
            return "i";
        case types::PrimitiveKind::I64:
            return "l";
        case types::PrimitiveKind::I128:
            return "x";
        case types::PrimitiveKind::U8:
            return "h";
        case types::PrimitiveKind::U16:
            return "t";
        case types::PrimitiveKind::U32:
            return "j";
        case types::PrimitiveKind::U64:
            return "m";
        case types::PrimitiveKind::U128:
            return "y";
        case types::PrimitiveKind::F32:
            return "f";
        case types::PrimitiveKind::F64:
            return "d";
        case types::PrimitiveKind::Bool:
            return "b";
        case types::PrimitiveKind::Char:
            return "c";
        case types::PrimitiveKind::Str:
            return "S";
        case types::PrimitiveKind::Unit:
            return "v";
        case types::PrimitiveKind::Never:
            return "z";
        }
    }

    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        std::string code = std::to_string(named.name.size()) + named.name;
        // Append generic args if present: e.g., List[I32] → "4ListIiE"
        if (!named.type_args.empty()) {
            code += "I";
            for (const auto& arg : named.type_args) {
                code += mangle_type_code(arg);
            }
            code += "E";
        }
        return code;
    }

    if (type->is<types::RefType>()) {
        return "R" + mangle_type_code(type->as<types::RefType>().inner);
    }

    if (type->is<types::PtrType>()) {
        return "P" + mangle_type_code(type->as<types::PtrType>().inner);
    }

    if (type->is<types::SliceType>()) {
        return "A" + mangle_type_code(type->as<types::SliceType>().element);
    }

    if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        return "A" + std::to_string(arr.size) + "_" + mangle_type_code(arr.element);
    }

    if (type->is<types::TupleType>()) {
        const auto& tup = type->as<types::TupleType>();
        std::string code = "T";
        for (const auto& elem : tup.elements) {
            code += mangle_type_code(elem);
        }
        code += "E";
        return code;
    }

    if (type->is<types::FuncType>()) {
        const auto& fn = type->as<types::FuncType>();
        std::string code = "F";
        for (const auto& p : fn.params) {
            code += mangle_type_code(p);
        }
        code += "_" + mangle_type_code(fn.return_type);
        return code;
    }

    // Fallback for complex types: use "u" (unknown)
    return "u";
}

auto LLVMIRGen::fnv1a_hash_hex(const std::string& input) -> std::string {
    // FNV-1a 64-bit hash
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : input) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 0x100000001b3ULL;
    }
    // Format as 8 hex chars (lower 32 bits for compactness)
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08x", static_cast<uint32_t>(hash & 0xFFFFFFFF));
    return std::string(buf);
}

auto LLVMIRGen::get_suite_prefix() const -> std::string {
    // Suite prefix is only used for test-local functions (current_module_prefix_ empty)
    // Library functions should NOT have suite prefix - they're shared across tests
    if (options_.suite_test_index >= 0 && options_.force_internal_linkage &&
        current_module_prefix_.empty()) {
        return "s" + std::to_string(options_.suite_test_index) + "_";
    }
    return "";
}

auto LLVMIRGen::is_library_method(const std::string& type_name, const std::string& method) const
    -> bool {
    if (!env_.module_registry()) {
        return false;
    }

    // First check if type_name::method is directly registered (top-level functions)
    std::string qualified_name = type_name + "::" + method;
    const auto& all_modules = env_.module_registry()->get_all_modules();
    for (const auto& [mod_name, mod] : all_modules) {
        if (mod.functions.find(qualified_name) != mod.functions.end()) {
            return true;
        }
        // Also check if the TYPE itself is from this module (impl methods)
        if (mod.structs.find(type_name) != mod.structs.end()) {
            return true;
        }
        // Check enums
        if (mod.enums.find(type_name) != mod.enums.end()) {
            return true;
        }
        // Check classes
        if (mod.classes.find(type_name) != mod.classes.end()) {
            return true;
        }
    }
    return false;
}

auto LLVMIRGen::find_module_for_type(const std::string& type_name) const -> std::string {
    if (!env_.module_registry()) {
        return "";
    }

    // Primitive types and built-in enums are not registered in the module registry as
    // structs/enums/classes, but they need stable mangled names. Map them to canonical
    // modules so that mangle_impl_method produces context-independent Itanium-style names.
    // e.g., I32::eq → tml_N4core3I322eqE (stable, no suite prefix dependency)
    static const std::unordered_map<std::string, std::string> builtin_modules = {
        // Primitive types
        {"I8", "core"},
        {"I16", "core"},
        {"I32", "core"},
        {"I64", "core"},
        {"I128", "core"},
        {"U8", "core"},
        {"U16", "core"},
        {"U32", "core"},
        {"U64", "core"},
        {"U128", "core"},
        {"F32", "core"},
        {"F64", "core"},
        {"Bool", "core"},
        {"Char", "core"},
        {"Str", "core"},
        {"Never", "core"},
        // Built-in enums (defined in type system, not in .tml files)
        {"Maybe", "core"},
        {"Outcome", "core"},
        {"Ordering", "core"},
        {"Poll", "core"},
        {"ControlFlow", "core"},
    };
    auto prim_it = builtin_modules.find(type_name);
    if (prim_it != builtin_modules.end()) {
        return prim_it->second;
    }

    // For mangled generic types like "ManuallyDrop__I32" or "List__Str",
    // extract the base type name before "__" for registry lookup.
    std::string base_name = type_name;
    auto dunder_pos = type_name.find("__");
    if (dunder_pos != std::string::npos) {
        base_name = type_name.substr(0, dunder_pos);
        // Also check base name against builtin_modules for generic instantiations
        // e.g., Maybe__I32 → base "Maybe" → "core"
        auto base_it = builtin_modules.find(base_name);
        if (base_it != builtin_modules.end()) {
            return base_it->second;
        }
    }

    const auto& all_modules = env_.module_registry()->get_all_modules();
    for (const auto& [mod_name, mod] : all_modules) {
        if (mod.structs.find(type_name) != mod.structs.end() ||
            mod.structs.find(base_name) != mod.structs.end()) {
            return mod_name;
        }
        if (mod.internal_structs.find(type_name) != mod.internal_structs.end() ||
            mod.internal_structs.find(base_name) != mod.internal_structs.end()) {
            return mod_name;
        }
        if (mod.enums.find(type_name) != mod.enums.end() ||
            mod.enums.find(base_name) != mod.enums.end()) {
            return mod_name;
        }
        if (mod.classes.find(type_name) != mod.classes.end() ||
            mod.classes.find(base_name) != mod.classes.end()) {
            return mod_name;
        }
    }
    return "";
}

auto LLVMIRGen::mangle_impl_method(const std::string& type_name,
                                   const std::string& method_name) const -> std::string {
    std::string module = find_module_for_type(type_name);
    if (!module.empty()) {
        // Library type: use Itanium-style encoding with module path + type + method
        // We pass "module::TypeName" as the module path, and method_name as func_name,
        // so the result is N<module_segs><TypeName><method_name>E
        // e.g., ("Str", "split") in module "core::str" → "tml_N4core3str3Str5splitE"
        return "tml_" + mangle_tml_symbol(module + "::" + type_name, method_name);
    }
    // Local type: use flat naming with optional suite prefix
    // e.g., ("MyStruct", "foo") → "tml_s0_MyStruct_foo"
    return "tml_" + get_suite_prefix() + type_name + "_" + method_name;
}

} // namespace tml::codegen
