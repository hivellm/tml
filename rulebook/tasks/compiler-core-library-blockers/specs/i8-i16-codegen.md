# Spec: I8/I16 Codegen Fixes

## Problem 1: Negative Literal Returns

### Current Behavior

```tml
impl I8 {
    pub func min_value() -> I8 {
        return -128
    }
}
```

Generates incorrect LLVM IR:
```llvm
define i8 @tml_I8_min_value() {
  %t1 = sub i32 0, 128      ; Wrong! Should be i8
  ret i8 %t1                 ; Type mismatch error
}
```

### Expected Behavior

```llvm
define i8 @tml_I8_min_value() {
  ret i8 -128
}
```

### Root Cause

In `compiler/src/codegen/expr/unary.cpp`, negation of integer literals always produces i32:

```cpp
// Current: always uses i32 for subtraction
emit_line("  " + result + " = sub i32 0, " + operand);
```

### Fix

1. Check if we're in a return context with a known return type
2. Use the return type for negation if available
3. Generate appropriate `sub iN 0, X` instruction

```cpp
// Proposed: use target type from context
std::string neg_type = target_int_type_.empty() ? "i32" : target_int_type_;
emit_line("  " + result + " = sub " + neg_type + " 0, " + operand);
```

## Problem 2: Impl Method Type Mismatch

### Current Behavior

```tml
impl I8 {
    pub func abs(this) -> I8 {
        if this < 0 {
            return -this
        }
        return this
    }
}
```

Generates incorrect LLVM IR:
```llvm
define i8 @tml_I8_abs(i8 %this) {
  %t1 = icmp slt i32 %this, 0   ; Wrong! %this is i8
  ...
}
```

### Expected Behavior

```llvm
define i8 @tml_I8_abs(i8 %this) {
  %t1 = icmp slt i8 %this, 0    ; Correct
  ...
}
```

### Root Cause

In `compiler/src/codegen/expr/binary.cpp`, comparison operations widen operands to i32 but don't track original type.

### Fix

1. Track original operand type before widening
2. For i8/i16 operands, either:
   - Don't widen (LLVM supports i8/i16 comparisons)
   - Or truncate result back to original type

```cpp
// Option A: Don't widen for comparisons
// LLVM supports icmp on any integer type
emit_line("  " + result + " = icmp slt i8 " + left + ", " + right);

// Option B: Keep widening but truncate back
emit_line("  " + tmp + " = icmp slt i32 " + left_ext + ", " + right_ext);
// Result is i1, no truncation needed for comparison
```

## Test Cases

```tml
@test
func test_i8_min() -> I32 {
    let min: I8 = I8::min_value()
    assert_eq(min, -128i8, "I8 min should be -128")
    return 0
}

@test
func test_i8_abs() -> I32 {
    let x: I8 = -42i8
    let abs_x: I8 = x.abs()
    assert_eq(abs_x, 42i8, "abs(-42) should be 42")
    return 0
}

@test
func test_i16_min() -> I32 {
    let min: I16 = I16::min_value()
    assert_eq(min, -32768i16, "I16 min should be -32768")
    return 0
}
```
