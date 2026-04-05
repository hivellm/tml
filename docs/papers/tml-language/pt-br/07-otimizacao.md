# 7. Arquitetura de Otimização

## 7.1 Visão Geral

O TML emprega uma estratégia de otimização em dois níveis: um conjunto rico de passes de otimização na IR de nível médio (MIR), que operam sobre semânticas específicas do TML, seguido pelo pipeline completo de otimização do LLVM, responsável por otimizações de propósito geral e específicas para o alvo. Essa abordagem dupla espelha a arquitetura do Rust, onde otimizações no nível MIR exploram garantias específicas da linguagem antes de delegar ao LLVM para otimizações de nível de máquina.

A justificativa para otimização no nível MIR é direta: certas transformações são apenas válidas — ou dramaticamente mais eficazes — quando o compilador retém informação semântica sobre ownership, lifetimes e estrutura de tipos que se perde durante o lowering para LLVM IR. Realizando essas otimizações no nível MIR, o TML pode eliminar alocações desnecessárias, elevar destrutores, devirtualizar chamadas de método e provar propriedades de aliasing que o LLVM sozinho não consegue deduzir.

---

## 7.2 Passes de Otimização MIR

O TML implementa **52 passes de otimização MIR**, uma suite abrangente que rivaliza ou supera a contagem de passes MIR de compiladores comparáveis. Para comparação: o pipeline MIR do Rust inclui aproximadamente 50 passes, enquanto o pipeline completo do LLVM contém mais de 200 passes operando no nível de IR inferior. O GCC emprega cerca de 300 passes entre suas representações GIMPLE e RTL.

### 7.2.1 Categorias de Passes

Os 52 passes podem ser organizados em seis categorias funcionais:

**Otimização de Memória e Alocação (8 passes):**
- **mem2reg** — O passe individualmente mais crítico. Promove alocações de pilha (alloca) para registros SSA, eliminando operações de memória desnecessárias. Esse passe tipicamente reduz a contagem de instruções em 30-50% no código gerado pelo builder MIR, que conservadoramente coloca valores na memória.
- **sroa** (Scalar Replacement of Aggregates) — Decompõe alocações de struct em valores escalares individuais quando a struct nunca é usada como um todo.
- **load_store_opt** — Elimina cargas redundantes quando uma escrita para o mesmo local é comprovadamente a escrita mais recente.
- **destination_propagation** — Propaga endereços de destino conhecidos por cadeias de atribuição, reduzindo cópias intermediárias.
- **escape_analysis** — Determina se alocações no heap podem ser substituídas com segurança por alocações na pilha (afundamento de alocação).
- **rvo** (Return Value Optimization) — Elimina cópias de valores retornados construindo-os diretamente no destino do chamador.
- **sinking** — Move instruções para mais próximo dos seus pontos de uso, reduzindo pressão sobre registros.
- **memory_leak_check** — Passe de análise que detecta possíveis vazamentos de memória onde valores alocados nunca são liberados.

**Código Morto e Fluxo de Controle (8 passes):**
- **dead_code_elimination** — Remove instruções cujos resultados nunca são usados.
- **dead_function_elimination** — Remove funções que nunca são chamadas (após inlining).
- **dead_method_elimination** — Remove métodos de implementação de behavior que nunca são despachados.
- **dead_arg_elim** — Remove argumentos de função que nunca são lidos pelo corpo da função.
- **unreachable_code_elimination** — Remove blocos básicos que não podem ser alcançados a partir do bloco de entrada.
- **adce** (Aggressive Dead Code Elimination) — Mais poderoso que o DCE padrão; trabalha ao contrário a partir dos efeitos do programa para provar que código é desnecessário.
- **simplify_cfg** — Mescla blocos básicos, elimina blocos vazios e simplifica cadeias de ramificação.
- **block_merge** — Combina blocos básicos consecutivos com saltos incondicionais entre eles.

**Propagação de Valores e Simplificação (9 passes):**
- **constant_folding** — Avalia expressões constantes em tempo de compilação (ex.: 3 + 4 se torna 7).
- **constant_propagation** — Substitui usos de variáveis pelos seus valores constantes conhecidos.
- **copy_propagation** — Substitui usos de valores copiados pela fonte original.
- **inst_simplify** — Aplica simplificações algébricas (ex.: x * 1 = x, x + 0 = x, x & true = x).
- **early_cse** (Common Subexpression Elimination) — Elimina computações redundantes dentro de um bloco básico.
- **common_subexpression_elimination** — CSE global entre blocos básicos usando informação de dominância.
- **gvn** (Global Value Numbering) — Identifica e elimina computações redundantes usando numeração de valores baseada em hash.
- **reassociate** — Reordena operações associativas/comutativas para melhor constant folding e CSE.
- **narrowing** — Reduz a largura de bits de operações quando os bits superiores são comprovadamente não usados.

