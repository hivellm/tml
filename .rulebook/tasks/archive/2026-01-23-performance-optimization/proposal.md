# Performance Optimization Proposal

## Executive Summary

TML benchmarks reveal significant performance gaps vs C++ in string operations (250-2500x slower) and array iteration (6-7x slower). This proposal outlines the root causes and solutions for each.

---

## Problem Analysis

### 1. String Concatenation: 250-2500x Slower

**Root Cause Analysis:**

```
TML:  "a" + "b" + "c"
      ↓
      tml_str_concat("a", "b") → malloc + memcpy → temp1
      tml_str_concat(temp1, "c") → malloc + memcpy → result
      free(temp1)

C++:  "a" + "b" + "c"
      ↓
      (with SSO) stack buffer, no malloc for small strings
      (optimized) single allocation of final size
```

**Issues Identified:**
1. **No Small String Optimization (SSO)**: Every string allocates heap memory
2. **Quadratic concatenation**: N concats = N allocations + N copies
3. **FFI overhead**: Each concat calls into C runtime
4. **No escape analysis**: Temporary strings always heap-allocated

**Solution Architecture:**

```
Phase 1: Runtime SSO
┌─────────────────────────────────────┐
│ TmlString (32 bytes)                │
│ ┌─────────────────────────────────┐ │
│ │ union {                         │ │
│ │   struct { char[23]; u8 len; } │ │  ← Small (≤23 chars, inline)
│ │   struct { char* ptr; u64 len; │ │  ← Large (heap allocated)
│ │            u64 cap; u8 tag; }  │ │
│ │ }                               │ │
│ └─────────────────────────────────┘ │
└─────────────────────────────────────┘

Phase 2: Compiler concat fusion
"a" + "b" + "c"  →  tml_str_concat_n(["a","b","c"], 3)
                    ↓
                    Single allocation of strlen(a)+strlen(b)+strlen(c)
                    Single memcpy chain
```

---

### 2. Int to String: 22x Slower

**Root Cause Analysis:**

```
TML current (estimated):
  while (n > 0) {
    digit = n % 10       // Expensive division
    buffer[i++] = '0' + digit
    n = n / 10           // Another division
  }
  reverse(buffer)
  return alloc_string(buffer)

C++ optimized:
  Uses lookup table for 2-digit pairs
  Uses multiplication by reciprocal (faster than division)
  Writes directly to pre-sized buffer
```

**Solution: Implement Optimized Algorithm**

```c
// Lookup table approach
static const char digits[] =
    "00010203040506070809"
    "10111213141516171819"
    // ... up to 99

char* itoa_fast(int64_t n, char* buf) {
    if (n < 0) { *buf++ = '-'; n = -n; }

    char* p = buf;
    while (n >= 100) {
        int idx = (n % 100) * 2;
        n /= 100;
        *p++ = digits[idx];
        *p++ = digits[idx + 1];
    }
    // Handle remaining 1-2 digits
    if (n >= 10) {
        int idx = n * 2;
        *p++ = digits[idx];
        *p++ = digits[idx + 1];
    } else {
        *p++ = '0' + n;
    }
    // Reverse in-place
    reverse(buf, p);
    *p = '\0';
    return buf;
}
```

**Expected Improvement:** 10-20x faster (within 2x of C++)

---

### 3. Text Builder: 10-50x Slower

**Root Cause Analysis:**

```
TML Text::push_str current:
1. Check capacity
2. If full: realloc (possibly move entire buffer)
3. memcpy new data
4. Update length

C++ string::append (optimized):
1. Check capacity (usually has slack from 2x growth)
2. memcpy new data (inline, SIMD optimized)
3. Update length
```

**Issues:**
1. **Growth factor too small**: Causes frequent reallocations
2. **FFI overhead per push**: Each push_str crosses FFI boundary
3. **No bulk operations**: Can't batch multiple appends

**Solution:**

```cpp
// Improved Text implementation
class Text {
    char* data_;
    size_t len_;
    size_t cap_;

    void ensure_capacity(size_t additional) {
        size_t required = len_ + additional;
        if (required > cap_) {
            // 2x growth factor, minimum 64 bytes
            size_t new_cap = std::max(cap_ * 2, std::max(required, 64UL));
            data_ = (char*)realloc(data_, new_cap);
            cap_ = new_cap;
        }
    }

public:
    void push_str(const char* s, size_t slen) {
        ensure_capacity(slen);
        memcpy(data_ + len_, s, slen);  // SIMD-optimized memcpy
        len_ += slen;
    }

    // Batch operation for concat chains
    void push_many(const char** strs, size_t* lens, size_t count) {
        size_t total = 0;
        for (size_t i = 0; i < count; i++) total += lens[i];
        ensure_capacity(total);
        for (size_t i = 0; i < count; i++) {
            memcpy(data_ + len_, strs[i], lens[i]);
            len_ += lens[i];
        }
    }
};
```

**Compiler Optimization:**

```
// Detect this pattern:
let t = Text::new()
t.push_str("Hello, ")
t.push_str(name)
t.push_str("!")

// Transform to:
let t = Text::with_capacity(7 + name.len() + 1)
t.push_many(["Hello, ", name, "!"], [7, name.len(), 1], 3)
```

