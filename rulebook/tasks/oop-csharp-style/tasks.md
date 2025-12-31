# Tasks: C#-Style Object-Oriented Programming

## Phase 1: Lexer - New Keywords

### 1.1 Add Keywords
- [ ] 1.1.1 Add `class` keyword
- [ ] 1.1.2 Add `interface` keyword
- [ ] 1.1.3 Add `extends` keyword
- [ ] 1.1.4 Add `implements` keyword
- [ ] 1.1.5 Add `override` keyword
- [ ] 1.1.6 Add `virtual` keyword
- [ ] 1.1.7 Add `abstract` keyword
- [ ] 1.1.8 Add `sealed` keyword
- [ ] 1.1.9 Add `namespace` keyword
- [ ] 1.1.10 Add `base` keyword
- [ ] 1.1.11 Add `protected` keyword
- [ ] 1.1.12 Add `private` keyword (if not exists)
- [ ] 1.1.13 Add `static` keyword
- [ ] 1.1.14 Add `new` keyword (constructor context)
- [ ] 1.1.15 Add `prop` keyword (properties)
- [ ] 1.1.16 Update keyword count in documentation

## Phase 2: Parser - Grammar Extensions

### 2.1 Namespace Declaration
- [ ] 2.1.1 Parse `namespace Foo.Bar.Baz { ... }`
- [ ] 2.1.2 Create NamespaceDecl AST node
- [ ] 2.1.3 Parse nested namespace declarations
- [ ] 2.1.4 Parse `use Foo.Bar.*` imports

### 2.2 Interface Declaration
- [ ] 2.2.1 Parse `interface Name { ... }`
- [ ] 2.2.2 Parse `interface Name[T, U] { ... }` (generic)
- [ ] 2.2.3 Parse `interface Name extends Other { ... }`
- [ ] 2.2.4 Parse interface method signatures (no body)
- [ ] 2.2.5 Parse static interface methods
- [ ] 2.2.6 Create InterfaceDecl AST node

### 2.3 Class Declaration
- [ ] 2.3.1 Parse `class Name { ... }`
- [ ] 2.3.2 Parse `class Name[T] { ... }` (generic)
- [ ] 2.3.3 Parse `class Name extends Base { ... }`
- [ ] 2.3.4 Parse `class Name implements I1, I2 { ... }`
- [ ] 2.3.5 Parse `class Name extends Base implements I1 { ... }`
- [ ] 2.3.6 Parse `abstract class Name { ... }`
- [ ] 2.3.7 Parse `sealed class Name { ... }`
- [ ] 2.3.8 Create ClassDecl AST node

### 2.4 Class Members
- [ ] 2.4.1 Parse field declarations with visibility
- [ ] 2.4.2 Parse `private field: Type`
- [ ] 2.4.3 Parse `protected field: Type`
- [ ] 2.4.4 Parse `static field: Type`
- [ ] 2.4.5 Parse `static field: Type = value` (static initializer)

### 2.5 Class Methods
- [ ] 2.5.1 Parse `func name(this, ...) -> T { ... }`
- [ ] 2.5.2 Parse `virtual func name(...) { ... }`
- [ ] 2.5.3 Parse `override func name(...) { ... }`
- [ ] 2.5.4 Parse `abstract func name(...) -> T`
- [ ] 2.5.5 Parse `static func name(...) -> T { ... }`
- [ ] 2.5.6 Parse `private func name(...) { ... }`
- [ ] 2.5.7 Parse `protected func name(...) { ... }`

### 2.6 Constructor
- [ ] 2.6.1 Parse `func new(...) -> ClassName { ... }`
- [ ] 2.6.2 Parse base constructor call `base: Parent::new(...)`
- [ ] 2.6.3 Handle constructor overloading

### 2.7 Properties (Optional)
- [ ] 2.7.1 Parse `prop name: Type { get; set }`
- [ ] 2.7.2 Parse `prop name: Type { get => expr }`
- [ ] 2.7.3 Parse `prop name: Type { get => expr, set(v) => { ... } }`
- [ ] 2.7.4 Create PropertyDecl AST node

### 2.8 Base Calls
- [ ] 2.8.1 Parse `base.method()` expression
- [ ] 2.8.2 Parse `base.field` expression
- [ ] 2.8.3 Create BaseExpr AST node

## Phase 3: Type System - Class Hierarchy

