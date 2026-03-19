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
