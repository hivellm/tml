TML_MODULE("compiler")

//! # MIR-based LLVM IR Code Generator
//!
//! This file generates LLVM IR directly from MIR (Mid-level IR).
//!
//! ## Advantages of MIR-based Codegen
//!
//! MIR is already in SSA form, which maps naturally to LLVM IR:
//! - No need for SSA construction during codegen
//! - Direct mapping from MIR values to LLVM registers
//! - Simplified control flow handling
//!
//! ## Generation Pipeline
//!
//! | Phase            | Method              | Output                |
//! |------------------|---------------------|-----------------------|
//! | Preamble         | `emit_preamble`     | Target triple, attrs  |
//! | Type definitions | `emit_type_defs`    | Struct/enum layouts   |
//! | Functions        | `emit_function`     | Function definitions  |
//! | Basic blocks     | `emit_basic_block`  | Labels and terminators|
//! | Instructions     | `emit_instruction`  | LLVM instructions     |
//!
//! ## Value Mapping
//!
//! `value_regs_` maps MIR value IDs to LLVM register names (%t0, %t1, etc.).
//!
//! ## Code Organization
//!
//! The implementation is split across multiple files:
//! - mir_codegen.cpp: Core generation (this file)
//! - mir/instructions.cpp: Instruction emission
//! - mir/terminators.cpp: Terminator emission
//! - mir/types.cpp: Type conversion
//! - mir/helpers.cpp: Helper methods

#include "codegen/mir_codegen.hpp"

#include "codegen/target.hpp"
#include "profiler.hpp"
#include "version_generated.hpp"

#include <sstream>

namespace tml::codegen {

// ============================================================================
// TML name demangler for coverage reporting
// ============================================================================

/// Demangle a TML mangled function name to a human-readable form.
///
/// TML mangling uses Itanium-style nested names:
///   "tml_N<len>seg<len>seg...E"          → "seg::seg::..."
///   "tml_N<len>seg<len>seg...E_<params>"  → "seg::seg::..."  (params stripped)
///   "s0_tml_N<len>seg...E"               → "seg::..."        (suite prefix stripped)
///   "\"tml_N...E\""                      → demangled          (quoted names)
///   "plain_name"                         → "plain_name"       (passthrough)
///
/// The last two segments are used as "Type::method" when there are 3+ segments,
/// matching the legacy codegen's coverage format (e.g., "Str::len", "List::push").
/// For 2 segments like "core::len", the full path is returned.
/// For 1 segment, the bare name is returned.
static std::string demangle_tml_name(const std::string& mangled) {
    // Find the start of "tml_N" — may be preceded by suite prefix "sN_" or quote
    size_t tml_pos = mangled.find("tml_N");
    if (tml_pos == std::string::npos) {
        // Not a mangled name — strip suite prefix if present (e.g., "s0_main" → "main")
        if (mangled.size() > 2 && mangled[0] == 's' &&
            std::isdigit(static_cast<unsigned char>(mangled[1]))) {
            size_t us = mangled.find('_');
            if (us != std::string::npos && us + 1 < mangled.size()) {
                return mangled.substr(us + 1);
            }
        }
        return mangled;
    }

    size_t pos = tml_pos + 5; // skip "tml_N"

    // Parse Itanium-style nested name segments: <len><chars><len><chars>...E
    std::vector<std::string> segments;
    while (pos < mangled.size()) {
        if (!std::isdigit(static_cast<unsigned char>(mangled[pos])))
            break;

        // Read decimal length prefix
        size_t len_start = pos;
        while (pos < mangled.size() && std::isdigit(static_cast<unsigned char>(mangled[pos])))
            ++pos;
        int seg_len = std::stoi(mangled.substr(len_start, pos - len_start));

        if (seg_len <= 0 || pos + static_cast<size_t>(seg_len) > mangled.size())
            break;

        segments.push_back(mangled.substr(pos, static_cast<size_t>(seg_len)));
        pos += static_cast<size_t>(seg_len);

        // 'E' marks end of nested name; '_' starts param suffix; '"' for quoted names
        if (pos < mangled.size() && (mangled[pos] == 'E' || mangled[pos] == '"'))
            break;
    }

    if (segments.empty())
        return mangled;

    // The legacy codegen uses "Type::method" format for impl methods.
    // MIR mangled names include the full module path: core::str::Str::len
    // We want to match the legacy format:
    //   3+ segments ending in [Type, method] → "Type::method"
    //   2 segments → "seg0::seg1" (e.g., module-level function)
    //   1 segment  → bare name
    if (segments.size() >= 3) {
        // Check if the second-to-last segment starts with uppercase (type name)
        const auto& maybe_type = segments[segments.size() - 2];
        if (!maybe_type.empty() && std::isupper(static_cast<unsigned char>(maybe_type[0]))) {
            return maybe_type + "::" + segments.back();
        }
        // Otherwise return last two segments (module::func)
        return segments[segments.size() - 2] + "::" + segments.back();
    }
    if (segments.size() == 2) {
        return segments[0] + "::" + segments[1];
    }
    return segments[0];
}

MirCodegen::MirCodegen(MirCodegenOptions options) : options_(std::move(options)) {}

void MirCodegen::emit(const std::string& s) {
    output_ << s;
}

void MirCodegen::emitln(const std::string& s) {
    output_ << s << "\n";
}

void MirCodegen::emit_comment(const std::string& s) {
    if (options_.emit_comments) {
        emitln("; " + s);
    }
}

auto MirCodegen::new_temp() -> std::string {
    return "%t" + std::to_string(temp_counter_++);
}

auto MirCodegen::generate(const mir::Module& module) -> std::string {
    TML_ZONE("codegen::generate");
    output_.str("");
    output_.clear();
    temp_counter_ = 0;
    spill_counter_ = 0;
    value_regs_.clear();
    value_types_.clear();
    struct_field_types_.clear();
    value_spill_allocas_.clear();
    block_labels_.clear();
    emitted_types_.clear();
    string_constants_.clear();
    value_string_contents_.clear();
    used_enum_types_.clear();
    used_struct_types_.clear();
    vtables_.clear();
    behavior_method_order_.clear();
    emitted_vtables_.clear();
    emitted_dyn_types_.clear();
    value_dyn_behavior_.clear();
    coverage_name_map_.clear();

    // First pass: collect string constants, enum types, and generic enum defs
    generic_enum_defs_.clear();
    for (const auto& func : module.functions) {
        // Collect generic enum types from function signatures
        collect_enum_types_from_type(func.return_type);
        for (const auto& param : func.params) {
            collect_enum_types_from_type(param.type);
        }

        // Collect string constants for HTTP route decorators
        if (func.route_info.has_value()) {
            auto register_str = [this](const std::string& s) {
                if (string_constants_.find(s) == string_constants_.end()) {
                    string_constants_[s] = "@.str." + std::to_string(string_constants_.size());
                }
            };
            std::string method_str;
            switch (func.route_info->method) {
            case mir::RouteMethod::Get:
                method_str = "GET";
                break;
            case mir::RouteMethod::Post:
                method_str = "POST";
                break;
            case mir::RouteMethod::Put:
                method_str = "PUT";
                break;
            case mir::RouteMethod::Delete:
                method_str = "DELETE";
                break;
            case mir::RouteMethod::Patch:
                method_str = "PATCH";
                break;
            case mir::RouteMethod::Head:
                method_str = "HEAD";
                break;
            case mir::RouteMethod::Options:
                method_str = "OPTIONS";
                break;
            }
            register_str(method_str);
            register_str(func.route_info->path);
        }

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* const_inst = std::get_if<mir::ConstantInst>(&inst.inst)) {
                    if (auto* str_const = std::get_if<mir::ConstString>(&const_inst->value)) {
                        if (string_constants_.find(str_const->value) == string_constants_.end()) {
                            std::string global_name =
                                "@.str." + std::to_string(string_constants_.size());
                            string_constants_[str_const->value] = global_name;
                        }
                    }
                }
                // Collect enum types from EnumInitInst (for imported enums)
                if (auto* enum_inst = std::get_if<mir::EnumInitInst>(&inst.inst)) {
                    used_enum_types_.insert(enum_inst->enum_name);
                }
                // Collect struct types from StructInitInst (for imported/library structs
                // like Range[T] that aren't in module.structs)
                if (auto* struct_inst = std::get_if<mir::StructInitInst>(&inst.inst)) {
                    if (used_struct_types_.find(struct_inst->struct_name) ==
                        used_struct_types_.end()) {
                        used_struct_types_[struct_inst->struct_name] = struct_inst->field_types;
                    }
                }
                // Collect generic enum types from instruction result types
                if (inst.type) {
                    collect_enum_types_from_type(inst.type);
                }
            }
        }
    }

    // Collect profiler string constants (function names) for instrumentation
    if (options_.instrument_profiler) {
        for (const auto& func : module.functions) {
            if (!func.blocks.empty()) {
                // Register function name as a string constant for profiler_enter
                if (string_constants_.find(func.name) == string_constants_.end()) {
                    string_constants_[func.name] =
                        "@.str." + std::to_string(string_constants_.size());
                }
            }
        }
        // Register a placeholder file name (MIR functions don't carry source file info)
        static const std::string unknown_file = "<tml>";
        if (string_constants_.find(unknown_file) == string_constants_.end()) {
            string_constants_[unknown_file] = "@.str." + std::to_string(string_constants_.size());
        }
    }

    // Collect coverage string constants (function names) for tml_cover_func instrumentation.
    // Use demangled names so coverage report can match against source-extracted names.
    // Legacy codegen uses clean names like "Str::len", "List::push", "assert_true".
    if (options_.coverage_enabled) {
        for (const auto& func : module.functions) {
            if (!func.blocks.empty()) {
                std::string clean_name = demangle_tml_name(func.name);
                coverage_name_map_[func.name] = clean_name;
                if (string_constants_.find(clean_name) == string_constants_.end()) {
                    string_constants_[clean_name] =
                        "@.str." + std::to_string(string_constants_.size());
                }
            }
        }
    }

    emit_preamble();

    // Emit string constants after preamble
    for (const auto& [value, name] : string_constants_) {
        std::string escaped;
        for (char c : value) {
            if (c == '\n') {
                escaped += "\\0A";
            } else if (c == '\r') {
                escaped += "\\0D";
            } else if (c == '"') {
                escaped += "\\22";
            } else if (c == '\\') {
                escaped += "\\5C";
            } else if (c == '\0') {
                escaped += "\\00";
            } else {
                escaped += c;
            }
        }
        size_t len = value.size() + 1; // +1 for null terminator
        emitln(name + " = private constant [" + std::to_string(len) + " x i8] c\"" + escaped +
               "\\00\"");
    }
    if (!string_constants_.empty()) {
        emitln();
    }

    emit_type_defs(module);

    // Collect sret functions (those with uses_sret flag set by RVO pass)
    sret_functions_.clear();
    for (const auto& func : module.functions) {
        if (func.uses_sret && func.original_return_type) {
            sret_functions_[func.name] = mir_type_to_llvm(func.original_return_type);
        }
    }

    // Collect declared parameter types for all functions (for array-to-slice coercion)
    // Also convert this/self struct params to pointer types to match the actual
    // LLVM IR signature (where struct this/self is always passed as ptr).
    func_param_types_.clear();
    for (const auto& func : module.functions) {
        std::vector<mir::MirTypePtr> param_types;
        for (const auto& p : func.params) {
            if ((p.name == "this" || p.name == "self") && p.type) {
                std::string llvm_ty = mir_type_to_llvm(p.type);
                if (llvm_ty.starts_with("%struct.") || llvm_ty.starts_with("%enum.") ||
                    llvm_ty.starts_with("%class.") || llvm_ty.starts_with("%union.")) {
                    param_types.push_back(mir::make_ptr_type());
                    continue;
                }
            }
            param_types.push_back(p.type);
        }
        func_param_types_[func.name] = std::move(param_types);
    }

    // Emit functions: define for functions with bodies, declare for extern/imported
    for (const auto& func : module.functions) {
        if (func.blocks.empty()) {
            // Extern or imported function — no body, emit as declare
            emit_function_declaration(func);
        } else {
            emit_function(func);
        }
    }

    // Emit vtable constants for dyn dispatch
    emit_vtables(module);

    // Emit entry point wrappers.
    // The user's `main` function is emitted as `tml_main` (see emit_function),
    // and these wrappers provide the public entry symbols.
    if (options_.generate_exe_main) {
        emit_main_wrapper(module);
    } else if (!options_.test_entry_name.empty()) {
        emit_test_entry_wrapper(module);
    }

    // Emit route registration function for @Get/@Post/etc. decorators
    emit_route_registration(module);

    // Module identification metadata
    emitln();
    emitln("!llvm.ident = !{!0}");
    emitln("!0 = !{!\"tml version " + std::string(tml::VERSION) + "\"}");

    return output_.str();
}

