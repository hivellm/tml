# Short-Name Keying in Multi-Module Symbol Tables

**Category**: code
**Tags**: compiler, type-checker, codegen, fqn, symbol-table, collision

## Description

Keying any symbol table (behaviors, traits, functions, types) by short name when registrations can arrive from multiple modules causes silent last-write-wins collisions. Two modules defining a symbol with the same short name silently overwrite each other, producing wrong behavior with no error at compile or link time.

## Example of Anti-Pattern

```cpp
// WRONG: short-name only — last module registered wins
for (const auto& [mod_name, mod_info] : all_modules) {
    for (const auto& trait : mod_info.behaviors) {
        trait_decls_[trait.name] = &trait;  // core::fmt::Write overwrites core::io::Write
    }
}
```

## Correct Pattern

```cpp
// CORRECT: FQN primary key + first-write-wins short name
for (const auto& [mod_name, mod_info] : all_modules) {
    for (const auto& trait : mod_info.behaviors) {
        std::string fqn = mod_name + "::" + trait.name;
        trait_decls_[fqn] = &trait;                      // FQN always wins
        trait_decls_.emplace(trait.name, &trait);         // short name: first-write-wins
    }
}
```

## Lookup Fix

```cpp
// CORRECT: resolve FQN via import table BEFORE direct short-name find
auto TypeEnv::lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef> {
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);  // "Write" -> "core::io::Write"
        if (import_path) {
            auto fqn_it = behaviors_.find(*import_path);
            if (fqn_it != behaviors_.end()) return fqn_it->second;
        }
    }
    return behaviors_.find(name) != behaviors_.end() ? std::make_optional(behaviors_.at(name)) : std::nullopt;
}
```

## When to Apply

- Any map indexed by `behavior.name`, `trait.name`, `function.name` where multiple modules can register
- `TypeEnv::behaviors_`, `TypeEnv::functions_`, `LLVMIRGen::trait_decls_`
- Module registries, reflection tables, vtable caches

## Known Collision in TML stdlib

`core::io::Write` and `core::fmt::Write` both have short name `"Write"`. Before phase0i fix, importing both would cause fmt::Write's default methods (`write_char`, `write_fmt`) to be generated for types that only impl io::Write, producing wrong GEP instructions referencing `%struct.File`.
