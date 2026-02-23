TML_MODULE("compiler")

//! # Type Checker - OOP Core
//!
//! This file implements OOP type checking: interface and class registration,
//! validation (inheritance, overrides, abstract methods, value/pool constraints),
//! class body checking, and member visibility enforcement.
//!
//! Split from core.cpp for maintainability.

#include "lexer/token.hpp"
#include "types/builtins_cache.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <iostream>
#include <set>

namespace tml::types {

// Reserved type names - primitive types that cannot be redefined by user code
// Only language primitives are reserved - library types like Maybe, List can be shadowed
// (Duplicated from core.cpp for use in register_interface_decl / register_class_decl)
static const std::set<std::string> RESERVED_TYPE_NAMES = {
    // Primitive types
    "I8",
    "I16",
    "I32",
    "I64",
    "I128",
    "U8",
    "U16",
    "U32",
    "U64",
    "U128",
    "F32",
    "F64",
    "Bool",
    "Char",
    "Str",
    "Unit",
    "Never",
    // String builder
    "StringBuilder",
    // Async types
    "Future",
    "Context",
    "Waker",
};

// ============================================================================
// Size Estimation for Stack Allocation Eligibility
// ============================================================================

/// Maximum class size for stack allocation eligibility (in bytes).
/// Classes larger than this are always heap-allocated.
/// (Duplicated from core.cpp for use in register_class_decl)
static constexpr size_t MAX_STACK_CLASS_SIZE = 256;

/// Estimate the size of a type in bytes (for stack allocation eligibility).
/// Returns 0 for unsized types (slices, dyn, etc.).
/// (Duplicated from core.cpp for use in register_class_decl)
static size_t estimate_type_size(const TypePtr& type) {
    if (!type)
        return 8; // Default pointer size

    return std::visit(
        [](const auto& kind) -> size_t {
            using T = std::decay_t<decltype(kind)>;

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                switch (kind.kind) {
                case PrimitiveKind::Bool:
                case PrimitiveKind::I8:
                case PrimitiveKind::U8:
                    return 1;
                case PrimitiveKind::I16:
                case PrimitiveKind::U16:
                    return 2;
                case PrimitiveKind::I32:
                case PrimitiveKind::U32:
                case PrimitiveKind::F32:
                case PrimitiveKind::Char:
                    return 4;
                case PrimitiveKind::I64:
                case PrimitiveKind::U64:
                case PrimitiveKind::F64:
                    return 8;
                case PrimitiveKind::I128:
                case PrimitiveKind::U128:
                    return 16;
                case PrimitiveKind::Unit:
                    return 0;
                case PrimitiveKind::Never:
                    return 0;
                case PrimitiveKind::Str:
                    return 24; // Str is typically ptr + len + capacity
                }
                return 8; // Default for any unknown primitives
            } else if constexpr (std::is_same_v<T, PtrType> || std::is_same_v<T, RefType>) {
                return 8; // Pointer size
            } else if constexpr (std::is_same_v<T, ClassType>) {
                return 8; // Class instances are stored by reference (pointer)
            } else if constexpr (std::is_same_v<T, NamedType>) {
                return 8; // Conservative estimate - actual size computed during codegen
            } else if constexpr (std::is_same_v<T, TupleType>) {
                size_t total = 0;
                for (const auto& elem : kind.elements) {
                    total += estimate_type_size(elem);
                }
                return total;
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                return estimate_type_size(kind.element) * kind.size;
            } else if constexpr (std::is_same_v<T, SliceType> ||
                                 std::is_same_v<T, DynBehaviorType>) {
                return 16; // Fat pointer (ptr + vtable/len)
            } else if constexpr (std::is_same_v<T, GenericType>) {
                return 8; // Conservative - treat as pointer-sized
            } else {
                return 8; // Default to pointer size
            }
        },
        type->kind);
}

// ============================================================================
// OOP Type Checking - Interface Registration
// ============================================================================

