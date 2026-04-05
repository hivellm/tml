# 5. Arquitetura do Compilador

O compilador do TML é implementado em C++ e tem como alvo a geração de código nativo através do LLVM embutido. Seu design incorpora lições aprendidas em compiladores modernos de linguagens de sistemas — particularmente o rustc — enquanto faz escolhas distintas adequadas aos objetivos do TML de compatibilidade com LLMs e auto-hospedagem incremental.

---

## 5.1 Compilação por Consultas Demanda-Dirigida

### 5.1.1 Motivação

Compiladores tradicionais organizam o trabalho como passos em lote: fazer lexing de todo o código fonte, analisar todos os tokens, verificar tipos de todas as declarações, e assim por diante. Esse modelo é simples, mas força o compilador a reexecutar todos os passos para toda a unidade de compilação quando qualquer função individual muda.

O TML resolve isso com um **sistema de consultas demanda-dirigido** inspirado na infraestrutura `TyCtxt` do rustc. Em vez de executar fases em uma sequência fixa, o compilador expõe cada fase como uma consulta com nome e memoização. Uma consulta só é executada quando seu resultado é exigido. Os resultados são armazenados em cache na memória e persistidos em disco entre sessões.

### 5.1.2 Design do Sistema de Consultas

O `QueryContext` é o orquestrador central:

```
QueryContext::force(QueryKey) -> cached result OR compute
```

As consultas disponíveis formam um DAG (Grafo Acíclico Dirigido):

```
ReadSource -> Tokenize -> ParseModule -> Typecheck
           -> Borrowcheck -> HirLower -> ThirLower
           -> MirBuild -> MirOptimize -> CodegenUnit
```

Cada consulta:
1. Verifica se existe um resultado em cache com um fingerprint válido.
2. Se válido, retorna o resultado em cache (GREEN).
3. Se o fingerprint de entrada mudou, reexecuta e compara a saída (YELLOW se saída inalterada, RED se mudou).
4. Consultas downstream só são reexecutadas se a consulta de entrada produziu um resultado RED.

### 5.1.3 Compilação Incremental

O cache baseado em fingerprints se estende entre sessões de compilação via `.incr-cache/incr.bin`. Quando um arquivo de código-fonte muda:

1. A consulta `ReadSource` detecta a mudança de conteúdo (RED).
2. `Tokenize` e `ParseModule` são reexecutados (podem ser RED ou YELLOW dependendo se a mudança afeta a estrutura analisada).
3. `Typecheck` é reexecutado apenas se a árvore de análise mudou.
4. Funções cujos tipos não mudaram recebem YELLOW — sua geração de código downstream é ignorada.

Isso tipicamente reduz o tempo de reconstrução incremental de ~100s (build completo) para 5–15s (apenas a função modificada).

### 5.1.4 Comparação com Outras Abordagens

| Compilador | Modelo de Compilação | Incremental? | Persistência de Cache |
|------------|---------------------|-------------|----------------------|
| TML | Por consultas, demanda-dirigida | Sim (fingerprints) | `.incr-cache/incr.bin` |
| Rust (rustc) | Por consultas, demanda-dirigida | Sim (fingerprints) | Cache incremental por crate |
| Clang | Passos em lote | Não (depende do sistema de build) | Nenhum (cabeçalhos pré-compilados, parcial) |
| GCC | Passos em lote | Não (depende do sistema de build) | Nenhum |
| Go | Passo único por pacote | Parcial (nível de pacote) | Cache de build |
| Zig | Incremental, auto-hospedado | Sim (granular) | Em memória + disco |

O TML e o Rust compartilham a mesma abordagem arquitetural. A principal vantagem sobre compiladores em lote (Clang, GCC) é que uma mudança em uma função não requer a re-verificação de tipos de funções não relacionadas. A vantagem sobre Go é granularidade mais fina — o Go faz cache no nível de pacote, enquanto o TML faz cache no nível de função/consulta.

---

## 5.2 Pipeline de Compilação

O pipeline de compilação completo consiste em nove fases:

