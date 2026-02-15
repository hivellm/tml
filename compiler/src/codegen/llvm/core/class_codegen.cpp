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

#include "codegen/llvm/llvm_ir_gen.hpp"
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
            // Determine prefix: imported/library classes don't use suite prefix,
            // local classes do. This prevents name mismatches in vtables where
            // a local class inherits methods from an imported base class.
            std::string method_prefix = get_suite_prefix();
            if (!method_prefix.empty() && vm.impl_class != c.name &&
                is_library_method(vm.impl_class, vm.name)) {
                method_prefix = "";
            }
            vtable_value += "ptr @tml_" + method_prefix + vm.impl_class + "_" + vm.name;
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

} // namespace tml::codegen
