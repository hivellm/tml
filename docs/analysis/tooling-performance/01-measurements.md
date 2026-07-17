# 01 — Baseline Measurements

Measured 2026-07-17, warm OS cache.

| Metric | Value | Source |
|---|---|---|
| `tml --version` (process + DLL load, warm) | ~46–51 ms | measured |
| `tml check <trivial file>` (cold, no daemon) | ~461–466 ms | measured |
| `tml_compiler.dll` | **71.3 MB** (PDB 175 MB) | `build/debug/bin/plugins/` |
| `tml_codegen_x86.dll` | **51.9 MB** (PDB 47 MB) | same |
| `tml.exe` (thin launcher) | 1.34 MB | same |
| Cached test EXEs | **1339 files, 837 MB** | `build/debug/cache/tests/` |
| Per-test EXE size (each embeds stdlib) | ~345 KB | same |
| `obj_cache/` | 2755 objects, 273 MB | same |
| `tests.json` result cache | **618 bytes (≈empty)** | `build/debug/cache/tests.json` |
| Default build type | `debug` (`-O0`) | `scripts/build.bat:18` |

Prior-doc baselines (still directionally valid, `docs/analysis/benchmark/08-compilation.md`):

- Full test suite ~10 min (coverage).
- TML compile 27× Rust.
- Daemon cache-hit 22 ms vs cache-miss 7.7 s.
- Cold DLL load 2–3 s (the ~50 ms above is warm OS page cache — the cold penalty is real after reboot/rebuild).

**Key structural fact:** `tests.json` being ~empty means **the result cache is almost never populated**, so in practice most `tml test` invocations recompile from the obj-cache rather than skipping. Any compiler rebuild triggers `invalidate_all_exes()` (compiler hash = mtime:size of the 71 MB DLL), wiping the 837 MB EXE cache.

## phase40a baseline + warm-path results (2026-07-17)

Protocol: `subprocess` wall-clock timing from Python (`time.perf_counter` around
`tml.exe <cmd> .sandbox/phase40a_trivial.tml`, cwd = repo root), unchanged trivial
file, warm OS cache. Median over the listed n.

**Baseline (before fix, no daemon):**

| Command | n | Median | Range |
|---|---|---|---|
| `tml check` (cold subprocess) | 7 | **452.8 ms** | 422.7–580.4 ms |
| `tml build --emit-ir` (cold subprocess) | 5 | **842.9 ms** | 820.9–888.0 ms |

**Warm daemon path (after fix, `TML_DAEMON=1` subprocess — the exact path MCP now takes):**

| Command | n | Median | Range | vs baseline |
|---|---|---|---|---|
| `tml check` (daemon cache-hit) | 10 | **6.6 ms** | 6.2–25.3 ms | **69×** |
| `tml build --emit-ir` (daemon cache-hit) | 6 | **7.8 ms** | 6.7 ms–first-miss 585 ms | **108×** |

**Full MCP stack** (JSON-RPC request → `tml_mcp.exe` → subprocess → daemon → response,
measured by driving `tml_mcp.exe` over stdio): warm `check` = **26–31 ms** per call
(cold fallback ~405–465 ms only while the daemon (re)starts).

**GATE (task phase40a, item 1.6): `check` on unchanged file < 50 ms — PASS**
(6.6 ms CLI-level, ~28 ms through the whole MCP stack; baseline was ~453 ms).

First request after a daemon (re)start is a cache-miss and costs the normal
compile time (~405–620 ms observed) — it fills the daemon's mtime result cache;
every subsequent identical request on unchanged inputs is a cache-hit.
