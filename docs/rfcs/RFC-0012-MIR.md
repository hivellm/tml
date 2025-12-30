# RFC-0012: Mid-level Intermediate Representation (MIR)

## Status

**Active** - Implemented in v0.6.0

## Summary

This RFC specifies the Mid-level Intermediate Representation (MIR), an SSA-form IR that sits between the type-checked AST and LLVM IR generation. MIR enables language-specific optimizations before lowering to target code.

## Motivation

### Problem

The current compilation pipeline goes directly from type-checked AST to LLVM IR:

```
AST → Type Check → LLVM IR → Machine Code
```

This has limitations:
1. **No TML-specific optimizations** - We rely entirely on LLVM's optimizations
2. **Complex code generation** - LLVM IR gen must handle both semantics and lowering
3. **No intermediate caching** - No natural point to cache partially compiled code
4. **Debugging difficulty** - Large semantic gap between AST and LLVM IR

### Solution

Insert a Mid-level IR between type checking and LLVM generation:

```
AST → Type Check → MIR → Optimize → LLVM IR → Machine Code
```

MIR provides:
1. **SSA form** - Clean representation for dataflow analysis
2. **Explicit control flow** - Basic blocks with explicit terminators
3. **Type annotations** - All values carry type information
4. **Optimization hooks** - Natural place for TML-specific optimizations
5. **Caching boundary** - Can serialize and cache optimized MIR

## Specification

### 1. MIR Types

MIR uses a simplified type system that maps directly to low-level representations.

#### 1.1 Primitive Types

```
PrimitiveType ::=
    | Unit      ; zero-sized type
    | Bool      ; boolean (i1 in LLVM)
    | I8 | I16 | I32 | I64 | I128    ; signed integers
    | U8 | U16 | U32 | U64 | U128    ; unsigned integers
    | F32 | F64                       ; floating point
    | Ptr       ; raw pointer (void*)
    | Str       ; string pointer
```

#### 1.2 Composite Types

```
MirType ::=
    | PrimitiveType
    | Pointer(MirType, is_mut: bool)  ; pointer to type
    | Array(MirType, size: usize)     ; fixed-size array
    | Slice(MirType)                  ; slice reference
    | Tuple(MirType*)                 ; tuple of types
    | Struct(name, type_args*)        ; named struct
    | Enum(name, type_args*)          ; tagged union
    | Function(params*, return_type)  ; function signature
```

### 2. Values and Instructions

#### 2.1 Value Identifiers

Every computed value has a unique identifier:

```
ValueId ::= '%' INTEGER
```

Example:
```mir
%0 = const.i32 42
%1 = add %0, %0
%2 = call @double(%1)
```

#### 2.2 Instruction Categories

##### Constants
```
const.unit                    ; Unit value
const.bool <true|false>       ; Boolean constant
const.i8 <value>              ; 8-bit signed integer
const.i16 <value>             ; 16-bit signed integer
const.i32 <value>             ; 32-bit signed integer
const.i64 <value>             ; 64-bit signed integer
const.u8 <value>              ; 8-bit unsigned integer
const.u16 <value>             ; 16-bit unsigned integer
const.u32 <value>             ; 32-bit unsigned integer
const.u64 <value>             ; 64-bit unsigned integer
const.f32 <value>             ; 32-bit float
const.f64 <value>             ; 64-bit float
const.str <string>            ; String constant
```

##### Arithmetic
```
add %a, %b       ; Addition
sub %a, %b       ; Subtraction
mul %a, %b       ; Multiplication
div %a, %b       ; Division
rem %a, %b       ; Remainder/modulo
neg %a           ; Negation
```

##### Comparison
```
eq %a, %b        ; Equal
ne %a, %b        ; Not equal
lt %a, %b        ; Less than
le %a, %b        ; Less or equal
gt %a, %b        ; Greater than
ge %a, %b        ; Greater or equal
```

##### Logical
```
and %a, %b       ; Logical AND
or %a, %b        ; Logical OR
not %a           ; Logical NOT
```

##### Bitwise
```
bit_and %a, %b   ; Bitwise AND
bit_or %a, %b    ; Bitwise OR
bit_xor %a, %b   ; Bitwise XOR
bit_not %a       ; Bitwise NOT
shl %a, %b       ; Shift left
shr %a, %b       ; Shift right (arithmetic)
```

##### Memory
```
alloca <type>                 ; Stack allocation
load %ptr                     ; Load from pointer
store %val, %ptr              ; Store to pointer
gep %ptr, %idx                ; Get element pointer
field_ptr %struct_ptr, <field> ; Get field pointer
```

##### Control Flow
```
phi [%a, bb1], [%b, bb2], ... ; SSA phi node
call @func(%arg1, %arg2, ...) ; Function call
```

##### Type Conversions
```
cast %val to <type>           ; Type cast
trunc %val to <type>          ; Truncate integer
sext %val to <type>           ; Sign extend integer
zext %val to <type>           ; Zero extend integer
fptrunc %val to <type>        ; Truncate float
fpext %val to <type>          ; Extend float
```

### 3. Terminators

Each basic block MUST end with exactly one terminator:

```
Terminator ::=
    | ret <value>                     ; Return with value
    | ret void                        ; Return void
    | br <bb_target>                  ; Unconditional branch
    | br_cond %cond, <bb_true>, <bb_false>  ; Conditional branch
    | switch %val, [<case>: <bb>]*, <bb_default>  ; Switch
    | unreachable                     ; Unreachable code marker
```

### 4. Basic Blocks

A basic block is a sequence of instructions ending with a terminator:

