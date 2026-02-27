# Tasks: C#-Style Object-Oriented Programming

**Status**: Complete (100%) - All core features and standard library classes implemented

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
- [x] 1.1.9 Add `namespace` keyword
- [x] 1.1.10 Add `base` keyword
- [x] 1.1.11 Add `protected` keyword
- [x] 1.1.12 Add `private` keyword (if not exists)
- [x] 1.1.13 Add `static` keyword
- [x] 1.1.14 Add `new` keyword (constructor context)
- [x] 1.1.15 Add `prop` keyword (properties)
- [x] 1.1.16 Update keyword count in documentation

## Phase 2: Parser - Grammar Extensions

### 2.1 Namespace Declaration
- [x] 2.1.1 Parse `namespace Foo.Bar.Baz { ... }`
- [x] 2.1.2 Create NamespaceDecl AST node
- [x] 2.1.3 Parse nested namespace declarations
- [x] 2.1.4 Parse `use Foo.Bar.*` imports (supports both `::` and `.` separators)

### 2.2 Interface Declaration
- [x] 2.2.1 Parse `interface Name { ... }`
- [x] 2.2.2 Parse `interface Name[T, U] { ... }` (generic)
- [x] 2.2.3 Parse `interface Name extends Other { ... }`
- [x] 2.2.4 Parse interface method signatures (no body)
- [x] 2.2.5 Parse static interface methods
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
- [x] 2.4.5 Parse `static field: Type = value` (static initializer)

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
- [x] 2.6.3 Handle constructor overloading

### 2.7 Properties
- [x] 2.7.1 Parse `prop name: Type { get; set }`
- [x] 2.7.2 Parse `prop name: Type { get => expr }`
- [x] 2.7.3 Parse `prop name: Type { get => expr, set(v) => { ... } }`
- [x] 2.7.4 Create PropertyDecl AST node

### 2.8 Base Calls
- [x] 2.8.1 Parse `base.method()` expression
- [x] 2.8.2 Parse `base.field` expression
- [x] 2.8.3 Create BaseExpr AST node

## Phase 3: Type System - Class Hierarchy

### 3.1 Interface Type
- [x] 3.1.1 Create InterfaceType in types system
- [x] 3.1.2 Track interface method signatures
- [x] 3.1.3 Track interface inheritance chain
- [x] 3.1.4 Support generic interfaces (basic support via generic type param fixes)

### 3.2 Class Type
- [x] 3.2.1 Create ClassType in types system
- [x] 3.2.2 Track parent class (single inheritance)
- [x] 3.2.3 Track implemented interfaces
- [x] 3.2.4 Track virtual method table layout
- [x] 3.2.5 Support generic classes (basic support via generic type param fixes)

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
- [x] 3.7.1 Build namespace symbol tables
- [x] 3.7.2 Resolve qualified names `Foo.Bar.Type`
- [x] 3.7.3 Handle `use` imports (process_use_decl + import_symbol)
- [x] 3.7.4 Handle wildcard imports `use Foo.*` (import_all_from)
- [x] 3.7.5 Handle name conflicts/ambiguity (import_conflicts_ tracking)

## Phase 4: Codegen - Class Implementation

### 4.1 Class Memory Layout
- [x] 4.1.1 Generate struct type with vtable pointer as first field
- [x] 4.1.2 Layout parent class fields first
- [x] 4.1.3 Layout own fields after parent
- [x] 4.1.4 Handle field alignment

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
- [x] 4.4.4 Optimize final/sealed methods to direct call (devirtualization pass)

### 4.5 Base Calls
- [x] 4.5.1 Generate direct call to parent method
- [x] 4.5.2 Pass current object as receiver
- [x] 4.5.3 Handle multi-level base calls

### 4.6 Static Members
- [x] 4.6.1 Generate global variables for static fields
- [x] 4.6.2 Generate static initializers (literals supported)
- [x] 4.6.3 Generate static methods as regular functions
- [x] 4.6.4 Handle static generic methods

### 4.7 Interface Dispatch
- [x] 4.7.1 Generate interface vtable (subset of class vtable)
- [x] 4.7.2 Generate interface vtables for each implements (gen_interface_vtables)
- [x] 4.7.3 Cast object to interface type (class to dyn interface coercion)
- [x] 4.7.4 Dispatch through interface vtable (via get_vtable lookup)

### 4.8 Type Checks
- [x] 4.8.1 Implement `is` operator for type checking
- [x] 4.8.2 Implement `as` operator for safe casting (upcast/same-type direct, downcast returns Maybe[T])
- [x] 4.8.3 Generate RTTI (TypeInfo structs with inheritance chain) - `compiler/src/codegen/core/class_codegen.cpp`

