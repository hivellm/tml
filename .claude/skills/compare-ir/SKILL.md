---
name: compare-ir
description: Full Rust-as-Reference IR comparison workflow. Write equivalent .rs/.tml files, compile both, compare IR side-by-side. Use when the user says "compare ir", "compara ir", "rust reference", or needs to debug codegen quality.
user-invocable: true
allowed-tools: mcp__tml__emit-ir, Bash(rustc *), Write, Read, Glob
argument-hint: <feature-name> [func] — e.g. "compare-ir struct_init" or "compare-ir enum_match my_func"
---

## Compare IR Workflow (Rust-as-Reference Methodology)

This skill automates the mandatory Rust-as-Reference IR comparison workflow from CLAUDE.md.

### 1. Determine Feature

Extract the feature name from `$ARGUMENTS`. If a function name is also given, use it for filtering.

### 2. Check for Existing Files

Look for existing files in `.sandbox/`:
- `.sandbox/temp_<feature>.rs` (Rust version)
- `.sandbox/temp_<feature>.tml` (TML version)

If they DON'T exist, tell the user you need equivalent code in both languages. Ask them to describe what pattern to compare, then write both files.

If they DO exist, proceed to compilation.

### 3. Compile Rust IR

Run via Bash:
```bash
rustc --edition 2021 --emit=llvm-ir -C opt-level=0 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust.ll 2>&1
```

For release comparison, also run with `-C opt-level=3`.

### 4. Compile TML IR

Use `mcp__tml__emit-ir` with:
- `file`: `.sandbox/temp_<feature>.tml`
- `function`: The function name if specified in arguments
- `optimize`: "O0" for debug, "O3" for release

### 5. Compare

Read both IR outputs and compare function-by-function:
- **Instruction count** (TML must not exceed 2x Rust for equivalent logic)
- **Type layouts** (struct/enum sizes should match)
- **Alloca count** (TML should not have allocas that Rust avoids)
- **Safety features** (overflow checks, null checks)
- **Call overhead** (unnecessary wrappers, extra indirection)

### 6. Report

Present a side-by-side comparison with:
- Key differences highlighted
- Quality assessment (TML vs Rust efficiency ratio)
- Specific optimization suggestions if TML IR is significantly worse