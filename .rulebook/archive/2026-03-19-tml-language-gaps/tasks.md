# TML Language Gaps — Verified with real compilation tests (2026-03-19)

Only items where the feature crashes or generates invalid IR.
NOT included: things that exist and work but weren't used in HTTP code.

## Gap 1: Bool/i1 struct through fn pointers — FIXED ✅

**Fix**: Added sret convention to indirect (fn pointer) calls returning structs.
Direct calls used sret but indirect calls skipped it, causing calling convention mismatch.
**Commit**: d5939bec

- [x] 1.1 Reproduce with emit-ir — found sret missing in indirect calls
- [x] 1.2 Fix: emit sret alloca + ptr passing for indirect calls returning structs
- [x] 1.3 Verify: .sandbox/test_bool_fnptr.tml exits 0 (was SEGFAULT)
- [x] 1.4 Unblocks ServerResponse hooks/middleware

## Gap 2: dyn trait objects — FIXED ✅

**Fix**: Full vtable dispatch across 5 compiler layers (HIR, type checker, THIR, MIR, codegen).
Also fixed UB in DCE pass (inserting into unordered_set during iteration → worklist pattern).
**Commit**: dd013978

- [x] 2.1 Fixed HIR builder: resolve parser::DynType → DynBehaviorType
- [x] 2.2 Fixed type checker: unwrap RefType before DynBehaviorType check
- [x] 2.3 Fixed MIR: MirDynType, MakeDynObjectInst, vtable emission, dyn dispatch
- [x] 2.4 Fixed DCE: worklist-based propagate_liveness (was UB → infinite loop)
- [x] 2.5 Verify: .sandbox/test_dyn.tml outputs "Woof!" via dyn dispatch

**Blocks**: error chaining, middleware chains, plugin systems, any polymorphic dispatch

## RESOLVED: async/await — FIXED ✅

**Status**: Lexer ✅ Parser ✅ Typechecker ✅ Codegen ✅
**Test**: `.sandbox/test_async_await.tml` — outputs 42, exits 0
**Root cause**: Stale incremental cache served old IR (pre-trunc fix). `compiler_build_hash()` used `__DATE__`/`__TIME__` of one file, not actual binary mtime. Also: MIR codegen had no AwaitInst handler (silently ignored).

- [x] 3.1 Reproduce with emit-ir, find the i64/i32 mismatch in state machine lowering
- [x] 3.2 Fix: `compiler_build_hash()` now uses binary mtime for cache invalidation
- [x] 3.3 Fix: Added AwaitInst handler to MIR codegen (instructions.cpp)
- [x] 3.4 Verify: async func + .await chain works end-to-end (tml run, tml build --emit-ir)
- [x] 3.5 Verify: existing async tests pass (async_function.test.tml, async_function_types.test.tml)

## RESOLVED: Template literals — WORKS ✅

**Status**: Fully functional! `\`Hello, {name}!\`` compiles and runs correctly.
**Test**: `.sandbox/test_template.tml` — outputs "Hello, World!" and "The answer is 42"
**Note**: Returns Text type, not Str. Use `greeting.as_str()` to convert.
**Was not used in HTTP code** — should be used for response building.

## Gap 4: thread::Builder::spawn — FIXED ✅

**Fix**: Implemented trampoline pattern for thread spawning. The C FFI `tml_thread_spawn(func_ptr, arg, stack_size)` works; the blocker was passing TML functions across the thread boundary.
**Approach**: Type-specialized spawn functions (`spawn_fn`, `spawn_i64`) with non-generic `UnitJoinHandle`/`I64JoinHandle` types to work around generic method dispatch limitation. Each function uses a module-level entry wrapper that reads the user's function pointer from a heap-allocated state block, calls it, and stores the result for `join()` to retrieve.
**Tests**: `thread_spawn_basic.test.tml`, `thread_spawn_i64.test.tml`, `thread_spawn_compute.test.tml`, `thread_spawn_joinhandle.test.tml`, `spawn_blocking_basic.test.tml`

- [x] 4.1 Fix @extern declaration to match C ABI (*Unit params, U64 return)
- [x] 4.2 Implement thread_entry_unit and thread_entry_i64 wrappers
- [x] 4.3 Implement Builder::spawn_fn (func() -> Unit) with UnitJoinHandle
- [x] 4.4 Implement Builder::spawn_i64 (func() -> I64) with I64JoinHandle
- [x] 4.5 Implement UnitJoinHandle::join, is_finished, thread
- [x] 4.6 Implement I64JoinHandle::join, is_finished, thread
- [x] 4.7 Implement spawn_blocking / spawn_blocking_i64 convenience functions
- [x] 4.8 Tests: basic spawn, computation, return values, Builder options, double-join error
- [ ] 4.9 BLOCKED: Generic spawn[T] / JoinHandle[T] — compiler generic method dispatch bug
- [ ] 4.10 BLOCKED: Thread.name() access crashes (Maybe[Str] codegen from struct)

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
