## 1. Compile-time strlen for literals
- [ ] 1.1 In codegen method dispatch: detect when push_str argument is a StringLiteral
- [ ] 1.2 Compute string length at compile time from the literal value
- [ ] 1.3 Emit direct call to text_push_str_ptr(self, literal_ptr, KNOWN_LEN) bypassing strlen
- [ ] 1.4 Apply same optimization for concat_str and other methods taking Str literals
- [ ] 1.5 Build compiler

## 2. Benchmark gate
- [ ] 2.1 Run string_bench — Text push_str under 2 ns (from 4 ns)
- [ ] 2.2 Verify string content correctness for all literal lengths

## 3. Tail (mandatory)
- [ ] 3.1 Update docs/analysis/string/ with new numbers
- [ ] 3.2 Test: push_str with empty, 1-char, 23-char, and long literals
- [ ] 3.3 Run tests and confirm pass
