# Stack Promotion Specification

## Overview

Stack promotion transforms heap allocations into stack allocations for objects that don't escape their defining scope. This is the most impactful optimization for TML OOP code.

## Current Behavior

TML allocates all class instances on the heap:

```tml
func example() -> F64 {
    let circle = Circle::new(5.0)  // heap allocation
    let area = circle.area()
    return area
}  // implicit free
```

Generates:
```llvm
define double @example() {
  %1 = call ptr @tml_alloc(i64 24)           ; heap allocation
  call void @Circle_init(ptr %1, double 5.0)
  %2 = call double @Circle_area(ptr %1)
  call void @tml_free(ptr %1)                 ; heap free
  ret double %2
}
```

## Target Behavior

After stack promotion:
```llvm
define double @example() {
  %1 = alloca %Circle, align 8                ; stack allocation
  store double 5.0, ptr %1                    ; direct store
  %2 = call double @Circle_area(ptr %1)       ; still virtual or inlined
  ret double %2                               ; no free needed
}
```

With SROA and inlining:
```llvm
define double @example() {
  ret double 78.539816339744831               ; constant folded!
}
```

## Escape Analysis

An object escapes if any of these conditions hold:

### Definitely Escapes
1. **Returned from function**: `return obj`
2. **Stored to heap**: `self.field = obj`
3. **Passed to unknown function**: `unknown_fn(obj)`
4. **Stored to global**: `GLOBAL = obj`
5. **Used in closure**: `do { obj.method() }`

### Might Escape (conservative)
1. **Passed to function with ref param**: `fn(ref obj)` (may store)
2. **Cast to trait object**: `obj as dyn Trait`
3. **Address taken**: `ptr_to(obj)`

### Does Not Escape
1. **Local use only**: `let x = obj.field`
2. **Method call on self**: `obj.method()`
3. **Passed by value to pure function**: `pure_fn(obj.copy())`

## MIR Representation

### Allocation Instruction

Current:
```
%1 = alloc Circle
```

With stack eligibility:
```
%1 = alloc Circle @stack_eligible
```

### After Stack Promotion Pass

```
%1 = stack_alloc Circle  ; new instruction type
```

## Implementation Steps

### Step 1: Escape Analysis Enhancement

```cpp
// In escape_analysis.cpp

enum class EscapeState {
    NoEscape,       // Safe for stack promotion
    MayEscape,      // Conservative: treat as escaping
    Escapes         // Definitely escapes
};

class EscapeAnalysis {
    std::unordered_map<ValueId, EscapeState> escape_states_;

    EscapeState analyze_value(const mir::Value& val, const mir::Function& func) {
        // Track all uses of the value
        for (const auto& use : get_uses(val, func)) {
            if (is_return(use)) return Escapes;
            if (is_store_to_heap(use)) return Escapes;
            if (is_passed_to_unknown(use)) return MayEscape;
            // ... more checks
        }
        return NoEscape;
    }
};
```

### Step 2: Stack Promotion Pass

```cpp
// In stack_promotion.cpp

class StackPromotionPass : public mir::Pass {
    void run(mir::Function& func) override {
        EscapeAnalysis ea;
        ea.analyze(func);

        for (auto& block : func.blocks) {
            for (auto& inst : block.instructions) {
                if (auto* alloc = std::get_if<mir::AllocInst>(&inst.data)) {
                    if (ea.can_promote(alloc->result)) {
                        // Transform to stack allocation
                        inst.data = mir::StackAllocInst{
                            .result = alloc->result,
                            .type = alloc->type
                        };
                        // Remove associated free
                        remove_free_for(alloc->result, func);
                    }
                }
            }
        }
    }
};
```

### Step 3: Codegen Changes

```cpp
// In class_codegen.cpp

llvm::Value* CodeGen::emit_alloc(const mir::AllocInst& inst) {
    if (inst.stack_eligible) {
        // Stack allocation
        auto* type = get_llvm_type(inst.type);
        return builder_.CreateAlloca(type, nullptr, "stack_obj");
    } else {
        // Heap allocation (current behavior)
        auto size = get_type_size(inst.type);
        return emit_call("tml_alloc", {size});
    }
}
```

## Edge Cases

### Objects with Destructors

Stack-allocated objects still need destructor calls:

```llvm
define void @example() {
  %1 = alloca %ResourceHolder
  call void @ResourceHolder_init(ptr %1)
  ; ... use object ...
  call void @ResourceHolder_drop(ptr %1)  ; destructor before return
  ret void
}
```

### Conditional Allocation

```tml
func example(cond: Bool) -> F64 {
    let obj = if cond {
        Circle::new(5.0)
    } else {
        Circle::new(10.0)
    }
    return obj.area()
}
```

Both branches should use same stack slot:
```llvm
define double @example(i1 %cond) {
  %obj = alloca %Circle
  br i1 %cond, label %then, label %else
then:
  call void @Circle_init(ptr %obj, double 5.0)
  br label %merge
else:
  call void @Circle_init(ptr %obj, double 10.0)
  br label %merge
merge:
  %area = call double @Circle_area(ptr %obj)
  ret double %area
}
```

### Loops

Objects allocated in loops must be analyzed per-iteration:

```tml
func sum_areas() -> F64 {
    let mut total = 0.0
    loop i in 0 to 100 {
        let c = Circle::new(i as F64)  // Stack OK - doesn't escape iteration
        total = total + c.area()
    }
    return total
}
```

## Interaction with Other Passes

### Must Run Before
- LLVM optimization passes
- SROA (needs stack allocations)

### Must Run After
- Escape analysis
- Devirtualization (helps identify non-escaping calls)
- Inlining (exposes more promotion opportunities)

## Testing Strategy

1. **Unit tests**: Verify escape analysis identifies escaping/non-escaping cases
2. **IR inspection**: Check LLVM IR for `alloca` vs `call @tml_alloc`
3. **Benchmark**: Measure Object Creation benchmark improvement
4. **Memory sanitizer**: Ensure no use-after-scope bugs

## Performance Target

| Metric | Before | After |
|--------|--------|-------|
| Object Creation | 10,554 µs | <500 µs |
| Heap allocations per iteration | 1 | 0 |
| LLVM IR size | ~20 lines | ~5 lines |
