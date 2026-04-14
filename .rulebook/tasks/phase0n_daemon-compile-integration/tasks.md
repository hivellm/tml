## 1. Diagnosis
- [ ] 1.1 Profile `tml build` startup: measure time spent in process init, DLL load, LLVM context init, and actual codegen separately
- [ ] 1.2 Research named-pipe IPC on Windows: read `CreateNamedPipe`/`ConnectNamedPipe` MSDN docs — identify the right pipe mode (byte vs message)
- [ ] 1.3 Check if `tml daemon` command exists already — grep `cmd_daemon` in `compiler/src/cmd/`

## 2. Implementation
- [ ] 2.1 Create `compiler/src/cmd/cmd_daemon.cpp`: implements `tml daemon start` — forks/spawns a background process, writes PID to `.tml-daemon.pid`, enters named-pipe listen loop
- [ ] 2.2 Daemon request protocol: JSON-lines over named pipe — `{"cmd":"build","files":["main.tml"],"options":{}}` → response `{"ok":true,"diagnostics":[],"elapsed_ms":5}`
- [ ] 2.3 In `cmd_build.cpp`: check for `.tml-daemon.pid`, if present and process alive, forward build request over pipe and print response
- [ ] 2.4 Add `tml daemon stop` and `tml daemon status` subcommands
- [ ] 2.5 Auto-shutdown: daemon tracks last-request timestamp, exits after 30 minutes idle

## 3. Benchmark Gate
- [ ] 3.1 Start daemon: `tml daemon start`
- [ ] 3.2 Run first compile (warm daemon): `tml build main.tml --stage=parser:cpp` — record wall time
- [ ] 3.3 Run second compile (incremental, no changes): record wall time
- [ ] 3.4 Compare vs `cargo check` (incremental, no changes) wall time on equivalent Rust project
- [ ] 3.5 GATE: Incremental no-change compile must be <10ms via daemon. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=compiler` via daemon — all tests pass
- [ ] 4.2 Verify daemon auto-restarts if DLL mtime changes (stale cache invalidation)
- [ ] 4.3 Verify non-daemon path still works when daemon is not running

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `feat(daemon): persistent compilation daemon for sub-10ms incremental builds`
- [ ] 5.2 Update `docs/analysis/benchmark/08-compilation.md` with daemon-mode timings
- [ ] 5.3 Run tests and confirm they pass