**Otimização de Laços (5 passes):**
- **licm** (Loop-Invariant Code Motion) — Move computações invariantes de laço para fora do corpo do laço.
- **loop_rotate** — Transforma laços para colocar a condição de saída na parte inferior, habilitando melhor otimização.
- **loop_unroll** — Desenrola laços pequenos para reduzir sobrecarga de ramificação e habilitar otimizações adicionais.
- **loop_opts** — Otimização geral de laços (redução de força dentro de laços, simplificação de variáveis de indução).
- **normalize_array_len** — Eleva computações de comprimento de array para fora de laços quando o tamanho do array é invariante.

**Otimização Interprocedural e de Despacho (7 passes):**
- **inlining** — Inlina funções pequenas nos pontos de chamada, eliminando sobrecarga de chamada e habilitando otimizações adicionais.
- **ipo** (Interprocedural Optimization) — Análise entre funções para especialização de argumentos e avaliação parcial.
- **devirtualization** — Converte despacho dinâmico (chamadas de método em objetos behavior) em despacho estático quando o tipo concreto é conhecido.
- **tail_call** — Identifica e marca chamadas de cauda para otimização de cauda garantida, evitando crescimento da pilha.
- **constructor_fusion** — Funde atribuições sequenciais de campos em uma única construção de agregado.
- **peephole** — Otimizações locais baseadas em padrões (combinação de instruções, redução de força).
- **strength_reduction** — Substitui operações caras por equivalentes mais baratos (ex.: multiplicação por potência de 2 para deslocamento à esquerda).

**Passes Especializados (7 passes):**
- **batch_destruction** — Agrupa chamadas de destrutor para drops sequenciais, reduzindo sobrecarga de chamada de função.
- **destructor_hoist** — Move chamadas de destrutor para mais cedo quando é seguro fazer isso, melhorando a utilização de memória.
- **remove_unneeded_drops** — Elimina chamadas de drop para tipos com destrutores triviais (tipos Copy).
- **match_simplify** — Otimiza despacho de correspondência de padrões (tabelas de salto, busca binária em discriminantes).
- **simplify_select** — Simplifica operações de seleção condicional.
- **jump_threading** — Elimina ramificações condicionais redundantes passando saltos por condições conhecidas.
- **const_hoist** — Eleva materializações de constante para fora de laços e para blocos predecessores.

**Análise e Profiling (3 passes):**
- **alias_analysis** — Computa informação de aliasing para operações de memória (utilizada por load_store_opt, licm).
- **pgo** (Profile-Guided Optimization) — Usa dados de perfil de execução para guiar decisões de inlining, predição de ramificação e layout de código.
- **vectorization** — Identifica oportunidades de vetorização SIMD em laços e operações sequenciais.

**Específico para Async (1 passe):**
- **async_lowering** — Transforma corpos de funções async em máquinas de estado para agendamento cooperativo.

**Verificação de Limites (1 passe):**
- **bounds_check_elimination** — Prova que acessos a arrays estão dentro dos limites, removendo verificações de execução redundantes.

**Outros (3 passes):**
- **merge_returns** — Canonicaliza funções para ter um único ponto de retorno.
- **builder_opt** — Limpeza pós-builder para artefatos de construção MIR.
- **infinite_loop_check** — Detecta laços comprovadamente infinitos e emite diagnósticos.

### 7.2.2 Ordenação dos Passes

A ordenação dos passes é crítica para a eficácia da otimização. O TML segue um pipeline inspirado no design do gerenciador de passes do LLVM:

1. **Limpeza inicial**: simplify_cfg, block_merge, builder_opt
2. **Promoção**: mem2reg (deve executar cedo — a maioria dos outros passes assume forma SSA)
3. **Simplificação local**: constant_folding, inst_simplify, copy_propagation, early_cse
4. **Otimização global**: gvn, constant_propagation, dead_code_elimination
5. **Otimização de laços**: licm, loop_rotate, loop_unroll, normalize_array_len
6. **Interprocedural**: inlining, devirtualization, ipo (iterar com simplificação local)
7. **Otimização de memória**: sroa, load_store_opt, escape_analysis, rvo
8. **Limpeza**: adce, dead_function_elimination, dead_method_elimination, simplify_cfg
9. **Final**: batch_destruction, destructor_hoist, remove_unneeded_drops, tail_call

---

## 7.3 Otimização no Backend LLVM

