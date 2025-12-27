# LLVM Backend Specification

## Purpose

The LLVM Backend translates TML IR to LLVM IR, then uses LLVM's optimization and code generation infrastructure to produce optimized native binaries for multiple target architectures.

## ADDED Requirements

### Requirement: Type Conversion
The LLVM backend SHALL correctly convert TML IR types to LLVM types.

#### Scenario: Integer types
Given TML IR types i8, i16, i32, i64
When converting to LLVM types
Then they map to LLVM IntegerType of corresponding bit widths

#### Scenario: Float types
Given TML IR types f32, f64
When converting to LLVM types
Then they map to LLVM FloatTy and DoubleTy respectively

#### Scenario: Struct types
Given TML IR struct type with fields
When converting to LLVM types
Then it produces LLVM StructType with correct layout and alignment

#### Scenario: Pointer types
Given TML IR ptr type
When converting to LLVM types
Then it produces LLVM opaque pointer type (ptr in LLVM 17+)

#### Scenario: Function types
Given TML IR function type with params and return
When converting to LLVM types
Then it produces LLVM FunctionType with converted param and return types

### Requirement: Instruction Translation
The LLVM backend MUST translate all TML IR instructions to LLVM IR.

#### Scenario: Arithmetic instructions
Given TML IR instructions add, sub, mul, div
When translating to LLVM IR
Then they produce corresponding LLVM BinaryOperator instructions

#### Scenario: Signed vs unsigned
Given TML IR div instruction with signed types
When translating to LLVM IR
Then it produces sdiv; unsigned produces udiv

#### Scenario: Memory instructions
Given TML IR alloca, load, store instructions
When translating to LLVM IR
Then they produce corresponding LLVM memory instructions with correct alignment

#### Scenario: Control flow instructions
Given TML IR br, condbr, ret instructions
When translating to LLVM IR
Then they produce LLVM BranchInst, CondBranchInst, ReturnInst

#### Scenario: Call instructions
Given TML IR call instruction
When translating to LLVM IR
Then it produces LLVM CallInst with correct calling convention

#### Scenario: GEP instructions
Given TML IR gep instruction for field/element access
When translating to LLVM IR
Then it produces LLVM GetElementPtrInst with correct indices

#### Scenario: Phi instructions
Given TML IR phi instruction
When translating to LLVM IR
Then it produces LLVM PHINode with incoming values and blocks

### Requirement: Function Generation
The LLVM backend SHALL generate correct LLVM functions.

#### Scenario: Function signature
Given TML IR function with parameters and return type
When generating LLVM function
Then it creates LLVM Function with correct FunctionType

#### Scenario: Basic blocks
Given TML IR function with multiple basic blocks
When generating LLVM function
Then each TML IR block becomes an LLVM BasicBlock

#### Scenario: Function attributes
Given TML IR function with attributes (noinline, alwaysinline, etc.)
When generating LLVM function
Then appropriate LLVM Attributes are set

#### Scenario: Calling convention
Given function call on target platform
When generating LLVM call
Then correct calling convention is used (C, Fast, etc.)

### Requirement: Optimization Pipeline
The LLVM backend MUST support multiple optimization levels.

#### Scenario: O0 no optimization
Given optimization level O0
When running optimization pipeline
Then no optimization passes are run

#### Scenario: O1 basic optimization
Given optimization level O1
When running optimization pipeline
Then basic optimization passes run (mem2reg, simplifycfg, etc.)

#### Scenario: O2 standard optimization
Given optimization level O2
When running optimization pipeline
Then standard optimization passes run including inlining

#### Scenario: O3 aggressive optimization
Given optimization level O3
When running optimization pipeline
Then aggressive optimizations run including loop unrolling

#### Scenario: Os size optimization
Given optimization level Os
When running optimization pipeline
Then size-focused optimizations run

### Requirement: Code Emission
The LLVM backend SHALL emit correct object files.

#### Scenario: Object file generation
Given optimized LLVM module
When emitting object code
Then it produces valid platform object file (.o or .obj)

#### Scenario: Target triple
Given target specification (e.g., x86_64-unknown-linux-gnu)
When configuring code generation
Then LLVM uses correct target triple and features

#### Scenario: Relocation model
Given position-independent code requirement
When emitting object code
Then correct relocation model (PIC/PIE) is used

### Requirement: Debug Information
The LLVM backend SHOULD generate debug information.

#### Scenario: Source locations
Given source location info in TML IR
When generating LLVM IR with debug info
Then DILocation nodes are attached to instructions

#### Scenario: Variable info
Given local variable declarations
When generating debug info
Then DILocalVariable entries are created

#### Scenario: Type info
Given type definitions
When generating debug info
Then DIType entries describe the types

#### Scenario: DWARF format
Given debug info generation enabled
When emitting object file
Then DWARF debug sections are included

### Requirement: Linking Support
The LLVM backend SHALL support linking object files.

#### Scenario: Executable linking
Given compiled object files
When linking is requested
Then system linker is invoked to produce executable

#### Scenario: External symbols
Given calls to external functions (libc, etc.)
When linking
Then external symbols are resolved

#### Scenario: Static library
Given multiple object files
When static library is requested
Then ar/lib is invoked to create archive

### Requirement: Platform Support
The LLVM backend MUST support target platforms.

#### Scenario: x86-64 Linux
Given target x86_64-unknown-linux-gnu
When generating code
Then correct ABI and calling conventions are used

#### Scenario: x86-64 Windows
Given target x86_64-pc-windows-msvc
When generating code
Then Windows x64 ABI is used

#### Scenario: ARM64 macOS
Given target aarch64-apple-darwin
When generating code
Then ARM64 macOS ABI is used

#### Scenario: CPU features
Given CPU feature flags (avx2, neon, etc.)
When generating code
Then LLVM uses specified CPU features

### Requirement: Error Handling
The LLVM backend MUST handle errors gracefully.

#### Scenario: LLVM verification failure
Given invalid LLVM IR generated
When LLVM verifier runs
Then clear error is reported with context

#### Scenario: Code generation failure
Given target that cannot compile certain constructs
When code generation fails
Then error is reported with reason

#### Scenario: Linking failure
Given unresolved symbols during linking
When linker fails
Then linker errors are propagated to user
