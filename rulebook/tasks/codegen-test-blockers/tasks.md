# Tasks: Codegen & Type System Test Blockers

**Status**: In Progress (97%)

## Phase 1: Critical Codegen Bugs (100% Complete)

- [x] 1.1.1 Fix cross-type From conversions (I8→I16, etc) failing at runtime
- [x] 1.1.2 Fix U64 large literal codegen error
- [x] 1.1.3 Fix Ordering enum to_string `zext i64 to i64` error
- [x] 1.1.4 Fix I8/I16 MIN negative value codegen bug
- [x] 1.1.5 Fix I8 negative return value codegen bug
- [x] 1.1.6 Fix F32 float/double promotion issue
- [x] 1.1.7 Fix Bool variable codegen issue
- [x] 1.1.8 Fix U8/U16 bitwise operation type coercion

## Phase 2: Generic Type System Issues

- [x] 2.1.1 Fix generic Range types compiler bug
- [x] 2.1.2 Fix generic enum Nothing variant typing in generic context
- [x] 2.1.3 Fix generic enum payload type parameter substitution
- [x] 2.1.4 Fix generic tuple return type limitation
- [x] 2.1.5 Support generic methods on non-generic types
- [x] 2.1.6 Fix T::default() substitution in generic context
- [x] 2.1.7 Fix generic function return type inference
- [x] 2.1.8 Fix generic .duplicate() method resolution for Maybe[T]/Outcome[T,E]
- [x] 2.1.9 Fix Bound generic enum variant resolution
- [~] 2.1.10 Fix associated types codegen (Cow works, ToOwned partial)
- [x] 2.1.11 Fix generic Maybe/Outcome type inference (nested generics like Maybe[Maybe[I32]] now work)

## Phase 3: Display/Behavior Implementation

- [x] 3.1.1 Fix Display impl resolution for custom types (SimpleError, ParseError, IoError - working)
- [x] 3.1.2 Add runtime support for `impl Behavior` returns (concrete type inference from function body working)
- [x] 3.1.3 Implement Clone behavior verification

## Phase 4: Runtime Features

- [x] 4.1.1 Complete async/await support for async iterators
- [x] 4.1.2 Fix Poll types and function pointer field calling
- [x] 4.1.3 Add Drop behavior runtime support
- [x] 4.1.4 Implement drop_in_place in lowlevel context
- [ ] 4.1.5 Add codegen support for partial moves
- [x] 4.1.6 Support dyn return from functions (dyn coercion in return statements working)
- [x] 4.1.7 Support inherited field initialization in struct literals (struct update syntax `..base`)

## Phase 5: Other Issues

- [x] 5.1.1 Fix tuple literals defaulting to I64 instead of I32
- [x] 5.1.2 Support module constant access
- [x] 5.1.3 Fix core::option 'xor' keyword bug (renamed to one_of)
- [x] 5.1.4 Fix generic type inference with lifetime bounds (tests pass)

## Remaining Complex Tasks

The following tasks require significant implementation work:

- **4.1.5 Partial moves**: Requires field-level move tracking in codegen (not just whole-variable)
- **2.1.10 ToOwned**: Associated type resolution through generic bounds (e.g., `Maybe[T::Owned]`)
