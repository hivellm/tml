# MCP Documentation Complete Coverage — Tasks

## Phase 0: Fix Extractor Bug (CRITICAL — blocks all other phases)

### 0.1 Fix top-level function indexing
- [x] 0.1.1 Investigate why top-level `pub func` items with `///` doc comments don't appear in `docs_search` results — ROOT CAUSE: `skip_newlines()` consumed DocComment tokens between declarations
- [x] 0.1.2 Fix: `skip_newlines()` now only skips Newline tokens. DocComment tokens preserved for `collect_doc_comment()`. Also added propagation from top-level funcs to impl methods.
- [x] 0.1.3 Verified: `docs_get("core::str::len")` returns description "Returns the length of a string in bytes" + examples
- [x] 0.1.4 Verified: `docs_get("core::str::split")` returns description + examples

### 0.2 Add doc comments to impl methods (where top-level has docs)
- [x] 0.2.1 Not needed — extractor now propagates docs from top-level functions to impl methods automatically (`propagate_docs_to_impl_methods`)
- [x] 0.2.2 Not needed — automatic propagation handles this
- [x] 0.2.3 Verified: 11866 items indexed, descriptions + examples appear in docs_get output

## Phase 1: Core Library Doc Comments (highest impact)

### 1.1 Core Types (most used, highest ROI)
- [ ] 1.1.1 `lib/core/src/str.tml` — Str methods: split, contains, starts_with, trim, replace, len, etc.
- [ ] 1.1.2 `lib/core/src/option.tml` — Maybe[T]: map, and_then, unwrap, unwrap_or, is_just, is_nothing
- [ ] 1.1.3 `lib/core/src/result.tml` — Outcome[T,E]: map, and_then, unwrap, is_ok, is_err, map_err
- [ ] 1.1.4 `lib/core/src/fmt/mod.tml` — Display, Debug, Formatter, write!, format!
- [ ] 1.1.5 `lib/core/src/iter/mod.tml` — Iterator behavior: next, map, filter, fold, collect, enumerate
- [ ] 1.1.6 `lib/core/src/ops/arith.tml` — Add, Sub, Mul, Div, Rem operators
- [ ] 1.1.7 `lib/core/src/clone.tml` — Clone, Duplicate behaviors
- [ ] 1.1.8 `lib/core/src/cmp.tml` — Eq, PartialEq, Ord, PartialOrd, Ordering
- [ ] 1.1.9 `lib/core/src/convert.tml` — From, Into, TryFrom, TryInto
- [ ] 1.1.10 `lib/core/src/default.tml` — Default behavior

### 1.2 Core Collections & Memory
- [ ] 1.2.1 `lib/core/src/slice.tml` — Slice[T]: get, len, iter, split_at, contains
- [ ] 1.2.2 `lib/core/src/alloc/heap.tml` — Heap[T]: new, get, set, into_inner
- [ ] 1.2.3 `lib/core/src/alloc/shared.tml` — Shared[T]: new, get, strong_count
- [ ] 1.2.4 `lib/core/src/alloc/sync.tml` — Sync[T]: new, get, strong_count
- [ ] 1.2.5 `lib/core/src/cell.tml` — Cell[T], RefCell[T]: get, set, borrow, borrow_mut
- [ ] 1.2.6 `lib/core/src/pin.tml` — Pin[T]: new, get_ref, get_mut
- [ ] 1.2.7 `lib/core/src/ptr.tml` — Ptr[T], NonNull[T]: read, write, offset, null

### 1.3 Core Numeric & Char
- [ ] 1.3.1 `lib/core/src/num/integer.tml` — I8..I64, U8..U64: abs, pow, min, max, clamp
- [ ] 1.3.2 `lib/core/src/num/float.tml` — F32, F64: floor, ceil, round, sqrt, sin, cos
- [ ] 1.3.3 `lib/core/src/num/nonzero.tml` — NonZeroI32, etc.
- [ ] 1.3.4 `lib/core/src/char.tml` — Char: is_alphabetic, is_digit, to_lowercase, to_uppercase

## Phase 2: Std Library Doc Comments

### 2.1 Collections
- [ ] 2.1.1 `lib/std/src/collections/hashmap.tml` — HashMap[K,V]: new, insert, get, remove, contains_key, iter
- [ ] 2.1.2 `lib/std/src/collections/list.tml` — List[T]: new, push, pop, get, len, iter, sort
- [ ] 2.1.3 `lib/std/src/collections/buffer.tml` — Buffer: new, write_u8, read_u8, len, as_slice
- [ ] 2.1.4 `lib/std/src/collections/btreemap.tml` — BTreeMap[K,V]
- [ ] 2.1.5 `lib/std/src/collections/deque.tml` — Deque[T]: push_front, push_back, pop_front
- [ ] 2.1.6 `lib/std/src/collections/heap.tml` — BinaryHeap[T]: push, pop, peek

