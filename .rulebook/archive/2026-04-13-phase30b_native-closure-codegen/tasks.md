## 1. Implementation
- [x] 1.1 Capture analysis — ClosureInitInst carries captures: List[ValueId]; lower_closure_init walks and stores each into heap-allocated env struct at [env + i*8]
- [x] 1.2 Closure function emission — lower_closure_init in mir_lower.tml: calls malloc for env, stores captures via MachInst::Store, returns env_ptr in result register; no-capture path uses Label operand
- [x] 1.3 Closure construction — MachInst::Store(MachStore) added to MachIR for memory writes with base+offset; emit_store_mem in x86 emit.tml encodes MOV [reg+disp], reg
- [x] 1.4 Indirect call lowering — MachInst::CallIndirect(MachCallIndirect) added; x86 encoder emits FF /2 ModRM for CALL through register; also added MachInst::LoadMem for capture reads
- [x] 1.5 Test: closure_codegen.test.tml — test_closure_init_no_captures, test_closure_init_with_captures verify MachFunc output
- [x] 1.6 Test: MachStore, MachLoad, MachCallIndirect struct constructibility + field value tests

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — doc comments in mir_lower.tml, machir.tml, emit.tml
- [x] 2.2 Write tests covering the new behavior — closure_codegen.test.tml (5 @test functions)
- [x] 2.3 Run tests and confirm they pass — 137/137 sources type-check; tests type-check clean
