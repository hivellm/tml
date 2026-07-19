# Proposal: phase26c_memmodel-bandaid-revert

## Why

The phase24 campaign left the tree salted with negative-value workarounds:
`into_raw()`/`from_raw()` drop-suppression chains (0.3.41/0.3.42), the
deliberate `declarator_name_value_leak` (0.3.44), ~70 manual `.duplicate()`
calls at partial-move sites (0.3.51), `get_clone` call-site migrations, and
the `CollectedArgs` flat-list container dodge (0.3.50). They obscure real
defects, teach wrong idioms, and — critically — mask whether phase26b
actually fixed the model. The only honest proof of the fix is that ALL
workarounds can be deleted and every gate still passes.

## What Changes

Systematic inventory and revert of every phase24c–24n workaround in
`compiler-tml/src/cc/` and `lib/`, in dependency order, re-running the
phase25a determinism corpus after each cluster. API cleanup decision on
`Shared.get_clone`/`get_ref`/`ptr_read_clone` (keep as documented API or fold
into the now-sound `get`), and removal of the hazard docstrings describing
the dead bug class.

## Impact

- Affected specs: `Shared`/`Heap` API docs.
- Affected code: `compiler-tml/src/cc/{parser,lower,types,ast,parse_stmt}.tml`,
  `compiler-tml/src/cc/preproc/`, `lib/core/src/alloc/{heap,shared}.tml`,
  `lib/std/src/collections/{hashmap,list}.tml`.
- Breaking change: NO (removals restore intended semantics).
- User benefit: clean idiomatic codebase; the C-frontend becomes a readable
  reference for TML application code instead of a workaround museum; final
  gate `essential.c` ×100 = 100/100 certifies the class closed.

## Source

- docs/analysis/tml-table-analysis/04-self-hosting-and-strategy.md
  (collateral tech debt section). CHANGELOG 0.3.41–0.3.52.
