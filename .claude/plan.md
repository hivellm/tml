# Plano: Corrigir os 4 bugs de codegen que falham nos testes

## Diagnóstico

Todos os 4 bugs são **PRE-EXISTENTES no codegen** — o novo test system funciona corretamente.
O último estado bom foi 28/fev: 1096 passed, 45 failed, 73.47% coverage.

Os 45 failures se dividem em 4 categorias com raiz identificada:

---

## Bug 1: `toowned_assoc` (1 test) — FÁCIL, ~10 linhas

**Erro**: `'%this' defined with type 'i32' but expected 'ptr'`

**Causa**: `compiler/src/codegen/llvm/decl/impl.cpp:234-246` — quando `this: ref This` com `This = I32`, o codegen verifica apenas `is_mut_this` para decidir se usa `ptr`. Mas `ref This` é referência pelo TYPE, não pelo mut flag. Resultado: define `(i32 %this)` mas o body gera `load i32, ptr %this`.

**Fix**: Na linha ~234 de `impl.cpp`, adicionar check se o tipo do primeiro parâmetro é `RefType`. Se for, forçar `this_type = "ptr"`.

---

## Bug 2: `union_basic` (1 test) — FÁCIL (workaround), ~15 linhas

**Erro**: `invalid type for undef constant` — `%struct.IntOrPtr undef`

**Causa**: HIR builder e THIR lowering NÃO suportam `UnionDecl`. O tipo nunca é declarado no LLVM IR. O MIR codegen tenta usar `insertvalue` (errado para unions) com um tipo que não existe.

**Fix (workaround rápido)**: Em `query_core.cpp:551-603`, adicionar detecção de `UnionDecl` que força fallback para AST codegen (que já suporta unions via `gen_union_decl`).

---

## Bug 3: `core/mem/*` (9 tests) — MÉDIO, ~20 linhas

**Erro**: `assertion failed at :19: into_inner should return original value`

**Causa**: `compiler/src/codegen/llvm/expr/method_impl.cpp:585` — ao chamar `ManuallyDrop::into_inner(slot)`, o call site assume que o primeiro arg é sempre `ptr` para tipos não-primitivos. Mas `into_inner` tem `slot: ManuallyDrop[T]` (by-value, não `this`). Define `(%struct.ManuallyDrop__I32 %slot)` mas chama com `(ptr %t1)`. ABI mismatch → garbage.

**Fix**: No call site (~linha 585), verificar a assinatura da função. Se o primeiro parâmetro NÃO é `this`/`self`, usar o tipo real do struct ao invés de hardcoded `"ptr"`.

---

## Bug 4: `core/any/*` (4 tests) — DIFÍCIL, ~50+ linhas

**Erro**: `UNRESOLVED reference: @tml_N4core3any2ofE`

**Causa**: `TypeId::of[T]()` é uma função genérica estática. O sistema lazy library (`runtime_modules.cpp`) encontra a referência em pending mas não consegue instanciar porque não faz monomorphization de genéricos.

**Fix**: No lazy resolution pass de `runtime_modules.cpp`, quando uma referência está em pending mas é genérica, realizar a instanciação com os tipos concretos derivados do call site.

---

## Ordem de execução (do mais fácil ao mais difícil)

1. **Bug 1 (toowned_assoc)** — Fix em `impl.cpp` — 5 min
2. **Bug 2 (union_basic)** — Workaround em `query_core.cpp` — 5 min
3. **Bug 3 (core/mem)** — Fix em `method_impl.cpp` — 15 min
4. **Bug 4 (core/any)** — Fix em `runtime_modules.cpp` — 30 min

**DEPOIS de todos os 4 fixes**: rebuild compiler UMA VEZ, rodar testes UMA VEZ.

## Estratégia anti-lentidão

- **NÃO rebuildar entre cada fix** — fazer todos os 4 fixes no C++ primeiro
- **Rebuildar o compilador 1 vez** após todos os edits
- **Rodar testes 1 vez** após rebuild para validar tudo
- Se algum fix estiver errado, corrigir e rebuildar de novo
- Objetivo: máximo 2 ciclos build+test