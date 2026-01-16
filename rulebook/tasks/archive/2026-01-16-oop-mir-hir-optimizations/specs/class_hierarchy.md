# Specification: Class Hierarchy Analysis

## Overview

Class Hierarchy Analysis (CHA) builds a complete graph of class inheritance relationships to enable devirtualization and other OOP optimizations.

## Data Structures

### ClassHierarchy

```cpp
namespace tml::hir {

struct ClassHierarchy {
    // Inheritance relationships
    std::unordered_map<std::string, std::string> parent_;
    std::unordered_map<std::string, std::vector<std::string>> children_;

    // Class modifiers
    std::unordered_set<std::string> sealed_classes_;
    std::unordered_set<std::string> abstract_classes_;

    // Method modifiers (class::method -> is_final)
    std::unordered_set<std::string> final_methods_;
    std::unordered_set<std::string> abstract_methods_;

    // Virtual method table (class -> [methods with impl class])
    struct MethodImpl {
        std::string declaring_class;
        std::string implementing_class;
    };
    std::unordered_map<std::string, std::vector<MethodImpl>> vtable_layout_;

    // Interface implementations
    std::unordered_map<std::string, std::vector<std::string>> interfaces_;

    // === Query Methods ===

    // Get direct parent class (or nullopt if no parent)
    auto get_parent(const std::string& class_name) const
        -> std::optional<std::string>;

    // Get direct children classes
    auto get_children(const std::string& class_name) const
        -> const std::vector<std::string>&;

    // Get all transitive subclasses (includes class itself)
    auto get_all_subclasses(const std::string& class_name) const
        -> std::vector<std::string>;

    // Get only concrete (non-abstract) subclasses
    auto get_concrete_subclasses(const std::string& class_name) const
        -> std::vector<std::string>;

    // Check if class is sealed (cannot be extended)
    bool is_sealed(const std::string& class_name) const;

    // Check if class is abstract
    bool is_abstract(const std::string& class_name) const;

    // Check if method is final (cannot be overridden)
    bool is_final_method(const std::string& class_name,
                         const std::string& method) const;

    // Check if class has exactly one concrete implementation
    bool has_single_implementation(const std::string& class_name) const;

    // Get the implementing class for a virtual method call
    // Returns nullopt if multiple implementations possible
    auto resolve_virtual_call(const std::string& static_type,
                              const std::string& method) const
        -> std::optional<std::string>;

    // Check if derived is a subclass of base
    bool is_subclass_of(const std::string& derived,
                        const std::string& base) const;

    // Get all interfaces implemented by a class (including inherited)
    auto get_all_interfaces(const std::string& class_name) const
        -> std::vector<std::string>;

    // === Building ===

    // Add class to hierarchy
    void add_class(const std::string& name,
                   std::optional<std::string> parent,
                   bool is_sealed,
                   bool is_abstract);

    // Add method information
    void add_method(const std::string& class_name,
                    const std::string& method_name,
                    bool is_virtual,
                    bool is_final,
                    bool is_abstract,
                    bool is_override);

    // Add interface implementation
    void add_interface(const std::string& class_name,
                       const std::string& interface_name);

    // Finalize hierarchy (compute transitive closures)
    void finalize();
};

} // namespace tml::hir
```

## Building the Hierarchy

### During Type Checking

```cpp
void TypeChecker::check_class_decl(const parser::ClassDecl& decl) {
    // Register class in hierarchy
    std::optional<std::string> parent;
    if (decl.extends) {
        parent = decl.extends->segments.back();
    }

    hierarchy_.add_class(
        decl.name,
        parent,
        decl.is_sealed,
        decl.is_abstract
    );

    // Register interfaces
    for (const auto& iface : decl.implements) {
        hierarchy_.add_interface(decl.name, iface.segments.back());
    }

    // Register methods
    for (const auto& method : decl.methods) {
        hierarchy_.add_method(
            decl.name,
            method.name,
            method.is_virtual,
            method.is_final,
            method.is_abstract,
            method.is_override
        );
    }
}
```

### Finalization

After all classes are registered, compute:
1. Transitive closure of subclass relationships
2. Virtual method resolution table
3. Inherited interface list

```cpp
void ClassHierarchy::finalize() {
    // Build transitive children lists
    for (const auto& [name, _] : parent_) {
        auto all = compute_all_subclasses(name);
        all_subclasses_[name] = std::move(all);
    }

    // Build vtable layouts with resolved implementations
    for (const auto& [name, _] : parent_) {
        compute_vtable_layout(name);
    }

    // Compute concrete class count for each type
    for (const auto& [name, _] : parent_) {
        concrete_impl_count_[name] = count_concrete_implementations(name);
    }
}
```

## Virtual Method Resolution

For each virtual call site, determine if a single implementation exists:

```cpp
auto ClassHierarchy::resolve_virtual_call(
    const std::string& static_type,
    const std::string& method) const -> std::optional<std::string> {

    // Get all concrete subclasses
    auto concrete = get_concrete_subclasses(static_type);

    if (concrete.empty()) {
        return std::nullopt;  // Abstract type, no concrete impl
    }

    // Check if all concrete types have same implementation
    std::string impl_class;
    for (const auto& cls : concrete) {
        auto impl = get_method_impl(cls, method);
        if (impl_class.empty()) {
            impl_class = impl;
        } else if (impl != impl_class) {
            return std::nullopt;  // Multiple implementations
        }
    }

    return impl_class;
}

std::string ClassHierarchy::get_method_impl(
    const std::string& class_name,
    const std::string& method) const {

    // Walk up hierarchy to find implementing class
    std::string current = class_name;
    while (true) {
        // Check if this class has the method
        auto& layout = vtable_layout_.at(current);
        for (const auto& m : layout) {
            if (m.method_name == method && !m.is_abstract) {
                return m.implementing_class;
            }
        }

        // Move to parent
        auto parent = get_parent(current);
        if (!parent) break;
        current = *parent;
    }

    return "";  // Should not happen for valid code
}
```

## Testing

### Test Hierarchy

```tml
abstract class Animal {
    abstract func speak(this)
    func breathe(this) { }  // Non-virtual, inherited
}

class Dog extends Animal {
    override func speak(this) { print("woof") }
}

class Cat extends Animal {
    override func speak(this) { print("meow") }
}

sealed class GermanShepherd extends Dog {
    override func speak(this) { print("WOOF") }
}
```

### Expected CHA Results

| Query | Result |
|-------|--------|
| `get_parent("Dog")` | `"Animal"` |
| `get_children("Animal")` | `["Dog", "Cat"]` |
| `get_all_subclasses("Animal")` | `["Animal", "Dog", "Cat", "GermanShepherd"]` |
| `get_concrete_subclasses("Animal")` | `["Dog", "Cat", "GermanShepherd"]` |
| `is_sealed("GermanShepherd")` | `true` |
| `is_abstract("Animal")` | `true` |
| `has_single_implementation("Dog")` | `false` (Dog + GermanShepherd) |
| `has_single_implementation("GermanShepherd")` | `true` |
| `resolve_virtual_call("GermanShepherd", "speak")` | `"GermanShepherd"` |
| `resolve_virtual_call("Animal", "speak")` | `nullopt` (multiple impls) |
