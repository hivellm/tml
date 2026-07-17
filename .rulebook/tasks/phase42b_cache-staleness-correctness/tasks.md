## 1. Implementation
- [ ] 1.1 Write repro scripts FIRST (in `.sandbox/`, then promoted to tests in 2.2) for each hole: (a) stale `.ast.bin` overriding an edited source; (b) warm daemon returning old diagnostics after editing an imported module; (c) warm daemon type-checking against pre-edit lib interfaces; (d) `cache invalidate <file>` removing nothing — confirm each reproduces (F-029 needs runtime confirmation, audit confidence was Medium)
- [ ] 1.2 F-027 `.ast.bin`: store + validate source content-hash before use (`query_context.cpp:134-173`); fall through to real parse on mismatch; record the ReadSource dep either way; stop re-deserializing on every call (respect the in-memory cache)
- [ ] 1.3 F-028 daemon result cache: snapshot the transitive source set (reuse `collect_transitive_source_files`, `query_context.cpp:507-558`, or `TypeEnv::loaded_source_files`; lib-tree max-mtime probe as fallback) instead of argv-only `.tml` mtimes (`cmd_daemon.cpp:329-347`)
- [ ] 1.4 F-029 daemon meta epoch: per-request cheap lib-change probe (max mtime of `lib/*/src` + meta dir); on change drop `GlobalModuleCache`, re-run preload, clear result cache (convert `call_once` to validated epoch, `module_binary_read.cpp:1371-1429`)
- [ ] 1.5 F-030 real `cache invalidate`: map a source file to its artifacts across ALL cache layers (test EXEs + tests.json rewrite, incr entries + ir/ files, obj_cache, `.ast.bin`) and delete them, reporting exactly what was removed; extend `cache info`/`clean` beyond `cache/run` (`cmd_cache.cpp:50-53,364-524`)
- [ ] 1.6 Rebuild + run all four repro scripts: each hole now closed (new diagnostics appear without daemon restart / cache nuking); representative suites result-identical
- [ ] 1.7 GATE: all repro scripts pass post-fix and fail pre-fix (documented); `cache invalidate <file>` verifiably removes ≥1 artifact of each class it claims; zero result divergence on representative suites

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation: `docs/analysis/incremental-cache/02-findings.md` statuses (F-027..F-030); 05-mcp-warm-state.md residual-caveats section; CHANGELOG/VERSION bump
- [ ] 2.2 Write tests covering the new behavior (promote the four repro scripts to `compiler/tests/cli/`)
- [ ] 2.3 Run tests and confirm they pass
