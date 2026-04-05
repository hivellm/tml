# 2. Design de Sintaxe: Palavras-Chave em vez de Símbolos

## 2.1 Filosofia de Design

A sintaxe do TML é guiada por um único princípio norteador: **cada token deve ter exatamente um significado, e esse significado deve ser autoevidente tanto para humanos quanto para grandes modelos de linguagem.** Esse princípio se manifesta como uma preferência sistemática por palavras-chave em inglês em vez de operadores simbólicos, e por estrutura explícita em vez de inferência contextual.

Esta não é meramente uma escolha estética. É uma resposta a um problema mensurável: LLMs treinados em corpora multilinguísticos frequentemente confundem símbolos dependentes de contexto. O caractere `<` serve como operador de comparação, delimitador de genérico, abridor de tag HTML, redirecionador de shell e operando de deslocamento bitwise — dependendo do contexto. Um LLM gerando código deve desambiguar esses significados através do contexto circundante, um processo que introduz erros. O TML elimina essa classe de erro inteiramente, garantindo que cada token tenha um e somente um papel sintático.

A gramática é projetada para ser **LL(1)** — um único token de antecipação é suficiente para determinar qual regra de produção aplicar. Essa propriedade é incomum entre linguagens de programação de sistemas modernas (Rust requer antecipação ilimitada para certas construções; C++ é sensível ao contexto). A análise LL(1) é significativa porque espelha o modelo de geração autoregressiva dos LLMs: cada token é produzido dado apenas o contexto precedente, sem capacidade de retroceder.

---

## 2.2 Decisões Sistemáticas de Sintaxe

### 2.2.1 Generics: [T] vs <T>

| Linguagem | Sintaxe | Ambiguidade |
|-----------|---------|-------------|
| TML | `List[T]` | Nenhuma — `[` é sempre genérico/índice |
| Rust | `Vec<T>` | `<` vs comparação requer retrocesso do analisador |
| C++ | `vector<T>` | `>>` fechamento vs deslocamento à direita, `<` vs menor-que |
| Go | `List[T]` (desde 1.18) | Mesma escolha de colchetes que TML |
| Python | `list[T]` (desde 3.9) | Mesma escolha de colchetes que TML |

O TML usa colchetes para parâmetros de tipo genérico: `HashMap[K, V]`, `Maybe[T]`, `func identity[T](x: T) -> T`. A justificativa é tanto sintática quanto pragmática:

1. **Sem ambiguidade no analisador.** A expressão `a < b` é sempre uma comparação. Não existe contexto em que `<` introduz uma lista de parâmetros de tipo.
2. **Sem confusão de múltiplos tokens.** A expressão Rust `a<b, c>` exige que o analisador determine se são duas comparações ou uma instanciação genérica — uma decisão que depende do ambiente de tipos, não da sintaxe.
3. **Convergência da indústria.** Go (1.18+), Python (3.9+) e Scala 3 adotaram independentemente colchetes para generics, sugerindo que a comunidade de design de linguagens está convergindo para longe dos colchetes angulares.
4. **Precisão na geração por LLMs.** Em nossos testes informais, LLMs geram instanciações genéricas sintaticamente corretas a uma taxa maior com sintaxe de colchetes, porque `[` é inequívoco em praticamente todos os contextos de programação.

### 2.2.2 Closures: do(x) vs |x|

| Linguagem | Sintaxe | Ambiguidade |
|-----------|---------|-------------|
| TML | `do(x) x * 2` | Nenhuma — `do` é uma palavra-chave |
| Rust | `\|x\| x * 2` | `\|` vs OR bitwise, separador de tabela markdown |
| C++ | `[](auto x) { return x * 2; }` | Sintaxe de captura complexa |
| Go | `func(x int) int { return x * 2 }` | Verboso, mas claro |
| Python | `lambda x: x * 2` | Palavra-chave `lambda` (clara) |

A sintaxe de closure do TML usa a palavra-chave `do` seguida de parâmetros entre parênteses:

```
items.filter(do(x) x > 0).map(do(x) x * 2)
```

O caractere de pipe `|` no Rust tem tripla função: operador de OR bitwise, delimitador de parâmetro de closure e (em markdown) separador de colunas de tabela. LLMs gerando código Rust dentro de documentação markdown frequentemente produzem closures malformadas porque o caractere de pipe é interpretado como borda de tabela. O TML elimina inteiramente essa classe de erro.

