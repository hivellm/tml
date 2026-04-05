# Relatório de Bloqueadores de Cobertura — TML Project

**Data**: 2026-03-13
**Gerado a partir de**: `build/coverage/coverage_history.jsonl` (entrada de 2026-03-13T08:11:19)

---

## 1. Resumo Executivo

| Métrica | Valor |
|---|---|
| Cobertura atual | **95,02%** (5213/5486 funções) |
| Meta | **100%** |
| Gap total | **273 funções** |
| Módulos analisados | 264 |
| Módulos com 0% | 9 módulos |
| Módulos com cobertura parcial | 42 módulos |
| Módulos com 100% | 213 módulos |

O projeto se encontra em estado avançado de cobertura. As 273 funções restantes estão distribuídas em três categorias: bloqueios técnicos (bugs de codegen/compilador), funcionalidades que requerem infraestrutura ausente (async runtime, rede real, threads), e gaps de testes escritos que ainda não foram criados.

---

## 2. Erros de Compilação

Módulos que falham ao compilar, impedindo qualquer execução de testes.

### 2.1 `std/hash` — Erro de Link OpenSSL

**Impacto**: 18 funções não cobertas (27/45 = 60,0%)

O módulo `hash` depende de símbolos OpenSSL que não são resolvidos durante a linkagem em modo de cobertura. A suite `std_hash` compila parcialmente mas falha ao linkar.

**Causa raiz**: OpenSSL é linkado condicionalmente mas o linker (LLD) não encontra os símbolos `EVP_*` quando o modo de cobertura ativa instrumentação que altera a ordem de inicialização dos objetos.

**Funções afetadas**: funções de hashing avançado (`Hmac`, `Sha256`, `Blake3`) que dependem da implementação OpenSSL subjacente.

### 2.2 `std/lowlevel` — Bug de Codegen `Maybe__I32`

**Impacto**: bloqueia testes que usam `Maybe[I32]` em contexto lowlevel

**Erro**: O codegen emite `i32` onde o ABI espera `%struct.Maybe__I32`, causando falha de verificação de tipo LLVM durante compilação da suite.

**Causa raiz**: Mismatch entre representação de enums genéricos no legacy codegen. Quando `Maybe[I32]` é usado como valor de retorno em funções `lowlevel`, o backend não aplica o boxing correto para o struct de 8 bytes `{ i32, i32 }`.

---

## 3. Crashes em Runtime

Suites que compilam com sucesso mas falham durante execução (exit code diferente de 0).

### 3.1 Suites com Exit Code 99 (Assertion Failure)

| Suite | Módulo | Causa |
|---|---|---|
| `alloc_global` | `alloc/global` | Alocação de layout global falha — 17/20 funções descobertas |
| `compiler/primitive_methods` | primitivos | `I32::MIN` assertion: `assert_eq(-2147483648, i32::MIN)` falha por overflow em modo coverage |

### 3.2 Crashes por Invalid GEP / Acesso de Memória

| Suite | Gap | Causa |
|---|---|---|
| `core/intrinsics/intrinsics_array_ops` | 10 funções | GEP inválido: índice fora dos bounds em operações de array com const generics |
| `core/future/future_fuse` | 6 funções | Acesso a future já consumido — `FusedFuture` não implementa guard correto |
| `core/ops/async_lazy_future` | 1 função | Lazy future não resolve corretamente sem executor async |

### 3.3 Crashes em Modo Coverage Only

| Suite | Gap | Causa |
|---|---|---|
| `std/sync/once_lock_get_or_init` | 1 função | Falha apenas em coverage — lock-free atomics interferem com instrumentação de cobertura |

### 3.4 Crashes por Type Mismatch em Runtime

| Suite | Gap | Causa |
|---|---|---|
| `std/zlib/zlib_zstd` | 15 funções | `Outcome` type mismatch: função retorna `Outcome[Bytes, ZlibError]` mas codegen emite `Outcome[Bytes, Str]` |

### 3.5 Crashes por Infraestrutura Ausente

| Suite | Gap | Causa |
|---|---|---|
| `std/net/net_tls` | 11 funções | TLS requer handshake com servidor real |
| `std/net/sys_socket_options` | 3 funções | Opções de socket específicas de plataforma não mockáveis |
| `std/net/tcp_timeout` | 7 funções | Timeout de TCP requer rede real ou mock de tempo |
| `other/capture` | desconhecido | Capture de output em runtime interfere com instrumentação |

### 3.6 Resumo de Crashes

```
Total de suites com crash: ~14
Funções bloqueadas por crash: ~71
```

---

## 4. Módulos com 0% de Cobertura

