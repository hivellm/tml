# Proposal: phase21a_debug-info-pdb

## Why
Without debug info, debuggers (Visual Studio, WinDbg, LLDB) cannot show source line numbers,
local variable values, or let developers set breakpoints when debugging TML-compiled binaries.
PDB (Program Database) is the canonical Windows debug information format. Every production
toolchain on Windows emits PDB data so developers can diagnose crashes from minidumps, step
through code, and inspect stack frames. The native backend currently emits raw object files
with no debug sections, making it impossible to debug TML programs natively on Windows.

## What Changes
- New module `compiler-tml/src/native/x86/debug_info.tml` implementing CodeView record emission.
- Emits a `.debug$S` COFF section containing the symbol subsection (S_GPROC32/S_END records)
  and the line-number subsection mapping each instruction offset to a source line.
- Emits a `.debug$T` section with CodeView type records for primitive types (T_INT4, T_INT8,
  T_REAL64) and composite types (structs, enums).
- Emits S_GPROC32 function begin/end records that name each compiled function, record its
  code offset and size, and reference its type record.
- Passes `/DEBUG` to the LLD linker invocation so it merges the COFF debug sections into a
  companion `.pdb` file beside the output executable.

## Impact
- Affected specs: native/x86 codegen spec, object emission spec
- Affected code: `compiler-tml/src/native/x86/debug_info.tml` (new), `compiler-tml/src/native/pipeline.tml` (link flags), `compiler-tml/src/native/x86/obj_emit.tml` (section registration)
- Breaking change: NO
- User benefit: Developers can attach Visual Studio or WinDbg to TML-compiled binaries, set
  breakpoints by source line, inspect local variables, and read crash minidumps with full
  source context — matching the debugging experience of C/C++ on Windows.
