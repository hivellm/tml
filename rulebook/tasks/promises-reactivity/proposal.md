# Proposal: Promises and Reactive Streams

## Why

TML needs high-level asynchronous and reactive programming primitives that are familiar
to developers coming from the JavaScript/TypeScript ecosystem. The current async/await
system based on Poll[T] is low-level and requires internal compiler knowledge. Promises
and Observables provide a high-level API that:

1. **Reduces barrier to entry** - JavaScript developers can use familiar patterns
2. **Improves ergonomics** - APIs like `.then()`, `.catch()`, `Promise.all()` are intuitive
3. **Enables reactive programming** - Data streams are fundamental for UIs and events
4. **Completes async runtime** - Current async/await implementation is insufficient for real apps

## What Changes

### New Types in Core
- `Promise[T]` - Async container with Pending/Fulfilled/Rejected states
- `Observable[T]` - Push-based value stream with subscription
- `Observer[T]` - Behavior for receiving events (on_next, on_error, on_complete)
- `Subject[T]`, `BehaviorSubject[T]`, `ReplaySubject[T]` - Multicasting subjects

### New APIs

**Promise:**
- Constructors: `Promise.new()`, `Promise.resolve()`, `Promise.reject()`
- Instance: `.then()`, `.catch()`, `.finally()`, `.map()`
- Combinators: `Promise.all()`, `Promise.race()`, `Promise.any()`, `Promise.allSettled()`

**Observable:**
- Creation: `Observable.of()`, `Observable.from()`, `Observable.interval()`, `Observable.timer()`
- Transformation: `.map()`, `.flat_map()`, `.switch_map()`, `.filter()`, `.take()`, `.skip()`
- Combination: `.merge()`, `.concat()`, `.zip()`, `.combine_latest()`
- Utilities: `.debounce()`, `.throttle()`, `.retry()`, `.catch_error()`

### Compiler Changes
- Parser: Support for pipe operator `|>` for fluent chaining
- Type Checker: Type inference for Observable/Promise operators
- Codegen: Callback and subscription generation

### Runtime
- Minimal event loop
- Microtask queue for Promises
- Scheduler for temporal Observables

## Impact

- **Affected specs**:
  - docs/04-TYPES.md (new types)
  - docs/13-BUILTINS.md (new functions)
  - docs/05-SEMANTICS.md (Promise/Observable semantics)

- **Affected code**:
  - lib/core/ (new modules promise.tml, reactive.tml)
  - compiler/src/parser/ (pipe operator)
  - compiler/src/types/ (operator type inference)
  - compiler/src/codegen/ (callback generation)
  - compiler/runtime/ (event loop, scheduler)

- **Breaking change**: NO
  - New types and APIs, does not modify existing behavior

- **User benefit**:
  - Familiar APIs for JavaScript/TypeScript developers
  - Native reactive programming in the language
  - Simplified async operation composition
  - Better integration with existing async/await