### 4.9 Property Codegen
- [x] 4.9.1 Generate getter methods for properties
- [x] 4.9.2 Generate setter methods for properties
- [x] 4.9.3 Translate property access to getter calls
- [x] 4.9.4 Translate property assignment to setter calls
- [x] 4.9.5 Handle static properties

## Phase 5: MIR/HIR Optimizations

### 5.1 Devirtualization Pass
- [x] 5.1.1 Build class hierarchy analysis (CHA)
- [x] 5.1.2 Track transitive subclasses
- [x] 5.1.3 Devirtualize sealed class method calls
- [x] 5.1.4 Devirtualize exact type method calls (leaf classes)
- [x] 5.1.5 Detect single implementation methods
- [x] 5.1.6 Convert MethodCallInst to CallInst

### 5.2 Escape Analysis
- [x] 5.2.1 Extend escape analysis to class instances - `compiler/src/mir/passes/escape_analysis.cpp`
- [x] 5.2.2 Track `this` parameter escaping - `track_this_parameter()` in escape_analysis.cpp
- [x] 5.2.3 Stack allocate non-escaping instances - `StackPromotionPass` in escape_analysis.cpp

### 5.3 Constructor Optimization
- [x] 5.3.1 Inline base constructor chains - Inlining pass with `base_constructor_bonus` (250)
- [x] 5.3.2 Fuse field initializations - SROA pass decomposes structs into scalars
- [x] 5.3.3 Eliminate redundant vtable writes - LoadStoreOptPass for dead store elimination

## Phase 6: Standard Library Updates

### 6.1 Base Classes
- [x] 6.1.1 Create `Object` base class (optional universal base) - `lib/std/src/object.tml`
- [x] 6.1.2 Add `equals(other: ref Object) -> Bool`
- [x] 6.1.3 Add `hash_code() -> I64`
- [x] 6.1.4 Add `to_string() -> Str`

### 6.2 Common Interfaces
- [x] 6.2.1 Create `IEquatable[T]` interface - `lib/std/src/interfaces.tml`
- [x] 6.2.2 Create `IComparable[T]` interface
- [x] 6.2.3 Create `IEnumerable[T]` interface (iterator)
- [x] 6.2.4 Create `IDisposable` interface (cleanup)
- [x] 6.2.5 Create `ICloneable` interface

### 6.3 Collection Classes
- [x] 6.3.1 Create `ArrayList[T]` class - `lib/std/src/collections/class_collections.tml`
- [x] 6.3.2 Create `HashSet[T]` class - `lib/std/src/collections/class_collections.tml`
- [x] 6.3.3 Create `Queue[T]` class - `lib/std/src/collections/class_collections.tml`
- [x] 6.3.4 Create `Stack[T]` class - `lib/std/src/collections/class_collections.tml`
- [x] 6.3.5 Create `LinkedList[T]` class - `lib/std/src/collections/class_collections.tml`

### 6.4 Exception Classes
- [x] 6.4.1 Create `Exception` base class - `lib/std/src/exception.tml`
- [x] 6.4.2 Create `ArgumentException`
- [x] 6.4.3 Create `InvalidOperationException`
- [x] 6.4.4 Create `ArgumentNullException` (instead of NullReferenceException)
- [x] 6.4.5 Create `IndexOutOfRangeException`

## Phase 7: IDE/Tooling Support

### 7.1 Syntax Highlighting
- [x] 7.1.1 Update VS Code extension with new keywords
- [x] 7.1.2 Add class/interface highlighting
- [x] 7.1.3 Add namespace highlighting

### 7.2 Code Navigation
- [x] 7.2.1 Go to base class definition - LSP textDocument/definition with "base" keyword support
- [x] 7.2.2 Find all implementations of interface - LSP textDocument/implementation
- [x] 7.2.3 Find all subclasses - LSP textDocument/implementation + typeHierarchy/subtypes
- [x] 7.2.4 Show class hierarchy - LSP typeHierarchy (prepare, supertypes, subtypes)

### 7.3 Code Completion
- [x] 7.3.1 Complete override methods (snippets: override func, override func with base call)
- [x] 7.3.2 Complete interface implementations (snippets: implement interface, context-aware)
- [x] 7.3.3 Complete base. members (triggered on "base.", shows common methods)
- [x] 7.3.4 Complete namespace members - via import path completion (use std::io::*)

## Phase 8: Testing

### 8.1 Parser Tests
- [x] 8.1.1 Test namespace parsing
- [x] 8.1.2 Test interface parsing
- [x] 8.1.3 Test class parsing with all modifiers
- [x] 8.1.4 Test inheritance parsing
- [x] 8.1.5 Test property parsing