A palavra-chave `do` também fornece um sinal visual claro dos limites de closure. Em expressões aninhadas, as closures delimitadas por pipe do Rust podem ser visualmente confusas:

```rust
// Rust: Where does one closure end and another begin?
items.iter().filter(|x| x.map(|y| y > 0).unwrap_or(false))
```

```
// TML: Closure boundaries are unambiguous
items.iter().filter(do(x) x.map(do(y) y > 0).unwrap_or(false))
```

### 2.2.3 Correspondência de Padrões: when vs match

| Linguagem | Palavra-Chave | Justificativa |
|-----------|---------------|---------------|
| TML | `when` | Lê como inglês natural: "when x is..." |
| Rust | `match` | Termo técnico da tradição ML |
| Python | `match` (3.10+) | Seguiu convenção Rust/ML |
| Kotlin | `when` | Mesma escolha que TML — legibilidade |

O TML usa `when` para correspondência de padrões, uma escolha compartilhada com Kotlin. A palavra-chave lê naturalmente em inglês:

```
when status {
    Ok(value) -> process(value),
    Err(e) -> log_error(e),
}
```

Isso se lê como "quando status for Ok(value), processe o valor; quando status for Err(e), registre o erro." A palavra-chave `match`, por sua vez, exige que o leitor saiba que "match" significa "correspondência de padrão" — um termo da tradição de teoria de tipos ML que não é autoevidente para programadores sem experiência em programação funcional.

### 2.2.4 Behaviors: behavior vs trait

| Linguagem | Palavra-Chave | Justificativa |
|-----------|---------------|---------------|
| TML | `behavior` | Descreve o que define — um conjunto de comportamentos |
| Rust | `trait` | Metáfora genética — menos intuitivo |
| Go | `interface` | Conjunto de métodos — claro, mas limitado |
| Swift | `protocol` | Metáfora de comunicação |
| C++ | `concept` (C++20) | Matemático — abstrato |

A escolha de `behavior` em vez de `trait` é semântica, não sintática. Um behavior define o que um tipo *pode fazer* — seus comportamentos. A palavra "trait" vem da genética e da psicologia da personalidade; seu uso em programação é um jargão técnico que precisa ser aprendido. "Behavior" é imediatamente compreensível:

```
behavior Printable {
    func to_text(this) -> Str
}

impl Printable for Point {
    func to_text(this) -> Str {
        return `({this.x}, {this.y})`
    }
}
```

Ler `behavior Printable` comunica: "isto define um comportamento imprimível que tipos podem implementar." Ler `trait Printable` exige saber que "trait" significa "interface" no vocabulário do Rust.

### 2.2.5 Operadores Booleanos: Palavras-Chave vs Símbolos

| Operação | TML | Rust/C++/Go | Python |
|----------|-----|-------------|--------|
| AND lógico | `and` | `&&` | `and` |
| OR lógico | `or` | `\|\|` | `or` |
| NOT lógico | `not` | `!` | `not` |

O TML segue a escolha do Python de usar palavras-chave para operadores booleanos. A justificativa é tripla:

1. **Significado único.** Em Rust, `!` é tanto NOT lógico quanto o símbolo de invocação de macro (`println!`). Em C, `!` é NOT lógico enquanto `~` é NOT bitwise. No TML, `not` é sempre NOT lógico, e não há sistema de macros.
2. **Clareza de tokens para LLMs.** A string `&&` é tipicamente tokenizada como dois ou três tokens por tokenizadores de LLMs (`&` + `&` ou `&&`), enquanto `and` é um token. Isso afeta tanto a precisão da geração quanto a eficiência da janela de contexto.
3. **Legibilidade.** `if x > 0 and x < 100` lê mais naturalmente do que `if x > 0 && x < 100`, especialmente para programadores vindos de Python, SQL ou com formação em linguagem natural.

### 2.2.6 Referências: ref T vs &T

| Linguagem | Ref Imutável | Ref Mutável |
|-----------|-------------|-------------|
| TML | `ref T` | `mut ref T` |
| Rust | `&T` | `&mut T` |
| C++ | `const T&` | `T&` |

O TML substitui o e-comercial pela palavra-chave `ref`:

```
func length(s: ref Str) -> I64 { ... }
func append(s: mut ref Str, suffix: Str) { ... }
```

O caractere `&` é sobrecarregado entre linguagens: endereço-de (C), referência (C++/Rust), AND bitwise (em todas as linguagens) e concatenação de strings (em algumas linguagens). Usar a palavra `ref` elimina toda ambiguidade e lê naturalmente: "uma referência a Str" versus "um e-comercial Str."

### 2.2.7 Tipos de Erro: Maybe/Outcome vs Option/Result

| TML | Rust | Justificativa |
|-----|------|---------------|
| `Maybe[T]` | `Option<T>` | "Talvez haja um valor" — autodocumentado |
| `Just(x)` | `Some(x)` | "Exatamente este valor" — afirmativo |
| `Nothing` | `None` | "Nada aqui" — descritivo |
| `Outcome[T, E]` | `Result<T, E>` | "O resultado de uma operação" — orientado ao processo |
| `Ok(x)` | `Ok(x)` | Idêntico — já é claro |
| `Err(e)` | `Err(e)` | Idêntico — já é claro |

Os nomes `Maybe` e `Outcome` descrevem seu *propósito* em vez de sua *estrutura*. Um valor `Maybe[User]` comunica "pode haver um usuário" de forma mais direta do que `Option<User>`. Um `Outcome[File, IoError]` comunica "o resultado de uma operação de arquivo" de forma mais clara do que `Result<File, IoError>`.

### 2.2.8 Ponteiros Inteligentes: Nomes com Propósito

| TML | Rust | Justificativa do Nome TML |
|-----|------|--------------------------|
| `Heap[T]` | `Box<T>` | Descreve *onde* o valor vive |
| `Shared[T]` | `Rc<T>` | Descreve *como* o ownership funciona |
| `Sync[T]` | `Arc<T>` | Descreve a propriedade de *thread-safety* |

`Box` é uma metáfora. `Rc` é uma abreviação (Reference Counted). `Arc` é uma abreviação (Atomically Reference Counted). Esses nomes requerem conhecimento de domínio para decodificar. Os nomes do TML — `Heap`, `Shared`, `Sync` — descrevem a propriedade semântica diretamente.

### 2.2.9 Código de Baixo Nível: lowlevel vs unsafe

O TML usa `lowlevel` em vez de `unsafe` para blocos que ignoram o borrow checker e as garantias de segurança de tipos:

```
lowlevel {
    let raw = mem_alloc(size)
    ptr_write(raw, value)
}
```

A palavra "unsafe" implica perigo e irresponsabilidade. Esse enquadramento desencoraja desenvolvedores de usá-la mesmo quando é a ferramenta correta — por exemplo, ao implementar estruturas de dados que exigem aritmética de ponteiros. A palavra "lowlevel" é descritivamente precisa sem julgamento moral: o código opera em um nível mais baixo de abstração. Não é inerentemente errado; simplesmente requer mais cuidado.

### 2.2.10 Outras Decisões Notáveis

**Construção de loop unificada:**
```
loop (condition) { body }          // while loop
for item in collection { body }    // iterator loop
for i in 0 to 10 { body }         // range loop (exclusive)
for i in 1 through 10 { body }    // range loop (inclusive)
```

As palavras-chave `to` e `through` substituem os operadores `..` e `..=` do Rust. "0 to 10" e "1 through 10" são imediatamente compreensíveis; "0..10" e "0..=10" requerem conhecimento da sintaxe de range do Rust.

**Literais de template:**
```
let greeting = `Hello, {name}! You have {count} messages.`
```

Literais de template usam delimitadores de crase com interpolação `{expr}`, seguindo a convenção JavaScript/TypeScript. É mais simples do que a macro `format!("{}", name)` do Rust e evita a mini-linguagem de string de formato.

**Guards let-else:**
```
let Just(user) = find_user(id) else { return Nothing }
let Just(email) = user.email else { return Nothing }
```

Este padrão substitui correspondência de padrões profundamente aninhada por expressões guard planas e sequenciais — melhorando drasticamente a legibilidade em caminhos de tratamento de erros.

**Encadeamento opcional:**
```
let name = parse(json)?.get_string("user")?.get_string("name")
```

O operador `?.` propaga `Nothing` através de cadeias de métodos, evitando expressões `when` aninhadas. Isso é emprestado de JavaScript/TypeScript e Kotlin, linguagens que comprovaram o valor ergonômico do encadeamento opcional.

