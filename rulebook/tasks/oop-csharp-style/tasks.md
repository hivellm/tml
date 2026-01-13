# Tasks: C#-Style Object-Oriented Programming

**Status**: In Progress (~50% complete)

**Priority**: High - Core language feature

## Phase 1: Lexer - New Keywords

### 1.1 Add Keywords
- [x] 1.1.1 Add `class` keyword
- [x] 1.1.2 Add `interface` keyword
- [x] 1.1.3 Add `extends` keyword
- [x] 1.1.4 Add `implements` keyword
- [x] 1.1.5 Add `override` keyword
- [x] 1.1.6 Add `virtual` keyword
- [x] 1.1.7 Add `abstract` keyword
- [x] 1.1.8 Add `sealed` keyword
- [ ] 1.1.9 Add `namespace` keyword
- [x] 1.1.10 Add `base` keyword
- [x] 1.1.11 Add `protected` keyword
- [x] 1.1.12 Add `private` keyword (if not exists)
- [x] 1.1.13 Add `static` keyword
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
- [x] 2.2.1 Parse `interface Name { ... }`
- [x] 2.2.2 Parse `interface Name[T, U] { ... }` (generic)
- [x] 2.2.3 Parse `interface Name extends Other { ... }`
- [x] 2.2.4 Parse interface method signatures (no body)
- [ ] 2.2.5 Parse static interface methods
- [x] 2.2.6 Create InterfaceDecl AST node

### 2.3 Class Declaration
- [x] 2.3.1 Parse `class Name { ... }`
- [x] 2.3.2 Parse `class Name[T] { ... }` (generic)
- [x] 2.3.3 Parse `class Name extends Base { ... }`
- [x] 2.3.4 Parse `class Name implements I1, I2 { ... }`
- [x] 2.3.5 Parse `class Name extends Base implements I1 { ... }`
- [x] 2.3.6 Parse `abstract class Name { ... }`
- [x] 2.3.7 Parse `sealed class Name { ... }`
- [x] 2.3.8 Create ClassDecl AST node

### 2.4 Class Members
- [x] 2.4.1 Parse field declarations with visibility
- [x] 2.4.2 Parse `private field: Type`
- [x] 2.4.3 Parse `protected field: Type`
- [x] 2.4.4 Parse `static field: Type`
- [ ] 2.4.5 Parse `static field: Type = value` (static initializer)

### 2.5 Class Methods
- [x] 2.5.1 Parse `func name(this, ...) -> T { ... }`
- [x] 2.5.2 Parse `virtual func name(...) { ... }`
- [x] 2.5.3 Parse `override func name(...) { ... }`
- [x] 2.5.4 Parse `abstract func name(...) -> T`
- [x] 2.5.5 Parse `static func name(...) -> T { ... }`
- [x] 2.5.6 Parse `private func name(...) { ... }`
- [x] 2.5.7 Parse `protected func name(...) { ... }`

### 2.6 Constructor
- [x] 2.6.1 Parse `func new(...) -> ClassName { ... }`
- [x] 2.6.2 Parse base constructor call `base: Parent::new(...)`
- [ ] 2.6.3 Handle constructor overloading

### 2.7 Properties (Optional)
- [ ] 2.7.1 Parse `prop name: Type { get; set }`
- [ ] 2.7.2 Parse `prop name: Type { get => expr }`
- [ ] 2.7.3 Parse `prop name: Type { get => expr, set(v) => { ... } }`
- [ ] 2.7.4 Create PropertyDecl AST node

### 2.8 Base Calls
- [x] 2.8.1 Parse `base.method()` expression
- [x] 2.8.2 Parse `base.field` expression
- [x] 2.8.3 Create BaseExpr AST node

## Phase 3: Type System - Class Hierarchy

### 3.1 Interface Type
- [x] 3.1.1 Create InterfaceType in types system
- [x] 3.1.2 Track interface method signatures
- [x] 3.1.3 Track interface inheritance chain
- [ ] 3.1.4 Support generic interfaces

### 3.2 Class Type
- [x] 3.2.1 Create ClassType in types system
- [x] 3.2.2 Track parent class (single inheritance)
- [x] 3.2.3 Track implemented interfaces
- [x] 3.2.4 Track virtual method table layout
- [ ] 3.2.5 Support generic classes

### 3.3 Inheritance Validation
- [x] 3.3.1 Verify single inheritance (no multiple class inheritance)
- [x] 3.3.2 Verify no circular inheritance
- [x] 3.3.3 Verify sealed class not extended
- [x] 3.3.4 Verify abstract class not instantiated directly

### 3.4 Override Validation
- [x] 3.4.1 Verify override method exists in parent
- [x] 3.4.2 Verify override signature matches exactly
- [x] 3.4.3 Verify non-virtual methods not overridden
- [x] 3.4.4 Verify abstract methods are implemented

### 3.5 Interface Implementation
- [x] 3.5.1 Verify all interface methods implemented
- [x] 3.5.2 Verify implementation signatures match
- [x] 3.5.3 Handle default interface methods (if supported)

### 3.6 Visibility Checking
- [x] 3.6.1 Enforce private visibility within class
- [x] 3.6.2 Enforce protected visibility in subclasses
- [x] 3.6.3 Enforce public visibility everywhere
- [x] 3.6.4 Handle visibility in inheritance

