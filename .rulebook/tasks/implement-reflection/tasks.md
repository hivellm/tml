# Tasks: Complete Reflection System Implementation

## Progress: 48% (33/69 tasks complete)

**Status**: Phase 4 complete - AnyValue type-erased container implemented

**Proposal**: See [proposal.md](proposal.md) for full RFC

## Phase 1: Core Intrinsics (P0) ✓

### 1.1 Field Count Intrinsic ✓
- [x] 1.1.1 Add `field_count` to intrinsics set in `compiler/src/codegen/builtins/intrinsics.cpp`
- [x] 1.1.2 Implement `field_count[T]() -> USize` returning struct field count
- [x] 1.1.3 Return 0 for primitives and enums without data
- [x] 1.1.4 Add `compiler/tests/compiler/reflect/field_count.test.tml` test file

### 1.2 Variant Count Intrinsic ✓
- [x] 1.2.1 Add `variant_count` to intrinsics set
- [x] 1.2.2 Implement `variant_count[T]() -> USize` returning enum variant count
- [x] 1.2.3 Return 0 for structs and primitives
- [x] 1.2.4 Add `compiler/tests/compiler/reflect/variant_count.test.tml` test file

### 1.3 Field Name Intrinsic ✓
- [x] 1.3.1 Add `field_name` to intrinsics set
- [x] 1.3.2 Implement `field_name[T](index: USize) -> Str` returning field name
- [ ] 1.3.3 Validate index at compile time (error if out of bounds)
- [x] 1.3.4 Store field names as string literals in .rdata section

### 1.4 Field Metadata Intrinsics ✓
- [x] 1.4.1 Implement `field_type_id[T](index: USize) -> U64` returning field type ID
- [x] 1.4.2 Implement `field_offset[T](index: USize) -> USize` returning byte offset
- [x] 1.4.3 Add `compiler/tests/compiler/reflect/field_metadata.test.tml` test file

## Phase 2: TypeInfo Generation (P1)

### 2.1 Core Reflection Types ✓
- [x] 2.1.1 Create `lib/core/reflect.tml` with TypeKind enum
- [x] 2.1.2 Define FieldInfo struct (name, type_id, type_name, offset, is_public)
- [x] 2.1.3 Define VariantInfo struct (name, tag, payload_types)
- [x] 2.1.4 Define TypeInfo struct (id, name, kind, size, align, fields, variants)

### 2.2 TypeInfo Code Generation ✓
- [x] 2.2.1 Create `compiler/src/codegen/derive/reflect.cpp` for derive macro
- [x] 2.2.2 Generate static TypeInfo for types with `@derive(Reflect)`
- [x] 2.2.3 Store TypeInfo in .rdata section (read-only)
- [x] 2.2.4 Add `compiler/tests/compiler/reflect/derive_reflect.test.tml` test file

## Phase 3: Reflect Behavior (P1)

### 3.1 Reflect Behavior Definition
- [x] 3.1.1 Define Reflect behavior in `lib/core/reflect.tml`
- [x] 3.1.2 Add `type_info() -> ref TypeInfo` static method
- [x] 3.1.3 Add `runtime_type_info(this) -> ref TypeInfo` method
- [ ] 3.1.4 Add `get_field(ref this, name: Str) -> Maybe[ref Any]` method

### 3.2 Derive Reflect Implementation
- [x] 3.2.1 Implement `@derive(Reflect)` in compiler
- [ ] 3.2.2 Generate `get_field_by_index(ref this, index: USize)` accessor
- [ ] 3.2.3 Generate `set_field(mut ref this, name: Str, value: ref Any)` mutator
- [x] 3.2.4 Generate `variant_name(ref this) -> Str` for enums
- [x] 3.2.5 Generate `variant_tag(ref this) -> I64` for enums
- [x] 3.2.6 Add `compiler/tests/compiler/derive_reflect.test.tml` test file

## Phase 4: Any Type (P2)

### 4.1 Any Type Implementation
- [x] 4.1.1 Create `AnyValue` type in `lib/core/src/any.tml`
- [x] 4.1.2 Implement `AnyValue::from[T](value: T) -> AnyValue`
- [x] 4.1.3 Implement `downcast[T](this) -> Maybe[ref T]`
- [x] 4.1.4 Implement `downcast_mut[T](mut this) -> Maybe[mut ref T]`
- [x] 4.1.5 Implement `is_type[T](this) -> Bool` (renamed from `is` due to keyword)
- [x] 4.1.6 Implement Drop for AnyValue (deallocs memory; inner Drop requires function pointers)
- [x] 4.1.7 Add AnyValue tests to `lib/core/tests/any.test.tml`
- [x] 4.1.8 Implement `TypeId::of[T]()` using `type_id[T]()` intrinsic

## Phase 5: OOP Reflection (P2)

### 5.1 Class Reflection Intrinsics
- [ ] 5.1.1 Implement `base_class[T]() -> Maybe[TypeId]` for inheritance
- [ ] 5.1.2 Implement `interfaces[T]() -> Slice[TypeId]` for implemented interfaces
- [ ] 5.1.3 Implement `is_abstract[T]() -> Bool` for abstract classes
- [ ] 5.1.4 Implement `is_sealed[T]() -> Bool` for sealed classes
- [ ] 5.1.5 Add `compiler/tests/compiler/class_reflection_intrinsics.test.tml`