```
Source (.tml)
    |
    v
[1] LEXER (lexer.cpp)
    Token stream: identifiers, keywords, literals, operators
    Approach: Hand-written, single-pass
    |
    v
[2] PARSER (parser.cpp, parser_expr.cpp, parser_decl.cpp)
    AST: Module with declarations, expressions, patterns
    Approach: Recursive descent for declarations, Pratt parser for expressions
    |
    v
[3] TYPE CHECKER (checker.cpp, checker_*.cpp)
    TypeEnv: Symbol table with resolved types
    Approach: Hindley-Milner inference, 4 phases
    Phase 1: Register all type/function signatures
    Phase 2: Resolve imports and foreign symbols
    Phase 3: Bind behavior implementations
    Phase 4: Check function bodies with full inference
    |
    v
[4] BORROW CHECKER (borrow/checker.cpp)
    Validation: Ownership and lifetime correctness
    Approach: NLL (Non-Lexical Lifetimes), place-based tracking
    |
    v
[5] HIR LOWERING (hir/hir_builder.cpp)
    HirModule: Typed, desugared, monomorphized
    Transforms: Type resolution, sugar expansion, generic instantiation,
                closure capture analysis, field/variant index resolution
    |
    v
[6] THIR LOWERING (thir/thir_lower.cpp)
    ThirModule: Coercions inserted, methods resolved
    Transforms: Implicit coercion insertion, operator desugaring,
                method resolution via trait solver, exhaustiveness checking
    |
    v
[7] MIR BUILDING (mir/hir_mir_builder.cpp OR mir/thir_mir_builder.cpp)
    mir::Module: SSA form with basic blocks
    Two parallel paths: HIR->MIR (legacy) and THIR->MIR (new)
    |
    v
[8] MIR OPTIMIZATION (mir/mir_pass.cpp, mir/passes/*.cpp)
    mir::Module (optimized): 52 passes applied
    Critical: mem2reg, dead code elimination, inlining, constant folding
    |
    v
[9] CODEGEN (codegen/mir_codegen.cpp)
    LLVM IR text: Generated from optimized MIR
    |
    v
[10] LLVM BACKEND (backend/llvm_backend.cpp)
     .obj file: Native object code
     |
     v
[11] LLD LINKER (backend/lld_linker.cpp)
     .exe: Final executable linked with C runtime
```

### 5.2.1 Analisador Pratt para Expressões

O TML usa um analisador Pratt (precedência de operadores top-down) para análise de expressões. Essa abordagem trata elegantemente:

- Precedência de operadores sem tabelas de precedência explícitas na gramática.
- Operadores prefixo, infixo e pós-fixo.
- Operadores associativos à direita (exponenciação `**`).
- Expressões de chamada, expressões de índice e acesso a campos como operadores pós-fixo.

O analisador Pratt é combinado com descida recursiva para declarações (`func`, `type`, `enum`, `behavior`, `impl`), onde a estrutura regular da sintaxe de declarações torna a descida recursiva mais natural.

### 5.2.2 Verificação de Tipos em Quatro Fases

A abordagem em quatro fases do verificador de tipos é necessária porque o TML suporta referências futuras e recursão mútua:

1. **Registro**: Todos os nomes de tipo e assinaturas de função entram no ambiente. Nenhum corpo é verificado.
2. **Imports**: Símbolos externos são vinculados. Neste ponto, todos os nomes no escopo são conhecidos.
3. **Implementações**: Blocos `impl Behavior for Type` são registrados. Coerência (regras orphan) é verificada.
4. **Corpos**: Corpos de função são verificados com inferência completa, usando o ambiente de tipos completo.

Essa abordagem em fases é similar à estratégia de resolução do Rust e difere da abordagem de passo único do Go (que requer declaração antes do uso dentro de um arquivo, embora não entre arquivos em um pacote).

---

## 5.3 LLVM e LLD Embutidos

O TML embute LLVM 19+ e LLD (o linker do LLVM) diretamente no binário do compilador. Esta é uma escolha arquitetural com implicações significativas:

### 5.3.1 Vantagens

1. **Sem toolchain externo**: O compilador TML é um único binário. Não é necessária a instalação separada de Clang, LLVM ou um linker de sistema.
2. **Processamento de IR em memória**: O LLVM IR é gerado como texto, analisado em memória e compilado para código objeto sem escrever arquivos temporários.
3. **Linking em processo**: O LLD vincula arquivos objeto em processo, eliminando o overhead de criar um subprocesso de linker.
4. **Saída determinística**: A versão do LLVM é fixada, garantindo geração de código idêntica entre ambientes.
5. **Compilação mais rápida**: Eliminar a criação de processos e E/S de arquivo economiza 100–500ms por unidade de compilação.

### 5.3.2 Desvantagens

1. **Binário grande**: O binário do compilador tem ~100MB (debug) porque inclui a biblioteca LLVM completa.
2. **Acoplamento à versão do LLVM**: Atualizar o LLVM requer reconstruir o compilador.
3. **Uso de memória**: O uso de memória em processo do LLVM aumenta o footprint do compilador.

### 5.3.3 Comparação

| Compilador | Integração LLVM | Linker | Tamanho do Binário |
|------------|----------------|--------|-------------------|
| TML | Embutido (em processo) | LLD embutido | ~100MB |
| Rust | Embutido (em processo) | Sistema ou LLD | ~50MB (somente rustc) |
| Clang | É LLVM | Sistema ou LLD | ~100MB |
| Go | Backend personalizado | Linker personalizado | ~20MB |
| Zig | LLVM embutido + personalizado | LLD embutido | ~150MB |