void TypeChecker::register_interface_decl(const parser::InterfaceDecl& decl) {
    // Check if the type name is reserved
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name + "'", decl.span, "T038");
        return;
    }

    // Build InterfaceDef
    InterfaceDef def;
    def.name = decl.name;
    def.span = decl.span;

    // Collect type parameters
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            def.type_params.push_back(param.name);
        } else if (param.const_type.has_value()) {
            def.const_params.push_back(
                ConstGenericParam{param.name, resolve_type(*param.const_type.value())});
        }
    }

    // Collect extended interfaces
    for (const auto& ext : decl.extends) {
        if (!ext.segments.empty()) {
            def.extends.push_back(ext.segments.back());
        }
    }

    // Collect methods
    for (const auto& method : decl.methods) {
        InterfaceMethodDef method_def;
        method_def.is_static = method.is_static;
        method_def.has_default = method.default_body.has_value();

        // Build signature
        FuncSig sig;
        sig.name = method.name;
        sig.is_async = false;
        sig.span = method.span;

        for (const auto& param : method.params) {
            // Skip 'this' parameter - it's the implicit receiver
            if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                param.pattern->as<parser::IdentPattern>().name == "this") {
                continue;
            }
            if (param.type) {
                sig.params.push_back(resolve_type(*param.type));
            }
        }

        if (method.return_type.has_value()) {
            sig.return_type = resolve_type(*method.return_type.value());
        } else {
            sig.return_type = make_unit();
        }

        method_def.sig = sig;
        def.methods.push_back(method_def);
    }

    env_.define_interface(std::move(def));
}

// ============================================================================
// OOP Type Checking - Class Registration
// ============================================================================