### 4.1 Tabela de Módulos Zerados

| Módulo | Funções | Razão do Bloqueio | Categoria |
|---|---|---|---|
| `array/ascii` | 9 | Nenhum teste escrito ainda | Gap de testes |
| `e2e/tls` | 13 | Requer servidor TLS real + certificados válidos | Infraestrutura |
| `iter/adapters/cloned` | 3 | `where I::Item = ref T` — constraint não suportado pelo type checker | Bug de compilador |
| `iter/adapters/copied` | 3 | Mesmo bloqueio: `where I::Item = ref T` | Bug de compilador |
| `iter/adapters/flatten` | 2 | Mesmo bloqueio: `where I::Item = ref T` | Bug de compilador |
| `iter/adapters/intersperse` | 2 | Mesmo bloqueio: `where I::Item = ref T` | Bug de compilador |
| `iter/adapters/peekable` | 7 | Mesmo bloqueio: `where I::Item = ref T` | Bug de compilador |
| `precompiled_symbols` | 1 | Símbolo só existe em builds pré-compilados | Não testável |
| `thread/scope` | 8 | `thread::scope` requer threads reais + join — não mockável em testes unitários | Infraestrutura |

### 4.2 Detalhamento por Categoria

**Bug de compilador — `where I::Item = ref T` (17 funções)**

Os 5 adaptadores de iteradores (`cloned`, `copied`, `flatten`, `intersperse`, `peekable`) usam a constraint associada `where I::Item = ref T`. O type checker do TML não consegue resolver associated type constraints com referências, causando erro de compilação antes mesmo de chegar ao codegen. Este é um bloqueio de feature no sistema de tipos.

**Infraestrutura ausente (21 funções)**

`e2e/tls` e `thread/scope` requerem recursos do sistema operacional que não são mockáveis nos testes unitários do TML. Seria necessário criar um mini-servidor TLS embutido ou uma implementação de scope threading para ambiente de teste.

**Gap de testes (9 funções)**

`array/ascii` não tem nenhum teste escrito. O módulo existe e compila mas nunca foi exercitado.

---

## 5. Módulos com Cobertura Parcial

### 5.1 Tabela Completa (gap >= 3), ordenada por gap decrescente

| Módulo | Total | Coberto | Gap | Cobertura |
|---|---|---|---|---|
| `hash` | 45 | 27 | 18 | 60,0% |
| `alloc/global` | 20 | 3 | 17 | 15,0% |
| `array` | 39 | 24 | 15 | 61,5% |
| `crypto/cipher` | 43 | 28 | 15 | 65,1% |
| `zlib/zstd` | 41 | 26 | 15 | 63,4% |
| `net/tls` | 35 | 24 | 11 | 68,6% |
| `intrinsics` | 95 | 85 | 10 | 89,5% |
| `array/iter` | 19 | 11 | 8 | 57,9% |
| `http/connection` | 12 | 4 | 8 | 33,3% |
| `http/client` | 10 | 3 | 7 | 30,0% |
| `net/tcp` | 43 | 36 | 7 | 83,7% |
| `pool` | 27 | 20 | 7 | 74,1% |
| `alloc/layout` | 30 | 24 | 6 | 80,0% |
| `fmt/rt` | 18 | 12 | 6 | 66,7% |
| `future` | 8 | 2 | 6 | 25,0% |
| `task` | 25 | 19 | 6 | 76,0% |
| `thread` | 20 | 14 | 6 | 70,0% |
| `async_iter` | 17 | 12 | 5 | 70,6% |
| `e2e/server` | 9 | 4 | 5 | 44,4% |
| `cell/lazy` | 8 | 4 | 4 | 50,0% |
| `iter/traits/accumulators` | 20 | 16 | 4 | 80,0% |
| `json/serialize` | 21 | 17 | 4 | 81,0% |
| `option` | 28 | 24 | 4 | 85,7% |
| `cell/ref_cell` | 13 | 10 | 3 | 76,9% |
| `collections/behaviors` | 20 | 17 | 3 | 85,0% |
| `net/parser` | 18 | 15 | 3 | 83,3% |
| `net/sys` | 50 | 47 | 3 | 94,0% |

### 5.2 Módulos com Gap Pequeno (1-2 funções)

