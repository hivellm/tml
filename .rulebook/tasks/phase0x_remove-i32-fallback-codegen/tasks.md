## 1. MIR codegen
- [x] 1.1 Fix `mir/instructions.cpp:483` — AwaitInst result_type

## 2. AST codegen — expressions
- [x] 2.1 Fix `method_generic.cpp:706` — closure param type
- [x] 2.2 Fix `method_generic.cpp:785` — func param type
- [x] 2.3 Fix `method_generic.cpp:857` — generic func return type
- [x] 2.4 Fix `method_class.cpp:70` — class method return type
- [x] 2.5 Fix `binary.cpp:168` — deref assignment type
- [x] 2.6 Fix `binary.cpp:804` — array element type
- [x] 2.7 Fix `closure.cpp:146` — closure param type
- [x] 2.8 Fix `method_primitive_ext.cpp:652` — primitive ext arg type
- [x] 2.9 Fix `unary.cpp:550` — deref operation type
- [x] 2.10 Fix `call_indirect.cpp:42,216` — indirect call return type
- [x] 2.11 Fix `method_dyn.cpp:134` — dyn trait method return type
- [x] 2.12 Fix `method_impl_module.cpp:370` — impl module arg type

## 3. AST codegen — builtins
- [x] 3.1 Fix `intrinsics_extended.cpp:246` — checked_add/sub/mul[T]
- [x] 3.2 Fix `mem.cpp:259` — mem::zeroed[T] type

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 4.1 Update CHANGELOG.md (v0.3.25)
- [x] 4.2 Run compiler test suite — confirm no regressions (spot check 5/5 pass)
- [x] 4.3 Run core test suite — confirm no regressions