void TypeChecker::register_class_decl(const parser::ClassDecl& decl) {
    // Check if the type name is reserved
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name + "'", decl.span, "T038");
        return;
    }

    // Build ClassDef
    ClassDef def;
    def.name = decl.name;
    def.is_abstract = decl.is_abstract;
    def.is_sealed = decl.is_sealed;
    def.span = decl.span;

    // Check for @value and @pool decorators
    def.is_value = false;
    def.is_pooled = false;
    for (const auto& deco : decl.decorators) {
        if (deco.name == "value") {
            def.is_value = true;
            // @value implies sealed
            def.is_sealed = true;
        } else if (deco.name == "pool") {
            def.is_pooled = true;
        }
    }

    // Collect type parameters
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            def.type_params.push_back(param.name);
        } else if (param.const_type.has_value()) {
            def.const_params.push_back(
                ConstGenericParam{param.name, resolve_type(*param.const_type.value())});
        }
    }

    // Collect base class
    if (decl.extends.has_value()) {
        const auto& base = decl.extends.value();
        if (!base.segments.empty()) {
            def.base_class = base.segments.back();
        }
    }

    // Collect implemented interfaces (supports generic interfaces like IEquatable[T])
    for (const auto& iface_type : decl.implements) {
        if (auto* named = std::get_if<parser::NamedType>(&iface_type->kind)) {
            if (!named->path.segments.empty()) {
                def.interfaces.push_back(named->path.segments.back());
            }
        }
    }

    // Collect fields
    for (const auto& field : decl.fields) {
        ClassFieldDef field_def;
        field_def.name = field.name;
        field_def.type = resolve_type(*field.type);
        field_def.is_static = field.is_static;
        field_def.vis = static_cast<MemberVisibility>(field.vis);
        def.fields.push_back(field_def);
    }

    // Collect methods
    for (const auto& method : decl.methods) {
        ClassMethodDef method_def;
        method_def.is_static = method.is_static;
        method_def.is_virtual = method.is_virtual;
        method_def.is_override = method.is_override;
        method_def.is_abstract = method.is_abstract;
        method_def.is_final = method.is_final;
        method_def.vis = static_cast<MemberVisibility>(method.vis);
        method_def.vtable_index = 0; // Will be assigned during codegen

        // Build signature
        FuncSig sig;
        sig.name = method.name;
        sig.is_async = false;
        sig.span = method.span;

        // Collect method's type parameters (for generic methods)
        for (const auto& gp : method.generics) {
            if (!gp.is_const) {
                sig.type_params.push_back(gp.name);
            }
        }

        // Collect parameters (skip 'this' to match module loading behavior)
        for (const auto& param : method.params) {
            // Skip 'this' parameter
            if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                param.pattern->as<parser::IdentPattern>().name == "this") {
                continue;
            }
            if (param.type) {
                sig.params.push_back(resolve_type(*param.type));
            }
        }

        if (method.return_type.has_value()) {
            // Check if return type is the class being registered (self-referential)
            // Since the class isn't registered yet, we need to handle this case specially
            const auto& ret_type = *method.return_type.value();
            if (auto* named = std::get_if<parser::NamedType>(&ret_type.kind)) {
                if (!named->path.segments.empty() && named->path.segments.back() == decl.name) {
                    // Return type is the class itself - create ClassType directly
                    auto class_type = std::make_shared<Type>();
                    class_type->kind = ClassType{decl.name, "", {}};
                    sig.return_type = class_type;
                } else {
                    sig.return_type = resolve_type(ret_type);
                }
            } else {
                sig.return_type = resolve_type(ret_type);
            }
        } else {
            sig.return_type = make_unit();
        }

        method_def.sig = sig;
        def.methods.push_back(method_def);
    }

    // Collect properties
    for (const auto& prop : decl.properties) {
        PropertyDef prop_def;
        prop_def.name = prop.name;
        prop_def.type = resolve_type(*prop.type);
        prop_def.is_static = prop.is_static;
        prop_def.vis = static_cast<MemberVisibility>(prop.vis);
        prop_def.has_getter = prop.has_getter;
        prop_def.has_setter = prop.has_setter;
        def.properties.push_back(prop_def);
    }

    // Collect constructors
    for (const auto& ctor : decl.constructors) {
        ConstructorDef ctor_def;
        ctor_def.vis = static_cast<MemberVisibility>(ctor.vis);
        ctor_def.calls_base = ctor.base_args.has_value();

        for (const auto& param : ctor.params) {
            if (param.type) {
                ctor_def.params.push_back(resolve_type(*param.type));
            }
        }

        def.constructors.push_back(ctor_def);
    }

    // ========================================================================
    // Compute stack allocation eligibility metadata
    // ========================================================================

    // Calculate inheritance depth
    def.inheritance_depth = 0;
    if (def.base_class.has_value()) {
        std::string current_base = *def.base_class;
        while (!current_base.empty()) {
            def.inheritance_depth++;
            auto base_def = env_.lookup_class(current_base);
            if (base_def.has_value() && base_def->base_class.has_value()) {
                current_base = *base_def->base_class;
            } else {
                break;
            }
        }
    }

    // Calculate estimated size:
    // - vtable pointer (8 bytes) for non-@value classes
    // - inherited fields (from base class)
    // - own fields
    def.estimated_size = 0;

    // vtable pointer (8 bytes) for non-@value classes
    if (!def.is_value) {
        def.estimated_size += 8;
    }

    // Add inherited field sizes
    if (def.base_class.has_value()) {
        auto base_def = env_.lookup_class(*def.base_class);
        if (base_def.has_value()) {
            // Include base class size (minus vtable since we already counted it)
            def.estimated_size += base_def->estimated_size;
            if (!base_def->is_value) {
                // Don't double-count vtable pointer
                def.estimated_size -= 8;
            }
        }
    }

    // Add own field sizes
    for (const auto& field : def.fields) {
        if (!field.is_static) {
            def.estimated_size += estimate_type_size(field.type);
        }
    }

    // Determine stack allocation eligibility:
    // A class is stack-allocatable if:
    // 1. It's a @value class (no vtable, no virtual methods), OR
    // 2. It's sealed (no subclasses) and small enough
    // AND:
    // 3. It's not abstract
    // 4. Its estimated size is within the threshold
    // 5. It doesn't contain unsized types
    def.stack_allocatable = false;

    if (!def.is_abstract && def.estimated_size <= MAX_STACK_CLASS_SIZE) {
        if (def.is_value) {
            // @value classes are always eligible if small enough
            def.stack_allocatable = true;
        } else if (def.is_sealed) {
            // Sealed classes with known type can be stack-allocated
            // (escape analysis determines actual placement at call sites)
            def.stack_allocatable = true;
        }
    }

    env_.define_class(std::move(def));
}