### 2.2 Sync & Concurrency
- [ ] 2.2.1 `lib/std/src/sync/mutex.tml` — Mutex[T]: new, lock, try_lock, is_locked
- [ ] 2.2.2 `lib/std/src/sync/rwlock.tml` — RwLock[T]: read, write, try_read, try_write
- [ ] 2.2.3 `lib/std/src/sync/arc.tml` — Arc[T]: new, strong_count, downgrade
- [ ] 2.2.4 `lib/std/src/sync/atomic.tml` — AtomicI64, etc.: load, store, fetch_add, compare_exchange
- [ ] 2.2.5 `lib/std/src/sync/condvar.tml` — Condvar: wait, notify_one, notify_all
- [ ] 2.2.6 `lib/std/src/sync/mpsc.tml` — channel, Sender, Receiver: send, recv, try_recv
- [ ] 2.2.7 `lib/std/src/sync/barrier.tml` — Barrier: new, wait
- [ ] 2.2.8 `lib/std/src/sync/once.tml` — Once, OnceLock: call_once, get_or_init

### 2.3 I/O & Networking
- [ ] 2.3.1 `lib/std/src/io/file.tml` — File: open, read, write, close
- [ ] 2.3.2 `lib/std/src/io/bufio.tml` — BufReader, BufWriter
- [ ] 2.3.3 `lib/std/src/net/tcp.tml` — TcpListener, TcpStream: bind, connect, accept, read, write
- [ ] 2.3.4 `lib/std/src/net/udp.tml` — UdpSocket: bind, send_to, recv_from
- [ ] 2.3.5 `lib/std/src/net/dns.tml` — resolve, lookup
- [ ] 2.3.6 `lib/std/src/net/ip.tml` — IpAddr, SocketAddr

### 2.4 JSON & Serialization
- [ ] 2.4.1 `lib/std/src/json/mod.tml` — JsonValue: parse, to_string, get, as_str, as_i64
- [ ] 2.4.2 `lib/std/src/json/serialize.tml` — ToJson, FromJson behaviors
- [ ] 2.4.3 `lib/std/src/json/parser.tml` — parse details

### 2.5 Other Std Modules
- [ ] 2.5.1 `lib/std/src/crypto/` — hash, hmac, cipher, sign modules
- [ ] 2.5.2 `lib/std/src/time/` — Instant, Duration, SystemTime
- [ ] 2.5.3 `lib/std/src/path/` — Path, PathBuf
- [ ] 2.5.4 `lib/std/src/regex/` — Regex: new, is_match, find, captures
- [ ] 2.5.5 `lib/std/src/thread/` — spawn, sleep, yield_now, JoinHandle
- [ ] 2.5.6 `lib/std/src/os/` — env, signal, subprocess

## Phase 3: Example Generation from Tests

- [ ] 3.1 Build script to extract `@test` functions as `@example` blocks
- [ ] 3.2 Run script on core/ tests → inject examples into core/ doc comments
- [ ] 3.3 Run script on std/ tests → inject examples into std/ doc comments
- [ ] 3.4 Manual review of generated examples for top 50 types

## Phase 4: Cross-References & Metadata

- [ ] 4.1 Add `@see` cross-references between related types (Maybe↔Outcome, Mutex↔RwLock, etc.)
- [ ] 4.2 Add `@since` version tags (0.1.0 for core, 0.2.0 for recent additions)
- [ ] 4.3 Add `@deprecated` tags for functions with known codegen bugs
- [ ] 4.4 Add category prefixes in summaries: `[Thread-safe]`, `[Pure TML]`, `[FFI]`, `[Iterator]`

## Phase 5: MCP Search Quality

- [ ] 5.1 Verify `docs_search` returns descriptions (not just signatures) after Phase 1-2
- [ ] 5.2 Verify `docs_get` returns examples after Phase 3
- [ ] 5.3 Verify `docs_search` with category keywords finds tagged items after Phase 4
- [ ] 5.4 Measure doc coverage: % of public functions with doc comments
- [ ] 5.5 Target: 90%+ of public functions have `///` with description + `@param` + `@returns`
