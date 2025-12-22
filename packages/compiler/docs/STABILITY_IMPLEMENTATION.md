# Stability Annotations Implementation Guide

## Overview

This document describes how to implement the `@stable` and `@deprecated` annotation system in the TML compiler.

## Implementation Steps

### 1. Update Type Definitions (env.hpp)

```cpp
// Add stability enum
enum class StabilityLevel {
    Stable,      // @stable - API is stable
    Unstable,    // Default - API may change
    Deprecated   // @deprecated - API will be removed
};

// Extend FuncSig struct
struct FuncSig {
    // ... existing fields ...
    StabilityLevel stability = StabilityLevel::Unstable;
    std::string deprecated_message;
    std::string since_version;
};
```

### 2. Annotate Builtin Functions (env_builtins.cpp)

Mark each builtin function with appropriate stability level:

```cpp
// Stable function (production-ready)
functions_["print"] = FuncSig{
    "print",
    params,
    return_type,
    {},
    false,
    span,
    StabilityLevel::Stable,  // NEW
    "",                       // NEW
    "1.0"                     // NEW
};

// Deprecated function (will be removed)
functions_["print_i32"] = FuncSig{
    "print_i32",
    params,
    return_type,
    {},
    false,
    span,
    StabilityLevel::Deprecated,                                    // NEW
    "Use toString(value) + print() instead",                      // NEW
    "1.2"                                                          // NEW
};
```

### 3. Add Stability Checks to Type Checker (checker.cpp)

```cpp
// In check_call_expr method
void TypeChecker::check_call_expr(const CallExpr& call) {
    // ... existing type checking ...
    
    // Check function stability
    if (auto func_sig = env_.lookup_func(func_name)) {
        check_function_stability(*func_sig, call.span);
    }
}

void TypeChecker::check_function_stability(const FuncSig& sig, const SourceSpan& span) {
    if (sig.stability == StabilityLevel::Deprecated) {
        emit_deprecation_warning(sig, span);
    }
    if (options_.warn_unstable && sig.stability == StabilityLevel::Unstable) {
        emit_unstable_info(sig, span);
    }
}
```

### 4. Add Compiler Flags (cli.cpp)

```cpp
// Add new command-line options
struct CompilerOptions {
    // ... existing options ...
    bool forbid_deprecated = false;   // Treat deprecation warnings as errors
    bool allow_unstable = false;      // Suppress unstable API warnings
    bool stability_report = false;    // Generate stability usage report
};

// Parse flags
if (arg == "--forbid-deprecated") {
    options.forbid_deprecated = true;
}
if (arg == "--allow-unstable") {
    options.allow_unstable = true;
}
if (arg == "--stability-report") {
    options.stability_report = true;
}
```

### 5. Generate Stability Reports (optional)

```cpp
// Generate report of all API usage by stability level
void generate_stability_report(const TypeChecker& checker) {
    std::map<StabilityLevel, std::vector<std::string>> usage;
    
    for (const auto& [name, count] : checker.function_usage()) {
        if (auto sig = env_.lookup_func(name)) {
            usage[sig->stability].push_back(name);
        }
    }
    
    std::cout << "=== Stability Report ===\n";
    std::cout << "Stable APIs: " << usage[StabilityLevel::Stable].size() << "\n";
    std::cout << "Unstable APIs: " << usage[StabilityLevel::Unstable].size() << "\n";
    std::cout << "Deprecated APIs: " << usage[StabilityLevel::Deprecated].size() << "\n";
}
```

## Testing

### Test Cases

1. **Test deprecated function warning**
```tml
func main() {
    print_i32(42)  // Should emit deprecation warning
}
```

2. **Test stable function (no warning)**
```tml
func main() {
    print("Hello")  // No warning
}
```

3. **Test unstable function warning**
```tml
func main() {
    let handle = thread_spawn(worker, 0)  // Info message if --warn-unstable
}
```

4. **Test --forbid-deprecated flag**
```bash
tml build --forbid-deprecated main.tml  # Should fail if using deprecated APIs
```

## Migration Timeline

### Phase 1: Infrastructure (Current)
- [ ] Add StabilityLevel enum
- [ ] Extend FuncSig struct
- [ ] Create documentation

### Phase 2: Annotation (Next)
- [ ] Mark stable functions (print, println, Instant API)
- [ ] Mark deprecated functions (print_i32, time_ms)
- [ ] Leave unstable functions unmarked

### Phase 3: Enforcement
- [ ] Implement deprecation warnings in type checker
- [ ] Add compiler flags
- [ ] Generate stability reports

### Phase 4: Cleanup (v2.0)
- [ ] Remove deprecated functions
- [ ] Promote stable unstable APIs to stable
- [ ] Update documentation

## Current Status

**Completed:**
- ✅ Design stability system
- ✅ Create documentation (STABILITY_GUIDE.md)
- ✅ Provide implementation examples

**TODO:**
- ⏳ Integrate StabilityLevel into env.hpp
- ⏳ Update env_builtins.cpp with annotations
- ⏳ Add stability checks to type checker
- ⏳ Implement compiler flags
- ⏳ Write tests for stability warnings

## References

- See `docs/STABILITY_GUIDE.md` for user-facing documentation
- See `docs/examples/stability_*.{hpp,cpp}` for implementation examples
- See `add_stability.cpp` for struct definition reference

