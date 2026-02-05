//! # LLVM IR Generator - Class OOP Support
//!
//! This file implements C#-style OOP class support with virtual dispatch.
//!
//! ## Class Layout
//!
//! Each class instance contains:
//! - Vtable pointer (first field, implicit)
//! - Fields from base class (inherited, in declaration order)
//! - Fields declared in this class
//!
//! ```llvm
//! %class.Dog = type { ptr, %class.Animal, i32 }  ; vtable, base, age
//! ```
//!
//! ## Vtable Structure
//!
//! Each class has a vtable containing:
//! - Destructor pointer (slot 0)
//! - Virtual methods in declaration order
//!
//! ```llvm
//! @vtable.Dog = global { ptr, ptr, ptr } { ptr @Dog_destructor, ptr @Dog_speak, ptr @Dog_walk }
//! ```
//!
//! ## Virtual Dispatch
//!
//! Virtual method calls:
//! 1. Load vtable pointer from object (field 0)
//! 2. Load function pointer from vtable slot
//! 3. Call function with object as first arg
//!
//! ## Key Methods
//!
//! | Method                 | Purpose                                |
//! |------------------------|----------------------------------------|
//! | `gen_class_decl`       | Generate class type and vtable         |
//! | `gen_class_vtable`     | Generate vtable global constant        |
//! | `gen_class_constructor`| Generate constructor function          |
//! | `gen_virtual_call`     | Generate virtual method dispatch       |

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/token.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

namespace tml::codegen {

// Helper to extract name from FuncParam pattern
static std::string get_class_param_name(const parser::FuncParam& param) {
    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
        return param.pattern->as<parser::IdentPattern>().name;
    }
    return "_anon";
}

// Helper to check if a class has a specific decorator
static bool has_decorator(const parser::ClassDecl& c, const std::string& name) {
    for (const auto& deco : c.decorators) {
        if (deco.name == name) {
            return true;
        }
    }
    return false;
}

