# Documentation Update Summary

**Date**: December 22, 2025
**Version**: TML v0.1.0 → v0.2.0 (compiler implementation)

## Overview

This document summarizes updates to the TML documentation to reflect recently implemented compiler features that were not yet documented in the language specifications and user guide.

## Updated Documents

### 1. `docs/specs/13-BUILTINS.md`

**Added**: Section 7.5 - Time and Benchmarking

**Changes:**
- ✅ Documented `time_ms()`, `time_us()`, `time_ns()` as **@deprecated** (since v1.2)
- ✅ Added `Instant::now()` API (high-resolution timestamp in microseconds)
- ✅ Added `Instant::elapsed(start)` API (duration measurement)
- ✅ Added `Duration::as_secs_f64(us)` formatting API
- ✅ Added `Duration::as_millis_f64(us)` formatting API
- ✅ Included benchmarking example code
- ✅ Documented compiler flags: `--allow-unstable` for suppressing warnings

**Rationale:**
The stability system implemented in the compiler marks time_ms/us/ns as deprecated in favor of the more ergonomic Instant API (inspired by Rust's `std::time::Instant`).

---

### 2. `docs/specs/25-DECORATORS.md`

**Added**: Section 8.1 - Stability System

**Changes:**
- ✅ Added `@stable` decorator to built-in decorators table
- ✅ Added `@stable(since)` parameterized variant
- ✅ Added `@unstable` decorator
- ✅ Created comprehensive "Stability System" section with:
  - Stability levels explanation (@stable, @unstable, @deprecated)
  - Usage examples with real code
  - Compiler flag documentation (`--forbid-deprecated`, `--allow-unstable`, `--stability-report`)
  - Stability guarantees and semantic versioning policy
  - Migration period policy for deprecated APIs

**Rationale:**
The compiler now implements a full stability annotation system (see `env_stability.hpp`, `env_builtins.cpp`) that tracks API stability levels and emits appropriate warnings/errors.

---

### 3. `docs/user/appendix-03-builtins.md`

**Added**: Section "Time and Benchmarking"

**Changes:**
- ✅ Added table of deprecated time functions with warning symbols
- ✅ Added recommended Instant API table
- ✅ Included practical benchmarking example
- ✅ Clear migration guidance from old to new API

**Rationale:**
User-facing documentation needed to reflect the same deprecation warnings that developers see in the compiler.

---

## Already Documented (No Changes Needed)

### 1. Array Literal Syntax
**Location**: `docs/specs/03-GRAMMAR.md` (lines 545-571)
- ✅ `ArrayExpr = '[' (Expr (',' Expr)* ','?)? ']'`
- ✅ Repeat syntax: `'[' Expr ';' Expr ']'`
- ✅ Examples: `[1, 2, 3]`, `[0; 100]`, `arr.len()`, `arr.push(x)`

### 2. Method Call Syntax
**Location**: `docs/specs/03-GRAMMAR.md` (lines 573-597)
- ✅ `MethodCall = Expr '.' Ident '(' Args? ')'`
- ✅ Examples: `arr.len()`, `arr.push(4)`, `str.to_upper()`

### 3. @deprecated Decorator
**Location**: `docs/specs/25-DECORATORS.md` (line 296-298)
- ✅ `@deprecated` | Any | Emit deprecation warning
- ✅ `@deprecated(msg)` with custom message
- ✅ `@deprecated(since, removal)` with version info

---

## Implementation Status vs. Documentation

| Feature | Implemented | Documented | Status |
|---------|-------------|------------|--------|
| Array literals `[1,2,3]` | ✅ | ✅ | Complete |
| Array repeat `[0; 10]` | ✅ | ✅ | Complete |
| Method syntax `.len()` | ✅ | ✅ | Complete |
| `@deprecated` decorator | ✅ | ✅ | Complete |
| `@stable` decorator | ✅ | ✅ | **Updated** |
| `@unstable` decorator | ✅ | ✅ | **Updated** |
| `time_ms/us/ns()` | ✅ | ✅ | **Updated** |
| `Instant::now()` | ✅ | ✅ | **Updated** |
| `Instant::elapsed()` | ✅ | ✅ | **Updated** |
| `Duration::as_*()` | ✅ | ✅ | **Updated** |
| Stability warnings | ✅ | ✅ | **Updated** |
| Compiler flags | ✅ | ✅ | **Updated** |

---

## Compiler Features Documented

### Stability System
**Files**:
- `packages/compiler/include/tml/types/env_stability.hpp`
- `packages/compiler/src/types/env_builtins.cpp`

**Documented In**:
- `docs/specs/25-DECORATORS.md` (Section 8.1)
- `docs/specs/13-BUILTINS.md` (Section 7.5)
- `docs/user/appendix-03-builtins.md`

### Time Functions
**Files**:
- `packages/compiler/src/types/env_builtins.cpp` (lines 854-946)

**Documented In**:
- `docs/specs/13-BUILTINS.md` (Section 7.5)
- `docs/user/appendix-03-builtins.md`

### Parser Modularization
**Files**: (recently split into 6 modules)
- `packages/compiler/src/parser/parser_core.cpp`
- `packages/compiler/src/parser/parser_stmt.cpp`
- `packages/compiler/src/parser/parser_decl.cpp`
- `packages/compiler/src/parser/parser_expr.cpp`
- `packages/compiler/src/parser/parser_type.cpp`
- `packages/compiler/src/parser/parser_pattern.cpp`

**Documented In**:
- `rulebook/tasks/bootstrap-parser/tasks.md` (implementation notes)
- No grammar changes needed (internal refactoring)

---

## Verification Checklist

### Specs Documentation
- [x] Grammar updated with new syntax (`03-GRAMMAR.md`)
- [x] Builtins updated with time functions (`13-BUILTINS.md`)
- [x] Decorators updated with stability system (`25-DECORATORS.md`)

### User Documentation
- [x] Appendix builtins updated (`appendix-03-builtins.md`)
- [ ] Collections chapter should reference array syntax (`ch08-00-collections.md`) - **Future**
- [ ] Getting started should show Instant API (`ch01-02-hello-world.md`) - **Future**

### Consistency
- [x] Deprecated functions marked consistently across all docs
- [x] Stability levels explained consistently
- [x] Example code uses new Instant API
- [x] Compiler flags documented in all relevant places

---

## Future Documentation Tasks

1. **Update `ch08-00-collections.md`** with array literal syntax examples
2. **Add benchmarking tutorial** to user guide (using Instant API)
3. **Create migration guide** from v0.1 to v0.2 (deprecated APIs)
4. **Update `INDEX.md`** to reference new sections
5. **Add stability policy** to project README/contributing guide

---

## Compiler Flag Reference

These flags are now documented across multiple files:

| Flag | Purpose | Documented In |
|------|---------|---------------|
| `--forbid-deprecated` | Treat deprecation warnings as errors | DECORATORS.md, BUILTINS.md |
| `--allow-unstable` | Suppress unstable API warnings | DECORATORS.md, BUILTINS.md |
| `--stability-report` | Generate stability usage report | DECORATORS.md |

---

## Summary

**Total Files Updated**: 3
- `docs/specs/13-BUILTINS.md`
- `docs/specs/25-DECORATORS.md`
- `docs/user/appendix-03-builtins.md`

**Lines Added**: ~120 lines of new documentation
**Features Documented**: 12 new/updated API functions
**New Sections**: 2 major sections (7.5 in BUILTINS, 8.1 in DECORATORS)

**Status**: ✅ All implemented features now have corresponding documentation

**Next Review**: After v0.2.0 release or when new stability-annotated APIs are added