// ============================================================================
// OOP Type Checking - Interface Validation (Pass 2)
// ============================================================================

void TypeChecker::check_interface_decl(const parser::InterfaceDecl& iface) {
    // Verify extended interfaces exist
    for (const auto& ext : iface.extends) {
        if (!ext.segments.empty()) {
            const std::string& name = ext.segments.back();
            if (!env_.lookup_interface(name).has_value()) {
                error("Interface '" + name + "' not found", iface.span, "T047");
            }
        }
    }
}

// ============================================================================
// OOP Type Checking - Class Validation (Pass 2)
// ============================================================================

void TypeChecker::check_class_decl(const parser::ClassDecl& cls) {
    // Run all validation checks
    validate_inheritance(cls);
    validate_interface_impl(cls);

    // Check override methods
    for (const auto& method : cls.methods) {
        if (method.is_override) {
            validate_override(cls, method);
        }
    }

    // Check abstract methods are implemented (non-abstract classes only)
    if (!cls.is_abstract && cls.extends.has_value()) {
        validate_abstract_methods(cls);
    }

    // Validate @value class constraints
    validate_value_class(cls);

    // Validate @pool class constraints
    validate_pool_class(cls);
}

void TypeChecker::validate_value_class(const parser::ClassDecl& cls) {
    // Check if class has @value decorator
    bool is_value = false;
    for (const auto& deco : cls.decorators) {
        if (deco.name == "value") {
            is_value = true;
            break;
        }
    }

    if (!is_value) {
        return; // Not a value class, no validation needed
    }

    // @value classes cannot be abstract
    if (cls.is_abstract) {
        error("@value class '" + cls.name + "' cannot be abstract", cls.span, "T043");
    }

    // @value classes cannot have virtual methods
    for (const auto& method : cls.methods) {
        if (method.is_virtual || method.is_abstract) {
            error("@value class '" + cls.name + "' cannot have virtual method '" + method.name +
                      "'. Value classes use direct dispatch only.",
                  method.span, "T042");
        }
    }

    // @value classes cannot extend non-value classes
    if (cls.extends.has_value()) {
        const auto& base_path = cls.extends.value();
        if (!base_path.segments.empty()) {
            const auto& base_name = base_path.segments.back();
            auto base_def = env_.lookup_class(base_name);
            if (base_def.has_value() && !base_def->is_value) {
                error("@value class '" + cls.name + "' cannot extend non-value class '" +
                          base_name + "'. Base class must also be @value.",
                      cls.span, "T041");
            }
        }
    }

    // Note: @value classes CAN implement interfaces, so no check needed there
}

void TypeChecker::validate_pool_class(const parser::ClassDecl& cls) {
    // Check if class has @pool decorator
    bool is_pooled = false;
    bool is_value = false;
    for (const auto& deco : cls.decorators) {
        if (deco.name == "pool") {
            is_pooled = true;
        } else if (deco.name == "value") {
            is_value = true;
        }
    }

    if (!is_pooled) {
        return; // Not a pooled class, no validation needed
    }

    // @pool and @value are mutually exclusive
    if (is_value) {
        error("@pool and @value are mutually exclusive on class '" + cls.name +
                  "'. Use one or the other.",
              cls.span, "T044");
    }

    // @pool classes cannot be abstract
    if (cls.is_abstract) {
        error("@pool class '" + cls.name + "' cannot be abstract", cls.span, "T040");
    }

    // @pool classes should not be sealed (pooling benefits from inheritance)
    // But we don't enforce this - just a note that sealed pools are unusual
}