---

### 4. Array Iteration: 6-7x Slower

**Root Cause Analysis:**

Comparing LLVM IR:

```llvm
; C++ (vectorized by LLVM)
vector.body:
  %vec.ind = phi <4 x i64> [ <i64 0, i64 1, i64 2, i64 3>, %vector.ph ], [ %vec.ind.next, %vector.body ]
  %sum.vec = phi <4 x i64> [ zeroinitializer, %vector.ph ], [ %sum.next, %vector.body ]
  %gep = getelementptr i64, ptr %arr, <4 x i64> %vec.ind
  %vals = call <4 x i64> @llvm.masked.gather(...)
  %sum.next = add <4 x i64> %sum.vec, %vals
  ; Process 4 elements per iteration!

; TML current (scalar)
loop.body:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop.body ]
  %sum = phi i64 [ 0, %entry ], [ %sum.next, %loop.body ]
  ; Bounds check
  %cmp = icmp ult i64 %i, %len
  br i1 %cmp, label %access, label %trap
access:
  %ptr = getelementptr i64, ptr %arr, i64 %i
  %val = load i64, ptr %ptr
  %sum.next = add i64 %sum, %val
  ; Process 1 element per iteration
```

**Issues:**
1. **Bounds checks not hoisted**: Check inside loop prevents vectorization
2. **No vectorization hints**: LLVM doesn't know it's safe to vectorize
3. **Inefficient loop structure**: Extra phi nodes and branches

**Solution:**

```cpp
// In MIR codegen, add bounds check elimination pass
class BoundsCheckEliminationPass {
    // For loops with provable bounds:
    // - Hoist bounds check to loop preheader
    // - Replace inner check with unconditional access

    bool can_eliminate(Loop* loop, ArrayAccess* access) {
        // Check if loop bound ≤ array length
        if (auto* cmp = dyn_cast<ICmpInst>(loop->getCondition())) {
            if (is_array_length(cmp->getOperand(1), access->array)) {
                return true;
            }
        }
        return false;
    }
};

// Add vectorization hints to LLVM IR
void emit_loop_hints(BasicBlock* header) {
    // Add metadata
    MDNode* hints = MDNode::get(ctx, {
        MDString::get(ctx, "llvm.loop.vectorize.enable"),
        ConstantAsMetadata::get(ConstantInt::getTrue(ctx))
    });
    header->getTerminator()->setMetadata("llvm.loop", hints);
}
```

**Add `for` syntax with guaranteed bounds:**

```tml
// New syntax that guarantees safe bounds
for i in 0..arr.len() {
    sum = sum + arr[i]  // Compiler knows i < arr.len()
}

// Compiles to bounds-check-free loop
```

---

### 5. Loop + Continue: 3.5x Slower

**Root Cause Analysis:**

```llvm
; TML current (extra blocks)
loop.body:
  %cond = ...
  br i1 %cond, label %continue.block, label %rest
continue.block:
  br label %loop.header    ; Extra branch
rest:
  ; actual work
  br label %loop.header

; C++ (optimized)
loop.body:
  %cond = ...
  br i1 %cond, label %loop.header, label %rest  ; Direct branch
rest:
  ; actual work
  br label %loop.header
```

**Solution: SimplifyCFG Enhancement**

```cpp
// In SimplifyCfgPass, add continue block elimination
bool simplify_continue_blocks(Function& func) {
    for (auto& bb : func.blocks) {
        // If block only contains unconditional branch to loop header
        if (bb.instructions.size() == 1) {
            if (auto* br = get_branch(bb)) {
                if (br->is_unconditional() && is_loop_header(br->target)) {
                    // Redirect all predecessors directly to loop header
                    redirect_predecessors(bb, br->target);
                    remove_block(bb);
                }
            }
        }
    }
}
```

---

## Implementation Priority

| Phase | Impact | Effort | Priority |
|-------|--------|--------|----------|
| String SSO | High (250x→10x) | Medium | P0 |
| Int to String | Medium (22x→2x) | Low | P1 |
| Text Builder | High (50x→2x) | Medium | P0 |
| Bounds Check Elim | High (7x→1.5x) | High | P1 |
| Loop Continue | Low (3.5x→1.5x) | Low | P2 |

**Recommended Order:**
1. String SSO + concat fusion (biggest user-visible impact)
2. Text Builder optimization (commonly used)
3. Array iteration / bounds check elimination
4. Int to String
5. Loop continue

---

## Testing Strategy

```bash
# After each phase, run:
tml run profile/tml/string_bench.tml --release
tml run profile/tml/text_bench.tml --release
tml run profile/tml/collections_bench.tml --release

# Compare with C++ baseline:
./profile/cpp/string_bench.exe
./profile/cpp/text_bench.exe
./profile/cpp/collections_bench.exe

# Generate comparison report:
python scripts/bench_compare.py --tml results/tml.json --cpp results/cpp.json
```

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| SSO breaks ABI | Version runtime, provide migration path |
| Bounds elim unsound | Extensive fuzzing, formal verification for key proofs |
| Regression in other benchmarks | CI tracks all benchmarks, alerts on regression |
| Increased compile time | Add optimization levels, only run expensive passes at -O2+ |
