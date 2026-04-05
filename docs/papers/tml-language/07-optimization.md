# 7. Optimization Architecture

## 7.1 Overview

TML employs a two-tier optimization strategy: a rich set of mid-level IR (MIR) optimization passes that operate on TML-specific semantics, followed by the full LLVM optimization pipeline that handles target-specific and general-purpose optimizations. This dual approach mirrors Rust's architecture, where MIR-level optimizations exploit language-specific guarantees before handing off to LLVM for machine-level optimization.

The rationale for MIR-level optimization is straightforward: certain transformations are only valid — or dramatically more effective — when the compiler retains semantic information about ownership, lifetimes, and type structure that is lost during lowering to LLVM IR. By performing these optimizations at the MIR level, TML can eliminate unnecessary allocations, hoist destructors, devirtualize method calls, and prove aliasing properties that LLVM alone cannot deduce.

---

## 7.2 MIR Optimization Passes

TML implements **52 MIR optimization passes**, a comprehensive suite that rivals or exceeds the MIR pass count of comparable compilers. For comparison, Rust's MIR pipeline includes approximately 50 passes, while LLVM's full pipeline contains over 200 passes operating at the lower IR level. GCC employs roughly 300 passes across its GIMPLE and RTL representations.

### 7.2.1 Pass Categories

The 52 passes can be organized into six functional categories:

**Memory and Allocation Optimization (8 passes):**
- **mem2reg** — The single most critical pass. Promotes stack allocations (alloca) to SSA registers, eliminating unnecessary memory operations. This pass typically reduces instruction count by 30-50% on code generated from the MIR builder, which conservatively places values in memory.
- **sroa** (Scalar Replacement of Aggregates) — Breaks apart struct allocations into individual scalar values when the struct is never used as a whole.
- **load_store_opt** — Eliminates redundant loads when a store to the same location is provably the most recent write.
- **destination_propagation** — Propagates known destination addresses through assignment chains, reducing intermediate copies.
- **escape_analysis** — Determines whether heap allocations can be safely replaced with stack allocations (allocation sinking).
- **rvo** (Return Value Optimization) — Eliminates copies for returned values by constructing them directly in the caller's destination.
- **sinking** — Moves instructions closer to their use sites to reduce register pressure.
- **memory_leak_check** — Analysis pass that detects potential memory leaks where allocated values are never freed.

**Dead Code and Control Flow (8 passes):**
- **dead_code_elimination** — Removes instructions whose results are never used.
- **dead_function_elimination** — Removes functions that are never called (after inlining).
- **dead_method_elimination** — Removes behavior implementation methods that are never dispatched.
- **dead_arg_elim** — Removes function arguments that are never read by the function body.
- **unreachable_code_elimination** — Removes basic blocks that cannot be reached from the entry block.
- **adce** (Aggressive Dead Code Elimination) — More powerful than standard DCE; works backwards from program effects to prove code unnecessary.
- **simplify_cfg** — Merges basic blocks, eliminates empty blocks, and simplifies branch chains.
- **block_merge** — Combines consecutive basic blocks with unconditional jumps between them.

**Value Propagation and Simplification (9 passes):**
- **constant_folding** — Evaluates constant expressions at compile time (e.g., 3 + 4 becomes 7).
- **constant_propagation** — Replaces variable uses with their known constant values.
- **copy_propagation** — Replaces uses of copied values with the original source.
- **inst_simplify** — Applies algebraic simplifications (e.g., x * 1 = x, x + 0 = x, x & true = x).
- **early_cse** (Common Subexpression Elimination) — Eliminates redundant computations within a basic block.
- **common_subexpression_elimination** — Global CSE across basic blocks using dominance information.
- **gvn** (Global Value Numbering) — Identifies and eliminates redundant computations using hash-based value numbering.
- **reassociate** — Reorders associative/commutative operations for better constant folding and CSE.
- **narrowing** — Reduces the bit width of operations when the upper bits are provably unused.

**Loop Optimization (5 passes):**
- **licm** (Loop-Invariant Code Motion) — Moves loop-invariant computations outside the loop body.
- **loop_rotate** — Transforms loops to place the exit condition at the bottom, enabling better optimization.
- **loop_unroll** — Unrolls small loops to reduce branch overhead and enable further optimizations.
- **loop_opts** — General loop optimization (strength reduction within loops, induction variable simplification).
- **normalize_array_len** — Hoists array length computations outside loops when the array size is invariant.

**Interprocedural and Dispatch Optimization (7 passes):**
- **inlining** — Inlines small functions at call sites, eliminating call overhead and enabling further optimization.
- **ipo** (Interprocedural Optimization) — Cross-function analysis for argument specialization and partial evaluation.
- **devirtualization** — Converts dynamic dispatch (behavior object method calls) to static dispatch when the concrete type is known.
- **tail_call** — Identifies and marks tail calls for guaranteed tail-call optimization, preventing stack growth.
- **constructor_fusion** — Fuses sequential field assignments into a single aggregate construction.
- **peephole** — Pattern-based local optimizations (instruction combining, strength reduction).
- **strength_reduction** — Replaces expensive operations with cheaper equivalents (e.g., multiplication by power of 2 to left shift).