### 5.2 Method Reflection
- [ ] 5.2.1 Implement `method_count[T]() -> USize` for class methods
- [ ] 5.2.2 Implement `method_name[T](index: USize) -> Str`
- [ ] 5.2.3 Implement `is_virtual[T](method_index: USize) -> Bool`
- [ ] 5.2.4 Implement `is_override[T](method_index: USize) -> Bool`
- [ ] 5.2.5 Implement `is_static[T](method_index: USize) -> Bool`

### 5.3 Class TypeInfo Generation
- [ ] 5.3.1 Extend TypeInfo with base_class field
- [ ] 5.3.2 Extend TypeInfo with interfaces array
- [ ] 5.3.3 Add MethodInfo struct (name, is_virtual, is_static, visibility)
- [ ] 5.3.4 Generate vtable slot index in MethodInfo
- [ ] 5.3.5 Handle inherited methods in TypeInfo

### 5.4 Interface Reflection
- [ ] 5.4.1 Define InterfaceInfo struct
- [ ] 5.4.2 Generate TypeInfo for interface types
- [ ] 5.4.3 Implement `implementors[I]() -> Slice[TypeId]` (compile-time known)
- [ ] 5.4.4 Add `compiler/tests/compiler/interface_reflection.test.tml`

### 5.5 Dynamic Dispatch Reflection
- [ ] 5.5.1 Implement `call_virtual(obj: ref Any, method: Str, args: Slice[Any]) -> Any`
- [ ] 5.5.2 Look up method in runtime TypeInfo
- [ ] 5.5.3 Resolve vtable slot for virtual call
- [ ] 5.5.4 Handle interface method dispatch
- [ ] 5.5.5 Add `compiler/tests/compiler/dynamic_dispatch_reflection.test.tml`

## Phase 6: Integration & Testing (P3)

### 6.1 Library Examples
- [ ] 6.1.1 Implement `debug_print[T: Reflect](value: ref T)` in `lib/std/debug.tml`
- [ ] 6.1.2 Implement `to_json[T: Reflect](value: ref T) -> Str` in `lib/std/json.tml`

### 6.2 Comprehensive Testing
- [ ] 6.2.1 Add comprehensive test suite in `compiler/tests/compiler/reflect_*.test.tml`
- [ ] 6.2.2 Test struct reflection (fields, types, values)
- [ ] 6.2.3 Test enum/ADT reflection (variants, payloads)
- [ ] 6.2.4 Test generic types with reflection bounds

### 6.3 OOP Testing
- [ ] 6.3.1 Test class reflection (fields, methods, base class)
- [ ] 6.3.2 Test interface reflection
- [ ] 6.3.3 Test virtual method reflection
- [ ] 6.3.4 Test dynamic virtual call via reflection
- [ ] 6.3.5 Benchmark reflection overhead vs direct call

### 6.4 Documentation
- [ ] 6.4.1 Create `docs/user/ch15-00-reflection.md` user guide
- [ ] 6.4.2 Update `CHANGELOG.md` with reflection features
- [ ] 6.4.3 Add reflection examples to `docs/14-EXAMPLES.md`
- [ ] 6.4.4 Document class/interface reflection API

## Validation

- [ ] V.1 All existing TML tests continue to pass
- [ ] V.2 `field_count[T]()` returns correct count for structs
- [ ] V.3 `variant_count[T]()` returns correct count for enums
- [ ] V.4 `@derive(Reflect)` generates valid TypeInfo
- [ ] V.5 `Any::downcast[T]()` correctly validates types
- [ ] V.6 `debug_print` works for any Reflect type
- [ ] V.7 `to_json` serializes Reflect types correctly
- [ ] V.8 Class reflection includes inherited fields/methods
- [ ] V.9 Interface reflection lists all methods
- [ ] V.10 Dynamic virtual dispatch works via reflection

## Summary

| Phase | Description | Priority | Status | Tasks |
|-------|-------------|----------|--------|-------|
| 1 | Core Intrinsics | P0 | ✓ Complete | 11/11 |
| 2 | TypeInfo Generation | P1 | ✓ Complete | 8/8 |
| 3 | Reflect Behavior | P1 | In Progress | 6/10 |
| 4 | Any Type | P2 | ✓ Complete | 8/8 |
| 5 | OOP Reflection | P2 | Pending | 0/20 |
| 6 | Integration & Testing | P3 | Pending | 0/13 |
| **Total** | | | | **33/70** |

## Dependencies

- Phase 2 depends on Phase 1 (intrinsics for field metadata)
- Phase 3 depends on Phase 2 (TypeInfo types must exist)
- Phase 4 depends on Phase 3 (Reflect behavior for type checking)
- Phase 5 depends on Phase 3 and `oop-csharp-style` task
- Phase 6 depends on all previous phases

## Design Decisions

1. **OOP Support**: Full reflection for classes, interfaces, virtual methods
2. **Opt-in**: Only `@derive(Reflect)` types have metadata overhead
3. **Zero-cost intrinsics**: All intrinsics are evaluated at compile time
4. **Heap-allocated Any**: Simplifies implementation, can optimize later
5. **Private fields accessible**: `is_public` flag allows runtime checks
6. **Inheritance chain**: TypeInfo includes base_class for full hierarchy
7. **Virtual dispatch**: Reflection can invoke virtual methods dynamically
