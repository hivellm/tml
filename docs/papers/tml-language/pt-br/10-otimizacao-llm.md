# 10. Design de Linguagem LLM-First

## 10.1 A Tese

Linguagens de programação foram projetadas exclusivamente para programadores humanos por mais de sete décadas. Do FORTRAN (1957) ao Rust (2015), cada decisão de design de linguagem otimizou para processos cognitivos humanos: velocidade de leitura, conveniência de digitação, reconhecimento visual de padrões e familiaridade cultural.

O surgimento de Large Language Models como geradores de código — GitHub Copilot, Claude, GPT-4 e seus sucessores — introduz um consumidor fundamentalmente novo de sintaxe de linguagem de programação. LLMs processam código por meio de tokenização em subpalavras, geram tokens de forma autorregressiva sem backtracking e carecem do raciocínio contextual que permite aos humanos desambiguar símbolos sobrecarregados. Essas diferenças criam um espaço de design para linguagens de programação otimizadas tanto para geração de código humana quanto por máquina.

O TML (To Machine Language) é, ao nosso conhecimento, a primeira linguagem de programação explicitamente projetada com geração de código por LLM como restrição primária de design. Esta seção descreve as decisões específicas de design que emergem de tratar a otimização para LLMs como uma preocupação de primeira classe.

---

## 10.2 O Problema: Ambiguidade de Símbolos na Geração por LLM

LLMs treinados em corpora de múltiplas linguagens encontram ambiguidade persistente de símbolos dependentes de contexto. Considere o caractere `<`:

| Contexto | Significado | Linguagem |
|----------|-------------|-----------|
| `a < b` | Comparação de menor que | Todas as linguagens |
| `Vec<T>` | Parâmetro de tipo genérico | Rust, C++, Java, TypeScript |
| `<div>` | Abridor de tag HTML | HTML, JSX |
| `<<` | Deslocamento de bits à esquerda | C, C++, Rust |
| `<<<` | Heredoc | Bash, PHP |
| `<-` | Recepção de canal | Go |

Um LLM gerando código precisa determinar qual significado se aplica a partir do contexto ao redor. Esse é um problema de classificação que o modelo resolve probabilisticamente — e soluções probabilísticas têm taxas de erro diferentes de zero. Quando o modelo gera `Vec<HashMap<K, V>>` e o tokenizador divide isso em tokens incluindo `>>`, o modelo precisa ter aprendido que `>>` nesse contexto representa dois colchetes angulares de fechamento, não um operador de deslocamento à direita.

O TML elimina toda essa classe de erros. O caractere `<` tem exatamente um significado: comparação de menor que. Parâmetros genéricos usam `[T]`, o que é inequívoco em todos os contextos.

### 10.2.1 Inventário de Ambiguidades

Catalogamos os símbolos que causam mais erros de geração em LLMs, e a resolução do TML para cada um:

| Símbolo | Ambiguidades (em outras linguagens) | Resolução no TML |
|---------|-------------------------------------|------------------|
| `<` / `>` | Comparação, genéricos, HTML, deslocamento de bit, heredoc, canal | `<` `>` = comparação apenas; genéricos usam `[` `]` |
| `\|` | OR bitwise, delimitador de closure, ramo de match, tabela markdown | `\|` = OR bitwise apenas; closures usam `do()` |
| `!` | NOT lógico, invocação de macro, operador de unwrap | `not` = NOT lógico; sem macros; `!` = propagação de erro |
| `&` | Referência, AND bitwise, endereço, AND lógico | `ref` = referência; `and` = AND lógico; `&` = AND bitwise |
| `?` | Propagação de erro, ternário, tipo opcional, regex | `!` = propagação de erro; `?.` = encadeamento opcional |
| `::` | Separador de caminho, turbofish, tipo associado | `::` = caminho apenas (sem turbofish necessário com `[T]`) |
| `..` / `...` | Range, spread, parâmetros rest, variádico | `to` / `through` = ranges; sem operador spread |
| `#` | Atributo, macro, comentário, preprocessador | `@` = decoradores; `//` = comentário; `#if` = compilação condicional |
| `->` | Tipo de retorno, retorno de closure, ramo de match | `->` = tipo de retorno e ramo de match (consistente) |

O TML reduz o número total de símbolos dependentes de contexto de 24+ (no Rust) para aproximadamente 5. Cada símbolo multiuso restante (`=`, `.`, `,`, `:`, `;`) tem significados que são estruturalmente inequívocos (determinados por posição, não por contexto de tipo).

