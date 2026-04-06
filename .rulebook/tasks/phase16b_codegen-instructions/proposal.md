# Proposal: Codegen Instructions — Rewrite in TML

## Why

The instruction emission layer translates each MIR instruction into one or more LLVM IR text lines.
It is the highest-volume code in the codegen subsystem — arithmetic, memory, control flow, and
aggregate operations together account for the majority of all IR output. The C++ implementation is
spread across `compiler/src/codegen/mir/instructions.cpp`, `instructions_misc.cpp`, and five files
in `llvm/expr/` and `llvm/control/` totaling approximately 12K LOC. Porting this layer to TML
completes the bulk of the MIR codegen path and enables IR-diff testing on realistic programs. It
builds directly on the type emission layer from phase16a.

## What Changes

The C++ instruction emission files are replaced by a TML implementation in
`compiler-tml/src/codegen/emit_inst.tml`. The complete MirInst enum (40+ variants) is handled by
a single dispatch function that returns a `Text` fragment for each instruction. Basic block
iteration and function body assembly remain in `emit_func.tml` (phase16a).

### Architecture

```
compiler-tml/src/codegen/
  emit_inst.tml    — InstructionEmitter: emit(MirInst) -> Text
                     arithmetic, comparison, bitwise (Phase 2)
                     alloca, load, store, GEP (Phase 3)
                     br, cond_br, switch, ret (Phase 4)
                     extractvalue, insertvalue, phi, select (Phase 5)
                     zext, sext, trunc, ptrtoint, inttoptr, bitcast,
                     fpext, fptrunc, fptosi, sitofp (Phase 6)
```

### Key Design Decisions

- **One Text per instruction** — `emit(inst: MirInst) -> Text` returns the full IR line including
  leading spaces and trailing newline. The caller joins all instruction texts with no separator.
  Template literals make each case readable: `` `  %{reg} = add nsw {ty} %{a}, %{b}\n` ``.
- **nsw on integer arithmetic** — all signed integer arithmetic emits `nsw` (no signed wrap)
  flags, matching the C++ default. This enables LLVM to apply algebraic optimizations. The
  `nsw` flag is omitted only for explicitly wrapping operations (future intrinsics).
- **Ordered float predicates** — all FCmp uses ordered predicates (`oeq`, `olt`, etc.) matching
  the C++ codegen. Unordered predicates are not emitted unless the MIR instruction carries an
  explicit `unordered` flag, which no current TML code generates.
- **GEP inbounds** — all GEP instructions emit `inbounds` matching the C++ output. This is safe
  because TML's borrow checker guarantees no out-of-bounds access at the TML level. The inbounds
  annotation enables LLVM's alias analysis.
- **instruction-by-instruction IR-diff** — the differential testing strategy compares individual
  instruction outputs rather than whole-function IR. This lets early failures pinpoint exactly
  which MIR instruction variant is emitting wrong text, without requiring full-program compilation.

### Instruction → LLVM IR Mapping (summary)

| MIR Instruction | LLVM IR |
|---|---|
| `Add(nsw, a, b)` | `%r = add nsw i64 %a, %b` |
| `ICmp(Eq, a, b)` | `%r = icmp eq i64 %a, %b` |
| `Alloca(T)` | `%r = alloca T, align A` |
| `Load(T, addr)` | `%r = load T, ptr %addr, align A` |
| `Store(val, addr)` | `store T %val, ptr %addr, align A` |
| `GEP(base, T, [0, N])` | `%r = getelementptr inbounds T, ptr %base, i32 0, i32 N` |
| `Br(bb)` | `br label %bb` |
| `CondBr(c, t, f)` | `br i1 %c, label %t, label %f` |
| `Switch(v, d, cases)` | `switch i64 %v, label %d [ ... ]` |
| `Ret(v)` | `ret i64 %v` |
| `ExtractValue(agg, N)` | `%r = extractvalue { ... } %agg, N` |
| `InsertValue(agg, v, N)` | `%r = insertvalue { ... } %agg, T %v, N` |
| `Phi([(v1,bb1),...])` | `%r = phi T [ %v1, %bb1 ], ...` |
| `Select(c, t, f)` | `%r = select i1 %c, T %t, T %f` |
| `ZExt(v, T)` | `%r = zext i32 %v to T` |

## Impact

- Affected code: `compiler/src/codegen/mir/instructions.cpp`, `instructions_misc.cpp`,
  `llvm/expr/binary.cpp`, `llvm/expr/binary_ops.cpp`, `llvm/control/when.cpp`,
  `llvm/expr/struct_field.cpp`, `llvm/expr/llvm_struct_expr.cpp` (all replaced)
- Affected phases: 16c (calls extend this layer with call/invoke instructions)
- Breaking change: NO — IR-diff testing ensures instruction-identical output
- User benefit: self-hosting progress; every IR instruction inspectable in TML

## Success Criteria

`emit(inst)` produces LLVM IR text that is character-identical to C++ output for all 40+ MIR
instruction variants. IR-diff on 10 stdlib functions shows zero instruction differences.

## Dependencies

- **Requires**: phase16a (emit_type, LayoutComputer, register naming infrastructure)
- **Blocks**: phase16c (call emission extends InstructionEmitter)
- **Risk**: Medium — large number of instruction variants, but each is mechanically straightforward.
  The main risk is alignment values diverging from C++ layout rules; mitigated by phase16a layout
  tests that verify field offsets before instruction emission begins.
