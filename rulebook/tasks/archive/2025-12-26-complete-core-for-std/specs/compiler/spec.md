# Compiler Specification: Core Features for std

This specification defines the core compiler features required for implementing the standard library in TML.

## ADDED Requirements

### Requirement: Lowlevel Block Syntax
The compiler SHALL parse and support `lowlevel { ... }` blocks for unsafe operations.

#### Scenario: Parse lowlevel block
Given a TML source file with `lowlevel { let ptr = alloc(size) }`
When the parser processes the file
Then it produces a LowlevelBlock AST node containing the statements

#### Scenario: Type check lowlevel block
Given a lowlevel block with pointer operations
When the type checker processes the block
Then borrow checking is skipped but type checking is enforced

#### Scenario: Generate code for lowlevel block
Given a valid lowlevel block
When the codegen phase runs
Then it produces LLVM IR for the block contents without safety checks

---

### Requirement: Pointer Type and Methods
The compiler SHALL support `Ptr[T]` type with read, write, and offset methods.

#### Scenario: Declare pointer type
Given a variable declaration `let p: Ptr[I32]`
When the type checker resolves the type
Then it recognizes Ptr as a valid generic pointer type

#### Scenario: Call read method
Given a pointer `p: Ptr[I32]`
When calling `p.read()`
Then the return type is I32 and codegen produces LLVM load instruction

#### Scenario: Call write method
Given a pointer `p: Ptr[I32]` and value `v: I32`
When calling `p.write(v)`
Then codegen produces LLVM store instruction

#### Scenario: Call offset method
Given a pointer `p: Ptr[I32]` and offset `n: I64`
When calling `p.offset(n)`
Then the return type is Ptr[I32] and codegen produces LLVM getelementptr

---

### Requirement: Closure Capture Analysis
The compiler SHALL analyze and capture variables from enclosing scopes in closures.

#### Scenario: Capture immutable variable
Given a closure `do(x) { outer + x }` where outer is in enclosing scope
When the type checker analyzes the closure
Then it identifies outer as a captured variable

#### Scenario: Generate closure environment
Given a closure with captured variables
When the codegen phase runs
Then it generates a struct containing captured values

#### Scenario: Access captured variable in closure
Given a closure call with captured variables
When executing the closure
Then captured values are accessible from the environment struct

---

### Requirement: Associated Types in Behaviors
The compiler SHALL support `type Name` declarations in behavior definitions.

#### Scenario: Declare associated type
Given a behavior `behavior Iterator { type Item func next(mut ref this) -> Maybe[This::Item] }`
When the parser processes the behavior
Then it stores the associated type declaration

#### Scenario: Implement associated type
Given an impl block `impl Iterator for Range { type Item = I32 }`
When the type checker processes the impl
Then it binds the associated type to the concrete type

#### Scenario: Use associated type in generic context
Given a function `func sum[T: Iterator](iter: T) -> T::Item`
When resolving the return type
Then it substitutes the associated type from the concrete impl

---

### Requirement: Default Implementations in Behaviors
The compiler SHALL support method bodies in behavior definitions as defaults.

#### Scenario: Define default implementation
Given a behavior `behavior Eq { func eq(ref this, ref other) -> Bool func ne(ref this, ref other) -> Bool { not this.eq(other) } }`
When the parser processes the behavior
Then it stores the default implementation for ne

#### Scenario: Use default without override
Given an impl block that does not define ne
When type checking the impl
Then it uses the default implementation from the behavior

#### Scenario: Override default implementation
Given an impl block that defines ne with custom body
When type checking the impl
Then it uses the custom implementation instead of default

---

### Requirement: Iterator Behavior Protocol
The compiler SHALL define and support the Iterator behavior for for-in loops.

#### Scenario: Implement Iterator for custom type
Given a type with `impl Iterator for MyRange`
When using the type in a for-in loop
Then the loop calls next() until Nothing is returned

#### Scenario: Chain iterator combinators
Given an iterator with default map implementation
When calling `iter.map(do(x) { x * 2 })`
Then it returns a new iterator that applies the transformation

## MODIFIED Requirements

None.

## REMOVED Requirements

None.
