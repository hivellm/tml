# phase26c — Revert phase24 Band-Aids + Close the Bug Class (Stabilization ERA 0, Phase B gate)

> The proof that phase26b actually fixed the model is that the workarounds can
> ALL be deleted and everything still passes. Analysis:
> `docs/analysis/tml-table-analysis/04-self-hosting-and-strategy.md` (collateral tech debt).

## 1. Implementation
- [ ] 1.1 Inventory every phase24c–24n workaround still in tree: `into_raw()`/`from_raw()` drop-suppression chains, `declarator_name_value_leak` deliberate leak, manual `.duplicate()` at partial-move sites (~70 in `compiler-tml/src/cc/`), `get_clone` call-site migrations, `CollectedArgs` flat-list dodge in preproc
- [ ] 1.2 Revert them in dependency order (parser → lower → types → preproc), re-running the phase25a determinism corpus after each cluster
- [ ] 1.3 Simplify the library API surface: decide fate of `Shared.get_clone`/`get_ref` and `ptr_read_clone` (keep as documented API or fold into now-sound `get`); remove the hazard docstrings that describe the old bug class
- [ ] 1.4 Final gates: `essential.c --emit=ast` ×100 = 100/100 under adversarial allocator; `std/collections` (btreemap/btreeset/arraylist) K001-free; compiler + core + std suites at or above pre-revert baseline
- [ ] 1.5 Capture learning: what the root cause was, why band-aids didn't converge, how the model fix closed it (`rulebook_memory`, kind: learning)

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