```
BasicBlock ::= Label ':' Instruction* Terminator

Label ::= IDENTIFIER
```

Example:
```mir
bb_entry:
    %0 = const.i32 1
    %1 = const.i32 2
    %2 = add %0, %1
    br bb_exit

bb_exit:
    ret %2
```

### 5. Functions

```mir
func @<name>(<params>) -> <return_type> {
    <basic_blocks>+
}
```

Parameters are named and typed:
```
param ::= '%' IDENTIFIER ':' MirType
```

Example:
```mir
func @factorial(%n: I32) -> I32 {
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

### 6. Modules

A MIR module contains function definitions and declarations:

```mir
; Module: <name>
; Version: <version>

<struct_definitions>*
<enum_definitions>*
<function_declarations>*
<function_definitions>*
```

### 7. SSA Properties

MIR maintains Single Static Assignment (SSA) form:

1. **Single Definition**: Each ValueId is defined exactly once
2. **Phi Nodes**: Values from different control flow paths merge via phi instructions
3. **Dominance**: A definition dominates all its uses

### 8. Optimization Passes

#### 8.1 Pass Interface

```cpp
class MirPass {
    virtual std::string name() const = 0;
    virtual bool run(Module& module) = 0;
};
```

#### 8.2 Required Passes

| Pass | Level | Description |
|------|-------|-------------|
| ConstantFolding | O1+ | Evaluate constant expressions at compile time |
| DeadCodeElimination | O1+ | Remove unused instructions |
| ConstantPropagation | O2+ | Replace uses of constants with their values |
| CopyPropagation | O2+ | Replace copies with original values |
| CommonSubexpressionElimination | O2+ | Reuse computed values |
| UnreachableCodeElimination | O1+ | Remove unreachable blocks |

#### 8.3 Optimization Levels

| Level | Passes Applied |
|-------|----------------|
| O0 | None (type checking only) |
| O1 | ConstantFolding, DCE, UnreachableCodeElimination |
| O2 | O1 + ConstantPropagation, CopyPropagation, CSE |
| O3 | O2 + Inlining (future) |

### 9. Serialization

MIR MUST support both text and binary serialization.

#### 9.1 Text Format

Human-readable format for debugging:
```mir
; Module: example
; Generated: 2025-12-29

func @add(%a: I32, %b: I32) -> I32 {
bb_entry:
    %result = add %a, %b
    ret %result
}
```

#### 9.2 Binary Format

Compact binary format for caching:
- Header: magic number, version, checksum
- Type table: all types used in module
- Function table: function signatures
- Instruction stream: encoded instructions

## Examples

### Simple Function

Source:
```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

MIR:
```mir
func @add(%a: I32, %b: I32) -> I32 {
bb_entry:
    %0 = add %a, %b
    ret %0
}
```

### Conditional

Source:
```tml
func abs(x: I32) -> I32 {
    if x < 0 {
        return -x
    }
    return x
}
```

MIR:
```mir
func @abs(%x: I32) -> I32 {
bb_entry:
    %0 = const.i32 0
    %cond = lt %x, %0
    br_cond %cond, bb_neg, bb_pos

bb_neg:
    %neg_x = neg %x
    ret %neg_x

bb_pos:
    ret %x
}
```

### Loop

Source:
```tml
func sum_range(start: I32, end: I32) -> I32 {
    let mut sum: I32 = 0
    let mut i: I32 = start
    loop {
        if i > end { break }
        sum = sum + i
        i = i + 1
    }
    return sum
}
```

MIR:
```mir
func @sum_range(%start: I32, %end: I32) -> I32 {
bb_entry:
    br bb_loop

bb_loop:
    %sum = phi [const.i32 0, bb_entry], [%sum_new, bb_body]
    %i = phi [%start, bb_entry], [%i_new, bb_body]
    %cond = gt %i, %end
    br_cond %cond, bb_exit, bb_body

bb_body:
    %sum_new = add %sum, %i
    %i_new = add %i, const.i32 1
    br bb_loop

bb_exit:
    ret %sum
}
```

## Compatibility

### With RFC-0007 (High-level IR)

MIR is distinct from the high-level IR in RFC-0007:
- **RFC-0007 IR**: Semantic representation for diffing/merging, preserves source structure
- **MIR**: Optimization-focused representation in SSA form

### With RFC-0001 (Core Language)

MIR preserves the semantics defined in RFC-0001 while providing an optimizable form.

## Alternatives Rejected

### Direct LLVM IR Generation

- Pro: Simpler pipeline
- Con: No TML-specific optimizations, harder debugging
- **Rejected**: Benefits of intermediate representation outweigh complexity

### Using LLVM's Own IR Throughout

- Pro: Leverage LLVM's tools and passes
- Con: Lose TML-specific semantic information
- **Rejected**: Need TML-level optimizations

### SSA at AST Level

- Pro: Earlier SSA conversion
- Con: AST structure doesn't suit SSA well
- **Rejected**: Separate IR is cleaner

## References

1. LLVM Language Reference Manual
2. Rust MIR documentation
3. "SSA is Functional Programming" - Appel, 1998
4. "Modern Compiler Implementation" - Appel

## Implementation

Implementation files:
- `include/mir/mir.hpp` - Core data structures
- `include/mir/mir_builder.hpp` - Builder API
- `include/mir/mir_pass.hpp` - Pass infrastructure
- `include/mir/mir_serialize.hpp` - Serialization
- `src/mir/mir_*.cpp` - Implementation
- `src/mir/passes/*.cpp` - Optimization passes

See [30-MIR.md](../specs/30-MIR.md) for detailed specification.