auto MirCodegen::generate_cgu(const mir::Module& module,
                              const std::vector<size_t>& function_indices) -> std::string {
    output_.str("");
    output_.clear();
    temp_counter_ = 0;
    spill_counter_ = 0;
    value_regs_.clear();
    value_types_.clear();
    struct_field_types_.clear();
    value_spill_allocas_.clear();
    block_labels_.clear();
    emitted_types_.clear();
    string_constants_.clear();
    value_string_contents_.clear();
    used_enum_types_.clear();
    used_struct_types_.clear();
    coverage_name_map_.clear();

    // Build index set for O(1) lookup
    std::unordered_set<size_t> included(function_indices.begin(), function_indices.end());

    // First pass: collect string constants, enum types, and generic enum defs
    // (same as generate() — all CGUs need the complete set)
    generic_enum_defs_.clear();
    for (const auto& func : module.functions) {
        // Collect generic enum types from function signatures
        collect_enum_types_from_type(func.return_type);
        for (const auto& param : func.params) {
            collect_enum_types_from_type(param.type);
        }

        // Collect route decorator strings for CGU
        if (func.route_info.has_value()) {
            auto register_str = [this](const std::string& s) {
                if (string_constants_.find(s) == string_constants_.end()) {
                    string_constants_[s] = "@.str." + std::to_string(string_constants_.size());
                }
            };
            std::string method_str;
            switch (func.route_info->method) {
            case mir::RouteMethod::Get:
                method_str = "GET";
                break;
            case mir::RouteMethod::Post:
                method_str = "POST";
                break;
            case mir::RouteMethod::Put:
                method_str = "PUT";
                break;
            case mir::RouteMethod::Delete:
                method_str = "DELETE";
                break;
            case mir::RouteMethod::Patch:
                method_str = "PATCH";
                break;
            case mir::RouteMethod::Head:
                method_str = "HEAD";
                break;
            case mir::RouteMethod::Options:
                method_str = "OPTIONS";
                break;
            }
            register_str(method_str);
            register_str(func.route_info->path);
        }

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* const_inst = std::get_if<mir::ConstantInst>(&inst.inst)) {
                    if (auto* str_const = std::get_if<mir::ConstString>(&const_inst->value)) {
                        if (string_constants_.find(str_const->value) == string_constants_.end()) {
                            std::string global_name =
                                "@.str." + std::to_string(string_constants_.size());
                            string_constants_[str_const->value] = global_name;
                        }
                    }
                }
                if (auto* enum_inst = std::get_if<mir::EnumInitInst>(&inst.inst)) {
                    used_enum_types_.insert(enum_inst->enum_name);
                }
                // Collect struct types from StructInitInst (for imported/library structs)
                if (auto* struct_inst = std::get_if<mir::StructInitInst>(&inst.inst)) {
                    if (used_struct_types_.find(struct_inst->struct_name) ==
                        used_struct_types_.end()) {
                        used_struct_types_[struct_inst->struct_name] = struct_inst->field_types;
                    }
                }
                if (inst.type) {
                    collect_enum_types_from_type(inst.type);
                }
            }
        }
    }

    // Collect profiler string constants (function names) for instrumentation — CGU path
    if (options_.instrument_profiler) {
        for (size_t i = 0; i < module.functions.size(); ++i) {
            const auto& func = module.functions[i];
            if (included.count(i) && !func.blocks.empty()) {
                if (string_constants_.find(func.name) == string_constants_.end()) {
                    string_constants_[func.name] =
                        "@.str." + std::to_string(string_constants_.size());
                }
            }
        }
        static const std::string unknown_file = "<tml>";
        if (string_constants_.find(unknown_file) == string_constants_.end()) {
            string_constants_[unknown_file] = "@.str." + std::to_string(string_constants_.size());
        }
    }

    // Collect coverage string constants (function names) for tml_cover_func — CGU path.
    // Use demangled names for coverage report matching (same as generate() path).
    if (options_.coverage_enabled) {
        for (size_t i = 0; i < module.functions.size(); ++i) {
            const auto& func = module.functions[i];
            if (included.count(i) && !func.blocks.empty()) {
                std::string clean_name = demangle_tml_name(func.name);
                coverage_name_map_[func.name] = clean_name;
                if (string_constants_.find(clean_name) == string_constants_.end()) {
                    string_constants_[clean_name] =
                        "@.str." + std::to_string(string_constants_.size());
                }
            }
        }
    }

    emit_preamble();

    // Emit string constants after preamble
    for (const auto& [value, name] : string_constants_) {
        std::string escaped;
        for (char c : value) {
            if (c == '\n') {
                escaped += "\\0A";
            } else if (c == '\r') {
                escaped += "\\0D";
            } else if (c == '"') {
                escaped += "\\22";
            } else if (c == '\\') {
                escaped += "\\5C";
            } else if (c == '\0') {
                escaped += "\\00";
            } else {
                escaped += c;
            }
        }
        size_t len = value.size() + 1;
        emitln(name + " = private constant [" + std::to_string(len) + " x i8] c\"" + escaped +
               "\\00\"");
    }
    if (!string_constants_.empty()) {
        emitln();
    }

    emit_type_defs(module);

    // Collect sret functions from ALL functions (same as generate())
    sret_functions_.clear();
    for (const auto& func : module.functions) {
        if (func.uses_sret && func.original_return_type) {
            sret_functions_[func.name] = mir_type_to_llvm(func.original_return_type);
        }
    }

    // Collect declared parameter types for all functions (for array-to-slice coercion)
    // Also convert this/self struct params to pointer types to match the actual
    // LLVM IR signature (where struct this/self is always passed as ptr).
    func_param_types_.clear();
    for (const auto& func : module.functions) {
        std::vector<mir::MirTypePtr> param_types;
        for (const auto& p : func.params) {
            if ((p.name == "this" || p.name == "self") && p.type) {
                std::string llvm_ty = mir_type_to_llvm(p.type);
                if (llvm_ty.starts_with("%struct.") || llvm_ty.starts_with("%enum.") ||
                    llvm_ty.starts_with("%class.") || llvm_ty.starts_with("%union.")) {
                    param_types.push_back(mir::make_ptr_type());
                    continue;
                }
            }
            param_types.push_back(p.type);
        }
        func_param_types_[func.name] = std::move(param_types);
    }

    // Emit functions: define for included (with body), declare for others/extern
    for (size_t i = 0; i < module.functions.size(); ++i) {
        const auto& func = module.functions[i];
        if (included.count(i) && !func.blocks.empty()) {
            emit_function(func);
        } else {
            emit_function_declaration(func);
        }
    }

    // Emit vtable constants for dyn dispatch (same as generate())
    emit_vtables(module);

    // Emit test runtime declarations needed by test entry wrappers
    if (!options_.test_entry_name.empty()) {
        emitln("declare dso_local void @tml_set_test_timeout(i32)");
        emitln("declare dso_local i32 @tml_run_test_with_catch(ptr)");
    }

    // Emit entry point wrappers for the CGU that contains the `main` function.
    {
        bool this_cgu_has_main = false;
        for (size_t idx : function_indices) {
            if (module.functions[idx].name == "main") {
                this_cgu_has_main = true;
                break;
            }
        }
        if (this_cgu_has_main) {
            if (options_.generate_exe_main) {
                emit_main_wrapper(module);
            } else if (!options_.test_entry_name.empty()) {
                emit_test_entry_wrapper(module);
            }
            emit_route_registration(module);
        }
    }

    // Module identification metadata
    emitln();
    emitln("!llvm.ident = !{!0}");
    emitln("!0 = !{!\"tml version " + std::string(tml::VERSION) + "\"}");

    return output_.str();
}

