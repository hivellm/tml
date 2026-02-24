TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Support Functions
//!
//! This file implements support functions for the LLVM IR generator:
//! - Loop metadata generation
//! - Lifetime intrinsics (scope-based alloca tracking)
//! - Print type inference
//! - Namespace support (qualified names, namespace declarations)
//! - Library state capture (serializing codegen state for parallel builds)

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "common.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"

#include <filesystem>
#include <iomanip>
#include <set>

namespace tml::codegen {

// Helper: Get the LLVM type string for a constant's declared type
// For primitives like I32, I64, Bool, etc.
// NOTE: Duplicated from generate.cpp - this static helper is used by gen_namespace_decl().
static std::string get_const_llvm_type(const parser::TypePtr& type) {
    if (!type)
        return "i64"; // Default fallback

    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            const std::string& name = named.path.segments.back();
            // Map TML primitive types to LLVM types
            if (name == "I8" || name == "U8")
                return "i8";
            if (name == "I16" || name == "U16")
                return "i16";
            if (name == "I32" || name == "U32")
                return "i32";
            if (name == "I64" || name == "U64")
                return "i64";
            if (name == "I128" || name == "U128")
                return "i128";
            if (name == "Bool")
                return "i1";
            if (name == "Isize" || name == "Usize")
                return "i64";
        }
    } else if (type->is<parser::TupleType>()) {
        const auto& tuple = type->as<parser::TupleType>();
        if (tuple.elements.empty())
            return "{}";
        std::string result = "{ ";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += get_const_llvm_type(tuple.elements[i]);
        }
        result += " }";
        return result;
    }
    return "i64"; // Default for unknown types
}

/// Try to extract a compile-time constant scalar value from an expression.
static std::string try_extract_scalar_const(const parser::Expr* expr) {
    if (!expr)
        return "";
    if (expr->is<parser::CastExpr>()) {
        const auto& cast = expr->as<parser::CastExpr>();
        if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
            expr = cast.expr.get();
        } else if (cast.expr && cast.expr->is<parser::UnaryExpr>()) {
            const auto& unary = cast.expr->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                const auto& lit = unary.operand->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral)
                    return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
            }
            return "";
        } else {
            return "";
        }
    }
    if (expr->is<parser::UnaryExpr>()) {
        const auto& unary = expr->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
            const auto& lit = unary.operand->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral)
                return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
        }
        return "";
    }
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral)
            return std::to_string(lit.token.int_value().value);
        if (lit.token.kind == lexer::TokenKind::BoolLiteral)
            return lit.token.bool_value() ? "1" : "0";
        if (lit.token.kind == lexer::TokenKind::NullLiteral)
            return "null";
    }
    return "";
}

/// Try to extract a compile-time constant value (scalar or tuple) from an expression.
static std::string try_extract_const_value(const parser::Expr* expr, const parser::TypePtr& type,
                                           std::string& out_llvm_type) {
    if (!expr)
        return "";
    if (expr->is<parser::TupleExpr>()) {
        const auto& tuple = expr->as<parser::TupleExpr>();
        if (tuple.elements.empty()) {
            out_llvm_type = "{}";
            return "zeroinitializer";
        }
        std::vector<std::string> elem_types;
        if (type && type->is<parser::TupleType>()) {
            for (const auto& et : type->as<parser::TupleType>().elements)
                elem_types.push_back(get_const_llvm_type(et));
        }
        std::vector<std::string> elem_values;
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            std::string val = try_extract_scalar_const(tuple.elements[i].get());
            if (val.empty())
                return "";
            elem_values.push_back(val);
        }
        if (elem_types.size() != elem_values.size()) {
            elem_types.clear();
            for (size_t i = 0; i < elem_values.size(); ++i)
                elem_types.push_back("i64");
        }
        std::string llvm_type = "{ ";
        std::string llvm_value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0) {
                llvm_type += ", ";
                llvm_value += ", ";
            }
            llvm_type += elem_types[i];
            llvm_value += elem_types[i] + " " + elem_values[i];
        }
        llvm_type += " }";
        llvm_value += " }";
        out_llvm_type = llvm_type;
        return llvm_value;
    }
    std::string scalar = try_extract_scalar_const(expr);
    if (!scalar.empty()) {
        out_llvm_type = get_const_llvm_type(type);
        return scalar;
    }
    return "";
}

// ============ Loop Metadata Implementation ============

