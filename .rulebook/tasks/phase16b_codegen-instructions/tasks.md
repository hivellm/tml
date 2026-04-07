# Tasks: Codegen Instructions — Rewrite in TML

**Status**: Planned (0/25)
**Depends on**: phase16a (type emission and struct layouts available)
**Blocks**: phase16c (calls need instruction emission), phase16d (legacy path needs instruction layer)
**Duration**: 5–7 weeks
**Risk**: Medium — each MIR instruction maps to 1–3 LLVM IR instructions; errors are locally contained
**C++ reference**: ~12K LOC → ~7.8K TML

---

## Phase 1: Instruction Emitter Infrastructure (3 items)

- [ ] 1.1 Create `compiler-tml/src/codegen/emit_inst.tml` — `InstructionEmitter` struct: current function context, register name counter, label counter, `emit(inst: MirInst) -> Text`
- [ ] 1.2 Implement register naming: `next_reg() -> Text` returns `%r0`, `%r1`, ... matching C++ SSA numbering
- [ ] 1.3 Implement basic block header emission: `emit_block_header(label: Str) -> Text` → `"{label}:\n"` with correct indentation (2-space indent for instructions)

## Phase 2: Arithmetic Instructions (5 items)

- [ ] 2.1 Implement integer arithmetic: `Add(a, b)` → `%r = add nsw i64 %a, %b`, `Sub` → `sub nsw`, `Mul` → `mul nsw`, integer division → `sdiv`/`udiv`, remainder → `srem`/`urem`
- [ ] 2.2 Implement float arithmetic: `FAdd` → `fadd double %a, %b`, `FSub`, `FMul`, `FDiv` — all with `"double"` for F64 and `"float"` for F32
- [ ] 2.3 Implement integer comparison: `ICmp(pred, a, b)` → `%r = icmp eq i64 %a, %b`; map TML predicates to LLVM: `Eq→eq`, `Ne→ne`, `Lt→slt`, `Le→sle`, `Gt→sgt`, `Ge→sge`, `ULt→ult`, etc.
- [ ] 2.4 Implement float comparison: `FCmp(pred, a, b)` → `%r = fcmp oeq double %a, %b`; ordered predicates: `oeq`, `one`, `olt`, `ole`, `ogt`, `oge`
- [ ] 2.5 Implement bitwise ops: `And` → `and i64`, `Or` → `or i64`, `Xor` → `xor i64`, `Shl` → `shl i64`, `LShr` → `lshr i64`, `AShr` → `ashr i64`

## Phase 3: Memory Instructions (5 items)

- [ ] 3.1 Implement `Alloca`: `%r = alloca %struct.Foo, align 8` — size from `LayoutComputer`, alignment from type alignment rules
- [ ] 3.2 Implement `Load`: `%r = load i64, ptr %addr, align 8` — type from MIR operand type, alignment from layout
- [ ] 3.3 Implement `Store`: `store i64 %val, ptr %addr, align 8` — void result (no register assigned)
- [ ] 3.4 Implement `GEP` (GetElementPtr): `%r = getelementptr inbounds %struct.Foo, ptr %base, i32 0, i32 N` — field index N from `LayoutComputer::field_index()`, nested GEP for deep field paths
- [ ] 3.5 Implement array/slice GEP: `%r = getelementptr inbounds i64, ptr %base, i64 %idx` — element type from array type, dynamic index operand

## Phase 4: Control Flow Instructions (4 items)

- [ ] 4.1 Implement `Br` (unconditional branch): `br label %target_bb`
- [ ] 4.2 Implement `CondBr` (conditional branch): `br i1 %cond, label %true_bb, label %false_bb`
- [ ] 4.3 Implement `Switch`: `switch i64 %val, label %default [ i64 0, label %case0  i64 1, label %case1 ... ]` — used for `when` expression emission
- [ ] 4.4 Implement `Ret`: `ret i64 %val` for value returns, `ret void` for unit/sret functions, `ret {}` for unit struct returns

## Phase 5: Aggregate Instructions (4 items)

- [ ] 5.1 Implement `ExtractValue`: `%r = extractvalue { i64, i32 } %agg, 0` — used for tuple field access and enum discriminant extraction
- [ ] 5.2 Implement `InsertValue`: `%r = insertvalue { i64, i32 } %agg, i64 %val, 0` — used for struct/tuple construction without alloca
- [ ] 5.3 Implement `Phi`: `%r = phi i64 [ %v1, %bb1 ], [ %v2, %bb2 ]` — emitted by mem2reg pass output; emitter must handle arbitrary predecessor count
- [ ] 5.4 Implement `Select`: `%r = select i1 %cond, i64 %true_val, i64 %false_val` — used for branchless ternary patterns from the optimizer

## Phase 6: Cast & Conversion Instructions (2 items)

- [ ] 6.1 Implement integer casts: `ZExt` → `zext i32 %v to i64`, `SExt` → `sext`, `Trunc` → `trunc i64 %v to i32`, `PtrToInt` → `ptrtoint ptr %p to i64`, `IntToPtr` → `inttoptr i64 %v to ptr`
- [ ] 6.2 Implement float casts: `FPExt` → `fpext float %v to double`, `FPTrunc` → `fptrunc double %v to float`, `FPToSI` → `fptosi double %v to i64`, `SIToFP` → `sitofp i64 %v to double`, `Bitcast` → `bitcast`

## Phase 7: Differential Testing (2 items)

- [ ] 7.1 Create `compiler-tml/tests/codegen/instructions.test.tml` — unit tests: for each MIR instruction variant, `emit(inst)` must produce the expected LLVM IR string; cover at least 2 test cases per instruction kind
- [ ] 7.2 IR-diff: compile 10 stdlib functions through TML instruction emitter → compare instruction-by-instruction against C++ `instructions.cpp` output; zero differences required

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