void TypeChecker::validate_abstract_methods(const parser::ClassDecl& cls) {
    // Collect all abstract methods from inheritance chain
    std::vector<std::pair<std::string, std::string>>
        abstract_methods; // (method_name, declaring_class)
    std::string current = cls.extends.value().segments.back();

    while (!current.empty()) {
        auto parent = env_.lookup_class(current);
        if (!parent.has_value())
            break;

        for (const auto& method : parent->methods) {
            if (method.is_abstract) {
                abstract_methods.emplace_back(method.sig.name, current);
            }
        }

        if (parent->base_class.has_value()) {
            current = parent->base_class.value();
        } else {
            break;
        }
    }

    // Check each abstract method has an implementation
    for (const auto& [method_name, declaring_class] : abstract_methods) {
        bool implemented = false;

        // Check if this class implements it
        for (const auto& method : cls.methods) {
            if (method.name == method_name && (method.is_override || !method.is_abstract)) {
                implemented = true;
                break;
            }
        }

        // Check if any intermediate class implements it
        if (!implemented) {
            current = cls.extends.value().segments.back();
            while (current != declaring_class && !current.empty()) {
                auto parent = env_.lookup_class(current);
                if (!parent.has_value())
                    break;

                for (const auto& method : parent->methods) {
                    if (method.sig.name == method_name && method.is_override) {
                        implemented = true;
                        break;
                    }
                }

                if (implemented)
                    break;

                if (parent->base_class.has_value()) {
                    current = parent->base_class.value();
                } else {
                    break;
                }
            }
        }

        if (!implemented) {
            error("Non-abstract class '" + cls.name + "' does not implement abstract method '" +
                      method_name + "' from '" + declaring_class + "'",
                  cls.span, "T045");
        }
    }
}

void TypeChecker::validate_inheritance(const parser::ClassDecl& cls) {
    if (!cls.extends.has_value()) {
        return; // No inheritance to validate
    }

    const auto& base_path = cls.extends.value();
    if (base_path.segments.empty()) {
        return;
    }

    const std::string& base_name = base_path.segments.back();

    // Check base class exists
    auto base_def = env_.lookup_class(base_name);
    if (!base_def.has_value()) {
        error("Base class '" + base_name + "' not found", cls.span, "T046");
        return;
    }

    // Check sealed class not extended (unless both are @value classes)
    if (base_def->is_sealed) {
        // @value classes can extend other @value classes
        bool this_is_value = false;
        for (const auto& deco : cls.decorators) {
            if (deco.name == "value") {
                this_is_value = true;
                break;
            }
        }
        // Only allow if both classes are @value
        if (!this_is_value || !base_def->is_value) {
            error("Cannot extend sealed class '" + base_name + "'", cls.span, "T041");
            return;
        }
    }

    // Check for circular inheritance
    std::unordered_set<std::string> visited;
    std::string current = base_name;

    while (!current.empty()) {
        if (visited.count(current) > 0) {
            error("Circular inheritance detected involving class '" + cls.name + "'", cls.span,
                  "T039");
            return;
        }
        visited.insert(current);

        auto parent = env_.lookup_class(current);
        if (!parent.has_value() || !parent->base_class.has_value()) {
            break;
        }
        current = parent->base_class.value();
    }

    // Check if current class would create a cycle
    if (visited.count(cls.name) > 0) {
        error("Circular inheritance: class '" + cls.name + "' cannot extend itself", cls.span,
              "T039");
    }
}

