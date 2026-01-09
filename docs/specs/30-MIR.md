# TML v1.0 — Mid-level Intermediate Representation (MIR)

## 1. Overview

The Mid-level IR (MIR) is an SSA-form intermediate representation that sits between the type-checked AST and LLVM IR generation. It provides a clean, optimizable representation for TML programs.

### 1.1 Design Goals

1. **SSA Form** - Each variable defined exactly once
2. **Explicit Control Flow** - Basic blocks with explicit terminators
3. **Type-Annotated** - All values carry type information
4. **Optimizable** - Clean structure for optimization passes
5. **Easy Lowering** - Close enough to LLVM IR for straightforward translation

### 1.2 Pipeline Position

```
TML Source → Lexer → Parser → AST → Type Checker → MIR → Optimizer → LLVM IR → Machine Code
                                         ↑                    ↑
                                    This document        Optimization passes
```

## 2. MIR Types

MIR has its own type system that maps directly to low-level representations.

### 2.1 Primitive Types

```cpp
enum class PrimitiveType {
    Unit,   // Zero-sized type
    Bool,   // Boolean (i1 in LLVM)
    I8, I16, I32, I64, I128,   // Signed integers
    U8, U16, U32, U64, U128,   // Unsigned integers
    F32, F64,                   // Floating point
    Ptr,    // Raw pointer (void*)
    Str,    // String pointer
};
```

### 2.2 Composite Types

| Type | Description | Example |
|------|-------------|---------|
| `MirPointerType` | Pointer to another type | `*mut I32` |
| `MirArrayType` | Fixed-size array | `[I32; 10]` |
| `MirSliceType` | Slice reference | `[]I32` |
| `MirTupleType` | Tuple of types | `(I32, Bool)` |
| `MirStructType` | Named struct | `Point { x: F64, y: F64 }` |
| `MirEnumType` | Tagged union | `Maybe[T]` |
| `MirFunctionType` | Function signature | `func(I32, I32) -> I32` |

## 3. Values and Instructions

### 3.1 Value Identifiers

Every computed value in MIR has a unique identifier (ValueId):

```
%0 = const.i32 42
%1 = add %0, %0
%2 = call @double(%1)
```

### 3.2 Instruction Categories

#### Constants
```
const.unit                    ; Unit value
const.bool true               ; Boolean constant
const.i32 42                  ; Integer constant
const.f64 3.14159             ; Float constant
const.str "hello"             ; String constant
```

#### Arithmetic Operations
```
add %a, %b       ; Addition
sub %a, %b       ; Subtraction
mul %a, %b       ; Multiplication
div %a, %b       ; Division
rem %a, %b       ; Remainder/modulo
neg %a           ; Negation
```

#### Comparison Operations
```
eq %a, %b        ; Equal
ne %a, %b        ; Not equal
lt %a, %b        ; Less than
le %a, %b        ; Less or equal
gt %a, %b        ; Greater than
ge %a, %b        ; Greater or equal
```

#### Logical Operations
```
and %a, %b       ; Logical AND
or %a, %b        ; Logical OR
not %a           ; Logical NOT
```

#### Bitwise Operations
```
bit_and %a, %b   ; Bitwise AND
bit_or %a, %b    ; Bitwise OR
bit_xor %a, %b   ; Bitwise XOR
bit_not %a       ; Bitwise NOT
shl %a, %b       ; Shift left
shr %a, %b       ; Shift right (arithmetic)
```

#### Memory Operations
```
alloca T                      ; Stack allocation
load %ptr                     ; Load from pointer
store %val, %ptr              ; Store to pointer
gep %ptr, %idx                ; Get element pointer
field_ptr %struct_ptr, field  ; Get field pointer
```

#### Control Flow
```
phi [%a, bb1], [%b, bb2]      ; SSA phi node
call @func(%arg1, %arg2)      ; Function call
```

#### Type Conversions
```
cast %val to T                ; Type cast
trunc %val to T               ; Truncate integer
ext %val to T                 ; Extend integer (sign/zero)
```

### 3.3 Terminators

Each basic block ends with exactly one terminator:

```
ret %value              ; Return with value
ret void                ; Return void
br bb_target            ; Unconditional branch
br_cond %cond, bb_true, bb_false  ; Conditional branch
switch %val, [case1: bb1, case2: bb2, ...], bb_default
unreachable             ; Unreachable code marker
```

