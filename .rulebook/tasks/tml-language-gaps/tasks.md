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

## RESOLVED: Nullable Maybe[ref T] double-load crash — FIXED ✅

**Fix**: Removed spurious `load ptr, ptr %receiver` in Maybe[ref T] method dispatch.
When `Maybe[ref T]` uses nullable-pointer optimization (the ptr IS the Maybe value),
the codegen was loading through the pointer as if it pointed to another pointer, causing
ACCESS_VIOLATION (0xC0000005) at runtime.
**File**: `compiler/src/codegen/llvm/expr/method.cpp:973-983`

- [x] Diagnosed: `arr.get(1).is_just()` generates double-load on nullable ptr
- [x] Fix: Remove the extra load for `enum_type_name == "ptr"` case
- [x] Verify: core/array 20/20, core/slice 21/21, core/cell 27/27, core/iter 52/52, core/types 7/7
- [x] Verify: core/option 23/25 (2 pre-existing IR bugs), core/result 17/19 (2 pre-existing)

## RESOLVED: Maybe[mut ref T] when-pattern dangling pointer — FIXED ✅

**Fix**: When-pattern bindings for enum payloads now alias the payload pointer directly
instead of load+copy to a local alloca. This ensures `mut ref val` / `ref val` returns
a pointer into the original struct, not a dangling pointer to a stack copy.
**File**: `compiler/src/codegen/llvm/control/when.cpp` (3 binding sites unified)

- [x] Diagnosed: `as_mut()` returns ptr to local alloca → ACCESS_VIOLATION after return
- [x] Fix: Alias payload_ptr for all enum pattern bindings (struct AND primitive types)
- [x] Verify: core/option/option_as_mut, core/alloc/shared_getmut, sync_getmut, array_get_mut
- [x] Verify: compiler tests 202/202, option 23/25 (2 pre-existing), result 17/19 (2 pre-existing)

## RESOLVED: dyn Behavior boxing/casting crashes — FIXED ✅

**Fix**: 5 bugs in AST codegen for `ref dyn Behavior` fat pointer types.
The initial dyn dispatch fix (dd013978) handled simple dispatch but crashed on:
boxing (`Box[dyn Error]`), downcasting, `Maybe[ref dyn Error]` returns, and `as ref dyn Error` casts.

- [x] Fix: `llvm_type_from_semantic(RefType{DynBehaviorType})` returns `%dyn.Name` not `ptr`
- [x] Fix: `llvm_type(parser::RefType{DynType})` returns fat pointer not `ptr`
- [x] Fix: `calc_type_size` handles `%dyn.*` types as 16 bytes
- [x] Fix: `emit_store` uses `zeroinitializer` for struct/aggregate types (not `0`)
- [x] Fix: `gen_cast` handles `ptr -> %dyn.BehaviorName` (creates `{data_ptr, vtable_ptr}`)
- [x] Fix: when expression result alloca uses `[4 x i64]` (32 bytes) for fat pointer support
- [x] Fix: loop variable zero-init uses `zeroinitializer` for aggregate types
- [x] Verify: boxed_error_new, any_downcast, json_casts, lines_iter_advanced — all pass
- [x] Verify: error_chain_coverage (was crashing) — now passes
- [x] Verify: core/error 35/35, core/any 11/11, core/iter 52/52, std/file 12/12, std/collections 70/70
- [x] No regressions in core/str 22/22, core/num 50/50, core/option 23/25 (2 pre-existing)

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
