# Function-Level Gap Analysis: Rust vs TML

> Generated: 2026-03-28 | Detailed audit of missing functions per module

---

## 1. Maybe[T] (Rust: Option\<T\>)

**Coverage: ~90%** — 26 methods implemented, 6 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `is_some_and` | `fn is_some_and(self, f: F) -> bool` | Medium |
| `unzip` | `fn unzip(self) -> (Option<A>, Option<B>)` | Low |
| `get_or_insert` | `fn get_or_insert(&mut self, val: T) -> &mut T` | Medium |
| `get_or_insert_with` | `fn get_or_insert_with(&mut self, f: F) -> &mut T` | Medium |
| `replace` | `fn replace(&mut self, val: T) -> Option<T>` | Low |
| `cloned` / `copied` | `fn cloned(self) -> Option<T>` | Low |

**Note:** TML renames `is_some`→`is_just`, `is_none`→`is_nothing`, `or`→`alt`, `and`→`also`. These are naming differences, not missing functionality.

---

## 2. Outcome[T,E] (Rust: Result\<T,E\>)

**Coverage: ~100%** — All Rust Result methods present (with TML naming)

| Missing Function | Notes |
|------------------|-------|
| (none) | Full parity. TML adds extras: `alt`, `also`, `duplicated`, `iter` |

---

## 3. Str (Rust: str)

**Coverage: ~65%** — 30 methods implemented, 16 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `splitn` / `rsplitn` | `fn splitn(&self, n: usize, pat: P) -> SplitN` | High |
| `rsplit` | `fn rsplit(&self, pat: P) -> RSplit` | Medium |
| `split_once` / `rsplit_once` | `fn split_once(&self, delim: P) -> Option<(&str, &str)>` | High |
| `trim_matches` | `fn trim_matches(&self, pat: P) -> &str` | Medium |
| `strip_prefix` / `strip_suffix` | `fn strip_prefix(&self, prefix: P) -> Option<&str>` | High |
| `matches` / `rmatches` | `fn matches(&self, pat: P) -> Matches` | Medium |
| `replacen` | `fn replacen(&self, pat: P, to: &str, count: usize) -> String` | Medium |
| `char_indices` | `fn char_indices(&self) -> CharIndices` | Low |
| `bytes` | `fn bytes(&self) -> Bytes` | Low |
| `is_ascii` | `fn is_ascii(&self) -> bool` | Medium |
| `eq_ignore_ascii_case` | `fn eq_ignore_ascii_case(&self, other: &str) -> bool` | Medium |
| `encode_utf8` / `encode_utf16` | `fn encode_utf8(&self, dst: &mut [u8]) -> &str` | Low |
| `make_ascii_uppercase` | in-place mutation | Low |
| `make_ascii_lowercase` | in-place mutation | Low |
| `parse[T]` (generic) | `fn parse<F: FromStr>(&self) -> Result<F, F::Err>` | High |

**Note:** TML has `parse_i32`, `parse_i64`, `parse_f64`, `parse_bool` — type-specific but no generic `parse[T]`.

---

## 4. List[T] (Rust: Vec\<T\>)

**Coverage: ~35%** — 16 methods implemented, 28 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `insert` | `fn insert(&mut self, index: usize, element: T)` | **Critical** |
| `remove` | `fn remove(&mut self, index: usize) -> T` | **Critical** |
| `contains` | `fn contains(&self, x: &T) -> bool` | **Critical** |
| `sort` / `sort_by` | `fn sort(&mut self) where T: Ord` | **Critical** |
| `reverse` | `fn reverse(&mut self)` | **Critical** |
| `swap` | `fn swap(&mut self, a: usize, b: usize)` | High |
| `swap_remove` | `fn swap_remove(&mut self, index: usize) -> T` | High |
| `binary_search` | `fn binary_search(&self, x: &T) -> Result<usize, usize>` | High |
| `iter` | `fn iter(&self) -> Iter<T>` | High |
| `extend` | `fn extend<I: IntoIterator<Item=T>>(&mut self, iter: I)` | High |
| `reserve` | `fn reserve(&mut self, additional: usize)` | Medium |
| `shrink_to_fit` | `fn shrink_to_fit(&mut self)` | Medium |
| `truncate` | `fn truncate(&mut self, len: usize)` | Medium |
| `dedup` | `fn dedup(&mut self) where T: PartialEq` | Medium |
| `windows` | `fn windows(&self, size: usize) -> Windows<T>` | Medium |
| `chunks` | `fn chunks(&self, chunk_size: usize) -> Chunks<T>` | Medium |
| `split_at` | `fn split_at(&self, mid: usize) -> (&[T], &[T])` | Medium |
| `resize` | `fn resize(&mut self, new_len: usize, value: T)` | Medium |
| `fill` | `fn fill(&mut self, value: T) where T: Clone` | Low |
| `flatten` | `Vec<Vec<T>> -> Vec<T>` | Low |
| `sort_unstable` | `fn sort_unstable(&mut self)` | Low |
| `sort_by_key` | `fn sort_by_key<K, F>(&mut self, f: F)` | Low |
| `dedup_by` / `dedup_by_key` | custom deduplicate | Low |
| `split_off` | `fn split_off(&mut self, at: usize) -> Vec<T>` | Low |
| `splice` | `fn splice<R, I>(&mut self, range: R, replace_with: I)` | Low |
| `repeat` | repeat into new Vec | Low |

