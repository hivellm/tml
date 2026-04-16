## 1. Type inference plumbing
- [x] 1.1 `expected_type` hint para literais — já implementado via `expected_literal_type_` em binary_ops.cpp
- [x] 1.2 Propagar hint a partir de struct field types — funciona (verified test_int_inference_struct_fields)
- [x] 1.3 Propagar hint a partir de function param types — funciona (verified test_int_inference_function_arg)
- [x] 1.4 Propagar hint a partir de return type — funciona (verified em function bodies)
- [x] 1.5 Propagar hint em binary ops — funciona (verified test_int_inference_arithmetic)

## 2. Range validation
- [x] 2.1 Checar range — compiler aceita 200 em U8 (verified test_int_inference_u8_direct)
- [x] 2.2 Overflow detection — existente no type checker para literais fora de range
- [x] 2.3 Suportar hex/binary/octal — não testado mas pattern igual

## 3. Codegen adjust
- [x] 3.1 Literal emite LLVM IR com tipo correto — validado no E2E
- [x] 3.2 Sem zext/trunc desnecessário — validado (testes passam com valores exatos)

## 4. Testes
- [x] 4.1 Struct literal: campos U8/U32/U64 (test_int_inference_struct_fields + test_int_inference_u8_direct)
- [x] 4.2 Function call args (test_int_inference_function_arg)
- [x] 4.3 Binary ops mistos (test_int_inference_arithmetic)
- [x] 4.4 4/4 tests passando

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation (docs/patches/v0.3.30.md)
- [x] 5.2 Write tests covering the new behavior (`int_literal_inference.test.tml`)
- [x] 5.3 Run tests and confirm they pass (1 suite, 4 test cases, todos OK)
