# IR Generator Specification

## Purpose

The IR Generator transforms the Typed AST (TAST) into TML IR, a lower-level intermediate representation in SSA form. The IR is designed for optimization and translation to LLVM IR for final code generation.

## ADDED Requirements

### Requirement: SSA Form Generation
The IR generator SHALL produce IR in Static Single Assignment form.

#### Scenario: Simple assignment
Given `let x = 1; let y = x + 2`
When the IR generator processes these statements
Then it produces `%0 = const i32 1; %1 = add i32 %0, 2`

#### Scenario: Mutable variable
Given `var x = 1; x = x + 1`
When the IR generator processes this code
Then it produces alloca for x and load/store operations

#### Scenario: Phi node for branches
Given `let x = if cond { 1 } else { 2 }`
When the IR generator processes this code
Then it produces a phi instruction joining both branch values

### Requirement: Memory Operation Generation
The IR generator MUST generate explicit memory operations.

#### Scenario: Stack allocation
Given `let x = SomeStruct { a: 1, b: 2 }`
When the IR generator processes this code
Then it produces an alloca instruction followed by field stores

#### Scenario: Load from reference
Given `let y = *ref` where ref is `&I32`
When the IR generator processes the dereference
Then it produces a load instruction

#### Scenario: Store through mutable reference
Given `*ref = value` where ref is `&mut I32`
When the IR generator processes the assignment
Then it produces a store instruction

### Requirement: Basic Block Structure
The IR generator SHALL organize code into basic blocks.

#### Scenario: Sequential code
Given straight-line code without branches
When the IR generator processes it
Then it produces a single basic block

#### Scenario: If-else branching
Given `if cond { a } else { b }`
When the IR generator processes this code
Then it produces: entry block with condbr, then block, else block, merge block

#### Scenario: Loop structure
Given `loop { body; if done { break } }`
When the IR generator processes this code
Then it produces: header block, body block, exit block with proper back edges

### Requirement: Expression Lowering
The IR generator SHALL correctly lower all expression types.

#### Scenario: Binary arithmetic
Given `a + b * c`
When the IR generator processes this expression
Then it produces mul followed by add respecting precedence

#### Scenario: Method call
Given `obj.method(arg)`
When the IR generator processes this call
Then it produces a call with obj as first argument (self)

#### Scenario: Field access
Given `point.x` where point is a struct
When the IR generator processes the access
Then it produces a GEP instruction to get field offset

#### Scenario: Array indexing
Given `arr[i]`
When the IR generator processes this expression
Then it produces bounds check followed by GEP and load

### Requirement: Control Flow Lowering
The IR generator MUST correctly lower control flow constructs.

#### Scenario: Early return
Given `if cond { return x }; y`
When the IR generator processes this code
Then return block terminates with ret, continuation has y

#### Scenario: Break in loop
Given `loop { if done { break } }`
When the IR generator processes this code
Then break produces an unconditional branch to loop exit

#### Scenario: Continue in loop
Given `loop { if skip { continue }; process() }`
When the IR generator processes this code
Then continue produces an unconditional branch to loop header

### Requirement: Pattern Matching Lowering
The IR generator SHALL lower pattern matching to decision trees.

#### Scenario: Enum pattern
Given `when opt { Some(x) -> use(x), None -> default }`
When the IR generator processes this when expression
Then it produces discriminant check followed by branching

#### Scenario: Literal pattern
Given `when n { 0 -> "zero", 1 -> "one", _ -> "other" }`
When the IR generator processes this when expression
Then it produces comparison chain or jump table

#### Scenario: Tuple destructuring
Given `let (a, b) = tuple`
When the IR generator processes this pattern
Then it produces GEP instructions to extract each element

### Requirement: Function Lowering
The IR generator MUST correctly lower function definitions.

#### Scenario: Simple function
Given `func add(a: I32, b: I32) -> I32 { return a + b }`
When the IR generator processes this function
Then it produces IR function with two i32 params and return

#### Scenario: Generic function instantiation
Given a call to generic function with concrete types
When the IR generator processes the call
Then it uses the monomorphized version of the function

#### Scenario: Closure
Given `let f = |x| x + captured`
When the IR generator processes this closure
Then it produces a struct for captures and a function taking that struct

### Requirement: Type Lowering
The IR generator SHALL correctly lower types to IR types.

#### Scenario: Primitive types
Given TML types I32, F64, Bool
When lowering to IR
Then they become i32, f64, i1 respectively

#### Scenario: Struct type
Given `type Point { x: F64, y: F64 }`
When lowering to IR
Then it becomes a struct type with layout { f64, f64 }

#### Scenario: Enum type
Given `type Option[T] { Some(T), None }`
When lowering to IR
Then it becomes tagged union: { i8 tag, [max_variant_size x i8] }

#### Scenario: Reference types
Given `&T` or `&mut T`
When lowering to IR
Then both become ptr type (mutability tracked elsewhere)

### Requirement: IR Serialization
The IR generator SHOULD support IR serialization for debugging.

#### Scenario: Text format
Given generated IR
When serialized to text
Then it produces human-readable format showing blocks and instructions

#### Scenario: Round-trip
Given IR serialized to text
When parsed back
Then it produces equivalent IR structure

### Requirement: Error Handling
The IR generator MUST handle errors gracefully.

#### Scenario: Unsupported construct
Given a TAST node type not yet implemented
When the IR generator encounters it
Then it reports a clear error with location

#### Scenario: Type mismatch
Given inconsistent types in IR construction
When detected during generation
Then it reports internal compiler error with context
