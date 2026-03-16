---
name: runtime-lib-naming-fix
description: All runtime library finders in builder_helpers.cpp now use dual .lib/.a naming for Zig CC compatibility
type: project
---

## Runtime Library Dual Naming Fix (2026-03-16) -- FIXED

**Bug**: `get_runtime_objects()` in `builder_helpers.cpp` used `#ifdef _WIN32` to select either `.lib` or `.a`
filename for json, profiler, search runtimes. Since Zig CC produces `lib*.a` files even on Windows,
the `.lib` name never matched.

**Pattern**: The zlib runtime already had dual naming `{"tml_zlib_runtime.lib", "libtml_zlib_runtime.a"}`.
Applied the same pattern to:
- `find_json_runtime`: `tml_json_runtime.lib` + `libtml_json_runtime.a`
- `tml_json.lib` dependency: `tml_json.lib` + `libtml_json.a`
- `find_profiler_runtime`: `tml_profiler.lib` + `libtml_profiler.a`
- `tml_log.lib` dependency: `tml_log.lib` + `libtml_log.a`
- `find_search_runtime`: `tml_search_runtime.lib` + `libtml_search_runtime.a`
- `tml_search.lib` dependency: `tml_search.lib` + `libtml_search.a`

**Also fixed**: `testing_compile.cpp` path_module_map was missing entries for zlib, search, profiler.
Added both path-based and import-scanning heuristic for empty registry fallback.
Scans first 30 lines of test files for `use std::*` imports to handle tests in non-standard locations.

**Files**: `compiler/src/cli/builder/builder_helpers.cpp`, `compiler/src/testing/testing_compile.cpp`

**How to apply**: When adding new C runtime libraries, always use dual naming in the finder function.