**Note:** List[T] is C-backed (via runtime). Adding methods requires either C runtime changes or pure TML wrappers. This is the **biggest gap** in the TML stdlib.

---

## 5. HashMap[K,V]

**Coverage: ~50%** — 10 methods implemented, 14 missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `is_empty` | `fn is_empty(&self) -> bool` | **Critical** |
| `entry` API | `fn entry(&mut self, key: K) -> Entry<K, V>` | **Critical** |
| `keys` | `fn keys(&self) -> Keys<K, V>` | High |
| `values` | `fn values(&self) -> Values<K, V>` | High |
| `contains_key` | `fn contains_key<Q>(&self, k: &Q) -> bool` | High (TML has `has`) |
| `retain` | `fn retain<F>(&mut self, f: F)` | Medium |
| `drain` | `fn drain(&mut self) -> Drain<K, V>` | Medium |
| `extend` | `fn extend<I: IntoIterator<Item=(K,V)>>(&mut self, iter: I)` | Medium |
| `get_or_insert_with` | via Entry API | Medium |
| `capacity` | `fn capacity(&self) -> usize` | Low |
| `reserve` | `fn reserve(&mut self, additional: usize)` | Low |
| `shrink_to_fit` | `fn shrink_to_fit(&mut self)` | Low |
| `get_mut` | `fn get_mut(&mut self, k: &K) -> Option<&mut V>` | Low (value semantics) |
| `iter_mut` | `fn iter_mut(&mut self) -> IterMut<K, V>` | Low (value semantics) |

**Note:** `contains_key` exists as `has()` and `insert` exists as `set()` — naming differences. The Entry API is the biggest missing feature.

---

## 6. Iterator Adapters

**Coverage: ~85%** — Extensive adapter coverage, few missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `collect` | `fn collect<B: FromIterator>(self) -> B` | **Critical** |
| `max` / `min` | `fn max(self) -> Option<T> where T: Ord` | High |
| `max_by_key` / `min_by_key` | `fn max_by_key<B, F>(self, f: F) -> Option<T>` | Medium |
| `partition` | `fn partition<B, F>(self, f: F) -> (B, B)` | Medium |
| `unzip` | `fn unzip<A, B>(self) -> (Vec<A>, Vec<B>)` | Medium |
| `by_ref` | `fn by_ref(&mut self) -> &mut Self` | Low |
| `is_sorted` | `fn is_sorted(self) -> bool` | Low |
| `partial_cmp` | `fn partial_cmp<I>(self, other: I) -> Option<Ordering>` | Low |
| `ne`/`lt`/`le`/`gt`/`ge` | comparison shortcuts | Low |

**Note:** `.collect()` is the single most important missing iterator method. Without it, iterators must be consumed manually with `fold` or `for_each`.

---

## 7. File I/O

**Coverage: ~70%** — Core operations present, advanced features missing

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `read` (binary) | `fn read(path: P) -> io::Result<Vec<u8>>` | High |
| `write` (binary) | `fn write(path: P, contents: &[u8]) -> io::Result<()>` | High |
| `remove_dir_all` | `fn remove_dir_all(path: P) -> io::Result<()>` | High |
| `read_dir` | `fn read_dir(path: P) -> io::Result<ReadDir>` | High |
| `metadata` | `fn metadata(path: P) -> io::Result<Metadata>` | Medium |
| `canonicalize` | `fn canonicalize(path: P) -> io::Result<PathBuf>` | Medium |
| `set_permissions` | `fn set_permissions(path: P, perm: Permissions)` | Low |
| `read_link` / `symlink` / `hard_link` | symbolic/hard link operations | Low |
| `File::read` (bytes) | `fn read(&mut self, buf: &mut [u8]) -> io::Result<usize>` | High |
| `File::write` (bytes) | `fn write(&mut self, buf: &[u8]) -> io::Result<usize>` | High |
| `File::read_to_string` | instance method (TML has static only) | Medium |

