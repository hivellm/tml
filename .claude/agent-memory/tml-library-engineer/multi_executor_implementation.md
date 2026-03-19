---
name: Multi-threaded Executor Implementation
description: Sprint 5 multi-executor design decisions and workarounds
type: project
---

## Implementation: `lib/std/src/runtime/multi_executor.tml`

**Architecture**: Global shared queue (mutex-protected) + N worker OS threads.
Workers pull tasks from queue, execute them, update counters.

**Key design decisions:**

1. **No global `var`** — TML doesn't support module-scope `var`. Used raw thread spawn
   with per-worker heap-allocated context blocks instead of a global launch slot.

2. **Raw pointer mutex FFI** — `mut ref RawMutex` from heap pointer cast causes SEGFAULT.
   Redeclared mutex FFI functions to take `*Unit` instead. Works because C ABI just needs
   a void pointer to the mutex memory.

3. **No `mut this` methods** — Avoided `mut this` methods on MultiExecutor due to codegen
   bugs with field assignment through owned mutable self. Used free functions
   (`start_workers`, `wait_for_all`, etc.) that take raw I64 pointers.

4. **TaskQueue as raw memory** — No TML struct for TaskQueue. Operated entirely through
   raw pointer functions (`tq_init`, `tq_push`, `tq_pop`, `tq_destroy`) with explicit
   offset constants. Avoids struct layout codegen issues.

5. **Task type**: `func(I64)` — single I64 arg can encode a pointer to any user data.
   Avoids generic closure limitations.

**Tests**: 3 files, 7 tests total
- `multi_exec_basic.test.tml` — create/destroy, single task, multiple tasks
- `multi_exec_shutdown.test.tml` — empty executor run, post-completion state
- `multi_exec_counter.test.tml` — counter with 1 worker, counter with 4 workers

**How to apply:** When extending the executor (e.g., per-worker local queues, work stealing),
continue using raw pointer patterns and free functions. Avoid `mut this` and `mut ref` from
heap pointers.
