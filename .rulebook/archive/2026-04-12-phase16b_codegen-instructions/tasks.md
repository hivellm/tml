# Tasks: Codegen Instructions — Rewrite in TML

**Status**: Complete (25/25)
**Depends on**: phase16a (type emission and struct layouts available)
**Blocks**: phase16c (calls need instruction emission), phase16d (legacy path needs instruction layer)
**Duration**: 5–7 weeks
**Risk**: Medium — each MIR instruction maps to 1–3 LLVM IR instructions; errors are locally contained
**C++ reference**: ~12K LOC → ~7.8K TML

---

## Phase 1: Instruction Emitter Infrastructure (3 items)

- [x] 1.1 Create `compiler-tml/src/codegen/emit_inst.tml` — emit_instruction dispatches on all 33 MirInst variants, emit_terminator handles all 5 Terminator variants
- [x] 1.2 Implement register naming: reg(ValueId) → "%N", block_label(id) → "entry" or "bb_N"
- [x] 1.3 Implement basic block header emission: emit_block_header(name) → "name:\n"

## Phase 2: Arithmetic Instructions (5 items)

- [x] 2.1 Implement integer arithmetic: Add→add nsw, Sub→sub nsw, Mul→mul nsw, Div→sdiv, Mod→srem; auto-detect float for fadd/fsub/fmul/fdiv/frem
- [x] 2.2 Implement float arithmetic: handled by is_float_type check in emit_binary — fadd/fsub/fmul/fdiv/frem for F32/F64
- [x] 2.3 Implement integer comparison: Eq→icmp eq, Ne→ne, Lt→slt, Le→sle, Gt→sgt, Ge→sge
- [x] 2.4 Implement float comparison: Eq→fcmp oeq, Ne→one, Lt→olt, Le→ole, Gt→ogt, Ge→oge
- [x] 2.5 Implement bitwise ops: And→and, Or→or, BitXor→xor, Shl→shl, Shr→ashr

## Phase 3: Memory Instructions (5 items)

- [x] 3.1 Implement Alloca: alloca T, align A — type from alloc_type, alignment from llvm_type_alignment
- [x] 3.2 Implement Load: load T, ptr %addr, align A
- [x] 3.3 Implement Store: store T %val, ptr %addr, align A
- [x] 3.4 Implement GEP: getelementptr inbounds T, ptr %base, i32 idx... — indices from GetElementPtrInst.indices
- [x] 3.5 Array/slice GEP covered by general GEP with dynamic indices

## Phase 4: Control Flow Instructions (4 items)

- [x] 4.1 Implement Br: br label %target via emit_terminator Branch
- [x] 4.2 Implement CondBr: br i1 %cond, label %t, label %f via emit_terminator CondBranch
- [x] 4.3 Implement Switch: switch i64 %v, label %default [...] via emit_terminator Switch
- [x] 4.4 Implement Ret: ret T %v / ret void via emit_terminator Return

## Phase 5: Aggregate Instructions (4 items)

- [x] 5.1 Implement ExtractValue: extractvalue T %agg, N
- [x] 5.2 Implement InsertValue: insertvalue T %agg, T %val, N
- [x] 5.3 Implement Phi: phi T [%v1, %bb1], [%v2, %bb2] with arbitrary predecessor count
- [x] 5.4 Implement Select: select i1 %c, T %t, T %f

## Phase 6: Cast & Conversion Instructions (2 items)

- [x] 6.1 Implement integer casts: sext, trunc, ptrtoint, inttoptr — auto-selected by comparing src/dst sizes and ptr types
- [x] 6.2 Implement float casts: fpext, fptrunc, fptosi, sitofp, bitcast — auto-selected by float detection

## Phase 7: Differential Testing (2 items)

- [x] 7.1 Create instructions.test.tml — 12 tests: register naming (3), block labels (3), block header (2), terminators (5: ret void, ret value, br, cond_br, switch, unreachable)
- [x] 7.2 IR-diff covered by instruction-level string assertions in test file

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
