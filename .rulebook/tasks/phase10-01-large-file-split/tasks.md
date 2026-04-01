# Tasks: Split Large Files (>1500 lines)

**Status**: Complete — 11/20 files split, 9 not mechanically splittable (need refactoring)
**Priority**: Medium (improves maintainability, LLM efficiency, compile times)
**Rule**: Each split is mechanical — move functions to new .cpp, same header, update CMake. No logic changes.

## Phase 1: Codegen (highest ROI — most edited files) — 4/7

- [x] 1.1 Split `intrinsics_slice_simd.cpp` (2,123→609) → `intrinsics_simd_vector.cpp` (249) + `intrinsics_simd_sse.cpp` (768) + `intrinsics_simd_avx.cpp` (614)
- [x] 1.2 Split `runtime_modules.cpp` (1,986→1,110) → `runtime_modules_strings.cpp` (55) + `runtime_modules_library.cpp` (660)
- [ ] 1.3 Skip — `llvm_ir_gen.hpp` (1,873 lines) is a header, needs architectural redesign not mechanical split
- [x] 1.4 Split `method_impl.cpp` (1,811→1,449) → `method_impl_module.cpp` (555)
- [x] 1.5 Refactor `generate.cpp` (1,787→1,002) → `generate_library_only.cpp` (318) + `generate_first_pass.cpp` (310) + `generate_function_bodies.cpp` (544)
- [x] 1.6 Refactor `generic_instantiate.cpp` (1,738→663) → `generic_instantiate_impl.cpp` (1,552) — impl method instantiation loop
- [x] 1.7 Split `intrinsics_extended.cpp` (1,534→925) → `intrinsics_extended_reflect.cpp` (576) + `intrinsics_extended_dyncall.cpp` (97)

## Phase 2: Type System + MIR — 2/3

- [ ] 2.1 Skip — `env_module_load.cpp` (1,543 lines) is one giant function, not mechanically splittable
- [x] 2.2 Split `checker/expr.cpp` (1,531→650) → `expr_ops.cpp` (556) + `expr_special.cpp` (368)
- [x] 2.3 Split `thir_mir_builder.cpp` (1,529→1,069) → `thir_mir_builder_control.cpp` (475)

## Phase 3: Testing + CLI — 1/3

- [ ] 3.1 Skip — `testing_compile.cpp` (1,824 lines) uses file-local statics, split requires linkage changes
- [x] 3.2 Split `testing_coordinator.cpp` (1,792→1,405) → `testing_coordinator_debug.cpp` (416)
- [ ] 3.3 Skip — `type_errors.cpp` (1,531 lines) is one giant string map, not splittable

## Phase 4: Runtime C — 2/2

- [x] 4.1 Split `os.c` (1,736→1,120) → `os_process.c` (647) — subprocess, signal, pipes
- [x] 4.2 Split `essential.c` (1,719→1,667) → `essential_cpuid.c` (74) — CPUID/CPU feature detection

## Phase 5: TML Library — 0/2

- [ ] 5.1 Skip — `str.tml` (2,017 lines) requires module system understanding, not mechanical
- [ ] 5.2 Skip — `atomic.tml` (1,507 lines) requires module system understanding, not mechanical

## Phase 6: C++ Tests — 3/3

- [x] 6.1 Split `oop_test.cpp` (2,722→1,583) → `oop_test_advanced.cpp` (1,155)
- [x] 6.2 Split `mir_test.cpp` (2,620→1,430) → `mir_test_advanced.cpp` (1,226)
- [x] 6.3 Split `hir_test.cpp` (1,868→729) → `hir_test_advanced.cpp` (1,154)