// Helper to check if a decorator has a specific boolean argument set to true
// Supports multiple formats:
// - @pool(thread_local = true) - BinaryExpr with Assign
// - @pool(thread_local) - IdentExpr (presence implies true)
// - @pool_tls - Alternative decorator name for thread-local
static bool has_decorator_bool_arg(const parser::ClassDecl& c, const std::string& deco_name,
                                   const std::string& arg_name) {
    for (const auto& deco : c.decorators) {
        if (deco.name != deco_name)
            continue;

        for (const auto& arg : deco.args) {
            // Check for BinaryExpr with Assign op: thread_local = true
            if (arg->is<parser::BinaryExpr>()) {
                const auto& bin = arg->as<parser::BinaryExpr>();
                if (bin.op == parser::BinaryOp::Assign && bin.left && bin.right) {
                    // Left side should be an identifier
                    if (bin.left->is<parser::IdentExpr>()) {
                        const auto& ident = bin.left->as<parser::IdentExpr>();
                        if (ident.name == arg_name) {
                            // Right side should be true literal
                            if (bin.right->is<parser::LiteralExpr>()) {
                                const auto& lit = bin.right->as<parser::LiteralExpr>();
                                if (lit.token.kind == lexer::TokenKind::BoolLiteral &&
                                    lit.token.bool_value()) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            // Check for just IdentExpr: @pool(thread_local) - presence implies true
            else if (arg->is<parser::IdentExpr>()) {
                const auto& ident = arg->as<parser::IdentExpr>();
                if (ident.name == arg_name) {
                    return true;
                }
            }
        }
    }

    // Also check for alternative decorator name: @pool_tls
    if (deco_name == "pool" && arg_name == "thread_local") {
        for (const auto& deco : c.decorators) {
            if (deco.name == "pool_tls") {
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// Class Type Generation
// ============================================================================

void LLVMIRGen::gen_class_decl(const parser::ClassDecl& c) {
    // Skip if already emitted
    if (class_types_.find(c.name) != class_types_.end()) {
        return;
    }

    // If class has generic parameters, defer generation until instantiation
    if (!c.generics.empty()) {
        pending_generic_classes_[c.name] = &c;
        return;
    }

    // Generate LLVM type name
    std::string type_name = "%class." + c.name;

    // Check if this is a @value class (no vtable, value semantics)
    // Also auto-apply value class optimization to sealed classes with no virtual methods
    bool is_value_class = has_decorator(c, "value") || env_.is_value_class_candidate(c.name);

    // Collect field types
    // Regular class layout: { vtable_ptr, base_class_fields..., own_fields... }
    // Value class layout: { base_class_fields..., own_fields... } (no vtable)
    std::vector<std::string> field_types;

    if (!is_value_class) {
        field_types.push_back("ptr"); // Vtable pointer is always first for regular classes
    }

    // If class extends another, include base class as embedded struct
    std::string base_class_name;
    int base_class_idx = -1;
    if (c.extends) {
        base_class_name = c.extends->segments.back();
        // Make sure base class type is generated first
        auto base_class = env_.lookup_class(base_class_name);
        if (base_class) {
            // If base class type hasn't been generated yet (external module), emit it now
            if (class_types_.find(base_class_name) == class_types_.end()) {
                emit_external_class_type(base_class_name, *base_class);
            }
            // Base class fields are embedded (excluding vtable since we have our own)
            // For simplicity, include base as embedded struct
            base_class_idx = static_cast<int>(field_types.size());
            field_types.push_back("%class." + base_class_name);
        }
    }

    // Add own instance fields (non-static)
    std::vector<ClassFieldInfo> field_info;
    size_t field_offset = field_types.size(); // Start after vtable (if present) and base

    // First, add inherited fields from base class (for initialization)
    // Build full inheritance path for multi-level inheritance
    if (!base_class_name.empty()) {
        auto base_fields_it = class_fields_.find(base_class_name);
        if (base_fields_it != class_fields_.end()) {
            for (const auto& base_field : base_fields_it->second) {
                // Add inherited field with full path
                ClassFieldInfo inherited;
                inherited.name = base_field.name;
                inherited.index = -1; // Not a direct index
                inherited.llvm_type = base_field.llvm_type;
                inherited.vis = base_field.vis;
                inherited.is_inherited = true;

                // Build the inheritance path: first step is to access base in current class
                // Then if the base field is inherited, append its path
                inherited.inheritance_path.push_back({base_class_name, base_class_idx});

                if (base_field.is_inherited) {
                    // Append the path from the base class to the actual field
                    for (const auto& step : base_field.inheritance_path) {
                        inherited.inheritance_path.push_back(step);
                    }
                } else {
                    // Field is directly in the base class - add final step
                    inherited.inheritance_path.push_back({base_class_name, base_field.index});
                }
                field_info.push_back(inherited);
            }
        }
    }

    for (const auto& field : c.fields) {
        if (field.is_static)
            continue; // Static fields are globals, not in instance

        std::string ft = llvm_type_ptr(field.type);
        if (ft == "void")
            ft = "{}"; // Unit type in struct
        field_types.push_back(ft);

        field_info.push_back(
            {field.name, static_cast<int>(field_offset++), ft, field.vis, false, {}});
    }

    // Emit class type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    emit_line(def);

    // Register class type
    class_types_[c.name] = type_name;
    class_fields_[c.name] = field_info;

    // Track value classes for direct dispatch
    if (is_value_class) {
        value_classes_.insert(c.name);
    }

    // Track @pool classes and generate global pool instance (if not thread-local)
    if (has_decorator(c, "pool")) {
        bool is_thread_local = has_decorator_bool_arg(c, "pool", "thread_local");

        if (is_thread_local) {
            // Thread-local pool - no global pool, use TLS functions
            tls_pool_classes_.insert(c.name);
            // Generate a string constant for the class name (used by TLS pool lookup)
            std::string name_const = "@pool.name." + c.name;
            emit_line(name_const + " = private constant [" + std::to_string(c.name.size() + 1) +
                      " x i8] c\"" + c.name + "\\00\"");
        } else {
            // Global pool - generate pool instance
            pool_classes_.insert(c.name);
            // Pool layout: { free_list_ptr, block_list_ptr, capacity, count }
            std::string pool_type = "%pool." + c.name;
            emit_line(pool_type + " = type { ptr, ptr, i64, i64 }");
            emit_line("@pool." + c.name + " = global " + pool_type + " zeroinitializer");
        }
    }

    // Generate static fields as global variables
    for (const auto& field : c.fields) {
        if (!field.is_static)
            continue;

        std::string field_type = llvm_type_ptr(field.type);
        if (field_type == "void")
            field_type = "{}";

        std::string global_name = "@class." + c.name + "." + field.name;

        // Generate initial value
        std::string init_value = "zeroinitializer";

        // Check if field has an explicit initializer
        if (field.init.has_value()) {
            const auto& init_expr = *field.init.value();
            if (init_expr.is<parser::LiteralExpr>()) {
                const auto& lit = init_expr.as<parser::LiteralExpr>();
                // Handle integer literals
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    init_value = std::to_string(lit.token.int_value().value);
                }
                // Handle float literals
                else if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                    init_value = std::to_string(lit.token.float_value().value);
                }
                // Handle bool literals
                else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    init_value = lit.token.bool_value() ? "true" : "false";
                }
                // Handle string literals
                else if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    // For strings, we need to emit a global string constant
                    // and initialize with ptr to it
                    init_value = "null"; // Strings need runtime initialization
                }
            }
            // Handle unary expressions (e.g., -5)
            else if (init_expr.is<parser::UnaryExpr>()) {
                const auto& unary = init_expr.as<parser::UnaryExpr>();
                if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                    const auto& lit = unary.operand->as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        // Cast to signed before negation to avoid unsigned overflow warning
                        init_value =
                            std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
                    } else if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                        init_value = std::to_string(-lit.token.float_value().value);
                    }
                }
            }
            // Handle binary constant expressions (e.g., 1 + 2)
            else if (init_expr.is<parser::BinaryExpr>()) {
                const auto& bin = init_expr.as<parser::BinaryExpr>();
                // Only handle simple constant folding for now
                if (bin.left->is<parser::LiteralExpr>() && bin.right->is<parser::LiteralExpr>()) {
                    const auto& left = bin.left->as<parser::LiteralExpr>();
                    const auto& right = bin.right->as<parser::LiteralExpr>();
                    if (left.token.kind == lexer::TokenKind::IntLiteral &&
                        right.token.kind == lexer::TokenKind::IntLiteral) {
                        int64_t lv = left.token.int_value().value;
                        int64_t rv = right.token.int_value().value;
                        int64_t result = 0;
                        switch (bin.op) {
                        case parser::BinaryOp::Add:
                            result = lv + rv;
                            break;
                        case parser::BinaryOp::Sub:
                            result = lv - rv;
                            break;
                        case parser::BinaryOp::Mul:
                            result = lv * rv;
                            break;
                        case parser::BinaryOp::Div:
                            result = rv != 0 ? lv / rv : 0;
                            break;
                        case parser::BinaryOp::Mod:
                            result = rv != 0 ? lv % rv : 0;
                            break;
                        case parser::BinaryOp::BitAnd:
                            result = lv & rv;
                            break;
                        case parser::BinaryOp::BitOr:
                            result = lv | rv;
                            break;
                        case parser::BinaryOp::BitXor:
                            result = lv ^ rv;
                            break;
                        case parser::BinaryOp::Shl:
                            result = lv << rv;
                            break;
                        case parser::BinaryOp::Shr:
                            result = lv >> rv;
                            break;
                        default:
                            break;
                        }
                        init_value = std::to_string(result);
                    }
                }
            }
        } else {
            // Generate default value based on type
            if (field_type == "i64" || field_type == "i32" || field_type == "i16" ||
                field_type == "i8") {
                init_value = "0";
            } else if (field_type == "double" || field_type == "float") {
                init_value = "0.0";
            } else if (field_type == "i1") {
                init_value = "false";
            }
        }

        emit_line(global_name + " = global " + field_type + " " + init_value);

        // Register static field for access
        static_fields_[c.name + "." + field.name] = {global_name, field_type};
    }

    // Register properties for getter/setter lookup during field access
    for (const auto& prop : c.properties) {
        std::string prop_key = c.name + "." + prop.name;
        std::string prop_llvm_type = llvm_type_ptr(prop.type);
        class_properties_[prop_key] = {prop.name, prop_llvm_type, prop.has_getter, prop.has_setter,
                                       prop.is_static};
    }

    // Generate vtable (skip for @value classes - they use direct dispatch)
    if (!is_value_class) {
        gen_class_vtable(c);
    }

    // Generate RTTI for runtime type checks
    gen_class_rtti(c);

    // Generate constructors
    for (const auto& ctor : c.constructors) {
        gen_class_constructor(c, ctor);
    }

    // Generate methods (store generic methods for later instantiation)
    for (size_t i = 0; i < c.methods.size(); ++i) {
        const auto& method = c.methods[i];
        if (!method.generics.empty()) {
            // Generic method - defer until instantiated
            std::string key = c.name + "::" + method.name;
            pending_generic_class_methods_[key] = PendingGenericClassMethod{&c, i};
            continue;
        }
        gen_class_method(c, method);
    }

    // Generate property getter/setter methods
    for (const auto& prop : c.properties) {
        gen_class_property(c, prop);
    }

    // Generate interface vtables for implemented interfaces
    if (!c.is_abstract) {
        gen_interface_vtables(c);
    }
}

// ============================================================================
// Interface Vtable Generation
// ============================================================================

void LLVMIRGen::gen_interface_vtables(const parser::ClassDecl& c) {
    // For each implemented interface, generate a separate vtable
    for (const auto& iface_type : c.implements) {
        // Extract interface name from the type (supports generic interfaces)
        auto* named = std::get_if<parser::NamedType>(&iface_type->kind);
        if (!named || named->path.segments.empty())
            continue;
        std::string iface_name = named->path.segments.back();

        // Get interface method order
        auto iface_methods_it = interface_method_order_.find(iface_name);
        if (iface_methods_it == interface_method_order_.end()) {
            // Try behavior method order (interfaces may be registered as behaviors)
            iface_methods_it = behavior_method_order_.find(iface_name);
            if (iface_methods_it == behavior_method_order_.end()) {
                continue;
            }
        }

        const auto& iface_methods = iface_methods_it->second;
        if (iface_methods.empty())
            continue;

        // Generate vtable type for this interface (if not already emitted)
        std::string vtable_type_name = "%vtable." + iface_name;
        if (emitted_interface_vtable_types_.find(iface_name) ==
            emitted_interface_vtable_types_.end()) {
            std::string vtable_type = vtable_type_name + " = type { ";
            for (size_t i = 0; i < iface_methods.size(); ++i) {
                if (i > 0)
                    vtable_type += ", ";
                vtable_type += "ptr";
            }
            vtable_type += " }";
            emit_line(vtable_type);
            emitted_interface_vtable_types_.insert(iface_name);
        }

        // Collect method implementations for this interface
        std::vector<std::pair<std::string, std::string>> impl_info; // (method_name, impl_func)

        for (size_t i = 0; i < iface_methods.size(); ++i) {
            std::string method_name = iface_methods[i];
            std::string impl_class = c.name;

            // Check if the method exists in this class or its parent
            bool found = false;
            std::string current_class = c.name;
            while (!current_class.empty() && !found) {
                auto class_def = env_.lookup_class(current_class);
                if (class_def) {
                    for (const auto& method : class_def->methods) {
                        if (method.sig.name == method_name) {
                            impl_class = current_class;
                            found = true;
                            break;
                        }
                    }
                    if (!found && class_def->base_class) {
                        current_class = *class_def->base_class;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            if (found) {
                std::string impl_func =
                    "@tml_" + get_suite_prefix() + impl_class + "_" + method_name;
                impl_info.push_back({method_name, impl_func});
            } else {
                impl_info.push_back({method_name, "null"});
            }
        }

        // Track statistics
        interface_vtable_stats_.total_interface_vtables++;

        // Compute content key for deduplication
        std::string content_key = compute_interface_vtable_key(iface_name, impl_info);

        // Check if an identical interface vtable already exists
        auto existing_it = interface_vtable_content_to_name_.find(content_key);
        if (existing_it != interface_vtable_content_to_name_.end()) {
            // Interface vtable deduplication: reuse existing vtable via alias
            std::string vtable_name = "@vtable." + c.name + "." + iface_name;
            std::string existing_vtable = existing_it->second;

            emit_line(vtable_name + " = internal alias " + vtable_type_name + ", ptr " +
                      existing_vtable);

            interface_vtables_[c.name + "::" + iface_name] = vtable_name;
            interface_vtable_stats_.deduplicated_interface++;
            continue;
        }

        // Generate new vtable global
        std::string vtable_name = "@vtable." + c.name + "." + iface_name;
        std::string vtable_value = "{ ";

        for (size_t i = 0; i < impl_info.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";

            if (impl_info[i].second == "null") {
                vtable_value += "ptr null";
            } else {
                vtable_value += "ptr " + impl_info[i].second;
            }
        }

        vtable_value += " }";
        emit_line(vtable_name + " = internal constant " + vtable_type_name + " " + vtable_value);

        // Record this interface vtable content for future deduplication
        interface_vtable_content_to_name_[content_key] = vtable_name;

        // Store interface vtable offset for casting
        interface_vtables_[c.name + "::" + iface_name] = vtable_name;
    }
}

// Helper to compute interface vtable content key for deduplication
auto LLVMIRGen::compute_interface_vtable_key(
    const std::string& iface_name, const std::vector<std::pair<std::string, std::string>>& impls)
    -> std::string {
    std::string key = iface_name + ":";
    for (const auto& impl : impls) {
        key += impl.second + ";";
    }
    return key;
}

// ============================================================================
// Vtable Generation
// ============================================================================

// Helper to compute vtable content key for deduplication
auto LLVMIRGen::compute_vtable_content_key(const std::vector<VirtualMethodInfo>& methods)
    -> std::string {
    std::string key;
    for (const auto& vm : methods) {
        if (!key.empty()) {
            key += ";";
        }
        // The key is based on the actual implementation class and method name
        // Two vtables are identical if they point to the same implementations
        key += vm.impl_class + "::" + vm.name;
    }
    return key;
}

void LLVMIRGen::gen_class_vtable(const parser::ClassDecl& c) {
    // Collect all virtual methods (inherited + own)
    std::vector<VirtualMethodInfo> vtable_methods;

    // First, inherit virtual methods from base class
    if (c.extends) {
        std::string base_name = c.extends->segments.back();
        auto it = class_vtable_layout_.find(base_name);
        if (it != class_vtable_layout_.end()) {
            vtable_methods = it->second;
        }
    }

    // Process own methods - add new virtuals or override existing
    for (const auto& method : c.methods) {
        if (!method.is_static) {
            bool found = false;

            // Check if this overrides a base method
            if (method.is_override) {
                for (auto& vm : vtable_methods) {
                    if (vm.name == method.name) {
                        // Override: update implementation
                        vm.impl_class = c.name;
                        found = true;
                        break;
                    }
                }
            }

            // Add new virtual method
            if (!found && (method.is_virtual || method.is_abstract)) {
                VirtualMethodInfo vmi;
                vmi.name = method.name;
                vmi.declaring_class = c.name;
                vmi.impl_class = method.is_abstract ? "" : c.name;
                vmi.vtable_index = vtable_methods.size();
                vtable_methods.push_back(vmi);
            }
        }
    }

    // Store vtable layout
    class_vtable_layout_[c.name] = vtable_methods;

    // Emit vtable type
    std::string vtable_type_name = "%vtable." + c.name;
    std::string vtable_type = vtable_type_name + " = type { ";
    for (size_t i = 0; i < vtable_methods.size(); ++i) {
        if (i > 0)
            vtable_type += ", ";
        vtable_type += "ptr"; // All method pointers
    }
    if (vtable_methods.empty()) {
        vtable_type += "ptr"; // At least one slot for type info
    }
    vtable_type += " }";
    emit_line(vtable_type);

    // Don't emit vtable global for abstract classes
    if (c.is_abstract) {
        return;
    }

    // Track statistics
    vtable_dedup_stats_.total_vtables++;

    // Compute vtable content key for deduplication
    std::string content_key = compute_vtable_content_key(vtable_methods);

    // Check if an identical vtable already exists
    auto existing_it = vtable_content_to_name_.find(content_key);
    if (existing_it != vtable_content_to_name_.end()) {
        // Vtable deduplication: reuse existing vtable via alias
        std::string vtable_name = "@vtable." + c.name;
        std::string existing_vtable = existing_it->second;

        // Emit an alias to the existing vtable
        // Note: We need to cast the type since vtable types differ by name
        emit_line(vtable_name + " = internal alias " + vtable_type_name + ", ptr " +
                  existing_vtable);

        // Track the shared vtable
        class_to_shared_vtable_[c.name] = existing_vtable;
        vtable_dedup_stats_.deduplicated++;

        // Estimate bytes saved: sizeof(ptr) * num_methods
        vtable_dedup_stats_.bytes_saved += vtable_methods.size() * 8;
        return;
    }

    // No existing vtable found - emit new vtable global constant
    vtable_dedup_stats_.unique_vtables++;

    std::string vtable_name = "@vtable." + c.name;
    std::string vtable_value = "{ ";

    for (size_t i = 0; i < vtable_methods.size(); ++i) {
        if (i > 0)
            vtable_value += ", ";
        const auto& vm = vtable_methods[i];

        if (vm.impl_class.empty()) {
            // Abstract method - should not happen for non-abstract class
            vtable_value += "ptr null";
        } else {
            vtable_value += "ptr @tml_" + get_suite_prefix() + vm.impl_class + "_" + vm.name;
        }
    }

    if (vtable_methods.empty()) {
        vtable_value += "ptr null"; // Placeholder
    }
    vtable_value += " }";

    emit_line(vtable_name + " = internal constant " + vtable_type_name + " " + vtable_value);

    // Record this vtable content for future deduplication
    vtable_content_to_name_[content_key] = vtable_name;
}

// ============================================================================
// RTTI (Runtime Type Information) Generation
// ============================================================================

void LLVMIRGen::gen_class_rtti(const parser::ClassDecl& c) {
    // Skip if already emitted
    if (emitted_rtti_.count(c.name) > 0) {
        return;
    }
    emitted_rtti_.insert(c.name);

    // Skip RTTI for @value classes (they use compile-time type info only)
    if (has_decorator(c, "value")) {
        return;
    }

    // TypeInfo structure: { ptr type_name, ptr base_typeinfo }
    // - type_name: string constant with class name
    // - base_typeinfo: pointer to base class RTTI (null if no base)

    // Emit TypeInfo type if not already emitted in this compilation unit
    if (!typeinfo_type_emitted_) {
        emit_line("%TypeInfo = type { ptr, ptr }");
        typeinfo_type_emitted_ = true;
    }

    // Generate type name string constant
    std::string name_const = "@.str.typeinfo." + c.name;
    emit_line(name_const + " = private unnamed_addr constant [" +
              std::to_string(c.name.size() + 1) + " x i8] c\"" + c.name + "\\00\"");

    // Get base class RTTI pointer
    std::string base_rtti = "null";
    if (c.extends) {
        std::string base_name = c.extends->segments.back();
        // Check if base is not a @value class
        auto base_def = env_.lookup_class(base_name);
        if (base_def && !base_def->is_value) {
            base_rtti = "@typeinfo." + base_name;
        }
    }

    // Emit TypeInfo global constant
    std::string typeinfo_name = "@typeinfo." + c.name;
    emit_line(typeinfo_name + " = internal constant %TypeInfo { ptr " + name_const + ", ptr " +
              base_rtti + " }");
}

// ============================================================================
// Constructor Generation
// ============================================================================

void LLVMIRGen::gen_class_constructor(const parser::ClassDecl& c,
                                      const parser::ConstructorDecl& ctor) {
    std::string class_type = "%class." + c.name;

    // Build parameter list
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    for (const auto& param : ctor.params) {
        param_types.push_back(llvm_type_ptr(param.type));
        param_names.push_back(get_class_param_name(param));
    }

    // Generate unique constructor name based on parameter types (for overloading)
    // Format: ClassName_new or ClassName_new_Type1_Type2 for overloaded constructors
    std::string func_name = "@tml_" + get_suite_prefix() + c.name + "_new";
    if (!param_types.empty()) {
        for (const auto& pt : param_types) {
            // Convert LLVM type to simple name for mangling: i32 -> I32, ptr -> ptr, etc.
            std::string type_suffix = pt;
            if (type_suffix == "i8")
                type_suffix = "I8";
            else if (type_suffix == "i16")
                type_suffix = "I16";
            else if (type_suffix == "i32")
                type_suffix = "I32";
            else if (type_suffix == "i64")
                type_suffix = "I64";
            else if (type_suffix == "i128")
                type_suffix = "I128";
            else if (type_suffix == "float")
                type_suffix = "F32";
            else if (type_suffix == "double")
                type_suffix = "F64";
            else if (type_suffix == "i1")
                type_suffix = "Bool";
            // For ptr types and complex types, use "ptr"
            else if (type_suffix.find("ptr") != std::string::npos ||
                     type_suffix.find("%") != std::string::npos)
                type_suffix = "ptr";
            func_name += "_" + type_suffix;
        }
    }

    // Register constructor in functions_ map for lookup during calls
    std::string ctor_key = c.name + "_new";
    if (!param_types.empty()) {
        for (const auto& pt : param_types) {
            ctor_key += "_" + pt;
        }
    }
    // Check if this is a value class - they return by value to prevent dangling pointers
    bool is_value_class = has_decorator(c, "value") || env_.is_value_class_candidate(c.name);

    // Register constructor info: value classes return struct type, others return ptr
    std::string ret_type = is_value_class ? class_type : "ptr";
    functions_[ctor_key] = FuncInfo{func_name, ret_type, ret_type, param_types};

    // Function signature - value classes return by value, others return pointer
    std::string sig =
        "define " + (is_value_class ? class_type : class_type + "*") + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Allocate object
    std::string obj = fresh_reg();
    bool is_pool_class = has_decorator(c, "pool");
    bool is_tls_pool = has_decorator_bool_arg(c, "pool", "thread_local");

    if (is_value_class) {
        // Stack allocate for @value classes (value semantics)
        emit_line("  " + obj + " = alloca " + class_type);
    } else if (is_tls_pool) {
        // Thread-local pool allocate for @pool(thread_local: true) classes
        // Call tls_pool_acquire with class name string and object size
        emit_line("  " + obj + " = call ptr @tls_pool_acquire(ptr @pool.name." + c.name +
                  ", i64 ptrtoint (" + class_type + "* getelementptr (" + class_type + ", " +
                  class_type + "* null, i32 1) to i64))");
    } else if (is_pool_class) {
        // Global pool allocate for @pool classes (pooled object reuse)
        // Call pool_acquire with the global pool and object size
        emit_line("  " + obj + " = call ptr @pool_acquire(ptr @pool." + c.name +
                  ", i64 ptrtoint (" + class_type + "* getelementptr (" + class_type + ", " +
                  class_type + "* null, i32 1) to i64))");
    } else {
        // Heap allocate for regular classes (reference semantics)
        emit_line("  " + obj + " = call ptr @malloc(i64 ptrtoint (" + class_type +
                  "* getelementptr (" + class_type + ", " + class_type + "* null, i32 1) to i64))");
    }

    // Initialize vtable pointer (field 0) - skip for @value classes
    if (!is_value_class) {
        std::string vtable_ptr = fresh_reg();
        emit_line("  " + vtable_ptr + " = getelementptr " + class_type + ", ptr " + obj +
                  ", i32 0, i32 0");
        emit_line("  store ptr @vtable." + c.name + ", ptr " + vtable_ptr);
    }

    // Call base constructor if specified
    if (ctor.base_args && c.extends) {
        std::string base_name = c.extends->segments.back();

        // Generate arguments for base constructor
        std::vector<std::string> base_args;
        std::vector<std::string> base_arg_types;
        for (const auto& arg : *ctor.base_args) {
            base_args.push_back(gen_expr(*arg));
            // Use the type from gen_expr which sets last_expr_type_
            base_arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
        }

        // Resolve overloaded base constructor
        std::string base_ctor_key = base_name + "_new";
        if (!base_arg_types.empty()) {
            for (const auto& at : base_arg_types) {
                base_ctor_key += "_" + at;
            }
        }

        std::string base_ctor_name;
        auto func_it = functions_.find(base_ctor_key);
        if (func_it != functions_.end()) {
            base_ctor_name = func_it->second.llvm_name;
        } else {
            // Fallback: try without overload suffix
            auto default_it = functions_.find(base_name + "_new");
            if (default_it != functions_.end()) {
                base_ctor_name = default_it->second.llvm_name;
            } else {
                // Last resort: generate basic name
                base_ctor_name = "@tml_" + get_suite_prefix() + base_name + "_new";
            }
        }

        // Call base constructor
        std::string base_result = fresh_reg();
        std::string call = "  " + base_result + " = call ptr " + base_ctor_name + "(";
        for (size_t i = 0; i < base_args.size(); ++i) {
            if (i > 0)
                call += ", ";
            call += base_arg_types[i] + " " + base_args[i];
        }
        call += ")";
        emit_line(call);

        // Copy base object to embedded base field (field 1)
        // The base constructor returns a pointer to a new base object
        // We need to copy its contents into our embedded base
        std::string base_field_ptr = fresh_reg();
        emit_line("  " + base_field_ptr + " = getelementptr " + class_type + ", ptr " + obj +
                  ", i32 0, i32 1");

        // Copy base vtable pointer
        std::string base_vtable_ptr = fresh_reg();
        emit_line("  " + base_vtable_ptr + " = load ptr, ptr " + base_result);
        emit_line("  store ptr " + base_vtable_ptr + ", ptr " + base_field_ptr);

        // Note: Field copying for base class fields would need to be added here
        // For now we just initialize the vtable pointer of embedded base
    }

    // Generate constructor body
    if (ctor.body) {
        // Set up 'this' reference
        locals_["this"] = VarInfo{obj, class_type + "*", nullptr, std::nullopt};

        // Set up constructor parameters in locals
        for (size_t i = 0; i < param_names.size(); ++i) {
            locals_[param_names[i]] =
                VarInfo{"%" + param_names[i], param_types[i], nullptr, std::nullopt};
        }

        // Generate body statements
        for (const auto& stmt : ctor.body->stmts) {
            gen_stmt(*stmt);
        }

        // Generate trailing expression (if any - common in blocks)
        if (ctor.body->expr.has_value()) {
            gen_expr(*ctor.body->expr.value());
        }

        locals_.erase("this");
    }

    // Return the object
    // For value classes, load the struct and return by value to prevent dangling pointers
    if (is_value_class) {
        std::string loaded_obj = fresh_reg();
        emit_line("  " + loaded_obj + " = load " + class_type + ", ptr " + obj);
        emit_line("  ret " + class_type + " " + loaded_obj);
    } else {
        emit_line("  ret " + class_type + "* " + obj);
    }
    emit_line("}");
    emit_line("");
}

// ============================================================================
// Generic Class Instantiation Helpers
// ============================================================================

void LLVMIRGen::gen_class_constructor_instantiation(
    [[maybe_unused]] const parser::ClassDecl& c, const parser::ConstructorDecl& ctor,
    const std::string& mangled_name,
    const std::unordered_map<std::string, types::TypePtr>& type_subs) {

    std::string class_type = "%class." + mangled_name;

    // Save current type subs and set new ones
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    // Build parameter list with type substitution
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    for (const auto& param : ctor.params) {
        auto resolved = resolve_parser_type_with_subs(*param.type, type_subs);
        param_types.push_back(llvm_type_from_semantic(resolved));
        param_names.push_back(get_class_param_name(param));
    }

    // Generate unique constructor name based on parameter types (for overloading)
    std::string func_name = "@tml_" + get_suite_prefix() + mangled_name + "_new";
    if (!param_types.empty()) {
        for (const auto& pt : param_types) {
            std::string type_suffix = pt;
            if (type_suffix == "i8")
                type_suffix = "I8";
            else if (type_suffix == "i16")
                type_suffix = "I16";
            else if (type_suffix == "i32")
                type_suffix = "I32";
            else if (type_suffix == "i64")
                type_suffix = "I64";
            else if (type_suffix == "i128")
                type_suffix = "I128";
            else if (type_suffix == "float")
                type_suffix = "F32";
            else if (type_suffix == "double")
                type_suffix = "F64";
            else if (type_suffix == "i1")
                type_suffix = "Bool";
            else if (type_suffix.find("ptr") != std::string::npos ||
                     type_suffix.find("%") != std::string::npos)
                type_suffix = "ptr";
            func_name += "_" + type_suffix;
        }
    }

    // Register constructor in functions_ map
    std::string ctor_key = mangled_name + "_new";
    if (!param_types.empty()) {
        for (const auto& pt : param_types) {
            ctor_key += "_" + pt;
        }
    }
    functions_[ctor_key] = FuncInfo{func_name, "ptr", "ptr", param_types};

    // Function signature - use ptr for opaque pointer mode
    std::string sig = "define ptr " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Allocate object
    std::string obj = fresh_reg();
    emit_line("  " + obj + " = call ptr @malloc(i64 ptrtoint (" + class_type + "* getelementptr (" +
              class_type + ", " + class_type + "* null, i32 1) to i64))");

    // Initialize vtable pointer
    std::string vtable_ptr = fresh_reg();
    emit_line("  " + vtable_ptr + " = getelementptr " + class_type + ", ptr " + obj +
              ", i32 0, i32 0");
    emit_line("  store ptr @vtable." + mangled_name + ", ptr " + vtable_ptr);

    // Generate constructor body
    if (ctor.body) {
        locals_["this"] = VarInfo{obj, class_type + "*", nullptr, std::nullopt};

        for (size_t i = 0; i < param_names.size(); ++i) {
            locals_[param_names[i]] =
                VarInfo{"%" + param_names[i], param_types[i], nullptr, std::nullopt};
        }

        for (const auto& stmt : ctor.body->stmts) {
            gen_stmt(*stmt);
        }

        if (ctor.body->expr.has_value()) {
            gen_expr(*ctor.body->expr.value());
        }

        locals_.erase("this");
    }

    emit_line("  ret ptr " + obj);
    emit_line("}");
    emit_line("");

    // Restore type subs
    current_type_subs_ = saved_subs;
}

void LLVMIRGen::gen_class_method_instantiation(
    [[maybe_unused]] const parser::ClassDecl& c, const parser::ClassMethod& method,
    const std::string& mangled_name,
    const std::unordered_map<std::string, types::TypePtr>& type_subs) {

    if (method.is_abstract) {
        return;
    }

    // Save and set type substitutions
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    std::string func_name = "@tml_" + get_suite_prefix() + mangled_name + "_" + method.name;
    std::string class_type = "%class." + mangled_name;

    // Build parameter list with type substitution
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    if (!method.is_static) {
        param_types.push_back("ptr");
        param_names.push_back("this");
    }

    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        if (pname == "this")
            continue;

        auto resolved = resolve_parser_type_with_subs(*param.type, type_subs);
        param_types.push_back(llvm_type_from_semantic(resolved));
        param_names.push_back(pname);
    }

    // Return type with substitution
    std::string ret_type = "void";
    if (method.return_type) {
        auto resolved = resolve_parser_type_with_subs(**method.return_type, type_subs);
        ret_type = llvm_type_from_semantic(resolved);
    }

    // Function signature
    std::string sig = "define " + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Save and set current return type for gen_return() to use
    std::string saved_ret_type = current_ret_type_;
    current_ret_type_ = ret_type;
    block_terminated_ = false;

    // Set up locals - mark as direct parameters (not allocas)
    for (size_t i = 0; i < param_names.size(); ++i) {
        auto sem_type = std::make_shared<types::Type>();
        if (param_names[i] == "this") {
            sem_type->kind = types::ClassType{mangled_name, "", {}};
        }
        VarInfo var_info;
        var_info.reg = "%" + param_names[i];
        var_info.type = param_types[i];
        var_info.semantic_type = sem_type;
        var_info.is_direct_param = true; // Mark as direct parameter
        locals_[param_names[i]] = var_info;
    }

    // Generate body
    if (method.body) {
        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }

        // Handle trailing expression if not already terminated by a return statement
        if (!block_terminated_ && method.body->expr.has_value()) {
            std::string result = gen_expr(*method.body->expr.value());
            // Only emit return if gen_expr didn't already terminate the block
            // (e.g., if the trailing expression was itself a return)
            if (!block_terminated_ && ret_type != "void") {
                emit_line("  ret " + ret_type + " " + result);
                block_terminated_ = true;
            }
        }
    }

    // Add implicit return if block wasn't terminated
    if (!block_terminated_) {
        if (ret_type == "void") {
            emit_line("  ret void");
        } else if (ret_type == "i64" || ret_type == "i32" || ret_type == "i1") {
            emit_line("  ret " + ret_type + " 0");
        } else {
            emit_line("  ret " + ret_type + " zeroinitializer");
        }
    }
    emit_line("}");
    emit_line("");

    // Restore return type and type substitutions
    current_ret_type_ = saved_ret_type;
    current_type_subs_ = saved_subs;

    // Clean up locals
    for (const auto& name : param_names) {
        locals_.erase(name);
    }

    // Register method in functions_ map
    functions_[mangled_name + "_" + method.name] =
        FuncInfo{func_name, ret_type, ret_type, param_types};
}

// ============================================================================
// Generic Static Method Generation (Method-Level Generics)
// ============================================================================

void LLVMIRGen::gen_generic_class_static_method(
    const parser::ClassDecl& c, const parser::ClassMethod& method, const std::string& method_suffix,
    const std::unordered_map<std::string, types::TypePtr>& type_subs) {

    if (method.is_abstract || !method.is_static) {
        return;
    }

    // Save and set type substitutions
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    // Function name: @tml_ClassName_methodName_TypeSuffix
    // e.g., @tml_Utils_identity_I32
    std::string func_name =
        "@tml_" + get_suite_prefix() + c.name + "_" + method.name + method_suffix;

    // Build parameter list with type substitution
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        auto resolved = resolve_parser_type_with_subs(*param.type, type_subs);
        param_types.push_back(llvm_type_from_semantic(resolved));
        param_names.push_back(pname);
    }

    // Return type with substitution
    std::string ret_type = "void";
    if (method.return_type) {
        auto resolved = resolve_parser_type_with_subs(*method.return_type.value(), type_subs);
        ret_type = llvm_type_from_semantic(resolved);
    }

    // Function signature
    std::string sig = "define " + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Set up locals for parameters
    for (size_t i = 0; i < param_names.size(); ++i) {
        types::TypePtr semantic = nullptr;
        if (i < method.params.size() && method.params[i].type) {
            semantic = resolve_parser_type_with_subs(*method.params[i].type, type_subs);
        }
        locals_[param_names[i]] =
            VarInfo{"%" + param_names[i], param_types[i], semantic, std::nullopt};
    }

    // Generate body
    if (method.body) {
        current_func_ = func_name;
        current_ret_type_ = ret_type;
        block_terminated_ = false;

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
            if (block_terminated_) {
                break;
            }
        }

        // Generate trailing expression (if any)
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string expr_val = gen_expr(*method.body->expr.value());
            // Return the expression value for non-void methods
            if (ret_type != "void" && !block_terminated_) {
                emit_line("  ret " + ret_type + " " + expr_val);
                block_terminated_ = true;
            }
        }

        // Default return if no explicit return
        if (!block_terminated_) {
            if (ret_type == "void") {
                emit_line("  ret void");
            } else {
                emit_line("  ret " + ret_type + " zeroinitializer");
            }
        }
    }

    emit_line("}");
    emit_line("");

    // Restore type substitutions
    current_type_subs_ = saved_subs;

    // Clean up locals
    for (const auto& name : param_names) {
        locals_.erase(name);
    }

    // Register method in functions_ map
    functions_[c.name + "_" + method.name + method_suffix] =
        FuncInfo{func_name, ret_type, ret_type, param_types};
}

