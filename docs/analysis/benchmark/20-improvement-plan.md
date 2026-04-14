# 20 — Plano de Melhoria de Performance do TML

Plano em 5 fases, ordenado por impacto × esforço. Cada fase tem pré-requisitos claros e métricas de sucesso.

---

## Fase 0 — Desbloqueio (3 bugs, ~2-3 dias)

**Pré-requisito**: Nenhum. Execução imediata.
**Meta**: Desbloquear 42% do stdlib para benchmarking.

| # | Task | Bug | Módulos Desbloqueados | Esforço |
|---|------|-----|----------------------|---------|
| 0.1 | `phase24a` | K001 `str::len` símbolo ausente | ~80 (str, text, fmt, json, url, mime) | 4-8h |
| 0.2 | `phase24b` | K001 bool `i32` vs `i1` | ~10 (json, bool-heavy code) | 2-4h |
| 0.3 | `phase24c` | N002 crypto `.obj` linking | ~100 (crypto, tls, net, http) | 2-4h |

**Métrica de sucesso**:
- `string_bench.tml`, `text_bench.tml`, `json_bench.tml`, `crypto_bench.tml`, `large_scale_bench.tml` compilam e rodam
- Coverage de benchmarks sobe de 8% → ~40% (core) e 1% → ~20% (std)

**Depois da Fase 0**: Re-rodar TODOS os benchmarks e atualizar a análise com dados reais para string, JSON, crypto, text, networking.

---

## Fase 1 — Quick Wins de Codegen (5 itens, ~1-2 semanas)

**Pré-requisito**: Fase 0 completa.
**Meta**: Fechar os gaps de 3-10x em control flow e collections com mudanças de baixa complexidade no codegen.

### 1.1 — Emit `switch` para `when` denso

**Gap**: G-005 (9.5x), G-020 (3.9x)
**Arquivo**: `compiler/src/codegen/instructions.cpp` ou `compiler/src/mir/emit_inst.cpp`
**Mudança**: Detectar `when` com padrões inteiros consecutivos → emitir LLVM `switch` ao invés de cascata `br`

```llvm
; ANTES (TML atual)
%cmp0 = icmp eq i64 %v, 0
br i1 %cmp0, label %case0, label %check1
check1:
%cmp1 = icmp eq i64 %v, 1
br i1 %cmp1, label %case1, label %check2
; ... 10 branches

; DEPOIS
switch i64 %v, label %default [
  i64 0, label %case0
  i64 1, label %case1
  ; ... LLVM gera jump table automaticamente
]
```

**Impacto esperado**: `when` dense 5-10x mais rápido. `when` sparse 2x (LLVM faz binary search).
**Esforço**: ~100 linhas de C++. Baixo risco.

### 1.2 — Inline `List.push()`, `List.pop()`, `List.get()`

**Gap**: G-015 (5.5x), G-016 (5x+), G-021 (3.6x), G-027 (2.4x)
**Arquivo**: `lib/core/src/collections/list.tml` ou atributo `@inline` no compilador
**Mudança**: Marcar `push`, `pop`, `get`, `set` como `@inline` ou `@always_inline`

**Impacto esperado**: 2-3x em todas operações de List. Bounds check pode ser eliminado pelo LLVM quando inlined.
**Esforço**: Se `@inline` já funciona: 4 linhas. Se precisa implementar: ~200 linhas de C++.

### 1.3 — Fix short-circuit boolean codegen

**Gap**: G-018 (4.2x), G-019 (3.8x)
**Arquivo**: `compiler/src/codegen/binary_ops.cpp` ou `compiler/src/mir/emit_inst.cpp`
**Mudança**: `and`/`or` geram basic blocks desnecessários. Reduzir a 1 branch condicional por operador.

```llvm
; ANTES (TML — 3 basic blocks por `and`)
%a = icmp ...
br i1 %a, label %eval_b, label %false
eval_b:
%b = icmp ...
br label %merge
merge:
%result = phi i1 [%b, %eval_b], [false, %entry]

; DEPOIS (otimizado — 1 branch)
%a = icmp ...
%b = icmp ...
%result = and i1 %a, %b  ; ou select se short-circuit necessário
```

