# Tasks: Codegen & Type System Test Blockers

**Status**: Not Started (0%)

## Phase 1: Critical Codegen Bugs

- [ ] 1.1.1 Fix cross-type From conversions (I8→I16, etc) failing at runtime
- [ ] 1.1.2 Fix U64 large literal codegen error
- [ ] 1.1.3 Fix Ordering enum to_string `zext i64 to i64` error
- [ ] 1.1.4 Fix I8/I16 MIN negative value codegen bug
- [ ] 1.1.5 Fix I8 negative return value codegen bug
- [ ] 1.1.6 Fix F32 float/double promotion issue
- [ ] 1.1.7 Fix Bool variable codegen issue
- [ ] 1.1.8 Fix U8/U16 bitwise operation type coercion

## Phase 2: Generic Type System Issues

- [ ] 2.1.1 Fix generic Range types compiler bug
- [ ] 2.1.2 Fix generic enum Nothing variant typing in generic context
- [ ] 2.1.3 Fix generic enum payload type parameter substitution
- [ ] 2.1.4 Fix generic tuple return type limitation
- [ ] 2.1.5 Support generic methods on non-generic types
- [ ] 2.1.6 Fix T::default() substitution in generic context
- [ ] 2.1.7 Fix generic function return type inference
- [ ] 2.1.8 Fix generic .duplicate() method resolution for Maybe[T]/Outcome[T,E]
- [ ] 2.1.9 Fix Bound generic enum variant resolution
- [ ] 2.1.10 Fix associated types codegen (Cow, ToOwned)
- [ ] 2.1.11 Fix generic Maybe/Outcome type inference

## Phase 3: Display/Behavior Implementation

- [ ] 3.1.1 Fix Display impl resolution for custom types (SimpleError, ParseError, IoError, etc)
- [ ] 3.1.2 Add runtime support for `impl Behavior` returns
- [ ] 3.1.3 Implement Clone behavior verification

## Phase 4: Runtime Features

- [ ] 4.1.1 Complete async/await support for async iterators
- [ ] 4.1.2 Fix Poll types (blocked by generic enum issues)
- [ ] 4.1.3 Add Drop behavior runtime support
- [ ] 4.1.4 Implement drop_in_place in lowlevel context
- [ ] 4.1.5 Add codegen support for partial moves
- [ ] 4.1.6 Support dyn return from functions (requires boxing)
- [ ] 4.1.7 Support inherited field initialization in struct literals

## Phase 5: Other Issues

- [ ] 5.1.1 Fix tuple literals defaulting to I64 instead of I32
- [ ] 5.1.2 Support module constant access
- [ ] 5.1.3 Fix core::option 'xor' keyword bug
- [ ] 5.1.4 Fix generic type inference with lifetime bounds
