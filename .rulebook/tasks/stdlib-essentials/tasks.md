# Tasks: Standard Library Essentials — Phase 2 (Compiler-Blocked Items)

**Status**: In Progress (70%) - Phase 1.2 & 1.3 verified complete Feb 26, Phase 1.4.2 blocking Phase 2

**Note**: These items were blocked in the previous iteration because the compiler
lacks the features needed to implement them in pure TML. Each item lists exactly
what must be added to the compiler first.

---

## Phase 1: Compiler Changes Required

### 1.1 Generic Iterator Behavior

**Compiler blocker**: No `behavior` (trait) with associated types + generic impls.
Required changes:
- [x] 1.1.1 Add `behavior Iterator[T]` with `next() -> Maybe[T]` to `lib/core/src/iter.tml`
- [x] 1.1.2 Support `impl Iterator[T] for MyType` in the type checker
- [x] 1.1.3 Support `for x in iterable` syntactic sugar desugaring to Iterator calls (gen_for_iterator + check_for)
- [x] 1.1.4 Support associated type `type Item` inside behavior impls (fix: check_impl_decl + generate_default_method)

### 1.2 Generic Slice Parameters (`[T]`)

**Status**: ✅ COMPLETE - All slice features work!
Required changes:
- [x] 1.2.1 Add slice type syntax `[T]` to parser and type checker (verified Feb 26)
- [x] 1.2.2 Support `ref [T]` and `mut ref [T]` as borrow-checked slice params (working)
- [x] 1.2.3 Support slice indexing `s[i]` and `s.len()` on slice params (via `.len()`, `.get()`)
- [x] 1.2.4 Codegen generates correct LLVM `{ ptr, i64 }` fat pointer (verified Feb 26)

### 1.3 Generic Type Constraints (`[T: SomeBehavior]`)

**Status**: ✅ COMPLETE - All constraint features work!
Required changes:
- [x] 1.3.1 Support `T: BehaviorWithAssociatedTypes` in impl blocks (working)
- [x] 1.3.2 Support `T: Behavior1 + Behavior2` compound constraints (verified Feb 26)
- [x] 1.3.3 Monomorphize generic impls correctly when constraint involves `type Item` (working)

### 1.4 Function Type Parameters

**Status**: ⏳ PARTIALLY COMPLETE - Core support done, lambda conversion missing
Required changes:
- [x] 1.4.1 Add function pointer types `func(A, B) -> R` to the type system (done)
- [ ] 1.4.2 Support passing lambdas `do(x) expr` where function pointer expected (BLOCKS 2.1)
  - ❌ Lambda codegen generates `void` when passed as function pointer
  - Found: `test_lambda_funcptr.tml` fails with "void type only allowed for results"
- [x] 1.4.3 Support higher-order functions in codegen (indirect calls work)

---

## Phase 2: stdlib Items (unblock after compiler changes above)

### 2.1 Vec[T] — needs 1.1 + 1.4
- [ ] 2.1.1 `Vec::from_iter(iter: impl Iterator[T]) -> Vec[T]`
- [ ] 2.1.2 `Vec::retain(mut this, pred: func(ref T) -> Bool)` — needs 1.4
- [ ] 2.1.3 `Vec::drain(mut this, start: I64, end: I64) -> Vec[T]` — needs 1.3
- [ ] 2.1.4 `impl Iterator[T] for Vec[T]` — needs 1.1

### 2.2 HashSet[T] — needs 1.1
- [ ] 2.2.1 `impl Iterator[T] for HashSet[T]` — needs 1.1
- [ ] 2.2.2 `HashSet::from_iter(iter: impl Iterator[T]) -> HashSet[T]`

### 2.3 BTreeMap / BTreeSet — needs 1.1
- [ ] 2.3.1 `impl Iterator[(I64, I64)] for BTreeMap` ordered iteration
- [ ] 2.3.2 `impl Iterator[I64] for BTreeSet` ordered iteration

### 2.4 BufReader — needs 1.1
- [ ] 2.4.1 Make `Lines` implement `Iterator[Str]` — needs 1.1
- [ ] 2.4.2 `BufReader[R: Read]` generic version — needs 1.3

### 2.5 os::env — needs 1.1
- [ ] 2.5.1 `env::vars() -> impl Iterator[(Str, Str)]` — needs 1.1 + OS FFI for env enumeration

### 2.6 Random — needs 1.3 + 1.2
- [ ] 2.6.1 `behavior Distribution[T]` with `sample(rng: mut ref Rng) -> T` — needs 1.3
- [ ] 2.6.2 `choose[T](slice: ref [T]) -> Maybe[ref T]` — needs 1.2
- [ ] 2.6.3 `random[T: Random]() -> T` generic convenience — needs 1.3

---

## Phase 3: Validation
- [ ] 3.1 Run full test suite after each compiler change
- [ ] 3.2 Verify no regressions in existing stdlib tests (358 collection + 62 file tests)
- [ ] 3.3 Add tests for each new stdlib item as it becomes unblocked