// ============================================================================
// Method Generation
// ============================================================================

void LLVMIRGen::gen_class_method(const parser::ClassDecl& c, const parser::ClassMethod& method) {
    if (method.is_abstract) {
        // Abstract methods have no body
        return;
    }

    std::string func_name = "@tml_" + get_suite_prefix() + c.name + "_" + method.name;
    std::string class_type = "%class." + c.name;

    // Build parameter list - first param is always 'this' for instance methods
    std::vector<std::string> param_types;
    std::vector<std::string> param_names;

    if (!method.is_static) {
        param_types.push_back("ptr"); // this pointer
        param_names.push_back("this");
    }

    for (const auto& param : method.params) {
        // Skip 'this' parameter - it's already added above for non-static methods
        std::string pname = get_class_param_name(param);
        if (pname == "this") {
            continue;
        }
        param_types.push_back(llvm_type_ptr(param.type));
        param_names.push_back(pname);
    }

    // Return type
    std::string ret_type = "void";
    bool return_value_class_by_value = false;
    std::string value_class_struct_type;
    if (method.return_type) {
        ret_type = llvm_type_ptr(*method.return_type);

        // Check if return type is a value class - return by value instead of ptr
        // This fixes dangling pointer bug for stack-allocated value class objects
        if (ret_type == "ptr" && (*method.return_type)->is<parser::NamedType>()) {
            const auto& named = (*method.return_type)->as<parser::NamedType>();
            std::string return_class_name =
                named.path.segments.empty() ? "" : named.path.segments.back();
            if (!return_class_name.empty() && env_.is_value_class_candidate(return_class_name)) {
                // Return value class by value (struct type) instead of ptr
                value_class_struct_type = "%class." + return_class_name;
                ret_type = value_class_struct_type;
                return_value_class_by_value = true;
            }
        }
    }

    // Function signature
    std::string sig = "define " + ret_type + " " + func_name + "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0)
            sig += ", ";
        sig += param_types[i] + " %" + param_names[i];
    }
    sig += ")";
    emit_line(sig + " {");
    emit_line("entry:");

    // Set up locals for parameters
    if (!method.is_static) {
        // Create semantic type for 'this' so field access can infer the correct class type
        auto this_type = std::make_shared<types::Type>();
        this_type->kind = types::ClassType{c.name, "", {}};
        // Mark 'this' as direct param - it's a pointer parameter, not an alloca
        locals_["this"] =
            VarInfo{"%this", "ptr", this_type, std::nullopt, false, true /*is_direct_param*/};
    }

    // Set up locals for other parameters (non-this)
    size_t param_idx = method.is_static ? 0 : 1; // Skip 'this' index
    for (const auto& param : method.params) {
        std::string pname = get_class_param_name(param);
        if (pname == "this")
            continue;
        if (param_idx >= param_names.size())
            break;

        // Resolve semantic type for the parameter
        types::TypePtr semantic = nullptr;
        if (param.type) {
            semantic = resolve_parser_type_with_subs(*param.type, {});
        }

        // Mark all class method params as direct - they're pointer parameters, not allocas
        locals_[param_names[param_idx]] = VarInfo{"%" + param_names[param_idx],
                                                  param_types[param_idx],
                                                  semantic,
                                                  std::nullopt,
                                                  false,
                                                  true /*is_direct_param*/};
        ++param_idx;
    }

    // Generate body
    if (method.body) {
        current_func_ = func_name;
        current_ret_type_ = ret_type;
        block_terminated_ = false; // Reset for new method body

        for (const auto& stmt : method.body->stmts) {
            gen_stmt(*stmt);
        }

        // Generate trailing expression (if any)
        if (method.body->expr.has_value() && !block_terminated_) {
            std::string expr_val = gen_expr(*method.body->expr.value());
            // Return the expression value for non-void methods
            // Note: If the expression was a ReturnExpr, gen_expr already emitted ret
            // and set block_terminated_, so we check again here
            if (ret_type != "void" && !block_terminated_) {
                // For value classes returned by value, load the struct from pointer
                if (return_value_class_by_value && last_expr_type_ == "ptr") {
                    std::string loaded_struct = fresh_reg();
                    emit_line("  " + loaded_struct + " = load " + value_class_struct_type +
                              ", ptr " + expr_val);
                    emit_line("  ret " + ret_type + " " + loaded_struct);
                } else {
                    emit_line("  ret " + ret_type + " " + expr_val);
                }
                block_terminated_ = true;
            }
        }

        // Add implicit return for void functions
        if (ret_type == "void" && !block_terminated_) {
            emit_line("  ret void");
        }
    }

    emit_line("}");
    emit_line("");

    // Clear locals
    locals_.clear();

    // Register function
    functions_[c.name + "_" + method.name] = FuncInfo{
        func_name, ret_type + " (" + (method.is_static ? "" : "ptr") + ")", ret_type, param_types};
}