---

## 8. Thread

**Coverage: ~75%** — Core API present, some stubs

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `sleep(Duration)` | `fn sleep(dur: Duration)` | Medium (has `sleep_ms`) |
| `park_timeout` | `fn park_timeout(dur: Duration)` | Low |
| `panicking()` | `fn panicking() -> bool` | Low |
| `Builder::spawn` | currently returns Err (stub) | Medium |
| `park` / `unpark` | real implementation (currently stubs) | Medium |
| `thread_local!` | thread-local storage | Medium |

---

## 9. Sync Primitives

**Coverage: ~85%** — Solid implementation, missing bounded channels and poisoning

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `sync_channel` | `fn sync_channel<T>(bound: usize) -> (SyncSender, Receiver)` | High |
| `Mutex::is_poisoned` | `fn is_poisoned(&self) -> bool` | Low (by design) |
| `Arc::make_mut` | `fn make_mut(this: &mut Arc<T>) -> &mut T` | Medium |
| `Receiver: Iterator` | `impl Iterator for Receiver<T>` | Medium |

**Note:** TML explicitly omits poisoning by design — simpler API.

---

## 10. Net

**Coverage: ~90%** — Exceeds Rust in some areas (TLS, async, IOCP)

| Missing Function | Rust Signature | Priority |
|------------------|---------------|----------|
| `TcpListener::incoming` | `fn incoming(&self) -> Incoming` | Medium |
| `TcpStream::read_to_end` | `fn read_to_end(&mut self, buf: &mut Vec<u8>)` | Medium |
| `TcpStream::write_all` | `fn write_all(&mut self, buf: &[u8])` | Medium |
| `try_clone` (TCP/UDP) | `fn try_clone(&self) -> io::Result<Self>` | Low |
| IPv6 multicast | `join_multicast_v6`, `leave_multicast_v6` | Low |

---

## Priority Summary

### Critical (blocks common patterns)

| Gap | Module | Impact |
|-----|--------|--------|
| `List.sort()` | List[T] | Can't sort lists |
| `List.insert()` / `remove()` | List[T] | Can't modify list at arbitrary positions |
| `List.contains()` | List[T] | Can't check membership |
| `List.reverse()` | List[T] | Can't reverse a list |
| `Iterator.collect()` | Iterator | Can't materialize iterators into collections |
| `HashMap.is_empty()` | HashMap | Basic check missing |
| `HashMap.entry()` API | HashMap | No upsert/get-or-insert pattern |
| `env` module | std | No environment variable access |

### High (frequently needed)

| Gap | Module |
|-----|--------|
| `Str.split_once()` / `strip_prefix()` / `strip_suffix()` | Str |
| `Str.splitn()` | Str |
| `Str.is_ascii()` / `eq_ignore_ascii_case()` | Str |
| `List.sort_by()` / `binary_search()` | List[T] |
| `List.iter()` / `extend()` | List[T] |
| `List.swap()` / `swap_remove()` | List[T] |
| `HashMap.keys()` / `values()` | HashMap |
| `Iterator.max()` / `min()` | Iterator |
| `File.read` (binary bytes) | File I/O |
| `read_dir()` (directory listing) | File I/O |
| `remove_dir_all()` | File I/O |
| `sync_channel` (bounded) | Sync |

### Medium

| Gap | Module |
|-----|--------|
| `Maybe.is_some_and()` / `get_or_insert()` | Maybe |
| `Str.replacen()` / `matches()` / `trim_matches()` | Str |
| `List.dedup()` / `truncate()` / `windows()` / `chunks()` | List[T] |
| `HashMap.retain()` / `drain()` | HashMap |
| `Iterator.partition()` / `unzip()` / `max_by_key()` | Iterator |
| `Thread.park_timeout()` / `Builder.spawn` (real impl) | Thread |
| `File.metadata()` / `canonicalize()` | File I/O |
| `prelude` module | Core |
| `Bool.then()` / `then_some()` | Bool |
