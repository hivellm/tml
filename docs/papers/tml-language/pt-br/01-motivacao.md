# 1. Introdução e Motivação

## 1.1 A Mudança no Cenário de Produção de Código

O ecossistema de desenvolvimento de software está passando por uma transformação fundamental na forma como o código é produzido. Desde 2022, os Large Language Models (LLMs) deixaram de ser curiosidades experimentais para se tornar ferramentas de produção usadas diariamente por milhões de desenvolvedores. O GitHub reporta que o Copilot gera mais de 46% do código nos arquivos em que está ativo. O Claude da Anthropic, o GPT-4 da OpenAI e o Gemini do Google são rotineiramente utilizados para geração, refatoração e depuração de código em todas as principais linguagens de programação.

Essa mudança levanta uma questão que a comunidade de linguagens de programação ainda não havia considerado: **As linguagens de programação deveriam ser projetadas pensando em geradores de código baseados em IA?**

Toda linguagem de programação existente — do FORTRAN (1957) ao Rust (2015) — foi projetada exclusivamente para programadores humanos. As decisões de sintaxe otimizavam velocidade de digitação manual, escaneamento visual e desambiguação contextual. Essas otimizações são irrelevantes ou contraproducentes para LLMs, que:

1. **Não digitam** — Verbosidade de tokens não é uma preocupação de usabilidade.
2. **Não podem retroceder** — A geração autoregressiva produz tokens da esquerda para a direita de forma irrevogável.
3. **Carecem de raciocínio contextual** — A ambiguidade de símbolos que os humanos resolvem por compreensão causa erros sistemáticos.
4. **Operam dentro de janelas de contexto** — A eficiência de tokens determina quanta quantidade de código cabe em um único contexto de geração.
5. **Beneficiam-se do aprendizado por transferência** — Palavras em inglês familiares ativam associações semânticas pré-treinadas.

O TML foi projetado para endereçar essas características.

---

## 1.2 O Problema: Erros de Geração Induzidos pela Sintaxe

A análise de código gerado por LLMs em múltiplos estudos revela padrões recorrentes de erros que se originam no design da sintaxe das linguagens:

### 1.2.1 Ambiguidade com Colchetes Angulares

Em Rust, C++, Java e TypeScript, o caractere `<` exerce dupla função: tanto como operador de comparação quanto como delimitador de parâmetros de tipo genérico. LLMs frequentemente geram expressões genéricas malformadas, especialmente em contextos aninhados:

```rust
// LLM-generated Rust with common error
let map: HashMap<String, Vec<(String, i32)>> = HashMap::new();
//                                         ^^ was >> correctly parsed? Depends on context
```

A sequência de fechamento `>>` é sintaticamente idêntica ao operador de deslocamento à direita. A análise requer desambiguação baseada em tipos — informação que o LLM não possui durante a geração token por token.

### 1.2.2 Confusão com o Caractere de Pipe

A sintaxe de closures do Rust usa `|` como delimitador: `|x| x + 1`. O mesmo caractere serve como OR bitwise (`a | b`), e em markdown (onde grande parte dos dados de treinamento de LLMs reside), como separador de colunas em tabelas. LLMs gerando código Rust em documentação markdown frequentemente produzem closures corrompidas, pois os caracteres de pipe são interpretados como bordas de tabela.

### 1.2.3 Complexidade das Anotações de Tempo de Vida

As anotações de tempo de vida explícitas do Rust (`'a`, `'b`, `'static`) são uma fonte persistente de erros em LLMs. A sintaxe apóstrofo-identificador é incomum, as regras de elisão de tempo de vida são complexas, e as anotações interagem com os generics de formas que exigem raciocínio teórico de tipos aprofundado:

```rust
// LLMs frequently generate incorrect lifetimes
fn longest<'a, 'b>(x: &'a str, y: &'b str) -> &'a str  // Should this be 'a or 'b?
```

### 1.2.4 Ambiguidade na Invocação de Macros

O caractere `!` do Rust serve como NOT lógico, operador de propagação de erros (na posição `?`) e símbolo de invocação de macro (`println!`). LLMs precisam determinar qual significado se aplica a partir do contexto — uma tarefa de classificação com taxa de erro não nula.

---

## 1.3 A Solução: TML

O TML aborda esses problemas por meio de princípios de design sistemáticos:

### Princípio 1: Um Token, Um Significado

Cada token no TML tem exatamente um papel sintático. Não existe contexto em que o significado de um token dependa de informação de tipo, profundidade de aninhamento ou construções ao redor.

| Token | Significado | Sempre |
|-------|-------------|--------|
| `[` | Parâmetro genérico ou índice de array | Sempre |
| `<` | Comparação menor-que | Sempre |
| `do` | Introdução de closure | Sempre |
| `and` | AND lógico | Sempre |
| `ref` | Tipo de referência | Sempre |

### Princípio 2: Gramática LL(1)

A gramática do TML é LL(1): um único token de antecipação determina a regra de produção aplicável. Essa propriedade está alinhada com a geração autoregressiva dos LLMs, onde cada token é produzido baseando-se apenas no contexto precedente.

### Princípio 3: Nomes Autodocumentados

Os nomes de tipos e palavras-chave são escolhidos para ativar associações semânticas adequadas em LLMs treinados em texto em inglês. `Maybe[T]` comunica opcionalidade; `behavior` comunica contratos comportamentais; `lowlevel` comunica abstração reduzida.

### Princípio 4: Inferência em vez de Anotação

Quando seguro fazê-lo, o TML infere informações em vez de exigir anotações. Todos os tempos de vida são inferidos (sem sintaxe `'a`). Argumentos de tipo genérico são inferidos nos locais de chamada. Tipos de retorno podem ser inferidos a partir dos corpos das funções. Isso reduz o fardo de anotações que causa erros em LLMs.

### Princípio 5: Sem Macros

Macros quebram a análise determinística porque introduzem transformações de sintaxe arbitrárias. O TML substitui macros por decorators (`@auto`, `@test`, `@extern`) que são processados pelo compilador de forma previsível, sem alterar a sintaxe.

---

## 1.4 Escopo e Contribuições

Este artigo faz as seguintes contribuições:

1. **Análise de design** (Seções 2–3): Descrevemos e analisamos as decisões de sintaxe e o sistema de tipos do TML, comparando cada decisão com alternativas em Rust, C++, Go, Python, Zig, Swift e Kotlin.

2. **Arquitetura do compilador** (Seções 5–6): Apresentamos o compilador por consultas demanda-dirigida do TML com seu pipeline de IR de cinco camadas e caminhos duais de construção MIR, comparando a arquitetura ao rustc, GCC, Clang e o compilador Go.

3. **Modelo de memória** (Seção 4): Analisamos o sistema de ownership do TML — inspirado no Rust, mas com sintaxe de palavras-chave e tempos de vida inferidos — e suas implicações para segurança e expressividade.

4. **Otimização** (Seção 7): Descrevemos os 52 passos de otimização MIR e a metodologia Rust-como-Referência para avaliar a qualidade do IR.

5. **Biblioteca padrão** (Seção 8): Apresentamos o design da biblioteca padrão em três camadas com 500+ tipos e 5.000+ funções, incluindo a estratégia de migração de C para TML.

6. **Comparação abrangente** (Seção 9): Fornecemos uma matriz de comparação multidimensional cobrindo 30+ recursos em 8 linguagens.

7. **Tese de design LLM-first** (Seção 10): Articulamos e defendemos a tese de que o design de linguagens de programação deve considerar a geração de código por LLMs, com princípios específicos e evidências.

8. **Testes e ecossistema** (Seções 11–12): Descrevemos a arquitetura de testes baseada em subprocessos, as ferramentas MCP e o recurso de camadas de debug para desenvolvimento assistido por LLMs.

---

## 1.5 Organização do Artigo

O restante deste artigo está organizado da seguinte forma: a Seção 2 apresenta as decisões de design de sintaxe; a Seção 3 aborda o sistema de tipos; a Seção 4 descreve o modelo de memória; a Seção 5 detalha a arquitetura do compilador; a Seção 6 explica o pipeline de IR; a Seção 7 analisa a otimização; a Seção 8 descreve a biblioteca padrão; a Seção 9 fornece comparações entre linguagens; a Seção 10 discute o design LLM-first em profundidade; a Seção 11 aborda a infraestrutura de testes; a Seção 12 descreve o ecossistema; e a Seção 13 discute trabalhos futuros e questões em aberto.
