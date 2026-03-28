# Compiler Codegen Spec Delta

## MODIFIED Requirements

### Requirement: Generic Trait Dispatch Return Types
The compiler SHALL correctly resolve return types for constrained generic behavior impl methods. When `impl[T: Hash] Hash for Array[T, N]` defines `func hash(this) -> I64`, calling `array.hash()` on `Array[I32, 3]` MUST return type `I64`, not `()`.

#### Scenario: Constrained impl method return type
Given a type `Array[I32, 3]` with `impl[T: Hash] Hash for Array[T, N]`
When the method `hash()` is called on an instance
Then the return type SHALL be `I64` and the value SHALL be a valid hash

#### Scenario: Cross-module constrained impl
Given a constrained impl defined in a library module
When the method is called from user code importing the type
Then the return type MUST be correctly substituted with concrete types

### Requirement: LLVM Intrinsic Declarations
The compiler MUST emit declarations for all runtime intrinsics used by the standard library, including `tml_ptr_read_unaligned`, `tml_ptr_write_unaligned`, `tml_ptr_read_volatile`, `tml_ptr_write_volatile`, `tml_memcpy`, `tml_memmove`, and `tml_memset`.

#### Scenario: Volatile pointer operations
Given a `RawMutPtr[I32]` pointing to valid memory
When `read_volatile()` or `write_volatile(value)` is called
Then the compiler SHALL emit valid LLVM IR calling the corresponding runtime intrinsic

### Requirement: Unit Type in Struct Fields
The compiler MUST represent Unit type in struct fields as a zero-sized type (e.g., `i8` or `{}`) instead of `void`. Using `void` in aggregate types produces invalid LLVM IR.

#### Scenario: Mutex containing Unit
Given a `Mutex[Unit]` type
When the struct is constructed with default initialization
Then the LLVM IR SHALL use a valid zero-sized representation, not `void zeroinitializer`

### Requirement: Const Generic Cross-Module Substitution
The compiler MUST substitute const generic parameters when importing struct types from other modules. `ArrayIter[I32, 3]` imported from a library MUST have field layout `[3 x i32]`, not `[0 x i32]`.

#### Scenario: Imported const generic struct
Given an `ArrayIter[I32, 3]` type defined in `core::array::iter`
When used in user code that imports the module
Then the `data` field MUST have LLVM type `[3 x i32]`