---

## 10.3 Eficiência de Tokens

LLMs operam dentro de janelas de contexto fixas (4K a 1M tokens). Cada token consumido pela sintaxe de boilerplate é um token indisponível para a lógica do programa. A sintaxe baseada em keywords do TML é projetada para ser eficiente em tokens apesar de usar identificadores mais longos.

### 10.3.1 Comportamento do Tokenizador

Tokenizadores modernos de LLM (BPE, SentencePiece) tratam palavras comuns em inglês como tokens únicos, mas podem dividir sequências simbólicas em múltiplos tokens:

| Expressão | Tokens Aproximados |
|-----------|-------------------|
| `and` | 1 token |
| `&&` | 1-2 tokens (depende do tokenizador) |
| `behavior` | 1-2 tokens |
| `trait` | 1 token |
| `do(x)` | 3 tokens (`do`, `(`, `x)`) |
| `\|x\|` | 3 tokens (`\|`, `x`, `\|`) |
| `Maybe[I32]` | 4 tokens |
| `Option<i32>` | 4-5 tokens |

As contagens de tokens são aproximadamente iguais para construtos individuais. No entanto, o TML ganha eficiência por meio de:

1. **Sem sintaxe de macro**: `println!("{}", x)` do Rust envolve invocação de macro, análise de string de formato e correspondência de argumentos. A abordagem de literal de template do TML é mais simples.
2. **Sem anotações de lifetime**: `'a`, `'b`, `'static` consomem tokens cada um. O TML não tem nenhum.
3. **Sem turbofish**: `collect::<Vec<_>>()` do Rust requer o operador turbofish para desambiguação de tipos. `collect[List[I32]]()` do TML ou simplesmente `collect()` com inferência evita isso.
4. **Sintaxe de atributo mais simples**: `@auto(debug, equal)` vs `#[derive(Debug, PartialEq, Eq)]`.

### 10.3.2 Economias Compostas

Para uma função típica implementando inserção ordenada em uma coleção:

```
// TML (~45 tokens)
func insert_sorted[T: Ordered](list: mut ref List[T], item: T) {
    let pos = list.iter().position(do(x) x.compare(ref item) == Ordering.Greater)
    when pos {
        Just(i) -> list.insert(i, item),
        Nothing -> list.push(item),
    }
}
```

```rust
// Rust (~55 tokens)
fn insert_sorted<T: Ord>(list: &mut Vec<T>, item: T) {
    let pos = list.iter().position(|x| x.cmp(&item) == Ordering::Greater);
    match pos {
        Some(i) => list.insert(i, item),
        None => list.push(item),
    }
}
```

A versão TML economiza aproximadamente 18% de tokens para semântica equivalente. Em um módulo de 1000 linhas, isso se acumula em economias significativas de janela de contexto.

---

## 10.4 Gramática LL(1) e Geração Autorregressiva

A gramática do TML é LL(1): um único token de lookahead é suficiente para determinar a regra de produção aplicável. Essa propriedade está arquiteturalmente alinhada com a forma como LLMs geram código.

LLMs geram tokens de forma autorregressiva — cada token é escolhido com base em todos os tokens anteriores, sem capacidade de retroceder e revisar escolhas anteriores. Isso é estruturalmente idêntico à análise LL(1), onde cada produção é determinada pelo token atual e pelo estado do analisador.

Uma gramática LL(1) significa que o próximo token "correto" é sempre determinável a partir do contexto local. O LLM não precisa gerar sequências especulativas que podem posteriormente se revelar sintaticamente inválidas (um problema com templates C++, onde `>>` pode precisar ser `> >` dependendo da profundidade de aninhamento que foi determinada muitos tokens atrás).

### 10.4.1 Implicações Práticas

| Propriedade | Impacto na Geração por LLM |
|-------------|---------------------------|
| Sem backtracking necessário | Cada token gerado é definitivamente correto dado o contexto |
| Significados únicos de tokens | Sem necessidade de resolver ambiguidade por contexto distante |
| Blocos iniciados por keyword | `func`, `type`, `when`, `loop` iniciam seus construtos inequivocamente |
| Sem expansão de macro | O código gerado é o código real — sem transformações ocultas |
| Retorno obrigatório | Sem confusão entre retornos implícitos e explícitos |

### 10.4.2 Comparação com Outras Gramáticas

