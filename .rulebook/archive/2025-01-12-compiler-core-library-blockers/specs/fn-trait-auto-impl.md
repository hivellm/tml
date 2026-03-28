# Spec: Fn Trait Auto-Implementation for Closures

## Problem

Closures don't automatically implement `Fn`/`FnMut`/`FnOnce` behaviors, preventing their use with generic higher-order functions.

### Current State

```tml
// Works: Direct closure calls
let add = do(x: I32) -> I32 { x + 1 }
let result = add(5)  // OK, returns 6

// Doesn't work: Generic functions with Fn bounds
func apply[F: Fn[I32] -> I32](f: F, x: I32) -> I32 {
    return f.call(x)  // Error: F doesn't implement Fn
}

let result = apply(add, 5)  // Error!
```

### Expected Behavior

```tml
func apply[F: Fn[I32] -> I32](f: F, x: I32) -> I32 {
    return f.call(x)
}

let add = do(x: I32) -> I32 { x + 1 }
let result = apply(add, 5)  // Should work, returns 6
```

## Design

### Fn Trait Hierarchy

```tml
// Already defined in lib/core/src/ops/function.tml
behavior FnOnce[Args] {
    type Output
    func call_once(this, args: Args) -> This::Output
}

behavior FnMut[Args]: FnOnce[Args] {
    func call_mut(mut this, args: Args) -> This::Output
}

behavior Fn[Args]: FnMut[Args] {
    func call(this, args: Args) -> This::Output
}
```

### Closure Struct Generation

For each closure, generate an anonymous struct:

```tml
// Source:
let x = 10
let add = do(y: I32) -> I32 { x + y }

// Generated (conceptual):
type __closure_0 {
    capture_x: I32  // Captured by value
}

impl Fn[(I32,)] for __closure_0 {
    type Output = I32

    pub func call(this, args: (I32,)) -> I32 {
        let y = args.0
        return this.capture_x + y
    }
}
```

### Capture Modes

| Capture | Trait Implemented | Struct Field |
|---------|-------------------|--------------|
| By value (copy) | `Fn` | `T` |
| By ref | `Fn` | `ref T` |
| By mut ref | `FnMut` | `mut ref T` |
| By move | `FnOnce` | `T` |

## Implementation

### Step 1: Closure Analysis

In type checker, analyze closure captures:

```cpp
struct CaptureInfo {
    std::string name;
    TypePtr type;
    CaptureMode mode;  // Copy, Ref, MutRef, Move
};

struct ClosureInfo {
    std::vector<CaptureInfo> captures;
    std::vector<TypePtr> param_types;
    TypePtr return_type;
    FnTraitKind highest_trait;  // Fn, FnMut, or FnOnce
};
```

### Step 2: Generate Closure Struct

In codegen, generate struct for closure:

```cpp
void LLVMIRGen::gen_closure_struct(const ClosureInfo& info) {
    std::string struct_name = "__closure_" + std::to_string(closure_counter_++);

    // Generate struct type
    emit_line("%struct." + struct_name + " = type {");
    for (const auto& capture : info.captures) {
        emit_line("  " + llvm_type(capture.type) + ",");
    }
    emit_line("}");

    // Store for later use
    closure_structs_[current_closure_id_] = struct_name;
}
```

### Step 3: Generate Fn Impl

```cpp
void LLVMIRGen::gen_closure_fn_impl(const ClosureInfo& info) {
    std::string struct_name = closure_structs_[current_closure_id_];

    // Generate call method
    emit_line("define " + llvm_type(info.return_type) +
              " @" + struct_name + "_call(" +
              "%struct." + struct_name + " %this, " +
              args_type + " %args) {");

    // Unpack captures
    for (size_t i = 0; i < info.captures.size(); ++i) {
        emit_line("  %" + info.captures[i].name +
                  " = extractvalue %struct." + struct_name + " %this, " +
                  std::to_string(i));
    }

    // Unpack args tuple
    for (size_t i = 0; i < info.param_types.size(); ++i) {
        emit_line("  %arg" + std::to_string(i) +
                  " = extractvalue " + args_type + " %args, " +
                  std::to_string(i));
    }

    // Generate body
    gen_closure_body(info);

    emit_line("}");
}
```

### Step 4: Update Method Dispatch

When calling `f.call(args)` on a closure type:

```cpp
if (is_closure_type(receiver_type)) {
    std::string struct_name = get_closure_struct(receiver_type);
    std::string call_fn = struct_name + "_call";
    return gen_call(call_fn, receiver, args);
}
```

## Test Cases

```tml
@test
func test_closure_fn_trait() -> I32 {
    func apply[F: Fn[I32] -> I32](f: F, x: I32) -> I32 {
        return f.call(x)
    }

    let double = do(x: I32) -> I32 { x * 2 }
    let result = apply(double, 5)
    assert_eq(result, 10, "apply should work with closure")
    return 0
}

@test
func test_closure_with_capture() -> I32 {
    func apply[F: Fn[I32] -> I32](f: F, x: I32) -> I32 {
        return f.call(x)
    }

    let multiplier = 3
    let mult = do(x: I32) -> I32 { x * multiplier }
    let result = apply(mult, 5)
    assert_eq(result, 15, "closure with capture should work")
    return 0
}

@test
func test_fn_mut_closure() -> I32 {
    func apply_mut[F: FnMut[I32] -> I32](mut f: F, x: I32) -> I32 {
        return f.call_mut(x)
    }

    var counter = 0
    let increment = do(x: I32) -> I32 {
        counter = counter + 1
        return x + counter
    }

    let r1 = apply_mut(increment, 10)  // counter = 1, returns 11
    let r2 = apply_mut(increment, 10)  // counter = 2, returns 12
    assert_eq(r1 + r2, 23, "FnMut closure should mutate capture")
    return 0
}
```
