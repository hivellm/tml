# phase26e — Collection Borrow Accessors (the zero-cost enabler)

> Addresses F-021 (08-memory-copy-audit): collections have NO working borrowing
> accessor, so every `get` of a handle-bearing element deep-clones. This is the
> single piece that turns "correct + leak-free" (delivered by 26b) into
> "zero-cost" (the actual Rust-parity thesis). Depends on phase26b (real move/drop
> model) being landed first — a borrow into a container is only sound once the
> drop model can track its lifetime. New language + codegen surface, not a stdlib
> patch.

## 1. Implementation
- [ ] 1.1 Interior-pointer codegen: emit a GEP into the type-erased `*Unit` backing buffer of `List`/`HashMap`/`BTreeMap`/`Deque` that yields a `ref T`/`mut ref T` without copying the element (both codegen paths, or the unified MIR path if 26b step 3 retired the AST path)
- [ ] 1.2 Borrow-checker lifetimes: bind the returned reference's lifetime to the container borrow so use-after-invalidation (get_ref then push/rehash) is a compile error; extend below the `lowlevel { *ptr }` boundary the checker is currently blind to
- [ ] 1.3 Read accessors: `List::get_ref(i) -> ref T`, `HashMap::get_ref(k) -> Maybe[ref V]`, `BTreeMap::get_ref`, `Deque::get_ref` — zero-alloc, no clone
- [ ] 1.4 Mut accessors: implement the currently-dead `list_get_mut` intrinsic (referenced by `IndexMut::index_mut` in `behaviors.tml` but ABSENT from `compiler/`), then `HashMap::get_mut`, etc.
- [ ] 1.5 Borrowing iterators: `iter_ref`/`values_ref` yielding `ref T`; make the existing by-value iterators either move-out (consuming) or delegate to the ref form so the F-019 asymmetry is resolved coherently
- [ ] 1.6 Migrate hot stdlib call sites (List sort/dedup/contains, BTreeMap shift, HashMap duplicate/to_string) from `get`(clone) to `get_ref` — IR-verify zero alloc on `List[Str]` sort and `HashMap[Str,Str]` iteration
- [ ] 1.7 Benchmark vs Rust: `HashMap::get`/`List::get`/iteration on handle-bearing elements must emit zero heap traffic (match Rust's `&T` return); record in a perf doc

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