auto LLVMIRGen::create_loop_metadata(bool enable_vectorize, int unroll_count) -> int {
    int loop_id = loop_metadata_counter_++;

    // Build the loop metadata node
    // Format: !N = distinct !{!N, !M, !O, ...} where M, O are property nodes
    std::stringstream meta;
    meta << "!" << loop_id << " = distinct !{!" << loop_id;

    // Add property nodes
    std::vector<int> prop_ids;

    // Vectorization hint
    if (enable_vectorize) {
        int vec_id = loop_metadata_counter_++;
        loop_metadata_.push_back("!" + std::to_string(vec_id) +
                                 " = !{!\"llvm.loop.vectorize.enable\", i1 true}");
        prop_ids.push_back(vec_id);
    }

    // Unroll hint
    if (unroll_count > 0) {
        int unroll_id = loop_metadata_counter_++;
        loop_metadata_.push_back("!" + std::to_string(unroll_id) +
                                 " = !{!\"llvm.loop.unroll.count\", i32 " +
                                 std::to_string(unroll_count) + "}");
        prop_ids.push_back(unroll_id);
    }

    // Add property references to loop node
    for (int prop_id : prop_ids) {
        meta << ", !" << prop_id;
    }
    meta << "}";

    loop_metadata_.push_back(meta.str());
    return loop_id;
}

void LLVMIRGen::emit_loop_metadata() {
    if (loop_metadata_.empty()) {
        return;
    }

    emit_line("");
    emit_line("; Loop optimization metadata");
    for (const auto& meta : loop_metadata_) {
        emit_line(meta);
    }
}

// ============ Lifetime Intrinsics Implementation ============

void LLVMIRGen::push_lifetime_scope() {
    scope_allocas_.push_back({});
}

void LLVMIRGen::pop_lifetime_scope() {
    if (scope_allocas_.empty()) {
        return;
    }

    // Emit lifetime.end for all allocas in this scope (in reverse order)
    auto& allocas = scope_allocas_.back();
    for (auto it = allocas.rbegin(); it != allocas.rend(); ++it) {
        emit_lifetime_end(it->reg, it->size);
    }

    scope_allocas_.pop_back();
}

void LLVMIRGen::clear_lifetime_scope() {
    // Just pop the scope without emitting lifetime.end
    // Used when lifetime.end was already emitted via emit_scope_lifetime_ends()
    if (!scope_allocas_.empty()) {
        scope_allocas_.pop_back();
    }
}

void LLVMIRGen::emit_lifetime_start(const std::string& alloca_reg, int64_t size) {
    // Use -1 for unknown size (LLVM will figure it out)
    std::string size_str = size > 0 ? std::to_string(size) : "-1";
    emit_line("  call void @llvm.lifetime.start.p0(i64 " + size_str + ", ptr " + alloca_reg + ")");
}

void LLVMIRGen::emit_lifetime_end(const std::string& alloca_reg, int64_t size) {
    // Use -1 for unknown size (LLVM will figure it out)
    std::string size_str = size > 0 ? std::to_string(size) : "-1";
    emit_line("  call void @llvm.lifetime.end.p0(i64 " + size_str + ", ptr " + alloca_reg + ")");
}

void LLVMIRGen::register_alloca_in_scope(const std::string& alloca_reg, int64_t size) {
    if (scope_allocas_.empty()) {
        return; // No scope to register in
    }
    scope_allocas_.back().push_back(AllocaInfo{alloca_reg, size});
}

void LLVMIRGen::emit_all_lifetime_ends() {
    // Emit lifetime.end for all allocas in all scopes (innermost first)
    // Used for early return - doesn't pop the scopes since we're exiting
    for (auto scope_it = scope_allocas_.rbegin(); scope_it != scope_allocas_.rend(); ++scope_it) {
        for (auto it = scope_it->rbegin(); it != scope_it->rend(); ++it) {
            emit_lifetime_end(it->reg, it->size);
        }
    }
}

void LLVMIRGen::emit_scope_lifetime_ends() {
    // Emit lifetime.end for allocas in current scope only (for break/continue)
    // Doesn't pop the scope since the block will handle that
    if (scope_allocas_.empty()) {
        return;
    }
    auto& allocas = scope_allocas_.back();
    for (auto it = allocas.rbegin(); it != allocas.rend(); ++it) {
        emit_lifetime_end(it->reg, it->size);
    }
}

