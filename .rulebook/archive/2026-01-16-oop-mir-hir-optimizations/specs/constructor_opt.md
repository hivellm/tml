# Specification: Constructor Optimization

## Overview

Constructor chains in class hierarchies generate redundant code. This pass optimizes:
1. Inline base constructors
2. Fuse field initializations
3. Eliminate dead stores
4. Optimize vtable pointer writes

## Problem

Consider this inheritance chain:

```tml
class A {
    x: I32

    func new(x: I32) -> A {
        return A { x: x }
    }
}

class B extends A {
    y: I32

    func new(x: I32, y: I32) -> B {
        return B { base: A::new(x), y: y }
    }
}

class C extends B {
    z: I32

    func new(x: I32, y: I32, z: I32) -> C {
        return C { base: B::new(x, y), z: z }
    }
}
```

### Current Generated Code (Unoptimized)

```llvm
define ptr @tml_C_new(i32 %x, i32 %y, i32 %z) {
entry:
    ; Allocate C
    %c = call ptr @malloc(i64 32)

    ; Initialize vtable for C
    %vtable_ptr = getelementptr %class.C, ptr %c, i32 0, i32 0
    store ptr @vtable.C, ptr %vtable_ptr

    ; Call B::new (allocates ANOTHER object!)
    %b = call ptr @tml_B_new(i32 %x, i32 %y)

    ; Copy B fields to C.base (redundant!)
    %c_base = getelementptr %class.C, ptr %c, i32 0, i32 1
    call void @llvm.memcpy(ptr %c_base, ptr %b, i64 16)

    ; Free temporary B object
    call void @free(ptr %b)

    ; Initialize C.z
    %z_ptr = getelementptr %class.C, ptr %c, i32 0, i32 2
    store i32 %z, ptr %z_ptr

    ret ptr %c
}
```

**Problems:**
1. Allocates temporary B object
2. Copies B to C.base
3. Frees temporary B
4. Vtable pointer is written multiple times (A, B, C)

### Optimized Code

```llvm
define ptr @tml_C_new(i32 %x, i32 %y, i32 %z) {
entry:
    ; Single allocation for C
    %c = call ptr @malloc(i64 32)

    ; Single vtable initialization (final type)
    %vtable_ptr = getelementptr %class.C, ptr %c, i32 0, i32 0
    store ptr @vtable.C, ptr %vtable_ptr

    ; Direct field initialization (inlined from A, B)
    %x_ptr = getelementptr %class.C, ptr %c, i32 0, i32 1, i32 1  ; A.x
    store i32 %x, ptr %x_ptr

    %y_ptr = getelementptr %class.C, ptr %c, i32 0, i32 1, i32 2  ; B.y
    store i32 %y, ptr %y_ptr

    %z_ptr = getelementptr %class.C, ptr %c, i32 0, i32 2         ; C.z
    store i32 %z, ptr %z_ptr

    ret ptr %c
}
```

## Optimizations

### 1. Constructor Inlining

Inline base constructor calls:

```cpp
void ConstructorOptPass::inline_base_constructor(MIRFunction& func) {
    for (auto& block : func.blocks()) {
        for (auto& inst : block.instructions()) {
            if (auto* call = inst.as<CallInst>()) {
                if (is_base_constructor_call(call)) {
                    // Clone base constructor body into current function
                    inline_constructor(call, get_constructor_body(call->callee));
                }
            }
        }
    }
}
```

### 2. Field Initialization Fusion

Combine multiple small stores into memset/aggregate store:

```cpp
void ConstructorOptPass::fuse_field_stores(MIRFunction& func) {
    // Group stores to consecutive fields
    std::vector<StoreGroup> groups = find_consecutive_stores(func);

    for (auto& group : groups) {
        if (group.all_zero()) {
            // Replace with memset
            emit_memset(group.base, 0, group.size);
        } else if (group.can_use_aggregate()) {
            // Replace with single aggregate store
            emit_aggregate_store(group.base, group.values);
        }
    }
}
```

### 3. Dead Store Elimination (Constructor-Specific)

Eliminate stores that are immediately overwritten:

```cpp
void ConstructorOptPass::eliminate_dead_stores(MIRFunction& func) {
    // Track all stores in constructor
    std::unordered_map<std::string, StoreInst*> field_stores;

    for (auto& inst : func.instructions()) {
        if (auto* store = inst.as<StoreInst>()) {
            if (auto field = get_field_path(store->dest)) {
                // If field already stored, previous store is dead
                if (auto it = field_stores.find(*field); it != field_stores.end()) {
                    mark_dead(it->second);
                }
                field_stores[*field] = store;
            }
        }
    }
}
```

### 4. Vtable Pointer Optimization

Only write vtable pointer once (for final type):

```cpp
void ConstructorOptPass::optimize_vtable_writes(MIRFunction& func) {
    // Find all vtable pointer stores
    std::vector<StoreInst*> vtable_stores;
    for (auto& inst : func.instructions()) {
        if (is_vtable_store(inst)) {
            vtable_stores.push_back(&inst);
        }
    }

    // Keep only the last one (final type's vtable)
    for (size_t i = 0; i < vtable_stores.size() - 1; ++i) {
        vtable_stores[i]->remove();
    }
}
```

## In-Place Construction

For return value optimization, construct directly in caller's memory:

```tml
func create_dog() -> Dog {
    return Dog::new("Rex", 5)  // Construct in caller's return slot
}

func main() {
    let dog = create_dog()  // No copy, dog allocated at return address
}
```

```llvm
; Caller passes hidden return pointer
define void @tml_create_dog(ptr %retval) {
    ; Initialize directly in retval
    store ptr @vtable.Dog, ptr %retval
    ; ... initialize fields in %retval ...
    ret void
}
```

## Testing

### Test Cases

1. **chain_constructor.tml**: A extends B extends C constructor chain
2. **field_fusion.tml**: Multiple zero-initialized fields
3. **dead_store.tml**: Field set in base, overwritten in derived
4. **vtable_multi_write.tml**: Ensure single vtable write
5. **in_place_construction.tml**: Return value optimization

### Verification

Compare instruction count before/after:
- Allocations: 1 (not N for N-level inheritance)
- Vtable stores: 1 (not N)
- No memcpy for base class