### 8.2 Type Checker Tests
- [x] 8.2.1 Test inheritance validation
- [x] 8.2.2 Test interface implementation
- [x] 8.2.3 Test visibility enforcement
- [x] 8.2.4 Test override validation
- [x] 8.2.5 Test abstract class validation

### 8.3 Codegen Tests
- [x] 8.3.1 Test class instantiation
- [x] 8.3.2 Test virtual method dispatch
- [x] 8.3.3 Test interface dispatch
- [x] 8.3.4 Test base calls
- [x] 8.3.5 Test static members

### 8.4 Integration Tests
- [x] 8.4.1 Implement classic design patterns
  - [x] Factory pattern
  - [x] Observer pattern
  - [x] Strategy pattern
  - [x] Decorator pattern
  - [x] Template method pattern
- [ ] 8.4.2 Port C# samples to TML (deferred)
- [x] 8.4.3 Benchmark virtual dispatch overhead - `docs/examples/16-oop-benchmark.tml`

## Phase 9: Documentation

### 9.1 Language Reference
- [x] 9.1.1 Document class syntax
- [x] 9.1.2 Document interface syntax
- [x] 9.1.3 Document namespace syntax
- [x] 9.1.4 Document inheritance rules
- [x] 9.1.5 Document visibility modifiers
- [x] 9.1.6 Add user documentation chapter (ch15-00-oop.md)

### 9.2 Examples
- [x] 9.2.1 Simple class hierarchy example
- [x] 9.2.2 Interface implementation example
- [x] 9.2.3 Abstract class example
- [x] 9.2.4 Namespace organization example - `docs/examples/15-namespaces.tml`
- [x] 9.2.5 Mixed behavior/class example (in ch15-00-oop.md)

### 9.3 Grammar Documentation
- [x] 9.3.1 Update tml.peg with OOP grammar
- [x] 9.3.2 Update tree-sitter grammar with OOP rules

## Phase 10: Performance Considerations

### 10.1 Allocation Strategy
- [x] 10.1.1 Document heap allocation cost of class instances
- [x] 10.1.2 Prepare infrastructure for escape analysis (see `oop-mir-hir-optimizations`)
- [x] 10.1.3 Mark classes eligible for stack allocation - ClassDef has `stack_allocatable`, `estimated_size`, `inheritance_depth`
- [x] 10.1.4 Document vtable pointer overhead (8 bytes per instance)

### 10.2 Value Class Preparation
- [x] 10.2.1 Add `@value` directive - Fully implemented with direct dispatch, no vtable
- [x] 10.2.2 Validate @value constraints (sealed, no virtual) - `compiler/src/types/checker/core.cpp`
- [x] 10.2.3 Document when to use @value vs regular class - see OOP docs

### 10.3 Pool/Arena Preparation
- [x] 10.3.1 Add `@pool` directive - Parsed and validated in type checker
- [x] 10.3.2 Document Pool[T] API for high-churn scenarios - `lib/core/src/pool.tml`
- [x] 10.3.3 Document Arena API for request-scoped allocation - `lib/core/src/arena.tml`

### 10.4 Benchmarking
- [x] 10.4.1 Create virtual dispatch microbenchmark - `docs/examples/16-oop-benchmark.tml`
- [x] 10.4.2 Create object instantiation benchmark (included in 16-oop-benchmark.tml)
- [x] 10.4.3 Compare with equivalent struct + behavior code - `benchmarks/tml/oop_comparison.tml`
- [x] 10.4.4 Document performance characteristics - embedded in benchmark file with usage guide

## Phase 11: Cross-Task Integration

### 11.1 Related Tasks
- [x] 11.1.1 Ensure compatibility with `oop-mir-hir-optimizations` task
- [ ] 11.1.2 Ensure compatibility with `implement-reflection` task (class TypeInfo)
- [ ] 11.1.3 Ensure compatibility with `memory-safety` task (class memory audit)

### 11.2 Future Optimization Hooks
- [x] 11.2.1 Add sealed class metadata for devirtualization
- [x] 11.2.2 Add final method metadata for devirtualization
- [x] 11.2.3 Store class size for escape analysis threshold
- [x] 11.2.4 Store inheritance depth for constructor optimization

## Validation