## 4. Basic Blocks and Functions

### 4.1 Basic Block Structure

```
bb0:                          ; Block label
    %0 = const.i32 1          ; Instructions
    %1 = const.i32 2
    %2 = add %0, %1
    br bb1                    ; Terminator

bb1:                          ; Successor block
    %3 = phi [%2, bb0]        ; Phi node for SSA
    ret %3
```

### 4.2 Function Structure

```mir
func @factorial(n: I32) -> I32 {
bb_entry:
    %cond = le %n, const.i32 1
    br_cond %cond, bb_base, bb_recurse

bb_base:
    ret const.i32 1

bb_recurse:
    %n_minus_1 = sub %n, const.i32 1
    %rec_result = call @factorial(%n_minus_1)
    %result = mul %n, %rec_result
    ret %result
}
```

## 5. Optimization Passes

### 5.1 Pass Infrastructure

```cpp
// Base class for all passes
class MirPass {
    virtual std::string name() const = 0;
    virtual bool run(Module& module) = 0;
};

// Function-level pass
class FunctionPass : public MirPass {
    virtual bool run_on_function(Function& func) = 0;
};

// Block-level pass
class BlockPass : public MirPass {
    virtual bool run_on_block(BasicBlock& block, Function& func) = 0;
};
```

### 5.2 Available Passes

The TML compiler implements 33 optimization passes organized by optimization level:

#### Core Passes (O1+)

| Pass | Level | Description |
|------|-------|-------------|
| `EarlyCSE` | Block | Fast local common subexpression elimination |
| `InstSimplify` | Function | Instruction simplification and canonicalization |
| `ConstantFolding` | Block | Evaluate constant expressions at compile time |
| `ConstantPropagation` | Function | Replace uses of constants with their values |
| `SimplifyCfg` | Function | Simplify control flow graph |
| `DeadCodeElimination` | Function | Remove unused instructions |

#### Standard Passes (O2+)

| Pass | Level | Description |
|------|-------|-------------|
| `SROA` | Function | Scalar Replacement of Aggregates - break up struct allocas |
| `Mem2Reg` | Function | Promote stack allocations to SSA registers |
| `Peephole` | Block | Algebraic simplifications (x+0→x, x*1→x, etc.) |
| `SimplifySelect` | Function | Simplify select/conditional instructions |
| `StrengthReduction` | Function | Replace expensive ops with cheaper ones (mul→shift) |
| `Reassociate` | Function | Reorder associative operations for optimization |
| `GVN` | Function | Global Value Numbering - cross-block CSE |
| `CopyPropagation` | Function | Replace copies with original values |
| `BlockMerge` | Function | Merge sequential basic blocks |
| `MatchSimplify` | Function | Simplify match/switch statements |
| `Inlining` | Function | Inline small function calls |
| `LoadStoreOpt` | Function | Eliminate redundant loads and stores |
| `LICM` | Function | Loop-Invariant Code Motion |
| `JumpThreading` | Function | Thread jumps through conditional blocks |
| `TailCall` | Function | Optimize tail-recursive calls |
| `UnreachableCodeElimination` | Function | Remove unreachable blocks |
| `DeadFunctionElimination` | Module | Remove unused functions |

#### Aggressive Passes (O3)

| Pass | Level | Description |
|------|-------|-------------|
| `Narrowing` | Function | Use smaller integer types when safe |
| `ConstantHoist` | Function | Hoist expensive constants out of loops |
| `LoopRotate` | Function | Transform loops for better optimization |
| `LoopUnroll` | Function | Unroll small constant-bound loops |
| `Sinking` | Function | Move computations closer to their uses |
| `ADCE` | Function | Aggressive Dead Code Elimination |
| `DeadArgElimination` | Module | Remove unused function arguments |
| `MergeReturns` | Function | Combine multiple returns into single exit |

#### Specialized Passes

| Pass | Level | Description |
|------|-------|-------------|
| `EscapeAnalysis` | Function | Track pointer escape for stack allocation |
| `AsyncLowering` | Function | Lower async/await to state machines |

### 5.3 Optimization Levels

```cpp
enum class OptLevel {
    O0,  // No optimization - just type checking
    O1,  // Basic optimizations (constant folding, DCE, early CSE)
    O2,  // Standard optimizations (O1 + SROA, mem2reg, GVN, inlining, LICM)
    O3,  // Aggressive optimizations (O2 + loop opts, narrowing, ADCE)
};
```

