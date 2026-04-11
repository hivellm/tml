## 1. Pattern guards
- [ ] 1.1 Parse `if <expr>` after PrimaryPattern in when-arm patterns
- [ ] 1.2 Add guard expression field to WhenArm AST node
- [ ] 1.3 Wire guard through HIR lowering
- [ ] 1.4 In MIR: after pattern match succeeds, emit conditional branch on guard → arm body or next arm

## 2. Or-patterns
- [ ] 2.1 Parse `<pattern> | <pattern>` as OrPattern in when-arms
- [ ] 2.2 Add OrPattern AST node
- [ ] 2.3 In MIR: emit match attempt for each alternative, share arm body

## 3. Tail (mandatory)
- [ ] 3.1 Add parser tests: `Just(x) if x > 0 => ...`, `North | South => ...`
- [ ] 3.2 Add codegen tests: guard filters matches correctly, or-pattern matches alternatives
- [ ] 3.3 Update CHANGELOG.md
- [ ] 3.4 Run tests and confirm they pass
