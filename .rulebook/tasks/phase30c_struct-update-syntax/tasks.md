## 1. Parser
- [ ] 1.1 Parse `..expr` after field inits in struct literal (after `,` peek `..` → struct update)
- [ ] 1.2 Add StructUpdate field to StructInit AST node (Maybe[Heap[Expr]])
- [ ] 1.3 Wire through AST serialization/visitor

## 2. Type checker
- [ ] 2.1 Verify `..expr` type matches the struct being constructed
- [ ] 2.2 Compute set of unspecified fields that need copying from source

## 3. Codegen
- [ ] 3.1 For each unspecified field, emit field extraction from source expression
- [ ] 3.2 For specified fields, use explicit values
- [ ] 3.3 Combine into struct construction (insertvalue chain or alloca+store)

## 4. Tail (mandatory)
- [ ] 4.1 Add parser tests: `Point { x: 5.0, ..old }`, `Arg { name: s, ..this }`
- [ ] 4.2 Add codegen tests: verify field values match expectations
- [ ] 4.3 Update CHANGELOG.md
- [ ] 4.4 Run tests and confirm they pass