**Impacto esperado**: 2-4x para expressões booleanas compostas.
**Esforço**: ~50-100 linhas. Baixo risco.

### 1.4 — Bounds-check elimination para `for-in`

**Gap**: G-015 (5.5x), G-016 (5x+)
**Arquivo**: MIR optimization pass ou codegen
**Mudança**: Em `for i in 0 to list.len()`, o `list.get(i)` não precisa de bounds check — `i` é provably `< len`.

**Impacto esperado**: 2-3x para iteração sequencial sobre List.
**Esforço**: ~200 linhas (novo MIR pass). Médio risco.

### 1.5 — Lower if-else para CMOV/select

**Gap**: G-010 (8.4x)
**Arquivo**: Codegen emission
**Mudança**: If-else simples com resultado escalar → `select` ao invés de phi-node

```llvm
; ANTES
br i1 %cond, label %then, label %else
then: br label %merge
else: br label %merge
merge: %r = phi i64 [%a, %then], [%b, %else]

; DEPOIS
%r = select i1 %cond, i64 %a, i64 %b
```

**Impacto esperado**: 4-8x para if-else simples (evita branch misprediction).
**Esforço**: ~100 linhas. Baixo risco — LLVM já faz isso com `-O2`, mas emitir direto é melhor.

### Métricas Fase 1

| Benchmark | Antes | Meta |
|-----------|-------|------|
| When Dense (10 cases) | 3 ns (331M ops/sec) | <1 ns (>2B ops/sec) |
| Short-Circuit AND | 4 ns (236M ops/sec) | 1-2 ns (>500M ops/sec) |
| List Random Access | 3 ns (258M ops/sec) | 1-2 ns (>500M ops/sec) |
| List Push (reserved) | 5 ns (170M ops/sec) | 2-3 ns (>350M ops/sec) |
| If-Else Chain (4) | 1 ns (509M ops/sec) | <1 ns (>2B ops/sec) |

---

## Fase 2 — LLVM Optimization Pipeline (3 itens, ~2-3 semanas)

**Pré-requisito**: Fase 1 completa.
**Meta**: Fechar o gap de 10-18x em struct/memory operations habilitando otimizações LLVM.

### 2.1 — Habilitar `-O2` para builds release do TML

**Gap**: G-001 a G-004 (10-18x), G-013 (9.2x), todos os struct gaps
**Arquivo**: `compiler/src/codegen/llvm_codegen.cpp` ou equivalente
**Mudança**: Adicionar flag `--release` / `-O` ao `tml build` que passa `-O2` ao LLVM

O LLVM já sabe fazer:
- `mem2reg`: promover alloca+store+load para registradores (fecha 10-18x)
- `sroa`: scalar replacement of aggregates (structs em registradores)
- `instcombine`: simplificar instruções redundantes
- `loop-vectorize`: auto-vetorizar loops simples
- `inline`: inlinar funções pequenas automaticamente

**Impacto esperado**: 5-15x para struct ops. 2-4x para loops. 2x geral.
**Esforço**: ~50-100 linhas (adicionar OptimizationLevel ao PassManager). Baixo risco — LLVM faz o trabalho.

### 2.2 — Use `insertvalue` para construção de structs

**Gap**: G-001 a G-004
**Arquivo**: Codegen de struct construction
**Mudança**: Gerar `insertvalue` chain ao invés de alloca+GEP+store

```llvm
; ANTES
%p = alloca %Point
%gep_x = getelementptr %Point, ptr %p, i32 0, i32 0
store i64 %x, ptr %gep_x
%gep_y = getelementptr %Point, ptr %p, i32 0, i32 1
store i64 %y, ptr %gep_y
%result = load %Point, ptr %p

; DEPOIS
%p0 = insertvalue %Point undef, i64 %x, 0
%p1 = insertvalue %Point %p0, i64 %y, 1
; sem alloca, sem load — tudo em registradores
```

**Impacto esperado**: 3-5x imediato, sem depender de `-O2`.
**Esforço**: ~200-300 linhas de refactor no codegen. Médio risco.

### 2.3 — NRVO (Named Return Value Optimization)