### 3.1 Interface Type
- [ ] 3.1.1 Create InterfaceType in types system
- [ ] 3.1.2 Track interface method signatures
- [ ] 3.1.3 Track interface inheritance chain
- [ ] 3.1.4 Support generic interfaces

### 3.2 Class Type
- [ ] 3.2.1 Create ClassType in types system
- [ ] 3.2.2 Track parent class (single inheritance)
- [ ] 3.2.3 Track implemented interfaces
- [ ] 3.2.4 Track virtual method table layout
- [ ] 3.2.5 Support generic classes

### 3.3 Inheritance Validation
- [ ] 3.3.1 Verify single inheritance (no multiple class inheritance)
- [ ] 3.3.2 Verify no circular inheritance
- [ ] 3.3.3 Verify sealed class not extended
- [ ] 3.3.4 Verify abstract class not instantiated directly

### 3.4 Override Validation
- [ ] 3.4.1 Verify override method exists in parent
- [ ] 3.4.2 Verify override signature matches exactly
- [ ] 3.4.3 Verify non-virtual methods not overridden
- [ ] 3.4.4 Verify abstract methods are implemented

### 3.5 Interface Implementation
- [ ] 3.5.1 Verify all interface methods implemented
- [ ] 3.5.2 Verify implementation signatures match
- [ ] 3.5.3 Handle default interface methods (if supported)

### 3.6 Visibility Checking
- [ ] 3.6.1 Enforce private visibility within class
- [ ] 3.6.2 Enforce protected visibility in subclasses
- [ ] 3.6.3 Enforce public visibility everywhere
- [ ] 3.6.4 Handle visibility in inheritance

### 3.7 Namespace Resolution
- [ ] 3.7.1 Build namespace symbol tables
- [ ] 3.7.2 Resolve qualified names `Foo.Bar.Type`
- [ ] 3.7.3 Handle `use` imports
- [ ] 3.7.4 Handle wildcard imports `use Foo.*`
- [ ] 3.7.5 Handle name conflicts/ambiguity

## Phase 4: Codegen - Class Implementation

### 4.1 Class Memory Layout
- [ ] 4.1.1 Generate struct type with vtable pointer as first field
- [ ] 4.1.2 Layout parent class fields first
- [ ] 4.1.3 Layout own fields after parent
- [ ] 4.1.4 Handle field alignment

### 4.2 VTable Generation
- [ ] 4.2.1 Create vtable struct type for each class
- [ ] 4.2.2 Include parent virtual methods first
- [ ] 4.2.3 Add new virtual methods after
- [ ] 4.2.4 Override entries for overridden methods
- [ ] 4.2.5 Generate global vtable constant

### 4.3 Constructor Codegen
- [ ] 4.3.1 Allocate object memory
- [ ] 4.3.2 Initialize vtable pointer
- [ ] 4.3.3 Call parent constructor if extends
- [ ] 4.3.4 Initialize own fields
- [ ] 4.3.5 Return constructed object

### 4.4 Virtual Method Calls
- [ ] 4.4.1 Load vtable pointer from object
- [ ] 4.4.2 Load method pointer from vtable
- [ ] 4.4.3 Call method with object as first arg
- [ ] 4.4.4 Optimize final/sealed methods to direct call

### 4.5 Base Calls
- [ ] 4.5.1 Generate direct call to parent method
- [ ] 4.5.2 Pass current object as receiver
- [ ] 4.5.3 Handle multi-level base calls

### 4.6 Static Members
- [ ] 4.6.1 Generate global variables for static fields
- [ ] 4.6.2 Generate static initializers
- [ ] 4.6.3 Generate static methods as regular functions
- [ ] 4.6.4 Handle static generic methods

### 4.7 Interface Dispatch
- [ ] 4.7.1 Generate interface vtable (subset of class vtable)
- [ ] 4.7.2 Generate interface-to-vtable offset
- [ ] 4.7.3 Cast object to interface type
- [ ] 4.7.4 Dispatch through interface vtable

### 4.8 Type Checks
- [ ] 4.8.1 Implement `is` operator for type checking
- [ ] 4.8.2 Implement `as` operator for safe casting
- [ ] 4.8.3 Generate runtime type info (RTTI) if needed

## Phase 5: Standard Library Updates

### 5.1 Base Classes
- [ ] 5.1.1 Create `Object` base class (optional universal base)
- [ ] 5.1.2 Add `equals(other: ref Object) -> Bool`
- [ ] 5.1.3 Add `hash_code() -> I64`
- [ ] 5.1.4 Add `to_string() -> Str`

