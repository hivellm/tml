# Tasks: Panic Recovery — catch_unwind and Panic Hooks

**Status**: Proposed
**Priority**: HIGH
**Phase**: 1 — Foundation

## Motivation

A panic in TML kills the entire process. For servers, this means one bad request crashes everything. Rust provides `catch_unwind` to recover from panics, and `set_hook` to customize panic behavior (logging, alerting). TML has C#-style exceptions but no panic recovery mechanism.

## Phase 1: Panic Hook Infrastructure (`lib/core/src/panic/`)

- [ ] 1.1 Create `lib/core/src/panic/mod.tml`
- [ ] 1.2 Define `PanicInfo` struct: `message: Str`, `file: Str`, `line: I64`, `column: I64`
- [ ] 1.3 Define `PanicHook` type alias: `func(info: ref PanicInfo) -> Unit`
- [ ] 1.4 Implement global panic hook storage (thread-safe via atomic pointer)
- [ ] 1.5 `set_hook(hook: PanicHook)` — install custom panic handler
- [ ] 1.6 `take_hook() -> PanicHook` — remove and return current hook
- [ ] 1.7 Write tests for hook installation and invocation

## Phase 2: catch_unwind (`lib/std/src/panic.tml`)

- [ ] 2.1 Design catch_unwind mechanism — requires compiler support for setjmp/longjmp or landing pads
- [ ] 2.2 Implement `catch_unwind[T](f: func() -> T) -> Outcome[T, PanicInfo]`
- [ ] 2.3 Implement `resume_unwind(info: PanicInfo)` — re-panic after catching
- [ ] 2.4 Implement `AssertUnwindSafe[T]` wrapper — mark types as safe across unwind boundary
- [ ] 2.5 Add compiler support: emit landing pads / setjmp-longjmp for catch points
- [ ] 2.6 Write tests: catch and recover, nested catch, re-panic

## Phase 3: Integration with HTTP Server

- [ ] 3.1 Wrap HTTP request handlers in `catch_unwind` — bad request doesn't crash server
- [ ] 3.2 Log panic info via panic hook when request handler panics
- [ ] 3.3 Return 500 Internal Server Error on caught panic
- [ ] 3.4 Write integration test: panic in handler → 500 response → server continues