---

## 5.4 Caminhos Duais de Construção MIR

O TML mantém dois caminhos paralelos para converter IR de alto nível em MIR:

**Caminho A — HIR para MIR (legado):**
- Arquivos: `hir_mir_builder.cpp`, `builder/hir_expr.cpp`, `builder/hir_expr_control.cpp`
- Entrada: HirModule (sem etapa THIR)
- Status: Maduro, pronto para produção, trata todos os recursos da linguagem
- Usado com: flag `--legacy`

**Caminho B — THIR para MIR (novo):**
- Arquivos: `thir_mir_builder.cpp`, `thir_mir_builder_expr.cpp`
- Entrada: ThirModule (após rebaixamento THIR)
- Status: Em desenvolvimento, cobertura crescente de recursos
- Usado por: Padrão (quando suportado)

A arquitetura de caminhos duplos existe para segurança de migração: o Caminho B pode ser desenvolvido e testado incrementalmente enquanto o Caminho A continua a servir a compilação de produção. Testes podem ser executados em ambos os caminhos para verificar equivalência.

Isso espelha a própria história do Rust: o rustc manteve caminhos de geração de código baseados em AST e baseados em MIR durante a migração para MIR, com o caminho antigo servindo como implementação de referência.

---

## 5.5 Sistema de Build

O TML usa CMake para configuração de build com scripts de build personalizados (`scripts/build.bat`) que tratam a configuração do ambiente:

| Modo | Comando | Saída |
|------|---------|-------|
| Debug (monolítico) | `scripts/build.bat` | `build/debug/bin/tml.exe` (~100MB) |
| Release | `scripts/build.bat release` | `build/release/bin/tml.exe` |
| Limpo | `scripts/build.bat --clean` | Build do zero |
| Modular | `scripts/build.bat --modular` | Launcher + `tml_compiler.dll` + `tml_codegen_x86.dll` |

O build modular produz um executável launcher leve que carrega funcionalidades do compilador a partir de DLLs. Isso habilita:
- Reconstruções incrementais mais rápidas (apenas revincula a DLL modificada).
- Arquitetura de plugin para extensões futuras.
- Atualizações menores quando apenas um componente muda.

O compilador usa Zig CC como compilador C/C++ (substituindo o MSVC), fornecendo compilação multiplataforma com libc incluída.

---

## 5.6 Comparação com Arquiteturas Principais de Compiladores

### 5.6.1 TML vs rustc

O parente arquitetural mais próximo. Ambos usam compilação por consultas demanda-dirigida, cinco camadas de IR (AST, HIR, THIR, MIR, LLVM IR), LLVM embutido e compilação incremental com fingerprinting. A implementação do TML é mais recente e menor (~100K linhas de C++ vs ~500K linhas de Rust do rustc), mas arquiteturalmente similar.

### 5.6.2 TML vs GCC

O GCC usa um modelo de compilação em lote tradicional com três camadas de IR (GENERIC, GIMPLE, RTL). Não tem sistema de consultas e depende do sistema de build (Make) para reconstruções incrementais. A força do GCC é seu pipeline de otimização maduro (~300 passos) e amplo suporte a alvos. A vantagem do TML é incrementalidade de granulação mais fina e arquitetura mais simples.

### 5.6.3 TML vs Clang

O Clang tem um pipeline mais raso (Clang AST -> LLVM IR, duas camadas) porque C++ tem construções semânticas mais simples do que TML (sem ownership, sem tipos algébricos, sem sistema de behavior). O Clang depende de cabeçalhos pré-compilados para velocidade de compilação em vez de cache baseado em consultas. Ambos embutem LLVM.

### 5.6.4 TML vs Compilador Go

O compilador Go é notavelmente simples: um compilador de passo único com um backend SSA personalizado. Compila uma ordem de magnitude mais rápido do que TML ou Rust, mas produz código menos otimizado. O design do Go prioriza velocidade de compilação em detrimento de desempenho em runtime — um tradeoff válido para software server-side onde a produtividade do desenvolvedor importa mais do que a eficiência da CPU.

### 5.6.5 TML vs Zig

O Zig é auto-hospedado com um backend personalizado que pode opcionalmente ter LLVM como alvo. Sua compilação incremental é mais granular do que a do TML (nível de instrução vs nível de consulta). A contribuição única do Zig é comptime (avaliação em tempo de compilação) que elimina a necessidade de um sistema de macros — uma filosofia que o TML compartilha (o TML usa decorators em vez de macros).
