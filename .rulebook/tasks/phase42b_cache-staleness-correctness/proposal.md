# Proposal: phase42b_cache-staleness-correctness

## Why
Three **under-invalidation holes** make the toolchain serve stale results until someone manually nukes a cache dir — and the manual tool is a stub (findings F-027..F-030 in `docs/analysis/incremental-cache/02-findings.md`):

- **F-027 (High, correctness):** the `.ast.bin` sidecar is trusted unconditionally — no source hash, no mtime check, bypasses dependency recording (`query_context.cpp:134-173`). A stale cached AST silently overrides an edited source file. Candidate contributor to the phase27c "flaky module-path corruption".
- **F-028 (Medium-High, correctness):** the daemon result cache snapshots mtimes of **argv `.tml` files only** (`cmd_daemon.cpp:329-347`) — edit an imported module and a warm `tml check main.tml` returns the *previous* diagnostics.
- **F-029 (Medium-High, correctness):** `preload_all_meta_caches` is `std::call_once` per process (`module_binary_read.cpp:1376-1379`); a daemon living up to 30 min serves type interfaces from before your lib edit — only a compiler-DLL change restarts it.
- **F-030 (Medium):** `tml cache invalidate` / `mcp__tml__cache_invalidate` is a stub: substring-matches stems against hash-named files (matches nothing), the "MIR cache" block deletes nothing, the tests.json block explicitly skips rewriting, and `cache/incr/`, `obj_cache/`, `.ast.bin` are never touched (`cmd_cache.cpp:364-524`). Its existence is a symptom of the three holes above; its implementation violates the no-stubs rule.

Stale-result bugs are worse than slow tools: they make users distrust every cache, which leads to manual full wipes — the exact "reprocess everything" waste we are eliminating.

## What Changes
- **`.ast.bin` validation:** store + check a source content-hash (same scheme as `.meta`) before use; on mismatch fall through to a real parse; register the ReadSource dependency either way.
- **Daemon result cache:** snapshot the *transitive* source set (query dep graph / `TypeEnv::loaded_source_files`; cheap lib-tree max-mtime probe as fallback) instead of argv-only args.
- **Daemon meta staleness:** convert `call_once` to a validated epoch — cheap per-request lib-tree change probe; on change, drop `GlobalModuleCache`, re-preload, clear the daemon result cache.
- **Real `cache invalidate`:** delete matching test EXEs + rewrite tests.json entries, drop the file's incr entries + IR files, delete `.ast.bin` sidecars, report exactly what was removed; extend `cache info`/`clean` to all cache dirs.

## Impact
- Affected specs: none
- Affected code: `compiler/src/query/query_context.cpp`, `compiler/src/cli/commands/cmd_daemon.cpp`, `cmd_cache.cpp`, `compiler/src/types/module_binary_read.cpp`, `compiler/src/mcp/mcp_tools_project.cpp`
- Breaking change: NO (strictly more correct; cache regenerates where stale)
- User benefit: no stale diagnostics/binaries, ever — edits to imported modules are seen by warm paths immediately; `cache invalidate` becomes trustworthy instead of decorative