// ============================================================================
// Virtual Method Dispatch
// ============================================================================

auto LLVMIRGen::gen_virtual_call(const std::string& obj_reg, const std::string& class_name,
                                 const std::string& method_name,
                                 const std::vector<std::string>& args,
                                 const std::vector<std::string>& arg_types) -> std::string {

    // Check if this is a @value class - use direct dispatch instead of virtual
    bool is_value = value_classes_.count(class_name) > 0;

    // Get actual return type from method signature
    std::string ret_type = "void";
    auto class_def = env_.lookup_class(class_name);
    if (class_def) {
        for (const auto& m : class_def->methods) {
            if (m.sig.name == method_name) {
                ret_type = llvm_type_from_semantic(m.sig.return_type);
                break;
            }
        }
    }

    if (is_value) {
        // Direct dispatch for @value classes - no vtable lookup
        std::string func_name = "@tml_" + get_suite_prefix() + class_name + "_" + method_name;

        // Call the method directly
        std::string result = fresh_reg();
        std::string call;
        if (ret_type == "void") {
            call = "  call void " + func_name + "(ptr " + obj_reg;
        } else {
            call = "  " + result + " = call " + ret_type + " " + func_name + "(ptr " + obj_reg;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            call += ", " + arg_types[i] + " " + args[i];
        }
        call += ")";
        emit_line(call);

        last_expr_type_ = ret_type;
        return ret_type == "void" ? "void" : result;
    }

    // Virtual dispatch for regular classes

    // Look up vtable slot for this method
    auto it = class_vtable_layout_.find(class_name);
    if (it == class_vtable_layout_.end()) {
        report_error("Unknown class for virtual dispatch: " + class_name, SourceSpan{}, "C005");
        return "null";
    }

    size_t vtable_slot = SIZE_MAX;
    for (const auto& vm : it->second) {
        if (vm.name == method_name) {
            vtable_slot = vm.vtable_index;
            break;
        }
    }

    if (vtable_slot == SIZE_MAX) {
        report_error("Method not found in vtable: " + method_name, SourceSpan{}, "C006");
        return "null";
    }

    std::string class_type = "%class." + class_name;
    std::string vtable_type = "%vtable." + class_name;

    // Load vtable pointer from object (field 0)
    std::string vtable_ptr_ptr = fresh_reg();
    emit_line("  " + vtable_ptr_ptr + " = getelementptr " + class_type + ", ptr " + obj_reg +
              ", i32 0, i32 0");

    std::string vtable_ptr = fresh_reg();
    emit_line("  " + vtable_ptr + " = load ptr, ptr " + vtable_ptr_ptr);

    // Load function pointer from vtable slot
    std::string func_ptr_ptr = fresh_reg();
    emit_line("  " + func_ptr_ptr + " = getelementptr " + vtable_type + ", ptr " + vtable_ptr +
              ", i32 0, i32 " + std::to_string(vtable_slot));

    std::string func_ptr = fresh_reg();
    emit_line("  " + func_ptr + " = load ptr, ptr " + func_ptr_ptr);

    // Build function type for indirect call
    std::string func_type = ret_type + " (ptr";
    for (const auto& at : arg_types) {
        func_type += ", " + at;
    }
    func_type += ")";

    // Call the virtual function
    std::string result = fresh_reg();
    std::string call;
    if (ret_type == "void") {
        call = "  call void " + func_ptr + "(ptr " + obj_reg;
    } else {
        call = "  " + result + " = call " + ret_type + " " + func_ptr + "(ptr " + obj_reg;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        call += ", " + arg_types[i] + " " + args[i];
    }
    call += ")";
    emit_line(call);

    last_expr_type_ = ret_type;
    return ret_type == "void" ? "void" : result;
}