void MirCodegen::emit_function_declaration(const mir::Function& func) {
    // Skip __tml_register_routes declaration — the codegen will generate
    // a define for this function when HTTP route decorators are present
    if (func.name == "__tml_register_routes") {
        return;
    }

    std::string ret_type = mir_type_to_llvm(func.return_type);
    // Rename `main` based on entry mode:
    // - generate_exe_main: rename to tml_main (C entry wrapper calls it)
    // - test_entry_name: rename to e.g. tml_test_0 (dispatcher calls it)
    std::string decl_name = func.name;
    if (func.name == "main") {
        // Always rename main to tml_main; wrappers provide the public entry name
        if (options_.generate_exe_main || !options_.test_entry_name.empty())
            decl_name = "tml_main";
    }
    // Use dso_local on extern declarations to prevent LLVM 23+ from merging
    // function declarations with similar signatures during codegen.
    emit("declare dso_local " + ret_type + " @" + quote_func_name(decl_name) + "(");

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            emit(", ");
        }
        std::string param_type = mir_type_to_llvm(func.params[i].type);
        const auto& param_name = func.params[i].name;
        // Unit type maps to "void" but LLVM doesn't allow void as a parameter type.
        if (param_type == "void") {
            param_type = "{}";
        }
        // Method 'this'/'self' parameters for struct/enum types → ptr
        if ((param_name == "this" || param_name == "self") &&
            (param_type.starts_with("%struct.") || param_type.starts_with("%enum.") ||
             param_type.starts_with("%class.") || param_type.starts_with("%union."))) {
            param_type = "ptr";
        }
        if (func.uses_sret && i == 0 && func.original_return_type) {
            std::string orig_ret_type = mir_type_to_llvm(func.original_return_type);
            emit(param_type + " sret(" + orig_ret_type + ")");
        } else {
            emit(param_type);
        }
    }

    emitln(")");
    emitln();
}

