# Tasks: Panic Recovery — catch_unwind and Panic Hooks

**Status**: 80% Complete (Phase 1-2 done, Phase 3 deferred)
**Priority**: HIGH
**Phase**: 1 — Foundation

## Motivation

A panic in TML kills the entire process. For servers, this means one bad request crashes everything. Rust provides `catch_unwind` to recover from panics, and `set_hook` to customize panic behavior (logging, alerting). TML has C#-style exceptions but no panic recovery mechanism.

## Phase 1: Panic Hook Infrastructure (`lib/core/src/panic/`)

- [x] 1.1 Create `lib/core/src/panic/mod.tml`
- [x] 1.2 Define `PanicInfo` struct: `message: Str`, `file: Str`, `line: I64`, `column: I64`
- [x] 1.3 `set_hook(hook: *U8)` — install custom panic handler (function pointer to `func(Str)->Unit`)
- [x] 1.4 Implement global panic hook storage — C runtime `tml_panic_hook` static pointer
- [x] 1.5 `clear_hook()` — remove current hook
- [x] 1.6 Hook called in `panic()` before longjmp/exit — verified working
- [x] 1.7 Tests: `test_panic_info_new`, `test_panic_info_with_location` passing

## Phase 2: catch_unwind (`lib/core/src/panic/mod.tml`)

- [x] 2.1 Implemented via setjmp/longjmp — reuses existing C runtime infrastructure
- [x] 2.2 `catch_unwind_fn(fn_ptr: *U8) -> CatchResult` — catches panics, returns Ok or Panicked(msg)
- [x] 2.3 `CatchResult` enum: `Ok` | `Panicked(Str)` — clean API for panic recovery
- [x] 2.4 C runtime: `tml_catch_unwind_fn()` with nested jmp_buf save/restore
- [x] 2.5 Tests: `test_catch_panic`, `test_catch_no_panic`, `test_catch_then_continue` — all passing
- [ ] 2.6 `resume_unwind(msg: Str)` — re-panic after catching (trivial: just call `panic()`)
- [ ] 2.7 `AssertUnwindSafe[T]` wrapper — deferred (needs generic closure support)

## Phase 3: Integration with HTTP Server — DEFERRED

- [ ] 3.1 Wrap HTTP request handlers in `catch_unwind` — bad request doesn't crash server
- [ ] 3.2 Log panic info via panic hook when request handler panics
- [ ] 3.3 Return 500 Internal Server Error on caught panic
- [ ] 3.4 Write integration test: panic in handler → 500 response → server continues

NOTE: Phase 3 deferred until HTTP performance regression (phase3-01) is resolved.

## Known Limitations

1. `catch_unwind_fn` takes `*U8` (plain function pointer) — closures with captures not supported yet due to `{ ptr, ptr }` struct casting codegen bug
2. `set_hook` also takes `*U8` — same reason
3. Hook is process-global (not thread-local) — acceptable for now