**Gap**: Method chaining 115 ns/op
**Arquivo**: Codegen de return-by-value
**Mudança**: Quando uma função retorna um struct construído localmente, construir direto no caller's memory (evitar copy).

**Impacto esperado**: 3-5x para method chaining e builder patterns.
**Esforço**: ~300-500 linhas. Médio-alto risco — requer mudança na calling convention.

### Métricas Fase 2

| Benchmark | Antes | Meta |
|-----------|-------|------|
| Stack Struct Small (16B) | 2 ns (412M) | <1 ns (>2B) |
| Struct Field Access | 2 ns (475M) | <1 ns (>3B) |
| Nested Struct Access | 3 ns (284M) | <1 ns (>2B) |
| Point Creation | 3 ns (324M) | <1 ns (>2B) |
| Method Chaining | 115 ns (8.6M) | 10-20 ns (>50M) |
| OOP Object Creation | 24 ns (40.8M) | 3-5 ns (>200M) |

---

## Fase 3 — Compilation Speed (4 itens, ~2-4 semanas)

**Pré-requisito**: Pode rodar em paralelo com Fases 1-2.
**Meta**: Reduzir tempo de compilação de 27x para <10x vs Rust.

### 3.1 — Lazy-load de plugins DLL

**Gap**: 2-3s de overhead por compilação (30% do tempo total)
**Arquivo**: `compiler/src/cli/dispatcher.cpp`, plugin loading
**Mudança**: Não carregar `tml_codegen_x86.dll` (63MB) até que o codegen seja necessário. Usar `LoadLibraryEx` com `DONT_RESOLVE_DLL_REFERENCES` para fast load.

**Impacto**: -2s por compilação (27x → ~20x).
**Esforço**: ~100-200 linhas.

### 3.2 — Build release do compilador TML

**Gap**: O compilador roda em debug mode (sem `-O2` no C++)
**Arquivo**: `scripts/build.bat`, CMake configuration
**Mudança**: Adicionar opção `--release-compiler` que compila o C++ do compilador com `-O2`

**Impacto**: 2-3x mais rápido (27x → ~10x). O compilador em si fica 2-3x mais rápido.
**Esforço**: Baixo (mudar CMake flags), mas build leva mais tempo.

### 3.3 — Daemon mode (compiler-as-service)

**Gap**: Plugin loading + initialization repetida
**Arquivo**: `build/debug/bin/tml_daemon.exe` (já existe)
**Mudança**: Usar o daemon para manter o compilador residente em memória. `tml build` envia request ao daemon via IPC.

**Impacto**: Elimina plugin loading completamente. Cold start ~7s → warm start ~2-3s.
**Esforço**: Daemon já existe. Precisa integrar com `tml build/run`.

### 3.4 — Parallel function codegen

**Arquivo**: LLVM module builder
**Mudança**: Compilar funções independentes em threads paralelas usando LLVM ThreadSafeModule.

**Impacto**: 2-3x para programas com muitas funções.
**Esforço**: ~500 linhas. Alto risco — thread safety no codegen.

### Métricas Fase 3

| Benchmark | Antes | Meta |
|-----------|-------|------|
| hashmap_bench compile | 9.8s | <2s |
| list_bench compile | 10.1s | <2s |
| math_bench compile | 7.1s | <1.5s |
| Ratio vs Rust | 27x | <8x |

---

## Fase 4 — Advanced Optimizations (5 itens, ~1-2 meses)

**Pré-requisito**: Fases 1-2 completas.
**Meta**: Atingir paridade com Rust para os casos restantes.

### 4.1 — Devirtualização de function pointers

**Gap**: G-011 (8.2x), G-012 (8.4x)
**Mudança**: MIR pass que rastreia atribuições de function pointers e substitui indirect calls por direct calls quando o alvo é conhecido.

**Impacto**: 4-8x para fn pointers e closures com alvo conhecido.

### 4.2 — Closure inlining

**Gap**: G-012 (8.4x)
**Mudança**: Quando um closure é passado para `map`/`filter`/`fold` e o corpo é pequeno, inlinar o corpo no loop.