void TypeChecker::validate_override(const parser::ClassDecl& cls,
                                    const parser::ClassMethod& method) {
    if (!cls.extends.has_value()) {
        error("Cannot override method '" + method.name + "': class has no base class", method.span,
              "T063");
        return;
    }

    const std::string& base_name = cls.extends.value().segments.back();
    auto base_def = env_.lookup_class(base_name);

    if (!base_def.has_value()) {
        return; // Error already reported
    }

    // Search for method in base class hierarchy
    bool found = false;
    bool is_virtual = false;
    std::string current = base_name;

    while (!current.empty()) {
        auto parent = env_.lookup_class(current);
        if (!parent.has_value()) {
            break;
        }

        for (const auto& parent_method : parent->methods) {
            if (parent_method.sig.name == method.name) {
                found = true;
                is_virtual = parent_method.is_virtual || parent_method.is_abstract;

                // Verify method is virtual/abstract
                if (!is_virtual && !parent_method.is_override) {
                    error("Cannot override non-virtual method '" + method.name + "' from '" +
                              current + "'",
                          method.span, "T064");
                    return;
                }

                // Check signature match - return type
                TypePtr override_ret = method.return_type.has_value()
                                           ? resolve_type(**method.return_type)
                                           : make_unit();
                TypePtr parent_ret =
                    parent_method.sig.return_type ? parent_method.sig.return_type : make_unit();

                if (!types_equal(override_ret, parent_ret)) {
                    error("Override method '" + method.name +
                              "' has different return type than base method",
                          method.span, "T016");
                    return;
                }

                // Check parameter count (excluding implicit 'this' from both sides)
                size_t override_params = method.params.size();
                size_t parent_params = parent_method.sig.params.size();
                // Exclude 'this' from override method params if present
                if (override_params > 0 && method.params[0].pattern &&
                    method.params[0].pattern->is<parser::IdentPattern>() &&
                    method.params[0].pattern->as<parser::IdentPattern>().name == "this") {
                    override_params -= 1;
                }
                // Note: parent_method.sig.params already excludes 'this' when loaded from module

                if (override_params != parent_params) {
                    error("Override method '" + method.name + "' has " +
                              std::to_string(override_params) +
                              " parameters, but base method has " + std::to_string(parent_params),
                          method.span, "T004");
                    return;
                }

                // Check parameter types match
                // Note: override params are at method.params[1], [2], etc. (index 0 is 'this')
                // parent params are at sig.params[0], [1], etc. ('this' already excluded)
                for (size_t i = 0; i < override_params; ++i) {
                    // Override param: skip 'this' by adding 1 to index
                    size_t override_idx = i + 1; // method.params[0] is 'this'
                    TypePtr override_param_type =
                        (override_idx < method.params.size() && method.params[override_idx].type)
                            ? resolve_type(*method.params[override_idx].type)
                            : make_unit();
                    // Parent param: 'this' already excluded from sig.params
                    TypePtr parent_param_type = parent_method.sig.params.size() > i
                                                    ? parent_method.sig.params[i]
                                                    : make_unit();

                    if (!types_equal(override_param_type, parent_param_type)) {
                        error("Override method '" + method.name + "' parameter " +
                                  std::to_string(i + 1) + " has different type than base method",
                              method.span, "T058");
                        return;
                    }
                }

                break;
            }
        }

        if (found)
            break;

        if (parent->base_class.has_value()) {
            current = parent->base_class.value();
        } else {
            break;
        }
    }

    if (!found) {
        error("Method '" + method.name + "' marked as override but not found in any base class",
              method.span, "T065");
    }
}