| Módulo | Total | Coberto | Gap | Cobertura |
|---|---|---|---|---|
| `any` | 21 | 19 | 2 | 90,5% |
| `error` | 28 | 26 | 2 | 92,9% |
| `fmt/helpers` | 33 | 31 | 2 | 93,9% |
| `ptr/non_null` | 25 | 23 | 2 | 92,0% |
| `iter/range` | 30 | 29 | 1 | 96,7% |
| `mem` | 19 | 18 | 1 | 94,7% |
| `net/error` | 29 | 28 | 1 | 96,6% |
| `ops/async_function` | 9 | 8 | 1 | 88,9% |
| `ops/drop` | 13 | 12 | 1 | 92,3% |
| `os` | 43 | 42 | 1 | 97,7% |
| `result` | 33 | 32 | 1 | 97,0% |
| `slice/sort` | 9 | 8 | 1 | 88,9% |
| `sync/condvar` | 6 | 5 | 1 | 83,3% |
| `sync/once` | 11 | 10 | 1 | 90,9% |
| `thread/local` | 11 | 10 | 1 | 90,9% |

### 5.3 Análise dos Maiores Gaps

**`hash` (gap 18)**: Funções de hashing que dependem de OpenSSL. O link error descrito na seção 2 é o bloqueador direto.

**`alloc/global` (gap 17)**: Alocador global customizado. As 3 funções cobertas são as de inicialização trivial. As 17 restantes requerem que o alocador global esteja registrado antes da execução dos testes, o que conflita com o alocador de sistema já registrado no harness de teste.

**`array` (gap 15)**: Métodos de array que envolvem const generics não-triviais (`resize`, `extend`, operações com `[T; N]` onde N é calculado).

**`crypto/cipher` (gap 15)**: Funções de cipher AES em modos autenticados (`GCM`, `CCM`). O módulo `cipher_authtag` e `cipher_enum` crasham por falha de link com símbolos `EVP_AEAD_*` ausentes.

**`zlib/zstd` (gap 15)**: Type mismatch em `Outcome` descrito na seção 3.4. O codegen emite tipo errado para o erro genérico.

**`net/tls` (gap 11)**: TLS requer handshake bidirecional. As 24 funções cobertas são as de configuração e parsing de certificados; as 11 restantes são funções de I/O de dados que requerem conexão ativa.

**`http/client` e `http/connection` (gaps 7 e 8)**: HTTP client requer servidor HTTP real ou mock. As poucas funções cobertas são parsing de URLs e construção de headers.

**`future` (gap 6)**: `FusedFuture`, `JoinFuture` e combinators async que requerem um executor para `poll()`. Sem runtime async nos testes, o `poll()` manual é frágil.

---

## 6. Plano de Ação Priorizado

### Prioridade Alta (maior impacto por esforço)

| # | Ação | Funções Recuperadas (estimativa) | Esforço | Categoria |
|---|---|---|---|---|
| 1 | Corrigir crash `alloc/global` — registrar alocador global antes do harness | ~17 | Médio | Bug de teste |
| 2 | Corrigir type mismatch `Outcome` em `zlib/zstd` | ~15 | Médio | Bug de codegen |
| 3 | Escrever testes para `array/ascii` | 9 | Baixo | Gap de testes |
| 4 | Corrigir link OpenSSL para `hash` em modo coverage | ~18 | Alto | Bug de infraestrutura |
| 5 | Corrigir bug codegen `Maybe__I32` em `std/lowlevel` | ~10-15 | Alto | Bug de codegen |
| 6 | Corrigir GEP inválido em `intrinsics_array_ops` | ~10 | Médio | Bug de codegen |

### Prioridade Média

| # | Ação | Funções Recuperadas (estimativa) | Esforço | Categoria |
|---|---|---|---|---|
| 7 | Implementar `where I::Item = ref T` no type checker | ~17 | Muito Alto | Feature de compilador |
| 8 | Escrever testes para `array/iter` usando iteradores existentes | ~8 | Baixo | Gap de testes |
| 9 | Corrigir `cell/lazy` — bug de codegen para closure-typed struct fields | ~4 | Alto | Bug de codegen |
| 10 | Completar testes de `alloc/layout` (funções de alinhamento avançado) | ~6 | Médio | Gap de testes |
| 11 | Corrigir `fmt/rt` — funções de formatação de runtime não exercitadas | ~6 | Médio | Gap de testes |
| 12 | Completar testes de `pool` (pool de objetos reutilizáveis) | ~7 | Médio | Gap de testes |
| 13 | Completar testes de `json/serialize` | ~4 | Baixo | Gap de testes |
| 14 | Completar testes de `iter/traits/accumulators` | ~4 | Baixo | Gap de testes |

### Prioridade Baixa (infraestrutura ou não-testável)

