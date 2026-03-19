# TML Language Gaps — Verified with real compilation tests (2026-03-19)

Only items where the feature crashes or generates invalid IR.
NOT included: things that exist and work but weren't used in HTTP code.

## Gap 1: Bool/i1 struct through fn pointers — SEGFAULT

**Status**: Lexer ✅ Parser ✅ Typechecker ✅ Codegen ❌ SEGFAULT
**Test**: `.sandbox/test_bool_fnptr.tml` — exit code -1073741819 (ACCESS_VIOLATION)
**Root cause**: Bool maps to i1 in LLVM IR; struct layout mismatch when passed through fn ptr

- [ ] 1.1 Reproduce with emit-ir, compare struct layout with/without Bool fields
- [ ] 1.2 Fix: Bool struct fields should use i8 in LLVM IR (like Rust does)
- [ ] 1.3 Verify: struct with Bool fields works through fn pointers
- [ ] 1.4 Re-enable ServerResponse hooks/middleware in HTTP dispatch

**Blocks**: HTTP middleware, hooks, any callback pattern with Bool-containing structs

## Gap 2: dyn trait objects — CODEGEN INVALID IR

**Status**: Lexer ✅ Parser ✅ Typechecker ✅ Codegen ❌ Invalid IR (`%v1` undefined)
**Test**: `.sandbox/test_dyn.tml` — "use of undefined value '%v1'"
**Root cause**: vtable dispatch codegen emits undefined value references

- [ ] 2.1 Reproduce with emit-ir, trace vtable call generation
- [ ] 2.2 Fix vtable method dispatch to properly load fn ptr from vtable
- [ ] 2.3 Verify: basic dyn Behavior dispatch works (Dog/Cat example)
- [ ] 2.4 Verify: ref dyn Error with source() chain works

**Blocks**: error chaining, middleware chains, plugin systems, any polymorphic dispatch

## Gap 3: async/await — CODEGEN TYPE MISMATCH

**Status**: Lexer ✅ Parser ✅ Typechecker ✅ Codegen ❌ IR type mismatch (i64 vs i32)
**Test**: `.sandbox/test_async_await.tml` — "defined with type 'i64' but expected 'i32'"
**Note**: `async func` without `.await` compiles fine. Crash is in the await state machine codegen.

- [ ] 3.1 Reproduce with emit-ir, find the i64/i32 mismatch in state machine lowering
- [ ] 3.2 Fix type propagation in async state machine transformation
- [ ] 3.3 Verify: async func + .await chain works end-to-end
- [ ] 3.4 Verify: async with I32, I64, Str return types

**Blocks**: idiomatic async I/O (HTTP server, database clients, file I/O)

## RESOLVED: Template literals — WORKS ✅

**Status**: Fully functional! `\`Hello, {name}!\`` compiles and runs correctly.
**Test**: `.sandbox/test_template.tml` — outputs "Hello, World!" and "The answer is 42"
**Note**: Returns Text type, not Str. Use `greeting.as_str()` to convert.
**Was not used in HTTP code** — should be used for response building.

## NOT gaps (exist and work, weren't used in HTTP code)

- Buffer (std/collections/buffer.tml) — byte buffer with get/set/read/write
- Text (std/text.tml) — mutable string builder with push_str/push_i64
- Template literals `` `{expr}` `` — string interpolation, returns Text
- HashMap[K,V] — key-value map with set/get/has
- List[T] — dynamic array with push/get/len
- Slice[T]/MutSlice[T] — zero-copy views
- TcpStream/TcpListener — typed socket abstractions
- Outcome[T,E] with ! operator — error handling
- Mutex[T]/RwLock[T]/Once/RefCell — thread-safe mutable state
- str::* — 58 string functions
- ReadableStream/WritableStream — streaming abstractions
- Display/Debug behaviors — string conversion
