# Spec: Default Behavior Method Dispatch

## Problem

Calling default behavior methods returns `()` (Unit) instead of the declared return type.

### Current Behavior

```tml
behavior Iterator {
    type Item
    func next(mut this) -> Maybe[This::Item]

    // Default implementation
    func count(mut this) -> I64 {
        var n: I64 = 0
        loop {
            when this.next() {
                Just(_) => n = n + 1,
                Nothing => return n
            }
        }
    }
}

impl Iterator for MyIter {
    type Item = I32
    pub func next(mut this) -> Maybe[I32] { ... }
    // count() uses default implementation
}

// Usage:
var iter = MyIter::new()
let c = iter.count()  // Returns () instead of I64!
```

### Expected Behavior

```tml
let c = iter.count()  // Should return I64
assert_eq(c, 5)       // Should work
```

## Root Cause

In `compiler/src/codegen/expr/method.cpp`, when looking up methods:

1. First checks impl block for the concrete type
2. If not found, should check behavior for default implementation
3. Currently returns Unit when no impl method found

The method dispatch doesn't properly:
- Find default implementations in behavior definitions
- Generate calls to default method instantiations
- Propagate return type from behavior method signature

## Fix

### Step 1: Find Default Method

When method not in impl block, search behavior definitions:

```cpp
// In method.cpp, after checking impl block
if (!found_in_impl) {
    // Check if type implements a behavior with this method
    for (const auto& behavior : implemented_behaviors) {
        auto method_it = behavior.default_methods.find(method_name);
        if (method_it != behavior.default_methods.end()) {
            // Found default implementation
            found_default = true;
            default_method = method_it->second;
            break;
        }
    }
}
```

### Step 2: Generate Default Method Call

Instantiate the default method for the concrete type:

```cpp
if (found_default) {
    // Generate monomorphized version of default method
    std::string mangled_name = mangle_default_method(
        behavior_name, method_name, concrete_type);

    // Queue for instantiation if not already generated
    if (!generated_default_methods_.contains(mangled_name)) {
        pending_default_method_instantiations_.push_back({
            behavior_name, method_name, concrete_type, type_subs
        });
        generated_default_methods_.insert(mangled_name);
    }

    // Generate call
    emit_call(mangled_name, receiver, args);
}
```

### Step 3: Propagate Return Type

Ensure return type from behavior definition is used:

```cpp
// Get return type from behavior method signature
auto return_type = behavior_method.return_type;
// Substitute type parameters (e.g., This::Item -> I32)
return_type = substitute_type(return_type, type_subs);
last_expr_type_ = llvm_type_from_semantic(return_type);
```

## Test Cases

```tml
behavior Counter {
    func increment(mut this)

    func count_to(mut this, n: I64) -> I64 {
        var count: I64 = 0
        loop {
            if count >= n { return count }
            this.increment()
            count = count + 1
        }
    }
}

type SimpleCounter { value: I64 }

impl Counter for SimpleCounter {
    pub func increment(mut this) {
        this.value = this.value + 1
    }
    // count_to uses default implementation
}

@test
func test_default_method_returns_value() -> I32 {
    var counter = SimpleCounter { value: 0 }
    let result = counter.count_to(5)
    assert_eq(result, 5, "default method should return I64")
    return 0
}
```