auto LLVMIRGen::get_type_size(const std::string& llvm_type) -> int64_t {
    // Return size in bytes for common LLVM types
    if (llvm_type == "i1")
        return 1;
    if (llvm_type == "i8")
        return 1;
    if (llvm_type == "i16")
        return 2;
    if (llvm_type == "i32")
        return 4;
    if (llvm_type == "i64")
        return 8;
    if (llvm_type == "i128")
        return 16;
    if (llvm_type == "float")
        return 4;
    if (llvm_type == "double")
        return 8;
    if (llvm_type == "ptr")
        return 8; // 64-bit pointers

    // For struct types, tuples, etc. - return -1 (unknown, LLVM will compute)
    return -1;
}

// Infer print argument type from expression
auto LLVMIRGen::infer_print_type(const parser::Expr& expr) -> PrintArgType {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
        case lexer::TokenKind::IntLiteral:
            return PrintArgType::Int;
        case lexer::TokenKind::FloatLiteral:
            return PrintArgType::Float;
        case lexer::TokenKind::BoolLiteral:
            return PrintArgType::Bool;
        case lexer::TokenKind::StringLiteral:
            return PrintArgType::Str;
        default:
            return PrintArgType::Unknown;
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
        case parser::BinaryOp::Add:
            // Check if operands are strings (string concatenation)
            if (infer_print_type(*bin.left) == PrintArgType::Str ||
                infer_print_type(*bin.right) == PrintArgType::Str) {
                return PrintArgType::Str;
            }
            // Check if operands are float
            if (infer_print_type(*bin.left) == PrintArgType::Float ||
                infer_print_type(*bin.right) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
        case parser::BinaryOp::Sub:
        case parser::BinaryOp::Mul:
        case parser::BinaryOp::Div:
        case parser::BinaryOp::Mod:
            // Check if operands are float
            if (infer_print_type(*bin.left) == PrintArgType::Float ||
                infer_print_type(*bin.right) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
        case parser::BinaryOp::Eq:
        case parser::BinaryOp::Ne:
        case parser::BinaryOp::Lt:
        case parser::BinaryOp::Gt:
        case parser::BinaryOp::Le:
        case parser::BinaryOp::Ge:
        case parser::BinaryOp::And:
        case parser::BinaryOp::Or:
            return PrintArgType::Bool;
        default:
            return PrintArgType::Int;
        }
    }
    if (expr.is<parser::UnaryExpr>()) {
        const auto& un = expr.as<parser::UnaryExpr>();
        if (un.op == parser::UnaryOp::Not)
            return PrintArgType::Bool;
        if (un.op == parser::UnaryOp::Neg) {
            // Check if operand is float
            if (infer_print_type(*un.operand) == PrintArgType::Float) {
                return PrintArgType::Float;
            }
            return PrintArgType::Int;
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        // For identifiers, we need to check the variable type
        // For now, default to Unknown (will be checked by caller)
        return PrintArgType::Unknown;
    }
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        // Check for known I64-returning functions
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& fn_name = call.callee->as<parser::IdentExpr>().name;
            if (fn_name == "time_us" || fn_name == "time_ns") {
                return PrintArgType::I64;
            }
        }
        return PrintArgType::Int; // Assume functions return int
    }
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();
        // to_string() methods return strings
        if (call.method == "to_string" || call.method == "debug_string") {
            return PrintArgType::Str;
        }
        return PrintArgType::Unknown;
    }
    return PrintArgType::Unknown;
}

// ============================================================================
// Namespace Support
// ============================================================================

auto LLVMIRGen::qualified_name(const std::string& name) const -> std::string {
    if (current_namespace_.empty()) {
        return name;
    }
    std::string result;
    for (const auto& seg : current_namespace_) {
        result += seg + ".";
    }
    return result + name;
}

