## 1. Implementation
- [ ] 1.1 Factor out `emit_pattern_match(pat, src_ptr, mismatch_label)` helper in emit_let.tml: generates the comparison/load sequence for a single pattern node and jumps to mismatch_label on failure; existing simple name and tuple branches are the first callers
- [ ] 1.2 Nested enum pattern: recursively call emit_pattern_match on the inner pattern using the variant payload pointer (base + tag-field size) as the new src_ptr; support arbitrary nesting depth
- [ ] 1.3 Guard pattern: after binding all variables from the inner pattern, emit a conditional branch on the guard expression; jump to mismatch_label if the guard evaluates to false
- [ ] 1.4 Or-pattern: emit a sequence of pattern-match attempts for each alternative; on the first successful match fall through; if all alternatives fail jump to mismatch_label
- [ ] 1.5 Nested struct pattern: for each field sub-pattern, compute the field's GEP offset from the struct type layout, load the field value, and recursively call emit_pattern_match
- [ ] 1.6 Integration test: `let Just(Just(x)) = v else { ... }` — nested enum; `let p if p > 0 = expr else { ... }` — guard; `let A | B = v else { ... }` — or-pattern; `let Point { x, y } = pt` — nested struct; all four forms produce correct bindings

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