void MirCodegen::emit_main_wrapper(const mir::Module& module) {
    // Find the `main` function to determine its return type.
    const mir::Function* main_func = nullptr;
    for (const auto& func : module.functions) {
        if (func.name == "main") {
            main_func = &func;
            break;
        }
    }
    if (!main_func) {
        return; // No main function in this module.
    }

    // Determine if user's main returns void or i32.
    std::string main_ret = mir_type_to_llvm(main_func->return_type);
    bool returns_void = (main_ret == "void");

    emitln("; C entry point — calls tml_main() generated from user's `main` function");
    emitln("define dso_local i32 @main(i32 %argc, ptr %argv) noinline {");
    emitln("entry:");
    if (returns_void) {
        emitln("  call void @tml_main()");
    } else {
        emitln("  %ret = call i32 @tml_main()");
    }

    // Write coverage data before exiting (standalone EXE mode).
    if (options_.coverage_enabled) {
        emitln("  %cov_file_env = call ptr @getenv(ptr @.tml_cov_file_env)");
        emitln("  %cov_file_not_null = icmp ne ptr %cov_file_env, null");
        emitln("  br i1 %cov_file_not_null, label %write_cov_file, label %cov_file_done");
        emitln("");
        emitln("write_cov_file:");
        emitln("  call void @tml_coverage_write_file(ptr %cov_file_env)");
        emitln("  br label %cov_file_done");
        emitln("");
        emitln("cov_file_done:");
    }

    if (returns_void) {
        emitln("  ret i32 0");
    } else {
        emitln("  ret i32 %ret");
    }
    emitln("}");
    emitln();
}

void MirCodegen::emit_test_entry_wrapper(const mir::Module& module) {
    // Collect @test functions from the module.
    // Test files have @test annotated functions instead of main.
    std::vector<const mir::Function*> test_funcs;
    const mir::Function* main_func = nullptr;
    for (const auto& func : module.functions) {
        if (func.name == "main") {
            main_func = &func;
        }
        for (const auto& attr : func.attributes) {
            if (attr == "test") {
                test_funcs.push_back(&func);
                break;
            }
        }
    }

    if (test_funcs.empty() && !main_func) {
        return;
    }

    // Generate an i32-returning wrapper that the dispatcher can call.
    emitln("; Test entry wrapper — calls test functions, returns i32 for dispatcher");
    emitln("define dllexport i32 @" + quote_func_name(options_.test_entry_name) + "() {");
    emitln("entry:");

    // Set per-test timeout (100ms) to kill tests stuck in infinite loops
    emitln("  call void @tml_set_test_timeout(i32 100)");

    if (!test_funcs.empty()) {
        // Call each @test function sequentially
        for (const auto* tf : test_funcs) {
            std::string ret_type = mir_type_to_llvm(tf->return_type);
            if (ret_type == "void") {
                emitln("  call void @" + quote_func_name(tf->name) + "()");
            } else {
                emitln("  call " + ret_type + " @" + quote_func_name(tf->name) + "()");
            }
        }
    } else if (main_func) {
        // Fallback: call main renamed to tml_main
        std::string main_ret = mir_type_to_llvm(main_func->return_type);
        if (main_ret == "void") {
            emitln("  call void @tml_main()");
        } else {
            emitln("  call i32 @tml_main()");
        }
    }

    // Write coverage data to file before returning.
    // The test coordinator sets TML_COVERAGE_FILE env var; we read it and write
    // the coverage data if the env var is set.
    if (options_.coverage_enabled) {
        emitln("  %cov_file_env = call ptr @getenv(ptr @.tml_cov_file_env)");
        emitln("  %cov_file_not_null = icmp ne ptr %cov_file_env, null");
        emitln("  br i1 %cov_file_not_null, label %write_cov_file, label %cov_file_done");
        emitln("");
        emitln("write_cov_file:");
        emitln("  call void @tml_coverage_write_file(ptr %cov_file_env)");
        emitln("  br label %cov_file_done");
        emitln("");
        emitln("cov_file_done:");
    }

    emitln("  ret i32 0");
    emitln("}");
    emitln();
}

void MirCodegen::emit_route_registration(const mir::Module& module) {
    struct RouteEntry {
        std::string method_str;
        std::string path;
        std::string func_name;
    };
    std::vector<RouteEntry> routes;

    for (const auto& func : module.functions) {
        if (!func.route_info.has_value())
            continue;
        std::string method_str;
        switch (func.route_info->method) {
        case mir::RouteMethod::Get:
            method_str = "GET";
            break;
        case mir::RouteMethod::Post:
            method_str = "POST";
            break;
        case mir::RouteMethod::Put:
            method_str = "PUT";
            break;
        case mir::RouteMethod::Delete:
            method_str = "DELETE";
            break;
        case mir::RouteMethod::Patch:
            method_str = "PATCH";
            break;
        case mir::RouteMethod::Head:
            method_str = "HEAD";
            break;
        case mir::RouteMethod::Options:
            method_str = "OPTIONS";
            break;
        }
        routes.push_back({method_str, func.route_info->path, func.name});
    }

    if (routes.empty())
        return;

    emitln("; Route registration from @Get/@Post/@Put/@Delete/@Patch/@Head/@Options decorators");
    emitln("; Inline registration: writes directly to the flat handler table (24 bytes/entry)");
    emitln("define void @__tml_register_routes(i64 %table, ptr %count_ptr, i64 %trees) {");
    emitln("entry:");
    emitln("  %count_init = load i64, ptr %count_ptr");

    for (size_t i = 0; i < routes.size(); ++i) {
        const auto& route = routes[i];
        std::string idx = std::to_string(i);
        std::string method_global = string_constants_[route.method_str];
        std::string path_global = string_constants_[route.path];
        size_t method_len = route.method_str.size() + 1;
        size_t path_len = route.path.size() + 1;

        // Compute offset = (count + i) * 24
        emitln("  %slot_" + idx + " = add i64 %count_init, " + std::to_string(i));
        emitln("  %offset_" + idx + " = mul i64 %slot_" + idx + ", 24");
        emitln("  %base_" + idx + " = add i64 %table, %offset_" + idx);

        // Get method and path string pointers
        emitln("  %method_" + idx + " = getelementptr [" + std::to_string(method_len) +
               " x i8], ptr " + method_global + ", i32 0, i32 0");
        emitln("  %path_" + idx + " = getelementptr [" + std::to_string(path_len) + " x i8], ptr " +
               path_global + ", i32 0, i32 0");
        emitln("  %handler_" + idx + " = ptrtoint ptr @" + quote_func_name(route.func_name) +
               " to i64");

        // Write method ptr at offset + 0
        emitln("  %method_i64_" + idx + " = ptrtoint ptr %method_" + idx + " to i64");
        emitln("  %mptr_" + idx + " = inttoptr i64 %base_" + idx + " to ptr");
        emitln("  store i64 %method_i64_" + idx + ", ptr %mptr_" + idx);

        // Write path ptr at offset + 8
        emitln("  %path_off_" + idx + " = add i64 %base_" + idx + ", 8");
        emitln("  %path_i64_" + idx + " = ptrtoint ptr %path_" + idx + " to i64");
        emitln("  %pptr_" + idx + " = inttoptr i64 %path_off_" + idx + " to ptr");
        emitln("  store i64 %path_i64_" + idx + ", ptr %pptr_" + idx);

        // Write handler ptr at offset + 16
        emitln("  %hndl_off_" + idx + " = add i64 %base_" + idx + ", 16");
        emitln("  %hptr_" + idx + " = inttoptr i64 %hndl_off_" + idx + " to ptr");
        emitln("  store i64 %handler_" + idx + ", ptr %hptr_" + idx);
    }

    // Update count = count + N
    emitln("  %new_count = add i64 %count_init, " + std::to_string(routes.size()));
    emitln("  store i64 %new_count, ptr %count_ptr");

    emitln("  ret void");
    emitln("}");
    emitln();
}