void LLVMIRGen::gen_namespace_decl(const parser::NamespaceDecl& ns) {
    // Save current namespace and extend it
    auto saved_namespace = current_namespace_;
    for (const auto& seg : ns.path) {
        current_namespace_.push_back(seg);
    }

    // Process all declarations in this namespace
    for (const auto& decl : ns.items) {
        if (decl->is<parser::StructDecl>()) {
            gen_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::UnionDecl>()) {
            gen_union_decl(decl->as<parser::UnionDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            gen_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::ClassDecl>()) {
            gen_class_decl(decl->as<parser::ClassDecl>());
        } else if (decl->is<parser::InterfaceDecl>()) {
            gen_interface_decl(decl->as<parser::InterfaceDecl>());
        } else if (decl->is<parser::NamespaceDecl>()) {
            // Nested namespace - recurse
            gen_namespace_decl(decl->as<parser::NamespaceDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            register_impl(&decl->as<parser::ImplDecl>());
        } else if (decl->is<parser::FuncDecl>()) {
            gen_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ConstDecl>()) {
            const auto& const_decl = decl->as<parser::ConstDecl>();
            std::string llvm_type;
            std::string value =
                try_extract_const_value(const_decl.value.get(), const_decl.type, llvm_type);
            if (!value.empty()) {
                global_constants_[qualified_name(const_decl.name)] = {value, llvm_type};
            }
        }
    }

    // Restore namespace
    current_namespace_ = saved_namespace;
}

// ============================================================================
// Library State Capture
// ============================================================================

/// Generate declaration-only IR from full library IR text.
/// Scans for `define` lines and converts them to `declare` statements.
/// Only converts TML-generated functions (tml_ prefix), skipping C runtime functions
/// that are already declared in the preamble.
/// Extract a set of function names declared in the preamble headers.
/// Used to filter out preamble declarations when generating library decls.
static std::set<std::string> extract_preamble_func_names(const std::string& headers) {
    std::set<std::string> names;
    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line)) {
        // Match "declare ... @funcname(...)" and "define ... @funcname(...) {"
        bool is_declare = line.size() > 8 && line.substr(0, 8) == "declare ";
        bool is_define = line.size() > 7 && line.substr(0, 7) == "define ";
        if (is_declare || is_define) {
            auto at_pos = line.find('@');
            if (at_pos != std::string::npos) {
                auto paren_pos = line.find('(', at_pos);
                if (paren_pos != std::string::npos) {
                    names.insert(line.substr(at_pos, paren_pos - at_pos));
                }
            }
        }
    }
    return names;
}

/// Generate declaration-only IR from the full library IR.
/// Converts `define` to `declare` for TML library functions.
/// Also includes `declare` lines for FFI functions that are NOT in the preamble.
static std::string generate_decls_from_ir(const std::string& full_ir,
                                          const std::set<std::string>& preamble_funcs) {
    std::ostringstream decls;
    decls << "; Declarations extracted from shared library IR\n";

    std::istringstream stream(full_ir);
    std::string line;
    while (std::getline(stream, line)) {
        // Match lines starting with "define " (function definitions) -> convert to declare
        if (line.size() > 7 && line.substr(0, 7) == "define ") {
            // Use rfind to find the LAST '{' on the line — the function body opener.
            // line.find('{') would incorrectly match '{' inside struct return types
            // like "{ i64, %struct.Maybe__I64 }", producing empty/truncated signatures.
            auto brace_pos = line.rfind('{');
            if (brace_pos != std::string::npos) {
                std::string signature = line.substr(7, brace_pos - 7);
                // Trim trailing whitespace
                while (!signature.empty() && (signature.back() == ' ' || signature.back() == '\t'))
                    signature.pop_back();
                // Remove attributes like #0
                auto hash_pos = signature.rfind(" #");
                if (hash_pos != std::string::npos)
                    signature = signature.substr(0, hash_pos);
                // Strip linkage qualifiers that are invalid on declarations.
                // define internal/linkonce_odr/dllexport → declare (no qualifier)
                for (const char* qual : {"internal ", "linkonce_odr ", "dllexport ", "private "}) {
                    if (signature.substr(0, strlen(qual)) == qual) {
                        signature = signature.substr(strlen(qual));
                        break;
                    }
                }
                // Skip functions already declared in preamble (e.g. runtime defines
                // like str_eq, str_concat_opt that are emitted by emit_runtime_decls)
                auto at_pos = signature.find('@');
                if (at_pos != std::string::npos) {
                    auto paren_pos = signature.find('(', at_pos);
                    if (paren_pos != std::string::npos) {
                        std::string func_ref = signature.substr(at_pos, paren_pos - at_pos);
                        if (preamble_funcs.count(func_ref) > 0) {
                            continue; // Already in preamble, skip
                        }
                    }
                }
                // Skip empty or malformed signatures (safety check)
                if (!signature.empty() && signature.find('@') != std::string::npos) {
                    decls << "declare " << signature << "\n";
                }
            }
        }
        // Include `declare` lines for FFI functions NOT already in preamble.
        // This is needed for FFI bindings like brotli_*, zlib_* that are declared
        // during emit_module_pure_tml_functions() but not in the preamble.
        else if (line.size() > 8 && line.substr(0, 8) == "declare ") {
            // Extract function name (@funcname) to check against preamble
            auto at_pos = line.find('@');
            if (at_pos != std::string::npos) {
                auto paren_pos = line.find('(', at_pos);
                if (paren_pos != std::string::npos) {
                    std::string func_ref = line.substr(at_pos, paren_pos - at_pos);
                    if (preamble_funcs.count(func_ref) == 0) {
                        decls << line << "\n";
                    }
                }
            }
        }
    }
    return decls.str();
}