| Linguagem | Classe de Gramática | Construtos Problemáticos para LLMs |
|-----------|--------------------|------------------------------------|
| TML | LL(1) | Nenhum por design |
| Rust | Sensível ao contexto (turbofish, lifetimes) | Ambiguidade `<>`, lifetimes `'a`, sintaxe de macro |
| C++ | Sensível ao contexto (templates, nomes dependentes) | `>>` em templates, `typename`, SFINAE |
| Go | LL(1) com exceções menores | Majoritariamente limpo; ponto e vírgula implícito pode confundir |
| Python | LL(1) com sensibilidade a recuo | Espaço em branco significativo, junção implícita de linha |

---

## 10.5 Identificadores Auto-Documentados

O TML sistematicamente prefere nomes que descrevem intenção sobre nomes abreviados ou metafóricos:

| TML | Alternativa | Por Que o Nome do TML é Melhor para LLMs |
|-----|------------|------------------------------------------|
| `behavior` | `trait` | Dados de treinamento de LLM contêm "behavior" em muitos contextos — o significado da palavra é claro |
| `when` | `match` | "when X is Y" (quando X é Y) lê como inglês natural — LLMs são treinados extensivamente em inglês |
| `Maybe[T]` | `Option<T>` | "maybe there's a value" (talvez haja um valor) é imediatamente compreensível |
| `Outcome[T,E]` | `Result<T,E>` | "the outcome of an operation" (o resultado de uma operação) é inequívoco |
| `Just(x)` | `Some(x)` | "just this value" (apenas este valor) é afirmativo e claro |
| `Nothing` | `None` | "nothing" (nada) é universalmente compreendido |
| `Heap[T]` | `Box<T>` | "on the heap" (no heap) descreve a localização de memória |
| `Shared[T]` | `Rc<T>` | "shared ownership" (ownership compartilhado) descreve a semântica |
| `Sync[T]` | `Arc<T>` | "thread-synchronized" (sincronizado por thread) descreve a garantia |
| `Duplicate` | `Clone` | "duplicate this value" (duplicar este valor) descreve a ação |
| `lowlevel` | `unsafe` | "low-level code" (código de baixo nível) é descritivo, não pejorativo |

Quando um LLM encontra `Shared[T]` em dados de treinamento ou contexto de geração, a palavra "shared" (compartilhado) ativa associações semânticas relacionadas a acesso compartilhado, múltiplos proprietários e semântica de referência — que é exatamente o que o tipo fornece. Quando encontra `Rc<T>`, precisa ter aprendido especificamente que "Rc" é uma abreviação de "Reference Counted" (contagem de referências) — um fato que existe apenas na documentação do Rust.

---

## 10.6 Ferramental MCP para Desenvolvimento Assistido por LLM

O TML fornece um servidor MCP (Model Context Protocol) completo que expõe operações do compilador como chamadas de ferramentas estruturadas. Isso permite que agentes LLM interajam com o compilador de forma programática em vez de analisar saída de texto.

### 10.6.1 Ferramentas Disponíveis

| Categoria | Ferramentas | Propósito |
|-----------|-------------|-----------|
| Compilação | `compile`, `build`, `run`, `check` | Construir e executar código TML |
| Testes | `test` (com `structured`, `debug_layers`, `coverage`) | Executar testes com saída legível por máquina |
| Diagnóstico | `emit-ir`, `emit-mir`, `explain` | Inspecionar intermediários do compilador |
| Documentação | `docs_search`, `docs_get`, `docs_list`, `docs_resolve` | Consultar 5000+ APIs documentadas |
| Qualidade | `format`, `lint`, `cache_invalidate` | Ferramentas de qualidade de código |
| Debug | `debug` (com `check_leaks`, `backtrace`), `profile` | Depuração em tempo de execução |

### 10.6.2 Debug Layers: Diagnóstico Multi-IR

O recurso `debug_layers` é único no TML. Quando um teste falha, o agente LLM pode solicitar todas as representações intermediárias para a função com falha:

1. **Saída HIR**: Mostra o código desugared e com tipos resolvidos. Se o HIR estiver errado, o bug está no verificador de tipos ou no builder HIR.
2. **Saída MIR**: Mostra os blocos básicos em forma SSA. Se o MIR estiver errado mas o HIR estiver correto, o bug está no builder MIR.
3. **Saída LLVM IR**: Mostra a IR de nível de máquina gerada. Se a LLVM IR estiver errada mas o MIR estiver correto, o bug está no codegen.
4. **Dicas de diagnóstico**: O compilador inclui dicas textuais sobre fontes prováveis de erros baseadas nos padrões de IR.