// ============================================================================
// Interface Vtable Generation
// ============================================================================

void LLVMIRGen::gen_interface_decl(const parser::InterfaceDecl& iface) {
    // Interface is similar to a behavior - defines method signatures
    // Classes implementing the interface will have vtable slots for these methods

    std::vector<std::string> method_names;
    for (const auto& method : iface.methods) {
        method_names.push_back(method.name);
    }

    // Store interface method order for vtable generation
    interface_method_order_[iface.name] = method_names;

    // Emit dyn type for interface (fat pointer: data + vtable)
    std::string dyn_type = "%dyn." + iface.name + " = type { ptr, ptr }";
    emit_line(dyn_type);
}

// ============================================================================
// External Class Type Generation
// ============================================================================

void LLVMIRGen::emit_external_class_type(const std::string& name, const types::ClassDef& def) {
    // Skip if already emitted
    if (class_types_.find(name) != class_types_.end()) {
        return;
    }

    std::string type_name = "%class." + name;

    // Collect field types
    std::vector<std::string> field_types;
    field_types.push_back("ptr"); // Vtable pointer is always first

    // If base class, recursively emit it first
    if (def.base_class) {
        auto base_class = env_.lookup_class(*def.base_class);
        if (base_class) {
            if (class_types_.find(*def.base_class) == class_types_.end()) {
                emit_external_class_type(*def.base_class, *base_class);
            }
            field_types.push_back("%class." + *def.base_class);
        }
    }

    // Add own instance fields
    std::vector<ClassFieldInfo> field_info;
    size_t field_offset = field_types.size();

    for (const auto& field : def.fields) {
        if (field.is_static)
            continue;

        std::string ft = llvm_type_from_semantic(field.type);
        if (ft == "void")
            ft = "{}";
        field_types.push_back(ft);

        ClassFieldInfo fi;
        fi.name = field.name;
        fi.index = static_cast<int>(field_offset++);
        fi.llvm_type = ft;
        fi.vis = static_cast<parser::MemberVisibility>(field.vis);
        field_info.push_back(fi);
    }

    // Emit class type definition
    std::string type_def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            type_def += ", ";
        type_def += field_types[i];
    }
    type_def += " }";
    emit_line(type_def);

    // Register class type
    class_types_[name] = type_name;
    class_fields_[name] = field_info;

    // Emit vtable type (even if empty)
    std::string vtable_type = "%vtable." + name + " = type { ptr }";
    emit_line(vtable_type);
}