auto LLVMIRGen::capture_library_state(const std::string& full_ir,
                                      const std::string& /*preamble_headers*/) const
    -> std::shared_ptr<CodegenLibraryState> {
    auto state = std::make_shared<CodegenLibraryState>();

    // Capture library IR text (saved during generate())
    state->imported_func_code = cached_imported_func_code_;
    state->imported_type_defs = cached_imported_type_defs_;

    // Generate declarations-only IR from the full library IR.
    // Includes define->declare conversions AND non-preamble declare lines (FFI functions).
    if (!full_ir.empty()) {
        auto preamble_funcs = extract_preamble_func_names(cached_preamble_headers_);
        state->imported_func_decls = generate_decls_from_ir(full_ir, preamble_funcs);
    }

    // Capture struct types
    state->struct_types = struct_types_;
    state->union_types = union_types_;

    // Capture enum variants
    state->enum_variants = enum_variants_;

    // Capture global constants
    for (const auto& [k, v] : global_constants_) {
        state->global_constants[k] = {v.value, v.llvm_type};
    }

    // Capture struct fields
    for (const auto& [struct_name, fields] : struct_fields_) {
        std::vector<CodegenLibraryState::FieldInfoData> field_data;
        field_data.reserve(fields.size());
        for (const auto& f : fields) {
            field_data.push_back({f.name, f.index, f.llvm_type, f.semantic_type});
        }
        state->struct_fields[struct_name] = std::move(field_data);
    }

    // Capture function signatures
    for (const auto& [k, v] : functions_) {
        state->functions[k] = {v.llvm_name, v.llvm_func_type, v.ret_type, v.param_types,
                               v.is_extern};
    }

    // Capture function return types
    state->func_return_types = func_return_types_;

    // Capture trait declaration names
    for (const auto& [name, _] : trait_decls_) {
        state->trait_decl_names.insert(name);
    }

    // Capture generated functions
    state->generated_functions = generated_functions_;

    // Capture string literals (needed when restoring full function definitions)
    state->string_literals = string_literals_;

    // Capture declared externals (to prevent duplicate declarations in worker threads)
    state->declared_externals = declared_externals_;

    // Capture class types (class_name -> LLVM type name)
    state->class_types = class_types_;

    // Capture class field info
    for (const auto& [class_name, fields] : class_fields_) {
        std::vector<CodegenLibraryState::ClassFieldInfoData> field_data;
        field_data.reserve(fields.size());
        for (const auto& f : fields) {
            CodegenLibraryState::ClassFieldInfoData fd;
            fd.name = f.name;
            fd.index = f.index;
            fd.llvm_type = f.llvm_type;
            fd.vis = static_cast<int>(f.vis);
            fd.is_inherited = f.is_inherited;
            for (const auto& step : f.inheritance_path) {
                fd.inheritance_path.push_back({step.class_name, step.index});
            }
            field_data.push_back(std::move(fd));
        }
        state->class_fields[class_name] = std::move(field_data);
    }

    // Capture value classes
    state->value_classes = value_classes_;

    // Capture emitted dyn types (prevents duplicate %dyn.X type definitions)
    state->emitted_dyn_types = emitted_dyn_types_;

    // Capture loop metadata (library functions with loops emit !N metadata)
    state->loop_metadata = loop_metadata_;
    state->loop_metadata_counter = loop_metadata_counter_;

    state->valid = true;

    TML_DEBUG_LN("[CODEGEN] Captured library state: "
                 << state->struct_types.size() << " struct types, " << state->functions.size()
                 << " functions, " << state->enum_variants.size() << " enum variants");

    return state;
}

} // namespace tml::codegen