### 3.7 Namespace Resolution
- [ ] 3.7.1 Build namespace symbol tables
- [ ] 3.7.2 Resolve qualified names `Foo.Bar.Type`
- [ ] 3.7.3 Handle `use` imports
- [ ] 3.7.4 Handle wildcard imports `use Foo.*`
- [ ] 3.7.5 Handle name conflicts/ambiguity

## Phase 4: Codegen - Class Implementation

### 4.1 Class Memory Layout
- [x] 4.1.1 Generate struct type with vtable pointer as first field
- [x] 4.1.2 Layout parent class fields first
- [x] 4.1.3 Layout own fields after parent
- [ ] 4.1.4 Handle field alignment

### 4.2 VTable Generation
- [x] 4.2.1 Create vtable struct type for each class
- [x] 4.2.2 Include parent virtual methods first
- [x] 4.2.3 Add new virtual methods after
- [x] 4.2.4 Override entries for overridden methods
- [x] 4.2.5 Generate global vtable constant

### 4.3 Constructor Codegen
- [x] 4.3.1 Allocate object memory
- [x] 4.3.2 Initialize vtable pointer
- [x] 4.3.3 Call parent constructor if extends
- [x] 4.3.4 Initialize own fields
- [x] 4.3.5 Return constructed object

### 4.4 Virtual Method Calls
- [x] 4.4.1 Load vtable pointer from object
- [x] 4.4.2 Load method pointer from vtable
- [x] 4.4.3 Call method with object as first arg
- [ ] 4.4.4 Optimize final/sealed methods to direct call

### 4.5 Base Calls
- [x] 4.5.1 Generate direct call to parent method
- [x] 4.5.2 Pass current object as receiver
- [ ] 4.5.3 Handle multi-level base calls

### 4.6 Static Members
- [x] 4.6.1 Generate global variables for static fields
- [x] 4.6.2 Generate static initializers (literals supported)
- [x] 4.6.3 Generate static methods as regular functions
- [ ] 4.6.4 Handle static generic methods

### 4.7 Interface Dispatch
- [x] 4.7.1 Generate interface vtable (subset of class vtable)
- [ ] 4.7.2 Generate interface-to-vtable offset
- [ ] 4.7.3 Cast object to interface type
- [ ] 4.7.4 Dispatch through interface vtable

### 4.8 Type Checks
- [x] 4.8.1 Implement `is` operator for type checking
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

## Phase 9: Performance Considerations

### 9.1 Allocation Strategy
- [ ] 9.1.1 Document heap allocation cost of class instances
- [ ] 9.1.2 Prepare infrastructure for escape analysis (see `oop-mir-hir-optimizations`)
- [ ] 9.1.3 Mark classes eligible for stack allocation
- [ ] 9.1.4 Document vtable pointer overhead (8 bytes per instance)

### 9.2 Value Class Preparation
- [ ] 9.2.1 Add `@value` directive placeholder (validated, not optimized yet)
- [ ] 9.2.2 Validate @value constraints (sealed, no virtual)
- [ ] 9.2.3 Document when to use @value vs regular class

### 9.3 Pool/Arena Preparation
- [ ] 9.3.1 Add `@pool` directive placeholder (parsed, not implemented)
- [ ] 9.3.2 Document Pool[T] API for high-churn scenarios
- [ ] 9.3.3 Document Arena API for request-scoped allocation

### 9.4 Benchmarking
- [ ] 9.4.1 Create virtual dispatch microbenchmark
- [ ] 9.4.2 Create object instantiation benchmark
- [ ] 9.4.3 Compare with equivalent struct + behavior code
- [ ] 9.4.4 Document performance characteristics in user guide

## Phase 10: Cross-Task Integration

### 10.1 Related Tasks
- [ ] 10.1.1 Ensure compatibility with `oop-mir-hir-optimizations` task
- [ ] 10.1.2 Ensure compatibility with `implement-reflection` task (class TypeInfo)
- [ ] 10.1.3 Ensure compatibility with `memory-safety` task (class memory audit)

### 10.2 Future Optimization Hooks
- [ ] 10.2.1 Add sealed class metadata for devirtualization
- [ ] 10.2.2 Add final method metadata for devirtualization
- [ ] 10.2.3 Store class size for escape analysis threshold
- [ ] 10.2.4 Store inheritance depth for constructor optimization

## Validation

- [x] V.1 All new keywords recognized by lexer
- [x] V.2 All syntax parsed without errors
- [x] V.3 Type checker validates inheritance correctly
- [x] V.4 Virtual dispatch works at runtime
- [ ] V.5 Interface dispatch works at runtime
- [ ] V.6 Static members work correctly
- [x] V.7 Visibility enforced at compile time
- [x] V.8 All existing behavior/impl code still works
- [x] V.9 No performance regression for non-OOP code
- [ ] V.10 Design patterns can be implemented idiomatically
- [ ] V.11 @value directive parsed (optimization deferred)
- [ ] V.12 @pool directive parsed (implementation deferred)
- [ ] V.13 Benchmarks establish performance baseline

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Lexer Keywords | Complete | 12/16 |
| 2 | Parser Grammar | In Progress | 27/32 |
| 3 | Type System | In Progress | 18/22 |
| 4 | Codegen | In Progress | 18/26 |
| 5 | Standard Library | Not Started | 0/19 |
| 6 | IDE/Tooling | Not Started | 0/11 |
| 7 | Testing | Not Started | 0/18 |
| 8 | Documentation | Not Started | 0/13 |
| 9 | Performance | Not Started | 0/14 |
| 10 | Integration | Not Started | 0/7 |
| **Total** | | **In Progress** | **~75/178** |