**Specialized Passes (7 passes):**
- **batch_destruction** — Batches destructor calls for sequential drops, reducing function call overhead.
- **destructor_hoist** — Moves destructor calls earlier when doing so is safe, improving memory utilization.
- **remove_unneeded_drops** — Eliminates drop calls for types with trivial destructors (Copy types).
- **match_simplify** — Optimizes pattern match dispatch (jump tables, binary search on discriminants).
- **simplify_select** — Simplifies conditional select operations.
- **jump_threading** — Eliminates redundant conditional branches by threading jumps through known conditions.
- **const_hoist** — Hoists constant materializations out of loops and into predecessor blocks.

**Analysis and Profiling (3 passes):**
- **alias_analysis** — Computes aliasing information for memory operations (leveraged by load_store_opt, licm).
- **pgo** (Profile-Guided Optimization) — Uses runtime profile data to guide inlining, branch prediction, and code layout decisions.
- **vectorization** — Identifies opportunities for SIMD vectorization in loops and sequential operations.

**Async-Specific (1 pass):**
- **async_lowering** — Transforms async function bodies into state machines for cooperative scheduling.

**Bounds Check (1 pass):**
- **bounds_check_elimination** — Proves array accesses are within bounds, removing redundant runtime checks.

**Other (3 passes):**
- **merge_returns** — Canonicalizes functions to have a single return point.
- **builder_opt** — Post-builder cleanup for MIR construction artifacts.
- **infinite_loop_check** — Detects provably infinite loops and emits diagnostics.

### 7.2.2 Pass Ordering

Pass ordering is critical for optimization effectiveness. TML follows a pipeline inspired by LLVM's pass manager design:

1. **Early cleanup**: simplify_cfg, block_merge, builder_opt
2. **Promotion**: mem2reg (must run early — most other passes assume SSA form)
3. **Local simplification**: constant_folding, inst_simplify, copy_propagation, early_cse
4. **Global optimization**: gvn, constant_propagation, dead_code_elimination
5. **Loop optimization**: licm, loop_rotate, loop_unroll, normalize_array_len
6. **Interprocedural**: inlining, devirtualization, ipo (iterate with local simplification)
7. **Memory optimization**: sroa, load_store_opt, escape_analysis, rvo
8. **Cleanup**: adce, dead_function_elimination, dead_method_elimination, simplify_cfg
9. **Late**: batch_destruction, destructor_hoist, remove_unneeded_drops, tail_call

---

## 7.3 LLVM Backend Optimization

After MIR optimization, TML generates LLVM IR text that is parsed by the embedded LLVM 19+ library and subjected to LLVM's full optimization pipeline.

### 7.3.1 Optimization Levels

| Level | TML Flag | LLVM Passes | Use Case |
|-------|----------|-------------|----------|
| O0 | `--debug` (default) | Minimal — fast compilation | Development, debugging |
| O1 | `--optimize=1` | Basic optimizations | Fast builds with some optimization |
| O2 | `--optimize=2` | Full optimization | Production builds |
| O3 | `--release` | Aggressive optimization + vectorization | Maximum performance |

### 7.3.2 Embedded LLVM Advantages

TML embeds LLVM and LLD directly into the compiler binary (in-process), unlike tools that shell out to external commands. This provides several advantages:

1. **No temporary files**: IR is passed in-memory from MIR codegen to LLVM, eliminating I/O overhead.
2. **Single binary distribution**: The compiler, optimizer, and linker are one executable — no toolchain setup.
3. **Consistent optimization**: The LLVM version is pinned, ensuring reproducible builds across environments.
4. **Faster compilation**: Eliminating process creation and file I/O saves 100-500ms per compilation unit.

---

## 7.4 Rust-as-Reference IR Methodology

TML employs a systematic methodology for evaluating and improving the quality of generated LLVM IR: the **Rust-as-Reference** approach. Since both TML and Rust target LLVM with similar semantic guarantees (ownership, borrowing, no data races), Rust's IR output serves as the quality benchmark.

### 7.4.1 Methodology

The workflow for every codegen optimization task is:

1. Write semantically equivalent code in both TML and Rust.
2. Compile both to LLVM IR at the same optimization level.
3. Compare function-by-function across four metrics:
   - **Instruction count** — TML must not exceed 2x Rust for equivalent logic.
   - **Type layouts** — Struct and enum sizes should match.
   - **Alloca count** — TML should not have stack allocations that Rust avoids.
   - **Safety overhead** — Overflow checks, null checks, bounds checks should be comparable.
4. Identify and fix divergences in the TML codegen.

### 7.4.2 Current Optimization Gaps