| # | Ação | Funções Recuperadas (estimativa) | Esforço | Categoria |
|---|---|---|---|---|
| 15 | Mock de servidor TLS para `e2e/tls` | ~13 | Muito Alto | Infraestrutura |
| 16 | Mock de executor async para `future` e `async_iter` | ~11 | Alto | Infraestrutura |
| 17 | Mock de `thread::scope` para `thread/scope` | ~8 | Alto | Infraestrutura |
| 18 | Gaps menores (1-2 funções cada, 15 módulos) | ~20 | Baixo-Médio | Vários |

### Impacto Agregado por Fase

```
Fase 1 (Prioridade Alta, ~2-3 semanas):   +69 funções → ~96,3%
Fase 2 (Prioridade Média, ~3-4 semanas):  +56 funções → ~97,3%
Fase 3 (Prioridade Baixa, ~4-6 semanas):  +52 funções → ~98,3%
Restante (não-testável + infraestrutura):  ~21 funções → ~98,7% máximo atingível
```

---

## 7. Funções Genuinamente Não-Testáveis

Funções que não podem ser testadas por design e devem receber a anotação `@no_coverage`.

### 7.1 Funções que Chamam `unreachable!()`

| Função | Módulo | Razão |
|---|---|---|
| `NeverError::to_string` | `error` | Corpo é `unreachable!()` — tipo nunca instanciado |
| `NeverError::debug_string` | `error` | Mesmo caso |

**Ação**: Adicionar `@no_coverage` a ambas. São implementações de behavior obrigatórias para tipos `Never` que por definição nunca são chamadas.

### 7.2 Funções de Símbolo Pré-compilado

| Função | Módulo | Razão |
|---|---|---|
| (1 função em `precompiled_symbols`) | `precompiled_symbols` | Existe apenas em builds pré-compilados, não em compilação normal |

**Ação**: Adicionar `@no_coverage` ou excluir do tracking de cobertura via filtro de módulo.

### 7.3 Funções Dependentes de Plataforma

| Função | Módulo | Razão |
|---|---|---|
| Funções de socket com opções Windows-only | `net/sys` | `SO_EXCLUSIVEADDRUSE`, `TCP_MAXRT` — só existem em Windows e não são mockáveis |
| Funções de `thread::scope` com join real | `thread/scope` | Requer que threads do SO terminem antes do join — não simulável sem SO real |

### 7.4 Funções de Inicialização de Panic Handler

| Função | Módulo | Razão |
|---|---|---|
| Handler de panic do harness | `test` | Só é chamado quando o processo está abortando — testar causaria abort |

### 7.5 Proposta de Marcação

Adicionar ao compilador suporte para `@no_coverage` em funções individuais (já existe) e aplicar nas seguintes localizações:

```tml
// lib/core/error.tml
@no_coverage  // NeverError é uninhabited — to_string nunca é chamado
func NeverError::to_string(self) -> Str { unreachable!() }

@no_coverage
func NeverError::debug_string(self) -> Str { unreachable!() }
```

---

## 8. Distribuição por Categoria de Bloqueio

```
Gap total: 273 funções

Por categoria:
  Bugs de codegen/compilador:    ~52 funções (19%)
  Infraestrutura ausente:        ~52 funções (19%)
  Gaps de testes não escritos:   ~80 funções (29%)
  Crashes em runtime:            ~71 funções (26%)
  Genuinamente não-testável:     ~18 funções  (7%)
```

### Gráfico de Pareto (aproximado)

```
Funções recuperáveis por categoria:
  Testes não escritos  ████████████████████████████  80 (29%)
  Crashes runtime      ████████████████████████      71 (26%)
  Codegen bugs         ████████████████              52 (19%)
  Infraestrutura       ████████████████              52 (19%)
  Não-testável         █████                         18  (7%)
```

---

## 9. Histórico de Progresso

Com base nas notas do projeto, a cobertura evoluiu significativamente:

| Período | Cobertura | Evento |
|---|---|---|
| Pré-março 2026 | ~93% | Baseline |
| 2026-03-09 | ~96% | Tuple generic dispatch fix (commit d4c92bd0) — +3% |
| 2026-03-09 | ~99,38% | Múltiplos fixes: Unit type, Slice named structs, semantic inference |
| 2026-03-12 | 99,38% | 16431/16531 (suite completa — crashes fazem parecer menor) |
| 2026-03-13 | **95,02%** | 5213/5486 (subset testado em coverage mode — crashes excluem funções) |

A diferença entre 99,38% e 95,02% reflete que o número de 99,38% era da suite **completa** (incluindo todos os módulos) enquanto 95,02% é a cobertura **efetiva** medida pelo instrumentador de coverage, que só conta funções cujo código foi efetivamente executado.

---

*Relatório gerado automaticamente a partir de `build/coverage/coverage_history.jsonl` e análise de memória do projeto.*
