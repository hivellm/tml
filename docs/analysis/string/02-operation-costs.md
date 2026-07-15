# 02 — Per-Operation Cost Breakdown

## Measured Results (release mode, --stage=parser:cpp)

| Operation | TML (ns/op) | Rust (ns/op) | Ratio | Iterations |
|-----------|-------------|-------------|-------|------------|
| Concat Small (3 literals) | 0 | 180 | **TML wins** | 1M |
| Text push_str (reserved) | 4 | 1 | 4.0x | 100K |
| Str += naive loop | 3,293 | 3 | 1,098x | 10K |
| String Length (strlen) | 0 | 0 | ~1x | 1M |
| String Compare (equal) | 0 | 0 | ~1x | 1M |
| Int to String | 41 | 7 | 5.9x | 1M |
| Log Building (Text, reserved) | 60 | 52 | 1.15x | 10K |
| Log Building (Str +=) | 4,155 | 93 | 44.7x | 1K |

## Operation Trace: `Text::from("hello")` (5 chars, inline SSO)

```
1. text_str_len("hello")          → calls C strlen via FFI           ~5 ns
2. slen = 5 ≤ 23 → inline path
3. text_pack_word(s, 0, 5)        → pack 5 bytes into I64           ~2 ns
4. w1 = 0, w2_data = 0
5. text_inline_tag(5)             → (128|5) << 56                   ~1 ns
6. Text { _w0: w0, _w1: 0, _w2: tag }  → struct construction       ~1 ns
                                                            TOTAL:  ~9 ns
                                                            ALLOCS: 0
```

## Operation Trace: `Text::from("long string exceeding 23 bytes...")`

```
1. text_str_len(s)                → strlen FFI                      ~5 ns
2. slen > 23 → heap path
3. mem_alloc(slen + 1)            → HEAP ALLOCATION                 ~15 ns
4. copy_nonoverlapping(s, data)   → memcpy                          ~5 ns
5. ptr_write null terminator                                        ~1 ns
6. Text { _w0: ptr, _w1: slen, _w2: slen }                         ~1 ns
                                                            TOTAL:  ~27 ns
                                                            ALLOCS: 1
```

## Operation Trace: `text.push_str("ab")` (inline, has room)

```
1. text_str_len("ab")             → strlen FFI                      ~5 ns
2. lowlevel { this as *Unit }     → get struct address               ~0 ns
3. text_push_str_ptr(self, "ab", 2)
   3a. Read _w2 → check inline                                      ~1 ns
   3b. cur_len + slen = N ≤ 23 → stay inline
   3c. Copy 2 bytes into struct at offset cur_len                    ~1 ns
   3d. Update _w2 tag byte                                          ~1 ns
                                                            TOTAL:  ~8 ns
                                                            ALLOCS: 0
```

## Operation Trace: `text.as_str()` (inline, 5 chars)

```
1. lowlevel { this as *Unit }     → struct address                   ~0 ns
2. text_sso_len(_w2, _w1)         → (w2 >> 56) & 127 = 5            ~1 ns
3. text_data_ptr(addr, w0, w2)    → addr (inline data IS the struct) ~1 ns
4. mem_alloc(6)                   → HEAP ALLOCATION for copy         ~15 ns
5. copy_nonoverlapping(data, buf, 5)  → memcpy 5 bytes              ~2 ns
6. ptr_write null terminator                                         ~1 ns
7. return buf as Str                                                 ~0 ns
                                                            TOTAL:  ~20 ns
                                                            ALLOCS: 1 (WASTEFUL)
```

**Note**: `as_str()` allocates a heap copy EVERY TIME. For a 5-byte inline string, the data is right there in the struct but gets copied to a new 6-byte buffer. This is the most wasteful operation in the string subsystem.

## Operation Trace: `I64.to_string()` (e.g., value = 42)

```
1. malloc(24)                     → HEAP ALLOCATION (24 bytes!)      ~15 ns
2. snprintf(buf, 24, "%ld", 42)   → libc format string parsing       ~20 ns
   - Parse "%ld" format specifier
   - Convert 42 to digits via division loop
   - Write "42\0" to buffer
3. return buf                                                        ~0 ns
                                                            TOTAL:  ~35-41 ns
                                                            ALLOCS: 1
```

**Compare Rust**: `42_i64.to_string()` uses stack buffer + `itoa`-style lookup table. Zero allocation. ~7 ns.

## Operation Trace: `result = result + "ab"` (Str concat in loop, iteration N)

```
Iteration 1 (result = ""):
1. str_concat_opt("", "ab")
   1a. strlen("") = 0                                               ~3 ns
   1b. strlen("ab") = 2                                             ~3 ns
   1c. mem_alloc(3)              → HEAP ALLOC                       ~15 ns
   1d. memcpy(buf, "", 0)                                           ~0 ns
   1e. memcpy(buf+0, "ab", 2)                                      ~2 ns
   1f. null terminate                                               ~1 ns
2. store result                                                     ~1 ns
                                                            TOTAL:  ~25 ns
                                                            ALLOCS: 1

Iteration N (result has 2N chars):
1. holds_heap_str = true → str_concat_reuse path
   1a. strlen(result) = 2N                                          O(N) !!!
   1b. strlen("ab") = 2                                             ~3 ns
   1c. mem_realloc(result, (2N+2)*2+1)                              ~15 ns (if in-place)
   1d. memcpy(buf+2N, "ab", 2)                                     ~2 ns
   1e. null terminate                                               ~1 ns
                                                            TOTAL:  O(N)
                                                            ALLOCS: 0-1 (realloc)

Total for 10K iterations:
  strlen cost: 0+2+4+...+20000 = O(N^2)
  memcpy cost: 2+2+2+...+2 = O(N)
  TOTAL: O(N^2) dominated by strlen
```

**The strlen on `result` is the killer.** Even with realloc optimization, scanning the growing string to find its length makes the loop O(n^2).

## Operation Trace: `tml_str_free(ptr)` on Windows

```
1. ptr == null? → return                                            ~1 ns
2. tml_image_ranges_initialized? → check flag                       ~1 ns
3. tml_is_image_ptr(ptr)
   3a. Binary search over ~20 loaded modules                        ~3-5 ns
   3b. If in image range → return (string constant)                 ~0 ns
4. HeapValidate(heap, 0, ptr)    → Windows kernel call              ~50-100 ns
5. mem_free(ptr)                 → free() + tracking deregister      ~20 ns
                                                            TOTAL:  ~75-125 ns (heap string)
                                                            TOTAL:  ~5 ns (string constant)
```

The `HeapValidate` call on the heap path is **~100 ns overhead** per free. This is necessary because `tml_str_free` can be called on any pointer (constant or heap) and must distinguish them safely.