Isso transforma a depuração do compilador de um opaco "algo deu errado" em um estruturado "o erro está nesta camada de compilação específica" — informação que um agente LLM pode agir diretamente.

### 10.6.3 Coleta de Dados de Pesquisa

Cada invocação de ferramenta MCP é registrada em `mcp-call-log.jsonl` com:
- Nome e parâmetros da ferramenta
- Duração em milissegundos
- Identificador de sessão
- Número de sequência

Esses dados habilitam pesquisa sobre como LLMs usam ferramentas do compilador — quais ferramentas são mais eficazes, quais padrões levam a depuração bem-sucedida e como o uso de ferramentas se correlaciona com qualidade do código. Um projeto de pesquisa complementar [36] analisa esses dados para melhorar tanto as ferramentas quanto as estratégias de depuração do LLM.

---

## 10.7 Implicações para o Design de Linguagens

A abordagem do TML sugere vários princípios para o design futuro de linguagens de programação na era dos LLMs:

### 10.7.1 Princípio: Minimizar Sobrecarga de Símbolos

Cada símbolo dependente de contexto é um problema de classificação para o LLM. Linguagens projetadas para geração por LLM devem visar uma bijeção entre tokens e significados.

### 10.7.2 Princípio: Preferir Keywords a Símbolos

Keywords se beneficiam dos dados de treinamento de LLM: a palavra "behavior" (comportamento) aparece em milhões de textos em inglês com significado consistente. O símbolo `trait` precisa ser aprendido especificamente a partir de contextos de programação. Keywords também comprimem melhor em tokenizadores BPE.

### 10.7.3 Princípio: Alinhar a Gramática com o Modelo de Geração

LLMs geram da esquerda para a direita sem backtracking. Gramáticas LL(1) se alinham naturalmente com esse modelo de geração. Gramáticas sensíveis ao contexto exigem que o modelo mantenha dependências de longo alcance que aumentam a probabilidade de erro.

### 10.7.4 Princípio: Fornecer Interfaces de Ferramentas Estruturadas

A saída de texto bruto de compiladores é difícil para LLMs analisarem de forma confiável. Interfaces estruturadas (MCP, LSP, saída JSON) habilitam LLMs a interagir com ferramentas de desenvolvimento de forma programática, reduzindo a carga de análise e habilitando depuração sistemática.

### 10.7.5 Princípio: Nomear as Coisas para Aprendizado por Transferência

O conhecimento do LLM se transfere entre domínios. Um tipo chamado `Maybe` ativa associações do inglês ("talvez exista, talvez não") que se transferem diretamente para o conceito de programação. Um tipo chamado `Option` ativa associações de múltiplos domínios (opções de ações, opções de configuração, opções de menu) que podem não se transferir.

---

## 10.8 Limitações e Questões em Aberto

O design LLM-first do TML é baseado em hipóteses sobre o comportamento dos LLMs que são apoiadas por testes informais mas ainda não validadas formalmente. As principais questões em aberto incluem:

1. **Melhora quantitativa de precisão**: A sintaxe baseada em keywords reduz mensuravelmente os erros de geração de LLM? Em quanto? Experimentos controlados comparando o desempenho de LLMs em tarefas equivalentes em TML e Rust forneceriam dados definitivos.

2. **Viés nos dados de treinamento**: Os LLMs atuais são treinados principalmente em linguagens existentes. Uma linguagem projetada para LLMs pode paradoxalmente ter desempenho pior simplesmente porque LLMs têm menos dados de treinamento para ela. Esse problema de bootstrapping pode se resolver à medida que as bases de código TML crescem.

3. **Trade-off de verbosidade**: O código TML é ligeiramente mais verboso que o Rust. A vantagem de clareza supera os tokens adicionais? Isso pode depender do tamanho da janela de contexto — à medida que as janelas de contexto crescem, a eficiência de tokens se torna menos crítica.

4. **Ergonomia humana**: O TML é projetado tanto para humanos quanto para LLMs. Algumas decisões de design que ajudam LLMs (sem retornos implícitos, anotações de tipo obrigatórias em alguns contextos) adicionam fricção para programadores humanos. O equilíbrio ideal é uma questão empírica.

5. **Generalização**: Os princípios do TML se transferem para outros designs de linguagem? Uma linguagem existente (Rust, Go) poderia adotar sintaxe amigável a LLMs sem quebrar compatibilidade retroativa?

Essas questões motivam pesquisa contínua, incluindo o estudo de depuração de IR por LLM descrito no artigo complementar [36].