---

## 2.3 Análise de Eficiência de Tokens

Uma consideração prática importante para uso com LLMs é a eficiência de tokens — quantos tokens uma determinada construção consome na janela de contexto do LLM. Comparamos contagens de tokens para construções equivalentes (usando uma aproximação de tokenizador BPE):

| Construção | TML | Rust | Economia |
|-----------|-----|------|---------|
| Assinatura de função genérica | `func max[T: Ord](a: T, b: T) -> T` (12 tokens) | `fn max<T: Ord>(a: T, b: T) -> T` (13 tokens) | ~8% |
| Closure em cadeia | `.filter(do(x) x > 0)` (8 tokens) | `.filter(\|x\| x > 0)` (9 tokens) | ~11% |
| Propagação de erro | `let v = try_parse()!` (6 tokens) | `let v = try_parse()?;` (7 tokens) | ~14% |
| Expressão booleana | `if a and b or not c` (7 tokens) | `if a && b \|\| !c` (8–10 tokens) | ~20% |
| Parâmetro de referência | `ref List[I32]` (4 tokens) | `&Vec<i32>` (5 tokens) | ~20% |
| Correspondência de padrão | `when x { ... }` (4 tokens) | `match x { ... }` (4 tokens) | 0% |

Embora as economias individuais sejam modestas (8–20%), elas se somam ao longo de um corpo de função típico. Um módulo de 500 linhas pode conter centenas dessas construções, resultando em 10–15% menos tokens para código TML equivalente comparado ao Rust. Dentro das janelas de contexto restritas dos LLMs, essa eficiência se traduz diretamente em mais código cabendo em um único contexto de geração.

---

## 2.4 Análise de Carga Cognitiva

A carga cognitiva na sintaxe de linguagens de programação pode ser decomposta em três componentes:

1. **Carga intrínseca**: A complexidade inerente do conceito sendo expresso.
2. **Carga extrínseca**: Complexidade introduzida pelo próprio sistema de notação.
3. **Carga germânica**: Esforço gasto na construção de modelos mentais úteis.

As decisões de sintaxe do TML reduzem sistematicamente a carga extrínseca:

- **Sobrecarga de símbolos** aumenta a carga extrínseca porque o leitor deve determinar qual significado se aplica. O TML elimina isso atribuindo a cada símbolo um único significado.
- **Abreviações** aumentam a carga extrínseca porque precisam ser memorizadas. O TML usa palavras completas (`behavior`, `func`, `when`) em vez de abreviações (`trait`, `fn`, `match`).
- **Sintaxe aninhada** aumenta a carga extrínseca porque o leitor precisa rastrear múltiplos níveis de estrutura. O TML fornece `let-else` e `?.` para achatar expressões profundamente aninhadas.

A contrapartida é verbosidade: o código TML é marginalmente mais longo do que o código Rust equivalente. No entanto, pesquisas em psicologia cognitiva sugerem que legibilidade e compreensibilidade são mais importantes do que brevidade para evitar erros — uma descoberta que se aplica igualmente a geradores de código humanos e de máquina.

---

## 2.5 Recuperação de Erros

A sintaxe baseada em palavras-chave do TML fornece mensagens de erro superiores em comparação com sintaxes carregadas de símbolos. Considere um colchete de fechamento ausente:

```
// TML error: Expected ']' to close generic parameter list starting at line 5
func sort[T: Ord(items: List[T]) -> List[T]
                 ^--- expected ']' here

// Rust equivalent: This is harder to diagnose because '<' could be comparison
fn sort<T: Ord(items: Vec<T>) -> Vec<T>
              ^--- is this a function call or a generic parameter?
```

Como `[` sempre abre uma lista de parâmetros genéricos no TML (não há outro significado), o compilador pode identificar imediatamente o erro e fornecer um diagnóstico preciso. No Rust, o duplo papel do caractere `<` (comparação vs genérico) significa que o analisador deve explorar múltiplas interpretações antes de identificar o erro, frequentemente produzindo mensagens menos precisas.

Essa propriedade é particularmente valiosa para código gerado por LLMs: quando o compilador fornece mensagens de erro claras e inequívocas, o LLM pode corrigir sua saída de forma mais confiável nas iterações subsequentes.
