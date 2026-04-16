# Proposal: phase1l_when-block-body-ice

## Why

A stdlib usa `when` arms com `{ }` block bodies (vide `btreemap.tml`, `cmp.tml`)
e compila corretamente. Mas em código de usuário, certos padrões com
block-body `when` arms triggam ICE (Internal Compiler Error). O trigger exato
não está documentado. Workaround seguro: usar single-expression arms +
`if/else` para multi-statement — mas isso não é documentado e quebra a
ergonomia da linguagem.

Problemas:
- Sharp edge inesperado
- ICE crasha o compilador (sem diagnóstico útil)
- MCP crasha junto com o compiler
- Usuário precisa descobrir workaround empiricamente

Source: UzDB feedback letter, P1-8.

## What Changes

1. **Auditoria:** escrever 10-15 testes minimamente reprodutíveis com `when`
   arms de bloco multi-statement em diferentes contextos:
   - fn body return
   - closure return
   - nested `when`
   - expression position
   - with guards
   - with Or-patterns
   - with enum payload destructuring
   - em loop bodies
   - em if/else bodies

2. **Trigger isolation:** identificar qual combinação causa ICE

3. **Root cause:** debuggar em `compiler/src/codegen/llvm/control/when.cpp`
   + `compiler/src/mir/thir_mir_builder_control.cpp`

4. **Fix:** corrigir codegen (provavelmente phi node mishandling ou
   last-expression-value tracking em block bodies)

5. **Regression tests:** todos os 15+ casos passam

6. **Doc update:** spec do `when` cobre block bodies explicitamente

## Impact

- Affected specs: language/when-expression
- Affected code: `compiler/src/codegen/llvm/control/when.cpp`, `compiler/src/mir/thir_mir_builder_control.cpp`
- Breaking change: NO (fix de ICE)
- User benefit: Remove sharp edge. ICE convertido em código correto. Estabilidade do MCP melhorada. P1.