// ============================================================================
// Base Expression Generation
// ============================================================================

auto LLVMIRGen::gen_base_expr(const parser::BaseExpr& base) -> std::string {
    // Get the 'this' pointer
    auto it = locals_.find("this");
    if (it == locals_.end()) {
        report_error("'base' used outside of class method", base.span, "C001");
        return "null";
    }

    std::string this_ptr = it->second.reg;

    // Look up the current class from the type context
    std::string current_class;
    for (const auto& [name, type] : class_types_) {
        if (it->second.type.find("%class." + name) != std::string::npos ||
            it->second.type == "ptr") {
            current_class = name;
            break;
        }
    }

    if (current_class.empty()) {
        report_error("Cannot determine current class for base expression", base.span, "C005");
        return "null";
    }

    // Look up base class from type environment
    auto class_def = env_.lookup_class(current_class);
    if (!class_def || !class_def->base_class) {
        report_error("Class has no base class", base.span, "C005");
        return "null";
    }

    std::string base_class = class_def->base_class.value();

    if (base.is_method_call) {
        // Generate direct (non-virtual) call to base class method
        std::string func_name = "@tml_" + get_suite_prefix() + base_class + "_" + base.member;

        // Cast this to base class type (embedded at field 1 after vtable)
        std::string base_ptr = fresh_reg();
        emit_line("  " + base_ptr + " = getelementptr %class." + current_class + ", ptr " +
                  this_ptr + ", i32 0, i32 1");

        // Generate arguments
        std::vector<std::string> args;
        std::vector<std::string> arg_types;
        for (const auto& arg : base.args) {
            args.push_back(gen_expr(*arg));
            arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
        }

        // Look up return type
        std::string ret_type = "void";
        auto base_def = env_.lookup_class(base_class);
        if (base_def) {
            for (const auto& method : base_def->methods) {
                if (method.sig.name == base.member && method.sig.return_type) {
                    ret_type = llvm_type_from_semantic(method.sig.return_type);
                    break;
                }
            }
        }

        // Call the base method directly (non-virtual)
        std::string call = "  ";
        std::string result;
        if (ret_type != "void") {
            result = fresh_reg();
            call += result + " = ";
        }
        call += "call " + ret_type + " " + func_name + "(ptr " + base_ptr;
        for (size_t i = 0; i < args.size(); ++i) {
            call += ", " + arg_types[i] + " " + args[i];
        }
        call += ")";
        emit_line(call);

        return result.empty() ? "void" : result;
    } else {
        // Field access on base class
        auto base_class_def = env_.lookup_class(base_class);
        if (!base_class_def) {
            report_error("Base class not found", base.span, "C005");
            return "null";
        }

        int field_idx = -1;
        std::string field_type;
        for (size_t i = 0; i < base_class_def->fields.size(); ++i) {
            if (base_class_def->fields[i].name == base.member) {
                field_idx = static_cast<int>(i) + 1; // +1 for vtable
                field_type = llvm_type_from_semantic(base_class_def->fields[i].type);
                break;
            }
        }

        if (field_idx < 0) {
            report_error("Field not found in base class: " + base.member, base.span, "C006");
            return "null";
        }

        std::string base_ptr = fresh_reg();
        emit_line("  " + base_ptr + " = getelementptr %class." + current_class + ", ptr " +
                  this_ptr + ", i32 0, i32 1");

        std::string field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr %class." + base_class + ", ptr " + base_ptr +
                  ", i32 0, i32 " + std::to_string(field_idx));

        std::string value = fresh_reg();
        emit_line("  " + value + " = load " + field_type + ", ptr " + field_ptr);

        return value;
    }
}

// ============================================================================
// New Expression Generation
// ============================================================================

auto LLVMIRGen::gen_new_expr(const parser::NewExpr& new_expr) -> std::string {
    std::string class_name;
    if (!new_expr.class_type.segments.empty()) {
        class_name = new_expr.class_type.segments.back();
    } else {
        report_error("Invalid class name in new expression", new_expr.span, "C003");
        return "null";
    }

    auto it = class_types_.find(class_name);
    if (it == class_types_.end()) {
        report_error("Unknown class: " + class_name, new_expr.span, "C005");
        return "null";
    }

    // Generate arguments and track types for constructor overload resolution
    std::vector<std::string> args;
    std::vector<std::string> arg_types;
    for (const auto& arg : new_expr.args) {
        args.push_back(gen_expr(*arg));
        arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
    }

    // Build constructor lookup key based on argument types (for overload resolution)
    std::string ctor_key = class_name + "_new";
    if (!arg_types.empty()) {
        for (const auto& at : arg_types) {
            ctor_key += "_" + at;
        }
    }

    // Look up the constructor in functions_ map to get mangled name and return type
    std::string ctor_name;
    std::string ctor_ret_type = "ptr"; // Default: pointer return
    auto func_it = functions_.find(ctor_key);
    if (func_it != functions_.end()) {
        ctor_name = func_it->second.llvm_name;
        // Use the registered return type (value classes return struct, not ptr)
        if (!func_it->second.ret_type.empty()) {
            ctor_ret_type = func_it->second.ret_type;
        }
    } else {
        // Fallback: try without overload suffix for default constructor
        auto default_it = functions_.find(class_name + "_new");
        if (default_it != functions_.end()) {
            ctor_name = default_it->second.llvm_name;
            if (!default_it->second.ret_type.empty()) {
                ctor_ret_type = default_it->second.ret_type;
            }
        } else {
            // Last resort: generate basic name
            ctor_name = "@tml_" + get_suite_prefix() + class_name + "_new";
        }
    }

    std::string result = fresh_reg();
    std::string call = "  " + result + " = call " + ctor_ret_type + " " + ctor_name + "(";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            call += ", ";
        call += arg_types[i] + " " + args[i];
    }
    call += ")";
    emit_line(call);

    last_expr_type_ = ctor_ret_type;
    return result;
}

// ============================================================================
// Property Getter/Setter Generation
// ============================================================================