Após a otimização MIR, o TML gera texto de LLVM IR que é interpretado pela biblioteca LLVM 19+ embarcada e submetido ao pipeline completo de otimização do LLVM.

### 7.3.1 Níveis de Otimização

| Nível | Flag TML | Passes LLVM | Caso de Uso |
|-------|----------|-------------|-------------|
| O0 | `--debug` (padrão) | Mínimo — compilação rápida | Desenvolvimento, depuração |
| O1 | `--optimize=1` | Otimizações básicas | Builds rápidos com alguma otimização |
| O2 | `--optimize=2` | Otimização completa | Builds de produção |
| O3 | `--release` | Otimização agressiva + vetorização | Performance máxima |

### 7.3.2 Vantagens do LLVM Embarcado

O TML embarca o LLVM e o LLD diretamente no binário do compilador (in-process), ao contrário de ferramentas que invocam comandos externos. Isso oferece diversas vantagens:

1. **Sem arquivos temporários**: A IR é passada em memória do codegen MIR para o LLVM, eliminando sobrecarga de I/O.
2. **Distribuição em binário único**: O compilador, otimizador e linker são um único executável — sem configuração de toolchain.
3. **Otimização consistente**: A versão do LLVM está fixada, garantindo builds reproduzíveis entre ambientes.
4. **Compilação mais rápida**: Eliminar a criação de processos e I/O de arquivo economiza 100-500ms por unidade de compilação.

---

## 7.4 Metodologia Rust-como-Referência para IR

O TML emprega uma metodologia sistemática para avaliar e melhorar a qualidade da LLVM IR gerada: a abordagem **Rust-como-Referência**. Como TML e Rust ambos têm como alvo o LLVM com garantias semânticas similares (ownership, borrowing, sem data races), a saída de IR do Rust serve como referência de qualidade.

### 7.4.1 Metodologia

O fluxo de trabalho para cada tarefa de otimização de codegen é:

1. Escrever código semanticamente equivalente em TML e em Rust.
2. Compilar ambos para LLVM IR no mesmo nível de otimização.
3. Comparar função por função em quatro métricas:
   - **Contagem de instruções** — O TML não deve exceder 2x o Rust para lógica equivalente.
   - **Layouts de tipos** — Tamanhos de struct e enum devem corresponder.
   - **Contagem de alloca** — O TML não deve ter alocações de pilha que o Rust evita.
   - **Sobrecarga de segurança** — Verificações de overflow, null, limites devem ser comparáveis.
4. Identificar e corrigir divergências no codegen do TML.

### 7.4.2 Lacunas de Otimização Atuais

| Problema | TML Atual | Referência Rust | Impacto |
|----------|-----------|-----------------|---------|
| Layout de Maybe[I32] | 16 bytes ({i32, [1 x i64]}) | 8 bytes ({i32, i32}) | ALTO — 2x memória para inteiros anuláveis |
| Construtores de struct | alloca + store + load (10 instruções) | cadeia insertvalue (3 instruções) | ALTO — 3x contagem de instruções |
| Declarações de runtime | 500+ linhas emitidas incondicionalmente | Apenas declarações usadas | MÉDIO — inchaço de IR, processamento LLVM mais lento |
| Aritmética inteira | add nsw (comportamento indefinido em overflow) | add verificado com panic | MÉDIO — lacuna de corretude |
| Tratamento de exceções | Nenhum | invoke + cleanuppad | BAIXO — sem suporte a unwinding ainda |

### 7.4.3 Paridade Alcançada

Em diversas áreas, o TML já corresponde ou se aproxima da qualidade de IR do Rust:

- **Sobrecarga de chamada de função**: Chamadas diretas geram IR idêntica ao Rust.
- **Tratamento de referências**: ref/mut ref se reduzem a tipos de ponteiro LLVM idênticos.
- **Layout de discriminante de enum**: Otimização de nicho padrão para enums de variante única.
- **Otimização de iteradores**: Corpos de laço se inlinam e otimizam comparavelmente após passes LLVM.
- **Operações SIMD**: Tipos de vetor nativos geram instruções de vetor LLVM idênticas.

---

## 7.5 Características de Performance

### 7.5.1 Velocidade de Compilação

| Métrica | TML | Rust | C++ (Clang) | Go |
|---------|-----|------|-------------|----|
| Build completo (projeto médio) | ~100s | 120-300s | 60-180s | 10-30s |
| Incremental (mudança em arquivo único) | 5-15s | 15-60s | 5-30s | 1-5s |
| Linking | 37s (LLD, limitado por I/O) | 10-60s (LLD) | 5-30s (LLD) | <5s (personalizado) |