**Impacto**: 2-4x para operações funcionais sobre coleções.

### 4.3 — Auto-vectorization hints

**Gap**: G-014 (6.7x), G-017 (4.3x), G-029 (2.2x)
**Mudança**: Emitir `!llvm.loop` metadata em loops sobre arrays/lists. Garantir que o loop body não tenha function calls que bloqueiam vectorization.

**Impacto**: 4-8x para loops aritméticos sobre arrays.

### 4.4 — LTO (Link-Time Optimization)

**Gap**: G-026 (binary size 2.4x)
**Mudança**: Emitir LLVM bitcode ao invés de object files. Linkar com LTO para eliminar dead code cross-module.

**Impacto**: 30-40% redução no tamanho do binário. Bonus: 5-15% performance.

### 4.5 — Escape analysis → stack promotion

**Mudança**: MIR pass que detecta heap allocations cujo lifetime não escapa da função → promover para stack.

**Impacto**: Elimina allocations em tight loops (encoding, string building).

### Métricas Fase 4

| Benchmark | Antes (pós Fase 2) | Meta |
|-----------|---------------------|------|
| Function Pointer | 1 ns (593M) | <1 ns (>2B) |
| Filter Simulation | 1 ns (729M) | <1 ns (>3B) |
| Integer Addition | <1 ns (1.2B) | <1 ns (>4B) |
| Binary size | ~350 KB | ~200 KB |

---

## Fase 5 — Memory Safety & Correctness (3 itens, ~1-2 meses)

**Pré-requisito**: Pode rodar em paralelo com Fase 4.
**Meta**: Eliminar leaks e adicionar RAII.

### 5.1 — Fix memory leaks em encoding

**Gap**: 200K leaks, 2.8MB lost no encoding_bench
**Mudança**: Audit de todas as funções em `core::encoding` que retornam `Str`. Garantir que o caller ou runtime libera a memória.

**Impacto**: Correctness. Previne OOM em servidores long-running.

### 5.2 — Implementar `Drop` / `Disposable` behavior

**Mudança**: Adicionar suporte a `impl Drop for T { func drop(self) { ... } }` no compilador. O codegen insere chamadas a `drop()` no fim do escopo.

**Impacto**: Elimina necessidade de `.destroy()` manual. Previne leaks por design.

### 5.3 — String interning / SSO (Small String Optimization)

**Mudança**: Strings < 24 bytes armazenadas inline no struct (sem heap allocation). Strings repetidas compartilham o mesmo buffer.

**Impacto**: 80% menos allocations para código típico (keys de JSON, nomes de variáveis, paths curtos).

---

## Resumo: Impacto Cumulativo

| Fase | Tempo | Performance Gain | Compilation | Coverage |
|------|-------|-----------------|-------------|----------|
| **0 — Desbloqueio** | 2-3 dias | — | — | 8% → 40% |
| **1 — Quick Wins** | 1-2 sem | 3-10x (control flow, list) | — | — |
| **2 — LLVM Pipeline** | 2-3 sem | 5-18x (structs, memory) | — | — |
| **3 — Compile Speed** | 2-4 sem | — | 27x → <8x | — |
| **4 — Advanced** | 1-2 meses | 4-8x (closures, vectors) | — | — |
| **5 — Memory** | 1-2 meses | correctness | — | — |

### Projeção: TML vs Rust Após Todas as Fases

| Category | Hoje | Pós Fase 2 | Pós Fase 4 |
|----------|------|-----------|-----------|
| Arithmetic | 1.0x | 1.0x | 1.0x |
| Control flow | 3-9.5x | 1-2x | 1.0x |
| Collections | 2-5x | 1.5-2x | 1-1.5x |
| Struct access | 10-18x | 1-2x | 1.0x |
| Function dispatch | 8x | 8x | 1-2x |
| Closures/iterators | 8x | 8x | 1-2x |
| Compilation | 27x | 27x | <8x |
| Binary size | 2.4x | 2.4x | 1.5x |
| Memory leaks | Present | Present | Fixed |

**Alvo final**: TML within **1-2x de Rust** em todas as categorias medidas, com compilação <10x e zero memory leaks.
