# Proposal: phase0c_jit-runtime-symbols

## Why
JIT'd TML code calls C runtime functions (`tml_print`, `tml_alloc`, `tml_free`, `tml_panic`, etc.) via `@extern("c")`. In compiled mode, LLD resolves these from `.obj` files. In JIT mode, these symbols must be available in the host process so ORC's symbol resolver can find them.

Without runtime symbol resolution, any TML program that uses I/O, allocation, or collections will crash under JIT.

## What Changes
- Static-link C runtime `.c` files into `tml.exe` (or the compiler DLL)
- Register `DynamicLibrarySearchGenerator::GetForCurrentProcess()` on the JIT engine
- This auto-resolves all symbols from the host process — zero manual registration
- Test that `tml_print`, `tml_alloc`, `tml_free`, and collection functions resolve

## Symbol Resolution Chain
```
JIT'd TML code calls tml_print()
  → ORC symbol lookup in JITDylib
  → Not found in IR modules
  → Falls through to DynamicLibrarySearchGenerator
  → Finds tml_print in host process (statically linked)
  → Resolved ✓
```

## C Runtime Files to Embed
| File | Functions | Size |
|------|-----------|------|
| `essential.c` | tml_print, tml_panic, tml_eprint, test harness | ~15KB |
| `mem.c` | tml_alloc, tml_free, tml_realloc | ~3KB |
| Collections (list, hashmap, buffer) | list_*, hashmap_*, buffer_* | ~30KB |
| String/Text ops | str_*, text_* | ~20KB |

Total: ~70KB of C code — negligible binary size impact.

## Impact
- Affected specs: none
- Affected code: `compiler/CMakeLists.txt` (link C runtime objects), `jit_engine.cpp` (add search generator)
- Breaking change: NO
- User benefit: JIT mode supports real TML programs with I/O and collections

## Dependencies
- Requires: phase0b (JIT engine core exists)