A compilação incremental baseada em consultas do TML com fingerprinting evita trabalho redundante. Arquivos alterados são recompilados e consultas downstream só são re-executadas se seus fingerprints de entrada mudaram (coloração RED/YELLOW/GREEN).

### 7.5.2 Performance em Tempo de Execução

Como TML e Rust ambos têm como alvo o LLVM com garantias semânticas similares, a performance em tempo de execução deveria teoricamente ser idêntica para código equivalente. Na prática:

- **Equivalente ao Rust** para código que gera IR correspondente (chamadas de função, iteração, aritmética).
- **Marginalmente pior** onde existem lacunas de otimização (construção de structs, layouts de enum).
- **Equivalente a C/C++** quando compilado pelo LLVM em O2/O3 — as mesmas otimizações de backend se aplicam.
- **Dramaticamente mais rápido que Go** devido à ausência de pausas de garbage collection e abstrações de custo zero.
- **Ordens de magnitude mais rápido que Python** — código nativo compilado vs bytecode interpretado.

### 7.5.3 Tamanho dos Binários

| Modo de Build | Tamanho | Conteúdo |
|---------------|---------|----------|
| Monolítico (debug) | ~100MB | Compilador + LLVM + LLD em binário único |
| Modular (debug) | ~180MB total | Launcher fino + tml_compiler.dll (104MB) + tml_codegen_x86.dll (78MB) |
| Programa do usuário (release) | 50KB-5MB | Depende do uso da stdlib, com eliminação de código morto |

O binário do compilador é grande porque embarca a biblioteca LLVM completa. Programas de usuário são pequenos porque a eliminação de funções mortas (tanto no nível MIR quanto LLVM) remove código da biblioteca padrão não utilizado.

---

## 7.6 Comparação com Outras Abordagens de Otimização

### 7.6.1 TML vs Otimização do Rust

Ambas as linguagens compartilham o backend LLVM, mas os 52 passes MIR do TML vs ~50 passes MIR do Rust desempenham papéis ligeiramente diferentes. Os passes MIR do Rust focam pesadamente em limpeza relacionada a borrow-check e otimização de monomorphization, enquanto os passes do TML incluem fusion de construtores e batching de destrutores mais agressivos — reflexo da ênfase do TML em reduzir sobrecarga de codegen para padrões comuns.

### 7.6.2 TML vs Otimização do Go

O compilador do Go usa um backend SSA customizado em vez do LLVM, priorizando velocidade de compilação em detrimento de otimização de pico. O Go compila 5-10x mais rápido que o TML, mas gera código tipicamente 10-30% mais lento em tempo de execução. A abordagem do TML troca tempo de compilação por performance em tempo de execução — adequado para programação de sistemas onde eficiência em tempo de execução é primordial.

### 7.6.3 TML vs C++ (Clang) Otimização

Ambos usam LLVM, mas C++ carece da semântica de ownership que habilita as otimizações no nível MIR do TML. O código C++ requer análise de aliasing mais conservadora porque ponteiros são irrestritos. As garantias do borrow checker do TML habilitam otimização de load/store e análise de aliasing mais agressivas no nível MIR.

### 7.6.4 TML vs Otimização do Zig

O Zig usa um backend customizado que também pode ter como alvo o LLVM. No modo ReleaseFast, o Zig remove todas as verificações de segurança (verificações de limites, overflow) para performance máxima. O TML mantém verificações de segurança mesmo no modo release (embora o bounds_check_elimination prove que muitas são desnecessárias). Essa diferença filosófica — o Zig confia no programador para ser correto, o TML confia no compilador para provar a corretude — reflete modelos de segurança fundamentalmente diferentes.

---

## 7.7 Trabalhos Futuros de Otimização

Diversas oportunidades de otimização permanecem:

1. **Otimização de nicho para tipos Maybe** — Codificar Nothing como um padrão de bits inválido dentro do payload, reduzindo Maybe[I32] de 16 para 8 bytes.
2. **Construção de struct via insertvalue** — Eliminando sequências desnecessárias de alloca/store/load.
3. **Declarações de runtime lazy** — Emitindo apenas as declarações de funções do runtime C que são efetivamente chamadas.
4. **Aritmética inteira verificada** — Substituindo operações nsw por operações verificadas que fazem panic em overflow.
5. **Otimização guiada por perfil** — Aproveitando o passe pgo com dados de execução do mundo real.
6. **Otimização em tempo de link (LTO)** — Otimização entre módulos pela infraestrutura LTO do LLVM.
7. **Compilação paralela** — Compilando módulos independentes em threads separadas (o sistema de consultas suporta isso arquiteturalmente).