void LLVMIRGen::gen_class_property(const parser::ClassDecl& c, const parser::PropertyDecl& prop) {
    std::string class_type = "%class." + c.name;
    std::string prop_type = llvm_type_ptr(prop.type);

    // Generate getter if present
    if (prop.has_getter) {
        std::string getter_name = "@tml_" + get_suite_prefix() + c.name + "_get_" + prop.name;

        // Getter signature: (this: ptr) -> PropertyType
        std::string sig;
        if (prop.is_static) {
            sig = "define " + prop_type + " " + getter_name + "()";
        } else {
            sig = "define " + prop_type + " " + getter_name + "(ptr %this)";
        }
        emit_line(sig + " {");
        emit_line("entry:");

        if (prop.getter) {
            // Set up 'this' for non-static properties
            if (!prop.is_static) {
                auto this_type = std::make_shared<types::Type>();
                this_type->kind = types::ClassType{c.name, "", {}};
                locals_["this"] = VarInfo{"%this", "ptr", this_type, std::nullopt};
            }

            // Generate getter expression body
            std::string result = gen_expr(**prop.getter);
            emit_line("  ret " + prop_type + " " + result);

            if (!prop.is_static) {
                locals_.erase("this");
            }
        } else {
            // No explicit getter body - generate default field access
            // Find backing field (typically _name or same as property)
            std::string backing_field = "_" + prop.name;
            bool found = false;
            int field_idx = -1;
            std::string field_type_str;

            // Look for backing field
            for (const auto& field_info : class_fields_[c.name]) {
                if (field_info.name == backing_field || field_info.name == prop.name) {
                    field_idx = field_info.index;
                    field_type_str = field_info.llvm_type;
                    found = true;
                    break;
                }
            }

            if (found) {
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + class_type +
                          ", ptr %this, i32 0, i32 " + std::to_string(field_idx));
                std::string value = fresh_reg();
                emit_line("  " + value + " = load " + prop_type + ", ptr " + field_ptr);
                emit_line("  ret " + prop_type + " " + value);
            } else {
                // Return zero-initialized value as fallback
                emit_line("  ret " + prop_type + " zeroinitializer");
            }
        }

        emit_line("}");
        emit_line("");

        // Register getter function
        std::string getter_sig = prop_type + " (" + std::string(prop.is_static ? "" : "ptr") + ")";
        std::vector<std::string> getter_params =
            prop.is_static ? std::vector<std::string>{} : std::vector<std::string>{"ptr"};
        functions_[c.name + "_get_" + prop.name] =
            FuncInfo{getter_name, getter_sig, prop_type, getter_params};
    }

    // Generate setter if present
    if (prop.has_setter) {
        std::string setter_name = "@tml_" + get_suite_prefix() + c.name + "_set_" + prop.name;

        // Setter signature: (this: ptr, value: PropertyType) -> void
        std::string sig;
        if (prop.is_static) {
            sig = "define void " + setter_name + "(" + prop_type + " %value)";
        } else {
            sig = "define void " + setter_name + "(ptr %this, " + prop_type + " %value)";
        }
        emit_line(sig + " {");
        emit_line("entry:");

        if (prop.setter) {
            // Set up 'this' and 'value' for the setter body
            if (!prop.is_static) {
                auto this_type = std::make_shared<types::Type>();
                this_type->kind = types::ClassType{c.name, "", {}};
                locals_["this"] = VarInfo{"%this", "ptr", this_type, std::nullopt};
            }

            // 'value' is the implicit parameter in setter
            auto value_type = resolve_parser_type_with_subs(*prop.type, {});
            locals_["value"] = VarInfo{"%value", prop_type, value_type, std::nullopt};

            // Generate setter expression body
            gen_expr(**prop.setter);

            locals_.erase("value");
            if (!prop.is_static) {
                locals_.erase("this");
            }
        } else {
            // No explicit setter body - generate default field store
            std::string backing_field = "_" + prop.name;
            bool found = false;
            int field_idx = -1;

            for (const auto& field_info : class_fields_[c.name]) {
                if (field_info.name == backing_field || field_info.name == prop.name) {
                    field_idx = field_info.index;
                    found = true;
                    break;
                }
            }

            if (found) {
                std::string field_ptr = fresh_reg();
                emit_line("  " + field_ptr + " = getelementptr " + class_type +
                          ", ptr %this, i32 0, i32 " + std::to_string(field_idx));
                emit_line("  store " + prop_type + " %value, ptr " + field_ptr);
            }
        }

        emit_line("  ret void");
        emit_line("}");
        emit_line("");

        // Register setter function
        std::vector<std::string> setter_params = prop.is_static
                                                     ? std::vector<std::string>{prop_type}
                                                     : std::vector<std::string>{"ptr", prop_type};
        std::string setter_sig =
            std::string("void (") + (prop.is_static ? "" : "ptr, ") + prop_type + ")";
        functions_[c.name + "_set_" + prop.name] =
            FuncInfo{setter_name, setter_sig, "void", setter_params};
    }

    // Clear locals after property generation
    locals_.clear();
}

// ============================================================================
// Phase 6.2: Vtable Splitting (Hot/Cold)
// ============================================================================

void LLVMIRGen::analyze_vtable_split(const parser::ClassDecl& c) {
    // Analyze methods and decide which should be in hot vs cold vtable
    // Heuristics for hot methods:
    // 1. Methods with @hot decorator
    // 2. Methods with "simple" names (get, set, is, has, do, on)
    // 3. Destructor is always cold (rarely called in tight loops)
    // 4. Abstract methods are cold (they have no implementation here)

    VtableSplitInfo split;
    split.primary_vtable_name = "@vtable." + c.name;
    split.secondary_vtable_name = "@vtable." + c.name + ".cold";

    // Get vtable layout if it exists
    auto it = class_vtable_layout_.find(c.name);
    if (it == class_vtable_layout_.end()) {
        return; // No vtable for this class
    }

    const auto& vtable_methods = it->second;

    // Analyze each method
    for (const auto& vm : vtable_methods) {
        bool is_hot = false;

        // Check heuristics
        const std::string& name = vm.name;

        // Hot patterns: common accessor patterns
        if (name.find("get") == 0 || name.find("set") == 0 || name.find("is") == 0 ||
            name.find("has") == 0 || name.find("do") == 0 || name.find("on") == 0 ||
            name == "size" || name == "len" || name == "length" || name == "empty" ||
            name == "count" || name == "value" || name == "next" || name == "prev" ||
            name == "item") {
            is_hot = true;
        }

        // Check for @hot decorator on the method
        for (const auto& method : c.methods) {
            if (method.name == name) {
                for (const auto& deco : method.decorators) {
                    if (deco.name == "hot") {
                        is_hot = true;
                        break;
                    }
                    if (deco.name == "cold") {
                        is_hot = false;
                        break;
                    }
                }
                break;
            }
        }

        // Destructor is typically cold
        if (name == "drop" || name == "destroy" || name == "finalize") {
            is_hot = false;
        }

        // Abstract methods are cold
        if (vm.impl_class.empty()) {
            is_hot = false;
        }

        if (is_hot) {
            split.hot_methods.push_back(name);
        } else {
            split.cold_methods.push_back(name);
        }
    }

    // Only split if we have both hot and cold methods and enough of each
    // to make splitting worthwhile (at least 2 cold methods)
    if (!split.hot_methods.empty() && split.cold_methods.size() >= 2) {
        vtable_splits_[c.name] = split;
        vtable_split_stats_.classes_with_split++;
        vtable_split_stats_.hot_methods_total += split.hot_methods.size();
        vtable_split_stats_.cold_methods_total += split.cold_methods.size();
    }
}

void LLVMIRGen::gen_split_vtables(const parser::ClassDecl& c) {
    auto split_it = vtable_splits_.find(c.name);
    if (split_it == vtable_splits_.end()) {
        return; // No split for this class
    }

    const auto& split = split_it->second;
    auto layout_it = class_vtable_layout_.find(c.name);
    if (layout_it == class_vtable_layout_.end()) {
        return;
    }

    const auto& vtable_methods = layout_it->second;

    // Generate hot vtable type and value
    std::string hot_type_name = "%vtable." + c.name + ".hot";
    std::string hot_type = hot_type_name + " = type { ";
    for (size_t i = 0; i < split.hot_methods.size(); ++i) {
        if (i > 0)
            hot_type += ", ";
        hot_type += "ptr";
    }
    if (split.hot_methods.empty()) {
        hot_type += "ptr"; // At least one slot
    }
    hot_type += " }";
    emit_line(hot_type);

    // Generate cold vtable type and value
    if (!split.cold_methods.empty()) {
        std::string cold_type_name = "%vtable." + c.name + ".cold";
        std::string cold_type = cold_type_name + " = type { ";
        for (size_t i = 0; i < split.cold_methods.size(); ++i) {
            if (i > 0)
                cold_type += ", ";
            cold_type += "ptr";
        }
        cold_type += " }";
        emit_line(cold_type);
    }

    // Generate hot vtable global
    std::string hot_value = "{ ";
    for (size_t i = 0; i < split.hot_methods.size(); ++i) {
        if (i > 0)
            hot_value += ", ";

        // Find the method implementation
        std::string impl_class;
        for (const auto& vm : vtable_methods) {
            if (vm.name == split.hot_methods[i]) {
                impl_class = vm.impl_class;
                break;
            }
        }

        if (impl_class.empty()) {
            hot_value += "ptr null";
        } else {
            hot_value += "ptr @tml_" + get_suite_prefix() + impl_class + "_" + split.hot_methods[i];
        }
    }
    if (split.hot_methods.empty()) {
        hot_value += "ptr null";
    }
    hot_value += " }";
    emit_line("@vtable." + c.name + ".hot = internal constant " + hot_type_name + " " + hot_value);

    // Generate cold vtable global
    if (!split.cold_methods.empty()) {
        std::string cold_value = "{ ";
        for (size_t i = 0; i < split.cold_methods.size(); ++i) {
            if (i > 0)
                cold_value += ", ";

            std::string impl_class;
            for (const auto& vm : vtable_methods) {
                if (vm.name == split.cold_methods[i]) {
                    impl_class = vm.impl_class;
                    break;
                }
            }

            if (impl_class.empty()) {
                cold_value += "ptr null";
            } else {
                cold_value +=
                    "ptr @tml_" + get_suite_prefix() + impl_class + "_" + split.cold_methods[i];
            }
        }
        cold_value += " }";
        std::string cold_type_name = "%vtable." + c.name + ".cold";
        emit_line("@vtable." + c.name + ".cold = internal constant " + cold_type_name + " " +
                  cold_value);
    }
}