### 5.4 Pass Manager

```cpp
PassManager pm(OptLevel::O2);
pm.configure_standard_pipeline();  // Add standard passes for level
pm.run(mir_module);                // Run all passes
```

## 6. Example Transformations

### 6.1 Constant Folding

Before:
```mir
%0 = const.i32 2
%1 = const.i32 3
%2 = add %0, %1       ; Can be folded
%3 = mul %2, %2       ; Can be folded after
ret %3
```

After:
```mir
%0 = const.i32 25     ; 2+3=5, 5*5=25
ret %0
```

### 6.2 Dead Code Elimination

Before:
```mir
%0 = const.i32 42
%1 = const.i32 10     ; Unused
%2 = add %0, %0       ; Unused
ret %0
```

After:
```mir
%0 = const.i32 42
ret %0
```

### 6.3 Common Subexpression Elimination

Before:
```mir
%0 = load %ptr
%1 = add %0, const.i32 1
%2 = load %ptr        ; Same as %0
%3 = add %2, const.i32 1  ; Same as %1
%4 = add %1, %3
ret %4
```

After:
```mir
%0 = load %ptr
%1 = add %0, const.i32 1
%4 = add %1, %1       ; Reuse %1
ret %4
```

### 6.4 Peephole Optimization

Before:
```mir
%0 = mul %x, const.i32 0   ; Multiply by 0
%1 = add %y, const.i32 0   ; Add 0
%2 = mul %z, const.i32 1   ; Multiply by 1
ret %2
```

After:
```mir
%0 = const.i32 0          ; x * 0 → 0
                          ; y + 0 → y (eliminated)
ret %z                    ; z * 1 → z
```

### 6.5 SimplifySelect

Before:
```mir
%0 = const.bool true
%1 = select %0, %a, %b    ; Select with constant true
%2 = select %cond, %x, %x ; Select with same values
ret %1
```

After:
```mir
ret %a                    ; select(true, a, b) → a
                          ; select(cond, x, x) → x
```

### 6.6 MergeReturns

Before:
```mir
func @example(x: I32) -> I32 {
bb_entry:
    br_cond %cond, bb_true, bb_false

bb_true:
    %1 = const.i32 1
    ret %1

bb_false:
    %2 = const.i32 0
    ret %2
}
```

After:
```mir
func @example(x: I32) -> I32 {
bb_entry:
    br_cond %cond, bb_true, bb_false

bb_true:
    %1 = const.i32 1
    br unified_exit

bb_false:
    %2 = const.i32 0
    br unified_exit

unified_exit:
    %ret = phi [%1, bb_true], [%2, bb_false]
    ret %ret
}
```

### 6.7 ConstantHoist

Before (constant inside loop):
```mir
bb_loop:
    %c = const.f64 3.14159   ; Expensive constant materialized each iteration
    %r = mul %x, %c
    br_cond %cond, bb_loop, bb_exit
```

After (constant hoisted to preheader):
```mir
bb_preheader:
    %c = const.f64 3.14159   ; Hoisted outside loop
    br bb_loop

bb_loop:
    %r = mul %x, %c          ; Uses hoisted value
    br_cond %cond, bb_loop, bb_exit
```

### 6.8 BlockMerge

Before:
```mir
bb0:
    %0 = const.i32 1
    br bb1

bb1:                         ; Single predecessor, no phi nodes
    %1 = add %0, %0
    ret %1
```

After:
```mir
bb0:
    %0 = const.i32 1
    %1 = add %0, %0          ; Merged into single block
    ret %1
```

## 7. Serialization

MIR supports both text and binary serialization for debugging and caching.

### 7.1 Text Format

Human-readable format for debugging:

```mir
; Module: example
; Generated: 2025-12-29

func @add(a: I32, b: I32) -> I32 {
bb_entry:
    %result = add %a, %b
    ret %result
}
```

### 7.2 Binary Format

Compact binary format for build caching:
- Header: magic number, version, checksum
- Type table: all types used in module
- Function table: function signatures
- Instruction stream: encoded instructions

## 8. Analysis Utilities

### 8.1 Value Usage Analysis

```cpp
// Check if a value is used anywhere in the function
bool is_value_used(const Function& func, ValueId value);
```