void TypeChecker::validate_interface_impl(const parser::ClassDecl& cls) {
    for (const auto& iface_type : cls.implements) {
        // Extract interface name from the type (supports generic interfaces)
        auto* named = std::get_if<parser::NamedType>(&iface_type->kind);
        if (!named || named->path.segments.empty()) {
            continue;
        }

        const std::string& iface_name = named->path.segments.back();
        auto iface_def = env_.lookup_interface(iface_name);

        if (!iface_def.has_value()) {
            error("Interface '" + iface_name + "' not found", cls.span, "T047");
            continue;
        }

        // Check all interface methods are implemented
        for (const auto& iface_method : iface_def->methods) {
            if (iface_method.has_default) {
                continue; // Has default implementation
            }

            bool implemented = false;
            for (const auto& cls_method : cls.methods) {
                if (cls_method.name == iface_method.sig.name) {
                    implemented = true;

                    // Check signature match
                    // 1. Check return type
                    TypePtr expected_return =
                        iface_method.sig.return_type ? iface_method.sig.return_type : make_unit();
                    TypePtr actual_return = cls_method.return_type
                                                ? resolve_type(**cls_method.return_type)
                                                : make_unit();
                    if (!types_equal(expected_return, actual_return)) {
                        error("Method '" + cls_method.name + "' in class '" + cls.name +
                                  "' has incompatible return type with interface '" + iface_name +
                                  "'. Expected '" + type_to_string(expected_return) +
                                  "' but got '" + type_to_string(actual_return) + "'",
                              cls_method.span, "T016");
                    }

                    // 2. Check parameter count (excluding 'this')
                    size_t expected_params = iface_method.sig.params.size();
                    size_t actual_params = 0;
                    for (const auto& p : cls_method.params) {
                        if (!p.pattern || !p.pattern->is<parser::IdentPattern>() ||
                            p.pattern->as<parser::IdentPattern>().name != "this") {
                            actual_params++;
                        }
                    }
                    if (expected_params != actual_params) {
                        error("Method '" + cls_method.name + "' in class '" + cls.name +
                                  "' has wrong number of parameters. Interface '" + iface_name +
                                  "' expects " + std::to_string(expected_params) +
                                  " parameters but got " + std::to_string(actual_params),
                              cls_method.span, "T004");
                    } else {
                        // 3. Check parameter types
                        size_t param_idx = 0;
                        for (const auto& cls_param : cls_method.params) {
                            if (cls_param.pattern &&
                                cls_param.pattern->is<parser::IdentPattern>() &&
                                cls_param.pattern->as<parser::IdentPattern>().name == "this") {
                                continue; // Skip 'this' parameter
                            }
                            if (param_idx < expected_params && cls_param.type) {
                                TypePtr expected_param_type = iface_method.sig.params[param_idx];
                                TypePtr actual_param_type = resolve_type(*cls_param.type);
                                if (!types_equal(expected_param_type, actual_param_type)) {
                                    error("Parameter " + std::to_string(param_idx + 1) +
                                              " of method '" + cls_method.name + "' in class '" +
                                              cls.name +
                                              "' has incompatible type with interface '" +
                                              iface_name + "'. Expected '" +
                                              type_to_string(expected_param_type) + "' but got '" +
                                              type_to_string(actual_param_type) + "'",
                                          cls_method.span, "T058");
                                }
                            }
                            param_idx++;
                        }
                    }
                    break;
                }
            }

            if (!implemented) {
                error("Class '" + cls.name + "' does not implement method '" +
                          iface_method.sig.name + "' from interface '" + iface_name + "'",
                      cls.span, "T026");
            }
        }
    }
}

// ============================================================================
// OOP Type Checking - Class Body Checking (Pass 3)
// ============================================================================

