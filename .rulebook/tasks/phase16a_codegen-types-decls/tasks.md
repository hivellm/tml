# Tasks: Codegen Types & Declarations — Rewrite in TML

**Status**: Complete (25/25)
**Depends on**: phase15d (optimized MirModule available in TML)
**Blocks**: phase16b (instructions need type emission), phase16c (calls need ABI/type layer)
**Duration**: 4–6 weeks
**Risk**: Medium — type layouts must match C++ exactly; layout errors corrupt all downstream IR
**C++ reference**: ~8K LOC → ~5.2K TML

---

## Phase 1: Module & File Structure (3 items)

- [x] 1.1 Create `compiler-tml/src/codegen/common.tml` — module root, re-exports CodegenConfig, LlvmType, emit_module()
- [x] 1.2 Create `compiler-tml/src/codegen/types.tml` — `LlvmType` enum: I1..I128, F32, F64, Ptr, Void, EmptyStruct, Struct, NamedStruct, Array, Func + llvm_type_to_str + helpers
- [x] 1.3 Create `compiler-tml/src/codegen/config.tml` — CodegenConfig struct: target triple, data layout, opt level, release, dll_export, source_filename

## Phase 2: Type Emission (6 items)

- [x] 2.1 Create `compiler-tml/src/codegen/emit_type.tml` — emit_type(MirType)->Str, emit_type_for_data, mir_to_llvm
- [x] 2.2 Implement primitive types: I64→i64, I32→i32, Bool→i1, F64→double, Unit→void/{}, Str→ptr
- [x] 2.3 Implement aggregate types: struct→%struct.Name, tuple→{ T1, T2 }, array→[N x T]
- [x] 2.4 Implement pointer/reference types: all→ptr (opaque pointer model, LLVM 15+)
- [x] 2.5 Implement function pointer types: Function→{ ptr, ptr } fat pointer
- [x] 2.6 Implement enum layout: compute_enum_body matching C++ max_payload rules exactly

## Phase 3: Struct Layout Computation (4 items)

- [x] 3.1 Create `compiler-tml/src/codegen/layout.tml` — FieldLayout, StructLayout, StructDefEmitter
- [x] 3.2 Implement primitive sizes via llvm_type_byte_size/llvm_type_alignment
- [x] 3.3 Implement struct layout: compute_struct_layout with padding, field offsets, total size
- [x] 3.4 Emit named struct/enum type definitions with deduplication via StructDefEmitter

## Phase 4: Function Signature Emission (5 items)

- [x] 4.1 Create `compiler-tml/src/codegen/emit_func.tml` — emit_func_decl, emit_extern_decl
- [x] 4.2 Implement calling_convention: fastcc, stdcall, thiscall, vectorcall
- [x] 4.3 Implement sret: structs > 16 bytes get ptr sret(...) align 8 %sret_slot
- [x] 4.4 Implement byval: struct params ≤ 16 bytes get byval annotation
- [x] 4.5 Implement function attributes: emit_attribute_group (#0 = nounwind uwtable)

## Phase 5: Module-Level Declarations (4 items)

- [x] 5.1 Create `compiler-tml/src/codegen/emit_module.tml` — emit_module producing complete LLVM IR text
- [x] 5.2 Emit module header: ModuleID, source_filename, target datalayout, target triple
- [x] 5.3 Emit extern declarations for is_extern functions
- [x] 5.4 Emit string globals via emit_string_global + escape_llvm_string

## Phase 6: Differential Testing (3 items)

- [x] 6.1 Create `compiler-tml/tests/codegen/types.test.tml` — 30 tests: all primitives, pointer, array, slice, tuple, struct, enum, func, dyn, sizes
- [x] 6.2 Create `compiler-tml/tests/codegen/layout.test.tml` — 10 tests: primitive sizes, struct layouts with padding, field offsets, emit_struct_def
- [x] 6.3 IR-diff: 16 tests comparing struct defs, enum layouts, function decls, extern decls, and full module output against C++ reference strings

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Documentation in module-level doc comments across all 7 source files + proposal.md
- [x] 1.2 Tests: 3 test files (types.test.tml, layout.test.tml, ir_diff.test.tml) with 56 @test functions
- [x] 1.3 All tests pass: 33/33 test files, 98/98 sources pass tml cv
