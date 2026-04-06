# Tasks: Incremental Linker — Sub-10ms Re-link in TML

**Status**: Planned (0/18)
**Depends on**: phase22a (PE/COFF linker), phase22b (ELF linker), phase22c (Mach-O linker) — all three base linkers must work
**Blocks**: nothing (this is the final ERA 3 phase; LLD fully eliminated after this)
**Duration**: 4–5 weeks
**Risk**: Medium — the delta relocation logic is the hardest correctness problem; benchmarking must confirm <10ms

---

## Phase 1: Link State Database (4 items)

- [ ] 1.1 Create `compiler-tml/src/link/incr/state.tml` — `LinkState` type: records the full result of the previous link invocation; fields: `input_fingerprints: HashMap[Str, U64]` (path → fingerprint), `symbol_table: HashMap[Str, SymbolEntry]` (name → section + offset + size), `section_layout: List[SectionRecord]` (name, start RVA, file offset, size, flags), `relocation_log: List[AppliedReloc]` (each relocation that was applied: source section + offset, target symbol, addend, type)
- [ ] 1.2 Implement `LinkState.save(path: Str) -> Outcome[Unit, IoError]` — serialize `LinkState` to a compact binary format at `<output>.link-state` alongside the output binary; use a simple tag-length-value encoding; write a version header so old state files are rejected cleanly on format change
- [ ] 1.3 Implement `LinkState.load(path: Str) -> Outcome[LinkState, LinkStateError]` — deserialize the `.link-state` file; verify version header; return `LinkStateError::NotFound` if the file does not exist (triggering a full link), `LinkStateError::Corrupt` if parsing fails (also triggers full link with a warning)
- [ ] 1.4 Integrate state save/load into the base linker: after every full link (phase 22a/22b/22c), write `.link-state`; at the start of every link invocation, attempt to load the state file; if load succeeds and input fingerprints match, route to incremental path instead of full link

## Phase 2: Change Detection (3 items)

- [ ] 2.1 Create `compiler-tml/src/link/incr/fingerprint.tml` — `fingerprint_file(path: Str) -> Outcome[U64, IoError]`: read the file bytes and compute a 64-bit xxHash3 of the content; xxHash3 is faster than SHA for non-cryptographic use and has excellent distribution; store the hash in `LinkState.input_fingerprints`
- [ ] 2.2 Implement `detect_changes(state: ref LinkState, inputs: List[Str]) -> ChangeSummary` — for each input `.obj` file in the current invocation, compare `fingerprint_file(path)` against `state.input_fingerprints[path]`; return `ChangeSummary` listing: `added` (new files not in previous state), `removed` (files in previous state but not in current invocation), `modified` (same path but different fingerprint), `unchanged` (same path and fingerprint)
- [ ] 2.3 Implement full-link fallback conditions: if any of the following are true, discard incremental state and do a full link: (a) any symbol was removed from a changed file, (b) any symbol was added to a changed file that conflicts with an existing symbol in an unchanged file, (c) section count changed in a modified file, (d) the set of input `.lib` / `.so` / `.dylib` files changed

## Phase 3: Delta Relocation (4 items)

- [ ] 3.1 Create `compiler-tml/src/link/incr/delta_reloc.tml` — for each modified `.obj` file, re-parse its sections and relocations (reusing the existing parsers from phase 22a/22b/22c); identify which sections changed by comparing raw content hashes of each section before and after; only sections that changed need relocation reprocessing
- [ ] 3.2 For changed sections: reapply all relocations that target symbols within those sections using the existing symbol table (unchanged symbols have the same addresses as in the previous link); update `LinkState.relocation_log` with the new applied relocations
- [ ] 3.3 For relocations in unchanged sections that reference symbols defined in changed sections: if the referenced symbol moved (its offset within the section changed), these relocations must also be reapplied; compute the set of "transitively affected" relocations by querying `LinkState.relocation_log` for entries whose `target_symbol` is in a changed section
- [ ] 3.4 Validate the delta: after computing all changed bytes, assert that no relocated address points outside the bounds of its target section; assert that all PC-relative offsets still fit in their field width; if any assertion fails, fall back to a full link and log a diagnostic

## Phase 4: In-Place Binary Patching (4 items)

- [ ] 4.1 Create `compiler-tml/src/link/incr/patch.tml` — `PatchSet` type: a list of `(file_offset: I64, bytes: Buffer)` entries representing the exact bytes to write to the output binary; compute `PatchSet` from the changed section data and reapplied relocations
- [ ] 4.2 Implement `apply_patches(output_path: Str, patches: ref PatchSet) -> Outcome[Unit, IoError]` — open the output binary for read-write (not truncate); for each patch entry, seek to `file_offset` and write the bytes; close the file; the binary is patched in place without being recreated
- [ ] 4.3 Handle section size changes: if a modified `.obj` file's section grew or shrank, in-place patching is impossible (downstream sections would overlap or have gaps); detect this case and fall back to a full link; log which section changed size so developers can diagnose why incremental linking fell back
- [ ] 4.4 Update the PE checksum / Mach-O code signature after patching: for PE binaries, recompute the `CheckSum` field and write it in place; for Mach-O binaries, recompute all `CodeDirectory` page hashes for pages that overlap with patched regions and write the updated signature blob

## Phase 5: Testing (3 items)

- [ ] 5.1 Benchmark: compile a 1,000-file TML project, link fully, change one `.tml` source file (producing one new `.obj`), re-link; measure re-link time; the target is < 10ms on a modern NVMe SSD for a single-file change; use `std::time::Instant` for measurement
- [ ] 5.2 Correctness test: for a multi-file project, change each source file individually and verify the incrementally-linked binary is bit-for-bit identical to a fresh full link of the same inputs; test at least 20 distinct single-file changes covering additions, modifications, and deletions of functions
- [ ] 5.3 Fallback test: trigger each full-link fallback condition (symbol added, symbol removed, section size change) and verify that (a) the fallback occurs, (b) the resulting binary is correct, (c) the next incremental link after the fallback succeeds and is fast