void TypeChecker::check_class_body(const parser::ClassDecl& cls) {
    // Set up self type for 'this' references
    auto class_type = std::make_shared<Type>();
    class_type->kind = ClassType{cls.name, "", {}};
    current_self_type_ = class_type;

    // Check constructor bodies
    for (const auto& ctor : cls.constructors) {
        if (ctor.body.has_value()) {
            // Push a new scope
            env_.push_scope();

            // Bind 'this' in scope
            auto this_type = make_ref(current_self_type_, true); // mut ref to class
            env_.current_scope()->define("this", this_type, false, ctor.span);

            // Bind constructor parameters
            for (const auto& param : ctor.params) {
                if (param.type) {
                    TypePtr param_type = resolve_type(*param.type);
                    bool is_mut = false;
                    std::string name;

                    if (param.pattern->is<parser::IdentPattern>()) {
                        const auto& ident = param.pattern->as<parser::IdentPattern>();
                        name = ident.name;
                        is_mut = ident.is_mut;
                    }

                    if (!name.empty()) {
                        env_.current_scope()->define(name, param_type, is_mut, param.span);
                    }
                }
            }

            // Check constructor body
            check_block(ctor.body.value());

            env_.pop_scope();
        }
    }

    // Check method bodies
    for (const auto& method : cls.methods) {
        if (method.body.has_value()) {
            // Set return type
            if (method.return_type.has_value()) {
                current_return_type_ = resolve_type(*method.return_type.value());
            } else {
                current_return_type_ = make_unit();
            }

            // Push a new scope
            env_.push_scope();

            // Bind 'this' for non-static methods
            if (!method.is_static) {
                auto this_type = make_ref(current_self_type_, true); // mut ref
                env_.current_scope()->define("this", this_type, false, method.span);
            }

            // Bind parameters
            for (const auto& param : method.params) {
                if (param.type) {
                    TypePtr param_type = resolve_type(*param.type);
                    bool is_mut = false;
                    std::string name;

                    if (param.pattern->is<parser::IdentPattern>()) {
                        const auto& ident = param.pattern->as<parser::IdentPattern>();
                        name = ident.name;
                        is_mut = ident.is_mut;
                    }

                    if (!name.empty()) {
                        env_.current_scope()->define(name, param_type, is_mut, param.span);
                    }
                }
            }

            // Check method body
            check_block(method.body.value());

            env_.pop_scope();
            current_return_type_ = nullptr;
        }
    }

    current_self_type_ = nullptr;
}

// ============================================================================
// Visibility Checking
// ============================================================================

bool TypeChecker::is_subclass_of(const std::string& derived_class, const std::string& base_class) {
    if (derived_class == base_class) {
        return true;
    }

    std::string current = derived_class;
    std::set<std::string> visited;

    while (!current.empty()) {
        if (visited.count(current))
            break; // Prevent infinite loop
        visited.insert(current);

        auto class_def = env_.lookup_class(current);
        if (!class_def.has_value())
            break;

        if (class_def->base_class.has_value()) {
            if (class_def->base_class.value() == base_class) {
                return true;
            }
            current = class_def->base_class.value();
        } else {
            break;
        }
    }

    return false;
}

bool TypeChecker::check_member_visibility(MemberVisibility vis, const std::string& defining_class,
                                          const std::string& member_name, SourceSpan span) {
    // Public members are always accessible
    if (vis == MemberVisibility::Public) {
        return true;
    }

    // Get current class context (if any)
    std::string current_class_name;
    if (current_self_type_ && current_self_type_->is<ClassType>()) {
        current_class_name = current_self_type_->as<ClassType>().name;
    }

    // Private: only accessible within the defining class
    if (vis == MemberVisibility::Private) {
        if (current_class_name != defining_class) {
            error("Cannot access private member '" + member_name + "' of class '" + defining_class +
                      "' from " +
                      (current_class_name.empty() ? "outside any class"
                                                  : "class '" + current_class_name + "'"),
                  span, "T048");
            return false;
        }
        return true;
    }

    // Protected: accessible within the class and subclasses
    if (vis == MemberVisibility::Protected) {
        if (current_class_name.empty()) {
            error("Cannot access protected member '" + member_name + "' of class '" +
                      defining_class + "' from outside any class",
                  span, "T048");
            return false;
        }

        if (!is_subclass_of(current_class_name, defining_class)) {
            error("Cannot access protected member '" + member_name + "' of class '" +
                      defining_class + "' from class '" + current_class_name +
                      "' which is not a subclass",
                  span, "T048");
            return false;
        }
        return true;
    }

    return true;
}

} // namespace tml::types
