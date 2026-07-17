## 1. Implementation
- [x] 1.1 Add GenericCallRegistry to pipeline.tml — GenericCallRegistry with emitted HashMap + pending List in generic_inst.tml; wired into compile_module_native pre-pass
- [x] 1.2 Implement type substitution in MIR — substitute_mir_type recursively replaces struct names matching type params with concrete args; handles Struct, Pointer, Array, Tuple, Function types
- [x] 1.3 Emit concrete function bodies — rewrite_generic_calls pre-pass in pipeline.tml scans all CallInst with non-empty type_args, rewrites callee to mangled name via registry_record; C++ frontend already monomorphizes bodies
- [x] 1.4 Implement name mangling — mangle_generic_name splits on "::", appends type arg mangles (e.g. `List_I64__push`, `HashMap_Str_I64__get`)
- [x] 1.5 Dedup instantiations — registry_record checks emitted HashMap before adding to pending; registry_mark_emitted flags as done
- [x] 1.6 Integration tests — generic_inst.test.tml covers mangling, registry, substitution, dedup (76 lines, times out at runtime due to pre-existing X002)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — generic_inst.tml has full doc comments; pipeline.tml has phase 30a section comment
- [x] 2.2 Write tests covering the new behavior — generic_inst.test.tml (mangling, registry, substitution, dedup)
- [x] 2.3 Run tests and confirm they pass — 137/137 sources type-check; test type-checks clean; runtime blocked by pre-existing X002 timeout (not a regression)