### 8.2 Side Effect Analysis

```cpp
// Check if an instruction has observable side effects
bool has_side_effects(const Instruction& inst);

// Side effect categories:
// - Memory writes (store)
// - Function calls (may have side effects)
// - I/O operations
```

### 8.3 Constant Analysis

```cpp
// Check if instruction produces a compile-time constant
bool is_constant(const Instruction& inst);

// Extract constant value if available
std::optional<int64_t> get_constant_int(const Instruction& inst);
std::optional<bool> get_constant_bool(const Instruction& inst);
```

## 9. Integration with Compiler

### 9.1 Building MIR from AST

```cpp
MirBuilder builder;
auto mir_module = builder.build(typed_ast);
```

### 9.2 Running Optimization Pipeline

```cpp
PassManager pm(opt_level);
pm.configure_standard_pipeline();
int changes = pm.run(mir_module);
```

### 9.3 Lowering to LLVM IR

```cpp
LLVMIRGen llvm_gen(type_env, options);
auto llvm_ir = llvm_gen.generate_from_mir(mir_module);
```

## 10. Source Files

### Core Infrastructure

| File | Description |
|------|-------------|
| `include/mir/mir.hpp` | Core data structures (Value, Instruction, BasicBlock, Function) |
| `include/mir/mir_builder.hpp` | MIR construction API |
| `include/mir/mir_pass.hpp` | Optimization pass infrastructure |
| `include/mir/mir_serialize.hpp` | Serialization interface |
| `src/mir/mir_type.cpp` | Type utilities |
| `src/mir/mir_function.cpp` | Function/block management |
| `src/mir/mir_printer.cpp` | Human-readable output |
| `src/mir/mir_builder.cpp` | AST to MIR conversion |
| `src/mir/mir_serialize.cpp` | Serialization implementation |
| `src/mir/mir_pass.cpp` | Pass manager and analysis utilities |

### Optimization Passes

| File | Description |
|------|-------------|
| `passes/constant_folding.cpp` | Evaluate constant expressions |
| `passes/constant_propagation.cpp` | Propagate constant values |
| `passes/copy_propagation.cpp` | Eliminate redundant copies |
| `passes/dead_code_elimination.cpp` | Remove unused instructions |
| `passes/unreachable_code_elimination.cpp` | Remove unreachable blocks |
| `passes/common_subexpression_elimination.cpp` | Local CSE |
| `passes/early_cse.cpp` | Fast early CSE |
| `passes/gvn.cpp` | Global Value Numbering |
| `passes/simplify_cfg.cpp` | CFG simplification |
| `passes/inst_simplify.cpp` | Instruction canonicalization |
| `passes/peephole.cpp` | Algebraic simplifications |
| `passes/simplify_select.cpp` | Select instruction simplification |
| `passes/strength_reduction.cpp` | Expensive op replacement |
| `passes/reassociate.cpp` | Operation reordering |
| `passes/sroa.cpp` | Scalar Replacement of Aggregates |
| `passes/mem2reg.cpp` | Alloca promotion to SSA |
| `passes/inlining.cpp` | Function inlining |
| `passes/dead_function_elimination.cpp` | Remove unused functions |
| `passes/dead_arg_elim.cpp` | Remove unused arguments |
| `passes/licm.cpp` | Loop-Invariant Code Motion |
| `passes/loop_rotate.cpp` | Loop rotation |
| `passes/loop_unroll.cpp` | Loop unrolling |
| `passes/const_hoist.cpp` | Constant hoisting from loops |
| `passes/jump_threading.cpp` | Jump threading |
| `passes/tail_call.cpp` | Tail call optimization |
| `passes/match_simplify.cpp` | Match/switch simplification |
| `passes/narrowing.cpp` | Integer type narrowing |
| `passes/sinking.cpp` | Code sinking |
| `passes/adce.cpp` | Aggressive DCE |
| `passes/block_merge.cpp` | Basic block merging |
| `passes/load_store_opt.cpp` | Memory operation optimization |
| `passes/merge_returns.cpp` | Return statement merging |
| `passes/escape_analysis.cpp` | Pointer escape analysis |
| `passes/async_lowering.cpp` | Async/await lowering |

---

*Previous: [29-PACKAGES.md](./29-PACKAGES.md)*
*Next: [INDEX.md](./INDEX.md)*
