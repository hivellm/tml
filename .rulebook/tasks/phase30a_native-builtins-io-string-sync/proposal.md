# Proposal: phase30a_native-builtins-io-string-sync (renumbered from phase31a, 2026-07-15 ERA 0 pivot)

## Why
Real TML programs require I/O (reading files, writing stdout/stderr), string
manipulation (split, join, trim, format), synchronization primitives (Mutex,
RwLock, atomic operations), time queries, and process control (env vars, exit).
The native backend's `emit_intrinsic.tml` currently covers only arithmetic and
memory intrinsics. Every call to `File.read`, `Str.split`, `Mutex.lock`,
`Time.now`, or `Process.exit` falls through to the LLVM backend, making the
native path unusable for any program that interacts with the outside world.
Covering these five domains unblocks the vast majority of real-world TML programs.

## What Changes
- `compiler-tml/src/native/x86/emit_intrinsic.tml` gains five new dispatch groups:
  string operations (split, join, trim, replace, to_upper, to_lower, parse_i64,
  parse_f64) via libc `strchr`/`strstr`/`sprintf`; I/O operations (File.open,
  File.read, File.write, File.close, stdin_read_line) via POSIX `open`/`read`/
  `write`/`close`; sync operations (Mutex.new, Mutex.lock, Mutex.unlock,
  AtomicI64.fetch_add, AtomicI64.load, AtomicI64.store) via `pthread_mutex_*`
  and `__atomic_*` builtins; time (Time.now_ms, Time.sleep_ms) via
  `clock_gettime`/`nanosleep`; process (Process.exit, Process.env, Process.args)
  via `exit`/`getenv`/`argc`/`argv`.

## Impact
- Affected specs: native-backend/builtins
- Affected code: compiler-tml/src/native/x86/emit_intrinsic.tml
- Breaking change: NO
- User benefit: Programs using file I/O, string manipulation, concurrency primitives, time, and process control compile and run natively without the LLVM backend.
