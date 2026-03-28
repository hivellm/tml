# Proposal: Panic Recovery — catch_unwind and Panic Hooks

## Status: PROPOSED

## Summary

A panic in TML currently kills the entire process. For long-running servers, this means a single malformed request or unexpected nil dereference takes down all connections. This task introduces two mechanisms borrowed from Rust: `set_hook` to intercept panics for logging and alerting before they unwind, and `catch_unwind` to recover from panics at well-defined boundaries (e.g., HTTP request handlers).

## Motivation

Production HTTP servers must not crash on bad input. Node.js, Go, and Java all provide domain/recover/catch mechanisms that let a server return a 500 response instead of dying. TML's HTTP server currently has no such boundary. A panic in any request handler takes down the whole server process.

Panic hooks provide observability: instead of getting a bare abort, operators can configure hooks that write structured logs, send alerts, or capture stack traces before the process terminates — or before `catch_unwind` swallows the panic.

## Design

**Phase 1 — Panic hooks** are implemented in `lib/core/src/panic/mod.tml` using an atomic global pointer to a function. `set_hook` performs an atomic swap; the compiler's existing panic codepath (in `essential.c`) calls the installed hook before aborting. `PanicInfo` carries file/line/column and message as TML `Str` values, making it safe to pass across the hook boundary.

**Phase 2 — catch_unwind** requires compiler support. Two implementation strategies:
1. `setjmp`/`longjmp` — simpler, works on all platforms, but skips destructors (Drop)
2. LLVM exception landing pads (`invoke`/`landingpad` instructions) — correct, runs Drop, matches Rust's model

The landing pad approach is preferred. The compiler must emit `invoke` instead of `call` for all calls inside a `catch_unwind` block, with a cleanup pad that captures the panic payload. `AssertUnwindSafe[T]` is a marker wrapper allowing types that are not automatically unwind-safe to cross the boundary with explicit programmer acknowledgment.

**Phase 3** wraps the HTTP request dispatch loop in `catch_unwind`, returning 500 on caught panics and logging via the installed hook.

## What Changes

- New: `lib/core/src/panic/mod.tml` — PanicInfo, PanicHook, set_hook, take_hook
- New: `lib/std/src/panic.tml` — catch_unwind, resume_unwind, AssertUnwindSafe
- Modified: `compiler/runtime/core/essential.c` — call installed hook before abort
- Modified: `compiler/src/codegen/` — emit `invoke`/`landingpad` for catch_unwind scopes
- Modified: `lib/std/src/http/dispatch.tml` — wrap handler calls in catch_unwind

## Dependencies

- Depends on: nothing for phase 1 (hooks are pure TML + C runtime)
- Depends on: codegen changes for phase 2 (landing pads require compiler work)
- Enables: resilient HTTP servers, test frameworks that catch expected panics

## Risks

- LLVM landing pad emission is significant codegen work; the `setjmp` path is a viable interim
- `catch_unwind` does not catch all panics — stack overflow and abort signals cannot be caught; documentation must set correct expectations
- Cross-`catch_unwind` boundary safety rules (UnwindSafe) need careful design to avoid data corruption when catching panics that leave shared state partially modified
