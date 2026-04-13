## 1. Implementation
- [ ] 1.1 Add GenericCallRegistry to pipeline.tml: HashMap[Str, List[List[Type]]] mapping function name to unique type-argument tuples seen at call sites
- [ ] 1.2 Implement type substitution in MIR: substitute_mir_type(ty, params, args) recursively replaces TypeParam(n) with args[n] in all MIR operand types
- [ ] 1.3 Emit one concrete function body per instantiation by cloning the generic MIR function and running substitute_mir_type on every instruction operand
- [ ] 1.4 Implement name mangling: mangle_generic(base, args) produces `base_Arg0_Arg1_...__` (e.g. `List_I64__push`) for unique symbol names
- [ ] 1.5 Dedup instantiations: check emitted_instantiations set before emitting; if already present, omit and reuse existing symbol
- [ ] 1.6 Add integration tests: List[I64] push/get round-trip and HashMap[Str,I64] set/get round-trip compiled via native backend

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
