# 3. Pure C ABI for Plugin Interface

**Status**: proposed
**Date**: 2026-03-15

## Context

Modular build produces thin launcher + plugin DLLs. Need a stable ABI across compiler versions and potentially different C++ compilers (MSVC, Zig CC/Clang).

## Decision

Use pure C types in plugin/abi.h with three mandatory exports: plugin_query(), plugin_init(void*), plugin_shutdown(). Plugins declare capabilities via NULL-terminated string arrays (CAP_PARSE, CAP_CODEGEN_IR, CAP_TARGET_X86). Host context passed as void*.

## Alternatives Considered

- C++ vtable-based interface (not ABI-stable across compilers)
- Protocol buffers over IPC (too complex for in-process plugins)
- COM/XPCOM interfaces (Windows-specific, heavyweight)

## Consequences

Pros: ABI-stable across MSVC and Clang, supports future plugins (Cranelift, GPU targets, remote compilation). Cons: No C++ type safety across boundaries; void* host context requires out-of-band knowledge. Plugin compressed with .zst for distribution.