| Issue | TML Current | Rust Reference | Impact |
|-------|------------|----------------|--------|
| Maybe[I32] layout | 16 bytes ({i32, [1 x i64]}) | 8 bytes ({i32, i32}) | HIGH — 2x memory for nullable integers |
| Struct constructors | alloca + store + load (10 instructions) | insertvalue chain (3 instructions) | HIGH — 3x instruction count |
| Runtime declarations | 500+ lines unconditionally emitted | Only used declarations | MEDIUM — IR bloat, slower LLVM processing |
| Integer arithmetic | add nsw (undefined behavior on overflow) | Checked add with panic | MEDIUM — correctness gap |
| Exception handling | None | invoke + cleanuppad | LOW — no unwinding support yet |

### 7.4.3 Achieved Parity

In several areas, TML already matches or closely approaches Rust's IR quality:

- **Function call overhead**: Direct calls generate identical IR to Rust.
- **Reference handling**: ref/mut ref lower to identical LLVM pointer types.
- **Enum discriminant layout**: Standard niche optimization for single-variant enums.
- **Iterator optimization**: Loop bodies inline and optimize comparably after LLVM passes.
- **SIMD operations**: Native vector types generate identical LLVM vector instructions.

---

## 7.5 Performance Characteristics

### 7.5.1 Compilation Speed

| Metric | TML | Rust | C++ (Clang) | Go |
|--------|-----|------|-------------|----|
| Full build (medium project) | ~100s | 120-300s | 60-180s | 10-30s |
| Incremental (single file change) | 5-15s | 15-60s | 5-30s | 1-5s |
| Linking | 37s (LLD, I/O bound) | 10-60s (LLD) | 5-30s (LLD) | <5s (custom) |

TML's query-based incremental compilation with fingerprinting avoids redundant work. Changed files are recompiled, and downstream queries are only re-executed if their input fingerprints have changed (RED/YELLOW/GREEN coloring).

### 7.5.2 Runtime Performance

Since TML and Rust both target LLVM with similar semantic guarantees, runtime performance should theoretically be identical for equivalent code. In practice:

- **Equivalent to Rust** for code that generates matching IR (function calls, iteration, arithmetic).
- **Marginally worse** where optimization gaps exist (struct construction, enum layouts).
- **Equivalent to C/C++** when compiled through LLVM at O2/O3 — the same backend optimizations apply.
- **Dramatically faster than Go** due to no garbage collection pauses and zero-cost abstractions.
- **Orders of magnitude faster than Python** — compiled native code vs interpreted bytecode.

### 7.5.3 Binary Size

| Build Mode | Size | Contents |
|------------|------|----------|
| Monolithic (debug) | ~100MB | Compiler + LLVM + LLD in single binary |
| Modular (debug) | ~180MB total | Thin launcher + tml_compiler.dll (104MB) + tml_codegen_x86.dll (78MB) |
| User program (release) | 50KB-5MB | Depends on stdlib usage, with dead code elimination |

The compiler binary is large because it embeds the full LLVM library. User programs are small because dead function elimination (both at MIR and LLVM levels) strips unused standard library code.

---

## 7.6 Comparison with Other Optimization Approaches

### 7.6.1 TML vs Rust Optimization

Both languages share the LLVM backend, but TML's 52 MIR passes vs Rust's ~50 MIR passes serve slightly different roles. Rust's MIR passes focus heavily on borrow-check-related cleanup and monomorphization optimization, while TML's passes include more aggressive constructor fusion and destructor batching — reflective of TML's emphasis on reducing codegen overhead for common patterns.

### 7.6.2 TML vs Go Optimization

Go's compiler uses a custom SSA backend rather than LLVM, prioritizing compilation speed over peak optimization. Go compiles 5-10x faster than TML but generates code that is typically 10-30% slower at runtime. TML's approach trades compilation time for runtime performance — appropriate for systems programming where runtime efficiency is paramount.

### 7.6.3 TML vs C++ (Clang) Optimization

Both use LLVM, but C++ lacks the ownership semantics that enable TML's MIR-level optimizations. C++ code requires more conservative alias analysis because pointers are unrestricted. TML's borrow checker guarantees enable more aggressive load/store optimization and alias analysis at the MIR level.

### 7.6.4 TML vs Zig Optimization

Zig uses a custom backend that can also target LLVM. In ReleaseFast mode, Zig removes all safety checks (bounds checks, overflow checks) for maximum performance. TML maintains safety checks even in release mode (though bounds_check_elimination proves many unnecessary). This philosophical difference — Zig trusts the programmer to be correct, TML trusts the compiler to prove correctness — reflects fundamentally different safety models.

---

## 7.7 Future Optimization Work

Several optimization opportunities remain:

1. **Niche optimization for Maybe types** — Encoding Nothing as an invalid bit pattern within the payload, reducing Maybe[I32] from 16 to 8 bytes.
2. **Struct construction via insertvalue** — Eliminating unnecessary alloca/store/load sequences.
3. **Lazy runtime declarations** — Emitting only the C runtime function declarations that are actually called.
4. **Checked integer arithmetic** — Replacing nsw operations with checked operations that panic on overflow.
5. **Profile-guided optimization** — Leveraging the pgo pass with real-world execution data.
6. **Link-time optimization (LTO)** — Cross-module optimization through LLVM's LTO infrastructure.
7. **Parallel compilation** — Compiling independent modules on separate threads (the query system supports this architecturally).