### 5.2 Common Interfaces
- [ ] 5.2.1 Create `IEquatable[T]` interface
- [ ] 5.2.2 Create `IComparable[T]` interface
- [ ] 5.2.3 Create `IEnumerable[T]` interface (iterator)
- [ ] 5.2.4 Create `IDisposable` interface (cleanup)
- [ ] 5.2.5 Create `ICloneable` interface

### 5.3 Collection Classes
- [ ] 5.3.1 Update `List[T]` as class
- [ ] 5.3.2 Update `HashMap[K,V]` as class
- [ ] 5.3.3 Create `HashSet[T]` class
- [ ] 5.3.4 Create `Queue[T]` class
- [ ] 5.3.5 Create `Stack[T]` class

### 5.4 Exception Classes
- [ ] 5.4.1 Create `Exception` base class
- [ ] 5.4.2 Create `ArgumentException`
- [ ] 5.4.3 Create `InvalidOperationException`
- [ ] 5.4.4 Create `NullReferenceException`
- [ ] 5.4.5 Create `IndexOutOfRangeException`

## Phase 6: IDE/Tooling Support

### 6.1 Syntax Highlighting
- [ ] 6.1.1 Update VS Code extension with new keywords
- [ ] 6.1.2 Add class/interface highlighting
- [ ] 6.1.3 Add namespace highlighting

### 6.2 Code Navigation
- [ ] 6.2.1 Go to base class definition
- [ ] 6.2.2 Find all implementations of interface
- [ ] 6.2.3 Find all subclasses
- [ ] 6.2.4 Show class hierarchy

### 6.3 Code Completion
- [ ] 6.3.1 Complete override methods
- [ ] 6.3.2 Complete interface implementations
- [ ] 6.3.3 Complete base. members
- [ ] 6.3.4 Complete namespace members

## Phase 7: Testing

### 7.1 Parser Tests
- [ ] 7.1.1 Test namespace parsing
- [ ] 7.1.2 Test interface parsing
- [ ] 7.1.3 Test class parsing with all modifiers
- [ ] 7.1.4 Test inheritance parsing
- [ ] 7.1.5 Test property parsing

### 7.2 Type Checker Tests
- [ ] 7.2.1 Test inheritance validation
- [ ] 7.2.2 Test interface implementation
- [ ] 7.2.3 Test visibility enforcement
- [ ] 7.2.4 Test override validation
- [ ] 7.2.5 Test abstract class validation

### 7.3 Codegen Tests
- [ ] 7.3.1 Test class instantiation
- [ ] 7.3.2 Test virtual method dispatch
- [ ] 7.3.3 Test interface dispatch
- [ ] 7.3.4 Test base calls
- [ ] 7.3.5 Test static members

### 7.4 Integration Tests
- [ ] 7.4.1 Implement classic design patterns
  - [ ] Factory pattern
  - [ ] Strategy pattern
  - [ ] Observer pattern
  - [ ] Decorator pattern
  - [ ] Template method pattern
- [ ] 7.4.2 Port C# samples to TML
- [ ] 7.4.3 Benchmark virtual dispatch overhead

## Phase 8: Documentation

### 8.1 Language Reference
- [ ] 8.1.1 Document class syntax
- [ ] 8.1.2 Document interface syntax
- [ ] 8.1.3 Document namespace syntax
- [ ] 8.1.4 Document inheritance rules
- [ ] 8.1.5 Document visibility modifiers

### 8.2 Migration Guide
- [ ] 8.2.1 C# to TML class migration
- [ ] 8.2.2 Java to TML class migration
- [ ] 8.2.3 Behavior vs Interface comparison

### 8.3 Examples
- [ ] 8.3.1 Simple class hierarchy example
- [ ] 8.3.2 Interface implementation example
- [ ] 8.3.3 Abstract class example
- [ ] 8.3.4 Namespace organization example
- [ ] 8.3.5 Mixed behavior/class example

## Validation

- [ ] V.1 All new keywords recognized by lexer
- [ ] V.2 All syntax parsed without errors
- [ ] V.3 Type checker validates inheritance correctly
- [ ] V.4 Virtual dispatch works at runtime
- [ ] V.5 Interface dispatch works at runtime
- [ ] V.6 Static members work correctly
- [ ] V.7 Visibility enforced at compile time
- [ ] V.8 All existing behavior/impl code still works
- [ ] V.9 No performance regression for non-OOP code
- [ ] V.10 Design patterns can be implemented idiomatically