void MirCodegen::emit_preamble() {
    emit_comment("Generated by TML MIR Codegen");

    // Target datalayout computed from the target triple
    auto target = Target::from_triple(options_.target_triple);
    if (target) {
        emitln("target datalayout = \"" + target->to_data_layout() + "\"");
    }

    emitln("target triple = \"" + options_.target_triple + "\"");
    emitln();

    // Compiler identification embedded in the binary
    std::string ident = "tml version " + std::string(tml::VERSION);
    emitln("$__tml_ident = comdat any");
    emitln("@__tml_ident = linkonce_odr constant [" + std::to_string(ident.size() + 1) +
           " x i8] c\"" + ident + "\\00\", comdat, align 1");
    emitln("@llvm.used = appending global [1 x ptr] [ptr @__tml_ident], section \"llvm.metadata\"");
    emitln();

    // Declare printf, println, print, and abort for print builtins.
    // Type-specific print functions (print_i32, print_i64, etc.) are needed because
    // println/print are polymorphic in TML but map to type-specific C runtime functions.
    // All non-intrinsic declarations use dso_local to prevent LLVM 23+ from merging
    // function declarations with similar signatures during codegen.
    emitln("declare dso_local i32 @printf(ptr, ...)");
    emitln("declare dso_local void @print(ptr)");
    emitln("declare dso_local void @println(ptr)");
    emitln("declare dso_local void @print_i32(i32)");
    emitln("declare dso_local void @print_i64(i64)");
    emitln("declare dso_local void @print_f64(double)");
    emitln("declare dso_local void @print_bool(i32)");
    emitln("declare dso_local void @abort() noreturn");
    // Exception handling personality function
#ifdef _WIN32
    emitln("declare i32 @__CxxFrameHandler3(...)");
#else
    emitln("declare i32 @__gxx_personality_v0(...)");
#endif
    // str_concat/_3/_4 — removed (Phase 49); time_ns — removed (Phase 49, 0 MIR callers)
    emitln("declare dso_local ptr @mem_alloc(i64)");
    emitln("declare dso_local void @mem_free(ptr)");
    emitln("declare dso_local void @tml_set_test_timeout(i32)");
    emitln("declare dso_local i32 @tml_run_test_with_catch(ptr)");
    emitln("declare dso_local i64 @strlen(ptr)");
    emitln("declare dso_local ptr @malloc(i64)");
    emitln("declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)");
    emitln("declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1)");
    emitln("declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)");

    // CPU profiler instrumentation declarations (tml_profiler_enter/exit from profiler.cpp)
    if (options_.instrument_profiler) {
        emitln("declare dso_local void @tml_profiler_enter(ptr, ptr, i32)");
        emitln("declare dso_local void @tml_profiler_exit()");
        emitln("declare dso_local i32 @tml_profiler_is_active()");
    }

    // Coverage instrumentation declarations (tml_cover_func/tml_coverage_write_file)
    if (options_.coverage_enabled) {
        emitln("declare dso_local void @tml_cover_func(ptr)");
        emitln("declare dso_local void @tml_coverage_write_file(ptr)");
        emitln("declare dso_local ptr @getenv(ptr)");
        emitln("@.tml_cov_file_env = private constant [18 x i8] c\"TML_COVERAGE_FILE\\00\"");
    }
    emitln();

    // str_concat_opt: null-safe string concatenation (inlined from runtime.cpp)
    emitln("@.str.empty = private constant [1 x i8] c\"\\00\"");
    emitln("define internal ptr @str_concat_opt(ptr %a, ptr %b) {");
    emitln("entry:");
    emitln("  %a_null = icmp eq ptr %a, null");
    emitln("  %a_safe = select i1 %a_null, ptr @.str.empty, ptr %a");
    emitln("  %b_null = icmp eq ptr %b, null");
    emitln("  %b_safe = select i1 %b_null, ptr @.str.empty, ptr %b");
    emitln("  %len_a = call i64 @strlen(ptr %a_safe)");
    emitln("  %len_b = call i64 @strlen(ptr %b_safe)");
    emitln("  %total = add i64 %len_a, %len_b");
    emitln("  %alloc = add i64 %total, 1");
    emitln("  %buf = call ptr @malloc(i64 %alloc)");
    emitln("  call void @llvm.memcpy.p0.p0.i64(ptr %buf, ptr %a_safe, i64 %len_a, i1 false)");
    emitln("  %dst = getelementptr i8, ptr %buf, i64 %len_a");
    emitln("  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %b_safe, i64 %len_b, i1 false)");
    emitln("  %end = getelementptr i8, ptr %buf, i64 %total");
    emitln("  store i8 0, ptr %end");
    emitln("  ret ptr %buf");
    emitln("}");
    emitln();

    // Inline to_string implementations for template literal support.
    // These use snprintf to convert primitives to heap-allocated strings,
    // eliminating the dependency on TML library functions (which aren't
    // available in standalone MIR builds).
    emitln("declare dso_local i32 @snprintf(ptr, i64, ptr, ...)");
    emitln("declare dso_local void @tml_str_free(ptr)");
    emitln("@.fmt.i64 = private constant [5 x i8] c\"%lld\\00\"");
    emitln("@.fmt.i32 = private constant [3 x i8] c\"%d\\00\"");
    emitln("@.fmt.f64 = private constant [3 x i8] c\"%g\\00\"");
    emitln();

    // I64::to_string — snprintf into malloc'd buffer
    emitln("define internal ptr @tml_N4core3I649to_stringE(i64 %v) {");
    emitln("entry:");
    emitln("  %buf = call ptr @malloc(i64 24)");
    emitln("  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 24, ptr @.fmt.i64, i64 %v)");
    emitln("  ret ptr %buf");
    emitln("}");

    // I32::to_string
    emitln("define internal ptr @tml_N4core3I329to_stringE(i32 %v) {");
    emitln("entry:");
    emitln("  %buf = call ptr @malloc(i64 16)");
    emitln("  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 16, ptr @.fmt.i32, i32 %v)");
    emitln("  ret ptr %buf");
    emitln("}");

    // I16::to_string — extend to i32
    emitln("define internal ptr @tml_N4core3I169to_stringE(i16 %v) {");
    emitln("entry:");
    emitln("  %ext = sext i16 %v to i32");
    emitln("  %buf = call ptr @malloc(i64 8)");
    emitln("  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 8, ptr @.fmt.i32, i32 %ext)");
    emitln("  ret ptr %buf");
    emitln("}");

    // I8::to_string — extend to i32
    emitln("define internal ptr @tml_N4core2I89to_stringE(i8 %v) {");
    emitln("entry:");
    emitln("  %ext = sext i8 %v to i32");
    emitln("  %buf = call ptr @malloc(i64 8)");
    emitln("  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 8, ptr @.fmt.i32, i32 %ext)");
    emitln("  ret ptr %buf");
    emitln("}");

    // I128::to_string — truncate to i64 (approx)
    emitln("define internal ptr @tml_N4core4I1289to_stringE(i128 %v) {");
    emitln("entry:");
    emitln("  %trunc = trunc i128 %v to i64");
    emitln("  %buf = call ptr @malloc(i64 24)");
    emitln(
        "  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 24, ptr @.fmt.i64, i64 %trunc)");
    emitln("  ret ptr %buf");
    emitln("}");

    // F64::to_string
    emitln("define internal ptr @tml_N4core3F649to_stringE(double %v) {");
    emitln("entry:");
    emitln("  %buf = call ptr @malloc(i64 32)");
    emitln("  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 32, ptr @.fmt.f64, double %v)");
    emitln("  ret ptr %buf");
    emitln("}");

    // F32::to_string — extend to double
    emitln("define internal ptr @tml_N4core3F329to_stringE(float %v) {");
    emitln("entry:");
    emitln("  %ext = fpext float %v to double");
    emitln("  %buf = call ptr @malloc(i64 32)");
    emitln(
        "  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 32, ptr @.fmt.f64, double %ext)");
    emitln("  ret ptr %buf");
    emitln("}");

    // Bool::to_string
    emitln("define internal ptr @tml_N4core4Bool9to_stringE(i1 %v) {");
    emitln("entry:");
    emitln("  %r = select i1 %v, ptr @.str.bool.true, ptr @.str.bool.false");
    emitln("  ret ptr %r");
    emitln("}");

    // Aliases for MIR method dispatch (which generates lowercase_type + "_" + method)
    // These handle cases where the MIR builder creates CallInst with bare function names
    emitln("@to_string = internal alias ptr (i1), ptr @tml_N4core4Bool9to_stringE");
    emitln();

    // Black box functions (prevent optimization)
    emitln("declare i32 @black_box_i32(i32)");
    emitln("declare i64 @black_box_i64(i64)");
    emitln("declare double @black_box_f64(double)");
    emitln();

    // String format constants
    // %d\n\0 = 4 chars, %lld\n\0 = 6 chars, %f\n\0 = 4 chars, %s\n\0 = 4 chars
    emitln("@.str.int = private constant [4 x i8] c\"%d\\0A\\00\"");
    emitln("@.str.long = private constant [6 x i8] c\"%lld\\0A\\00\"");
    emitln("@.str.float = private constant [4 x i8] c\"%f\\0A\\00\"");
    emitln("@.str.str = private constant [4 x i8] c\"%s\\0A\\00\"");
    emitln("@.str.bool.true = private constant [5 x i8] c\"true\\00\"");
    emitln("@.str.bool.false = private constant [6 x i8] c\"false\\00\"");
    emitln(
        "@.str.sq = private constant [2 x i8] c\"'\\00\""); // Single quote for Char::debug_string
    emitln(
        "@.str.dq = private constant [2 x i8] c\"\\22\\00\""); // Double quote for Str::debug_string
    emitln("@.str.assert = private constant [18 x i8] c\"assertion failed\\0A\\00\"");
    emitln();

    // digit_pairs lookup table removed — was only used by Text V8-style optimizations

    // Assert implementation
    emitln("define internal void @assert(i1 %cond) {");
    emitln("entry:");
    emitln("    br i1 %cond, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [18 x i8], ptr @.str.assert, i32 0, i32 0");
    emitln("    call i32 @printf(ptr %msg)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Assert_eq implementation for i64
    emitln(
        "@.str.assert_eq = private constant [32 x i8] c\"assert_eq failed: %lld != %lld\\0A\\00\"");
    emitln("define internal void @assert_eq(i64 %a, i64 %b) {");
    emitln("entry:");
    emitln("    %cmp = icmp eq i64 %a, %b");
    emitln("    br i1 %cmp, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [32 x i8], ptr @.str.assert_eq, i32 0, i32 0");
    emitln("    call i32 (ptr, ...) @printf(ptr %msg, i64 %a, i64 %b)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Assert_eq implementation for i32
    emitln(
        "@.str.assert_eq_i32 = private constant [28 x i8] c\"assert_eq failed: %d != %d\\0A\\00\"");
    emitln("define internal void @assert_eq_i32(i32 %a, i32 %b) {");
    emitln("entry:");
    emitln("    %cmp = icmp eq i32 %a, %b");
    emitln("    br i1 %cmp, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [28 x i8], ptr @.str.assert_eq_i32, i32 0, i32 0");
    emitln("    call i32 (ptr, ...) @printf(ptr %msg, i32 %a, i32 %b)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Assert_eq implementation for strings (ptr) — uses strcmp for content comparison
    emitln("declare i32 @strcmp(ptr, ptr)");
    emitln(
        "@.str.assert_eq_str = private constant [28 x i8] c\"assert_eq failed: %s != %s\\0A\\00\"");
    emitln("define internal void @assert_eq_str(ptr %a, ptr %b) {");
    emitln("entry:");
    emitln("    %cmp = call i32 @strcmp(ptr %a, ptr %b)");
    emitln("    %eq = icmp eq i32 %cmp, 0");
    emitln("    br i1 %eq, label %ok, label %fail");
    emitln("ok:");
    emitln("    ret void");
    emitln("fail:");
    emitln("    %msg = getelementptr [28 x i8], ptr @.str.assert_eq_str, i32 0, i32 0");
    emitln("    call i32 (ptr, ...) @printf(ptr %msg, ptr %a, ptr %b)");
    emitln("    call void @abort()");
    emitln("    unreachable");
    emitln("}");
    emitln();

    // Drop functions (no-ops for simple types) - alwaysinline for zero overhead
    emitln("define internal void @drop_Ptr(ptr %p) alwaysinline {");
    emitln("entry:");
    emitln("    ret void");
    emitln("}");
    emitln();

    emitln("define internal void @drop_F64(double %v) alwaysinline {");
    emitln("entry:");
    emitln("    ret void");
    emitln("}");
    emitln();
}

void MirCodegen::emit_type_defs(const mir::Module& module) {
    // Emit struct definitions
    for (const auto& s : module.structs) {
        emit_struct_def(s);
    }

    // Emit struct definitions for imported/library structs used in StructInitInst
    // (e.g., Range[T] from core/ops — not in module.structs but used via range expressions)
    for (const auto& [struct_name, field_types] : used_struct_types_) {
        std::string type_name = "%struct." + struct_name;
        if (!emitted_types_.count(type_name)) {
            emitted_types_.insert(type_name);
            emit(type_name + " = type { ");
            std::vector<std::string> llvm_field_types;
            for (size_t i = 0; i < field_types.size(); ++i) {
                if (i > 0) {
                    emit(", ");
                }
                std::string ft = field_types[i] ? mir_type_to_llvm(field_types[i]) : "i64";
                emit(ft);
                llvm_field_types.push_back(ft);
            }
            emitln(" }");
            struct_field_types_[struct_name] = std::move(llvm_field_types);
        }
    }

    // Emit enum definitions (local enums)
    for (const auto& e : module.enums) {
        emit_enum_def(e);
    }

    // Emit definitions for imported enums used in EnumInitInst
    // These are enums not defined in the current module but used via imports
    // Note: Use %struct. prefix to be consistent with AST-based codegen
    for (const auto& enum_name : used_enum_types_) {
        std::string type_name = "%struct." + enum_name;
        if (!emitted_types_.count(type_name)) {
            // Emit a simple enum type (just tag) for imported enums
            // Enums without payloads like Ordering use { i32 }
            emitln(type_name + " = type { i32 }");
            emitted_types_.insert(type_name);
        }
    }

    // Emit definitions for generic enum types collected from function signatures
    // These are enums like Maybe[Str], Maybe[I32], Outcome[I32, Str], etc.
    for (const auto& [mangled_name, payload_size] : generic_enum_defs_) {
        std::string type_name = "%struct." + mangled_name;
        if (!emitted_types_.count(type_name)) {
            if (payload_size > 0) {
                emitln(type_name + " = type { i32, [" + std::to_string(payload_size) + " x i8] }");
            } else {
                emitln(type_name + " = type { i32 }");
            }
            emitted_types_.insert(type_name);
        }
    }

    if (!module.structs.empty() || !module.enums.empty() || !used_enum_types_.empty() ||
        !generic_enum_defs_.empty() || !used_struct_types_.empty()) {
        emitln();
    }

    // Emit drop functions for struct types (no-ops, just for RAII compatibility)
    // Use alwaysinline for zero overhead
    for (const auto& s : module.structs) {
        std::string type_name = "%struct." + s.name;
        emitln("define internal void @drop_" + s.name + "(" + type_name + " %v) alwaysinline {");
        emitln("entry:");
        emitln("    ret void");
        emitln("}");
        emitln();
    }
}

void MirCodegen::emit_struct_def(const mir::StructDef& s) {
    std::string type_name = "%struct." + s.name;
    if (emitted_types_.count(type_name)) {
        return;
    }
    emitted_types_.insert(type_name);

    // Store field types for later use in struct initialization coercion
    std::vector<std::string> field_types;

    emit(type_name + " = type { ");
    for (size_t i = 0; i < s.fields.size(); ++i) {
        if (i > 0) {
            emit(", ");
        }
        std::string field_type = mir_type_to_llvm(s.fields[i].type);
        emit(field_type);
        field_types.push_back(field_type);
    }
    emitln(" }");

    struct_field_types_[s.name] = std::move(field_types);
}

void MirCodegen::emit_enum_def(const mir::EnumDef& e) {
    // Enums are represented as tagged unions
    // { i32 tag, [max_payload_size x i8] payload }
    // Use %struct. prefix to be consistent with AST-based codegen
    std::string type_name = "%struct." + e.name;
    if (emitted_types_.count(type_name)) {
        return;
    }
    emitted_types_.insert(type_name);

    // Calculate max payload size
    size_t max_payload_size = 0;
    bool has_payload = false;
    for (const auto& v : e.variants) {
        size_t payload_size = 0;
        for (const auto& t : v.payload_types) {
            has_payload = true;
            // Estimate size based on type
            if (t->is_integer()) {
                payload_size += t->bit_width() / 8;
            } else if (t->is_float()) {
                payload_size += t->bit_width() / 8;
            } else if (t->is_bool()) {
                payload_size += 1;
            } else if (std::holds_alternative<mir::MirPointerType>(t->kind)) {
                payload_size += 8; // 64-bit pointer
            } else if (auto* p = std::get_if<mir::MirPrimitiveType>(&t->kind);
                       p && p->kind == mir::PrimitiveType::Str) {
                payload_size += 8; // String pointer
            } else {
                payload_size += 8; // Default
            }
        }
        max_payload_size = std::max(max_payload_size, payload_size);
    }

    // For simple enums without payloads (like Ordering), use just { i32 }
    if (!has_payload) {
        emitln(type_name + " = type { i32 }");
    } else {
        // Minimum 8 bytes for alignment
        if (max_payload_size < 8) {
            max_payload_size = 8;
        }
        emitln(type_name + " = type { i32, [" + std::to_string(max_payload_size) + " x i8] }");
    }
}

void MirCodegen::emit_function(const mir::Function& func) {
    current_func_ = func.name;
    current_func_ret_type_ = func.return_type ? mir_type_to_llvm(func.return_type) : "void";
    value_regs_.clear();
    block_labels_.clear();
    block_exit_labels_.clear();
    value_types_.clear();      // Clear type tracking for new function
    bounds_check_counter_ = 0; // Reset per-function (see pre-scan below)

    // Setup block labels - use block ID, not index
    for (const auto& blk : func.blocks) {
        block_labels_[blk.id] = blk.name;
    }

    // Pre-scan: populate block_exit_labels_ before any block is emitted.
    // Bounds check injection in emit_instruction() splits MIR blocks into
    // multiple LLVM blocks (block -> bc.panic.N + bc.ok.N). Phi nodes in
    // earlier blocks reference later blocks by ID, so they need to know the
    // correct exit label (bc.ok.N) before those blocks are processed.
    // We use bounds_check_counter_ (separate from temp_counter_) so the scan
    // predicts labels accurately without simulating all temp_counter_ uses.
    {
        int bc_scan = 0;
        for (const auto& blk : func.blocks) {
            block_exit_labels_[blk.id] = blk.name; // default: entry label
            for (const auto& inst : blk.instructions) {
                std::visit(
                    [&](const auto& i) {
                        using T = std::decay_t<decltype(i)>;
                        if constexpr (std::is_same_v<T, mir::GetElementPtrInst>) {
                            if (i.needs_bounds_check && i.known_array_size >= 0 &&
                                !i.indices.empty()) {
                                // This GEP will emit bc.panic.N + bc.ok.N and update exit label
                                std::string ok = "bc.ok." + std::to_string(bc_scan++);
                                block_exit_labels_[blk.id] = ok;
                            }
                        }
                    },
                    inst.inst);
            }
        }
    }
    // Reset bounds_check_counter_ so actual emission matches the pre-scan values
    bounds_check_counter_ = 0;

    // Find fallback label for missing block targets
    // Prefer first block with a return terminator, otherwise use last block
    fallback_label_.clear();
    for (const auto& blk : func.blocks) {
        if (blk.terminator.has_value()) {
            if (std::holds_alternative<mir::ReturnTerm>(*blk.terminator)) {
                fallback_label_ = blk.name;
                break;
            }
        }
    }
    // If no return block found, use the last block
    if (fallback_label_.empty() && !func.blocks.empty()) {
        fallback_label_ = func.blocks.back().name;
    }

    // Setup parameter registers and track parameter info for indirect calls
    param_info_.clear();
    for (const auto& param : func.params) {
        value_regs_[param.value_id] = "%" + param.name;
        // Also store parameter types for correct type tracking
        if (param.type) {
            value_types_[param.value_id] = mir_type_to_llvm(param.type);
            // Track parameter info for function pointer indirect calls
            param_info_[param.name] = {param.value_id, param.type};
        }
    }

    // Function signature
    // In suite mode (force_internal_linkage), all user-defined functions get
    // internal linkage to prevent duplicate symbol errors when multiple test
    // files in the same suite define the same function names (e.g., main).
    // The test entry wrapper (tml_test_N) is generated separately with
    // dllexport linkage — user's main() is just another internal function.
    std::string linkage = "define";
    if (options_.dll_export && func.is_public) {
        linkage = "define dllexport";
    } else if (options_.force_internal_linkage) {
        linkage = "define internal";
    }

    // Add inline hints for small functions to help LLVM optimizer
    // When coverage is enabled, skip inlining so functions can be instrumented
    std::string inline_attr;
    if (!options_.coverage_enabled) {
        size_t total_instructions = 0;
        for (const auto& blk : func.blocks) {
            total_instructions += blk.instructions.size();
        }

        // Check if this is an iterator method that should always inline
        // Iterator methods are critical for zero-cost abstraction in for loops
        bool is_iterator_method = func.name.find("Iter__next") != std::string::npos ||
                                  func.name.find("__into_iter") != std::string::npos ||
                                  func.name.find("ArrayIter__") != std::string::npos ||
                                  func.name.find("SliceIter__") != std::string::npos ||
                                  func.name.find("Chunks__next") != std::string::npos ||
                                  func.name.find("Windows__next") != std::string::npos ||
                                  func.name.find("ChunksExact__next") != std::string::npos;

        // Small functions (<=10 instructions, single block) get inlinehint
        // drop_ functions and iterator methods get alwaysinline
        if (func.name.rfind("drop_", 0) == 0 || is_iterator_method) {
            inline_attr = " alwaysinline";
        } else if (total_instructions <= 10 && func.blocks.size() <= 2) {
            inline_attr = " inlinehint";
        }
    }

    std::string ret_type = mir_type_to_llvm(func.return_type);
    // Rename `main` based on entry mode:
    // - generate_exe_main: rename to tml_main (C entry wrapper calls it)
    // - test_entry_name: rename to e.g. tml_test_0 (dispatcher calls it)
    std::string emit_name = func.name;
    if (func.name == "main") {
        // Always rename main to tml_main; wrappers provide the public entry name
        if (options_.generate_exe_main || !options_.test_entry_name.empty())
            emit_name = "tml_main";
    }
    emit(linkage + " " + ret_type + " @" + quote_func_name(emit_name) + "(");

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i > 0) {
            emit(", ");
        }
        std::string param_type = mir_type_to_llvm(func.params[i].type);
        const auto& param_name = func.params[i].name;
        // Unit type maps to "void" but LLVM doesn't allow void as a parameter type.
        // Use "{}" (empty struct, zero-sized) as the data representation.
        if (param_type == "void") {
            param_type = "{}";
        }
        // Method 'this'/'self' parameters for struct/enum types must be passed
        // as ptr (pointer), not by value. The function body uses GEP instructions
        // that expect a pointer base, so the parameter type must match.
        if ((param_name == "this" || param_name == "self") &&
            (param_type.starts_with("%struct.") || param_type.starts_with("%enum.") ||
             param_type.starts_with("%class.") || param_type.starts_with("%union."))) {
            param_type = "ptr";
            // Update value_types_ so instructions correctly see this as ptr
            value_types_[func.params[i].value_id] = "ptr";
        }
        // If this function uses sret, the first parameter gets the sret attribute
        if (func.uses_sret && i == 0 && func.original_return_type) {
            std::string orig_ret_type = mir_type_to_llvm(func.original_return_type);
            emit(param_type + " sret(" + orig_ret_type + ") %" + param_name);
        } else {
            emit(param_type + " %" + param_name);
        }
    }

    // Add personality for exception handling (cleanup destructors on panic)
    std::string personality;
#ifdef _WIN32
    personality = " personality ptr @__CxxFrameHandler3";
#else
    personality = " personality ptr @__gxx_personality_v0";
#endif
    emitln(")" + inline_attr + personality + " {");

    // Prepare profiler entry instrumentation for the entry block (item 2.4).
    // When instrument_profiler is enabled, we inject a tml_profiler_is_active() check
    // (item 2.5) at the start of the entry block, and only call tml_profiler_enter()
    // if the profiler is actually running. This keeps overhead near-zero otherwise.
    profiler_entry_ir_.clear();
    if (options_.instrument_profiler && !func.blocks.empty()) {
        // Use func.name (original user-visible name) for profiler, not emit_name
        // (which may be mangled to tml_main).
        auto fname_it = string_constants_.find(func.name);
        auto ffile_it = string_constants_.find("<tml>");
        if (fname_it != string_constants_.end() && ffile_it != string_constants_.end()) {
            std::string t_active = new_temp();
            std::string t_cond = new_temp();
            std::string lbl_enter = "prof.enter." + std::to_string(temp_counter_);
            std::string lbl_skip = "prof.skip." + std::to_string(temp_counter_);
            temp_counter_++;

            profiler_entry_ir_ += "  " + t_active + " = call i32 @tml_profiler_is_active()\n";
            profiler_entry_ir_ += "  " + t_cond + " = icmp ne i32 " + t_active + ", 0\n";
            profiler_entry_ir_ +=
                "  br i1 " + t_cond + ", label %" + lbl_enter + ", label %" + lbl_skip + "\n";
            profiler_entry_ir_ += lbl_enter + ":\n";
            profiler_entry_ir_ += "  call void @tml_profiler_enter(ptr " + fname_it->second +
                                  ", ptr " + ffile_it->second + ", i32 0)\n";
            profiler_entry_ir_ += "  br label %" + lbl_skip + "\n";
            profiler_entry_ir_ += lbl_skip + ":\n";
        }
    }

    // Prepare coverage instrumentation for the entry block.
    // When coverage_enabled, inject a call to tml_cover_func() with the demangled function
    // name so coverage data matches source-extracted names (e.g., "Str::len" not mangled).
    coverage_entry_ir_.clear();
    if (options_.coverage_enabled && !func.blocks.empty()) {
        // Look up the clean name via coverage_name_map_, then find its string constant
        auto cov_it = coverage_name_map_.find(func.name);
        if (cov_it != coverage_name_map_.end()) {
            auto fname_it = string_constants_.find(cov_it->second);
            if (fname_it != string_constants_.end()) {
                coverage_entry_ir_ = "  call void @tml_cover_func(ptr " + fname_it->second + ")\n";
            }
        }
    }

    // Emit basic blocks
    for (const auto& block : func.blocks) {
        emit_block(block);
    }

    emitln("}");
    emitln();
}

void MirCodegen::emit_block(const mir::BasicBlock& block) {
    emitln(block.name + ":");

    // Inject profiler entry instrumentation at the start of the entry block.
    // profiler_entry_ir_ is prepared by emit_function() and consumed here once.
    if (!profiler_entry_ir_.empty()) {
        emit(profiler_entry_ir_);
        profiler_entry_ir_.clear();
    }

    // Inject coverage instrumentation at the start of the entry block.
    // coverage_entry_ir_ is prepared by emit_function() and consumed here once.
    if (!coverage_entry_ir_.empty()) {
        emit(coverage_entry_ir_);
        coverage_entry_ir_.clear();
    }

    // Track current block ID for exit label updates (bounds check injection etc.)
    // NOTE: block_exit_labels_[block.id] was pre-populated in emit_function() pre-scan.
    // We do NOT reset it here to avoid overwriting the pre-scanned exit label.
    current_block_id_ = block.id;

    // Emit instructions
    for (const auto& inst : block.instructions) {
        emit_instruction(inst);
    }

    // Emit terminator
    if (block.terminator.has_value()) {
        emit_terminator(*block.terminator);
    }
}

} // namespace tml::codegen