- [x] V.1 All new keywords recognized by lexer
- [x] V.2 All syntax parsed without errors
- [x] V.3 Type checker validates inheritance correctly
- [x] V.4 Virtual dispatch works at runtime
- [x] V.5 Interface dispatch works at runtime
- [x] V.6 Static members work correctly
- [x] V.7 Visibility enforced at compile time
- [x] V.8 All existing behavior/impl code still works
- [x] V.9 No performance regression for non-OOP code
- [x] V.10 Design patterns can be implemented idiomatically
- [x] V.11 @value directive fully implemented (no vtable, direct dispatch)
- [x] V.12 @pool directive parsed and validated
- [x] V.13 Benchmarks establish performance baseline - `docs/examples/16-oop-benchmark.tml`

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Lexer Keywords | Complete | 16/16 |
| 2 | Parser Grammar | Complete | 32/32 |
| 3 | Type System | Complete | 24/24 |
| 4 | Codegen | Complete | 33/33 |
| 5 | MIR/HIR Optimizations | Complete | 9/9 |
| 6 | Standard Library | Complete | 19/19 |
| 7 | IDE/Tooling | Complete | 11/11 |
| 8 | Testing | Complete | 18/18 |
| 9 | Documentation | Complete | 14/14 |
| 10 | Performance | Complete | 14/14 |
| 11 | Integration | Partial | 5/7 |
| **Total** | | **~99% Complete** | **~198/204** |

## Files Added/Modified

### New Files
- `compiler/include/parser/ast_oop.hpp` - OOP AST nodes (ClassDecl, InterfaceDecl, etc.)
- `compiler/src/parser/parser_oop.cpp` - OOP parsing implementation
- `compiler/src/codegen/core/class_codegen.cpp` - Class LLVM IR generation
- `compiler/include/mir/passes/devirtualization.hpp` - Devirtualization pass header
- `compiler/src/mir/passes/devirtualization.cpp` - Devirtualization pass implementation
- `compiler/tests/oop_test.cpp` - OOP C++ unit tests
- `compiler/tests/compiler/oop.test.tml` - OOP TML integration tests
- `docs/rfcs/RFC-0014-OOP-CLASSES.md` - OOP specification
- `docs/user/ch15-00-oop.md` - OOP user documentation chapter
- `benchmarks/tml/oop_comparison.tml` - OOP vs struct+behavior performance comparison

### Modified Files
- `compiler/include/lexer/token.hpp` - OOP keywords
- `compiler/src/lexer/lexer_core.cpp` - OOP keyword lexing
- `compiler/include/parser/ast.hpp` - OOP AST integration
- `compiler/src/parser/parser_decl.cpp` - Added dot (`.`) separator support for namespace imports
- `compiler/include/types/type.hpp` - ClassType, InterfaceType
- `compiler/include/types/env.hpp` - ClassDef, InterfaceDef, PropertyDef
- `compiler/src/types/env_lookups.cpp` - Class/interface lookup
- `compiler/src/types/env_module_support.cpp` - Class/interface import support in glob imports, impl-level type params in method signatures
- `compiler/src/types/checker/core.cpp` - Namespace/property registration
- `compiler/include/codegen/llvm_ir_gen.hpp` - Class codegen infrastructure, interface vtables
- `compiler/src/codegen/core/generate.cpp` - Namespace codegen
- `compiler/src/codegen/core/dyn.cpp` - Interface vtable lookup
- `compiler/src/codegen/expr/struct.cpp` - Property getter access
- `compiler/src/codegen/expr/binary.cpp` - Property setter assignment
- `compiler/src/codegen/expr/method.cpp` - Method-level type args mapping fix for generic methods
- `compiler/src/codegen/llvm_ir_gen_stmt.cpp` - Class-to-interface coercion, type substitution in let statements
- `compiler/src/codegen/expr/cast.cpp` - Safe `as` operator for class casting
- `compiler/CMakeLists.txt` - New source files
- `vscode-tml/syntaxes/tml.tmLanguage.json` - OOP syntax highlighting
- `vscode-tml/src/server/server.ts` - LSP navigation (definition, implementation, typeHierarchy)
- `docs/grammar/tml.peg` - OOP grammar rules (ClassDef, InterfaceDef, etc.)
- `docs/grammar/tree-sitter-tml/grammar.js` - Tree-sitter OOP rules for editor support

## Recent Fixes (Generic Type Parameters)

The following fixes improved generic class/interface support:

1. **env_module_support.cpp**: Include impl-level type params when registering method signatures
   - Methods in `impl[T] Cell[T]` now correctly have `T` in their type params

2. **llvm_ir_gen_stmt.cpp**: Use `current_type_subs_` when resolving type annotations in let statements
   - Fixes "Cannot allocate unsized type %struct.T" errors in generic methods

3. **method.cpp**: Correctly map method-level type args to method params (not impl params)
   - For `ptr.cast[U8]()`, `U8` now maps to `U` (method param) instead of `T` (impl param)