bool LLVMIRGen::is_hot_method(const std::string& class_name, const std::string& method_name) const {
    auto it = vtable_splits_.find(class_name);
    if (it == vtable_splits_.end()) {
        return true; // No split, all methods are in primary vtable
    }

    const auto& split = it->second;
    for (const auto& hot : split.hot_methods) {
        if (hot == method_name) {
            return true;
        }
    }
    return false;
}

auto LLVMIRGen::get_split_vtable_index(const std::string& class_name,
                                       const std::string& method_name) -> std::pair<bool, size_t> {

    auto split_it = vtable_splits_.find(class_name);
    if (split_it == vtable_splits_.end()) {
        // No split - use original vtable layout
        auto layout_it = class_vtable_layout_.find(class_name);
        if (layout_it == class_vtable_layout_.end()) {
            return {true, SIZE_MAX};
        }

        for (const auto& vm : layout_it->second) {
            if (vm.name == method_name) {
                return {true, vm.vtable_index};
            }
        }
        return {true, SIZE_MAX};
    }

    const auto& split = split_it->second;

    // Check hot methods
    for (size_t i = 0; i < split.hot_methods.size(); ++i) {
        if (split.hot_methods[i] == method_name) {
            return {true, i};
        }
    }

    // Check cold methods
    for (size_t i = 0; i < split.cold_methods.size(); ++i) {
        if (split.cold_methods[i] == method_name) {
            return {false, i};
        }
    }

    return {true, SIZE_MAX};
}

// ============================================================================
// Phase 3: Speculative Devirtualization
// ============================================================================

void LLVMIRGen::init_type_frequency_hints() {
    // Initialize type frequency hints based on class hierarchy analysis
    // Higher frequency for:
    // - Sealed classes (most specific type)
    // - Leaf classes (no subclasses)
    // - Classes with @hot decorator

    for (const auto& [name, type] : class_types_) {
        float frequency = 0.5f; // Default

        auto class_def = env_.lookup_class(name);
        if (!class_def)
            continue;

        // Sealed classes are very likely to be the concrete type
        if (class_def->is_sealed) {
            frequency = 0.95f;
        }

        // Check if this is a leaf class (no known subclasses)
        bool is_leaf = true;
        for (const auto& [other_name, other_type] : class_types_) {
            auto other_def = env_.lookup_class(other_name);
            if (other_def && other_def->base_class && *other_def->base_class == name) {
                is_leaf = false;
                break;
            }
        }

        if (is_leaf && !class_def->is_abstract) {
            frequency = std::max(frequency, 0.85f);
        }

        // Abstract classes are never the concrete type
        if (class_def->is_abstract) {
            frequency = 0.0f;
        }

        type_frequency_hints_[name] = frequency;
    }
}

auto LLVMIRGen::analyze_spec_devirt(const std::string& receiver_class,
                                    const std::string& method_name)
    -> std::optional<SpeculativeDevirtInfo> {

    // Get frequency hint for the receiver class
    auto freq_it = type_frequency_hints_.find(receiver_class);
    float frequency = (freq_it != type_frequency_hints_.end()) ? freq_it->second : 0.5f;

    // If frequency is too low, speculative devirtualization is not profitable
    // Threshold: 70% - we want at least 70% probability of correct guess
    if (frequency < 0.70f) {
        return std::nullopt;
    }

    // Check if the method exists in this class
    auto class_def = env_.lookup_class(receiver_class);
    if (!class_def) {
        return std::nullopt;
    }

    bool has_method = false;
    for (const auto& m : class_def->methods) {
        if (m.sig.name == method_name) {
            has_method = true;
            break;
        }
    }

    // If method not found, check base classes
    if (!has_method && class_def->base_class) {
        std::string current = *class_def->base_class;
        while (!current.empty() && !has_method) {
            auto base_def = env_.lookup_class(current);
            if (!base_def)
                break;

            for (const auto& m : base_def->methods) {
                if (m.sig.name == method_name) {
                    has_method = true;
                    break;
                }
            }

            current = base_def->base_class.value_or("");
        }
    }

    if (!has_method) {
        return std::nullopt;
    }

    SpeculativeDevirtInfo info;
    info.expected_type = receiver_class;
    info.direct_call_target = "@tml_" + get_suite_prefix() + receiver_class + "_" + method_name;
    info.confidence = frequency;

    return info;
}

auto LLVMIRGen::gen_guarded_virtual_call(const std::string& obj_reg,
                                         const std::string& receiver_class,
                                         const SpeculativeDevirtInfo& spec_info,
                                         const std::string& method_name,
                                         const std::vector<std::string>& args,
                                         const std::vector<std::string>& arg_types) -> std::string {

    // Generate a type guard with fast path (direct call) and slow path (virtual dispatch)
    //
    // Code pattern:
    //   %vtable = load ptr, ptr %obj
    //   %expected_vtable = @vtable.ExpectedClass
    //   %is_expected = icmp eq ptr %vtable, %expected_vtable
    //   br i1 %is_expected, label %fast_path, label %slow_path
    // fast_path:
    //   %result_fast = call <ret> @direct_function(%obj, args...)
    //   br label %merge
    // slow_path:
    //   %result_slow = <virtual dispatch code>
    //   br label %merge
    // merge:
    //   %result = phi <ret> [ %result_fast, %fast_path ], [ %result_slow, %slow_path ]

    spec_devirt_stats_.guarded_calls++;

    std::string class_type = "%class." + receiver_class;

    // Look up return type
    std::string ret_type = "void";
    auto class_def = env_.lookup_class(receiver_class);
    if (class_def) {
        for (const auto& m : class_def->methods) {
            if (m.sig.name == method_name) {
                ret_type = llvm_type_from_semantic(m.sig.return_type);
                break;
            }
        }
    }

    // Load actual vtable pointer
    std::string vtable_ptr_ptr = fresh_reg();
    emit_line("  " + vtable_ptr_ptr + " = getelementptr " + class_type + ", ptr " + obj_reg +
              ", i32 0, i32 0");

    std::string actual_vtable = fresh_reg();
    emit_line("  " + actual_vtable + " = load ptr, ptr " + vtable_ptr_ptr);

    // Compare with expected vtable
    std::string expected_vtable = "@vtable." + spec_info.expected_type;
    std::string cmp_result = fresh_reg();
    emit_line("  " + cmp_result + " = icmp eq ptr " + actual_vtable + ", " + expected_vtable);

    // Generate labels
    std::string fast_path = fresh_label("spec_fast");
    std::string slow_path = fresh_label("spec_slow");
    std::string merge = fresh_label("spec_merge");

    emit_line("  br i1 " + cmp_result + ", label %" + fast_path + ", label %" + slow_path);

    // Fast path: direct call
    emit_line(fast_path + ":");
    std::string result_fast;
    std::string call_fast = "  ";
    if (ret_type != "void") {
        result_fast = fresh_reg();
        call_fast += result_fast + " = ";
    }
    call_fast += "call " + ret_type + " " + spec_info.direct_call_target + "(ptr " + obj_reg;
    for (size_t i = 0; i < args.size(); ++i) {
        call_fast += ", " + arg_types[i] + " " + args[i];
    }
    call_fast += ")";
    emit_line(call_fast);
    emit_line("  br label %" + merge);

    // Slow path: virtual dispatch
    emit_line(slow_path + ":");

    // Get vtable slot for this method
    auto layout_it = class_vtable_layout_.find(receiver_class);
    size_t vtable_slot = 0;
    if (layout_it != class_vtable_layout_.end()) {
        for (const auto& vm : layout_it->second) {
            if (vm.name == method_name) {
                vtable_slot = vm.vtable_index;
                break;
            }
        }
    }

    std::string vtable_type = "%vtable." + receiver_class;

    std::string func_ptr_ptr = fresh_reg();
    emit_line("  " + func_ptr_ptr + " = getelementptr " + vtable_type + ", ptr " + actual_vtable +
              ", i32 0, i32 " + std::to_string(vtable_slot));

    std::string func_ptr = fresh_reg();
    emit_line("  " + func_ptr + " = load ptr, ptr " + func_ptr_ptr);

    std::string result_slow;
    std::string call_slow = "  ";
    if (ret_type != "void") {
        result_slow = fresh_reg();
        call_slow += result_slow + " = ";
    }
    call_slow += "call " + ret_type + " " + func_ptr + "(ptr " + obj_reg;
    for (size_t i = 0; i < args.size(); ++i) {
        call_slow += ", " + arg_types[i] + " " + args[i];
    }
    call_slow += ")";
    emit_line(call_slow);
    emit_line("  br label %" + merge);

    // Merge
    emit_line(merge + ":");

    std::string result;
    if (ret_type != "void") {
        result = fresh_reg();
        emit_line("  " + result + " = phi " + ret_type + " [ " + result_fast + ", %" + fast_path +
                  " ], [ " + result_slow + ", %" + slow_path + " ]");
    }

    last_expr_type_ = ret_type;
    return ret_type == "void" ? "void" : result;
}

} // namespace tml::codegen
