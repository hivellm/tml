# 3. Sistema de Tipos

## 3.1 Visão Geral

O TML implementa um sistema com tipagem estática e inferência de tipos completa baseada na unificação de Hindley-Milner, estendida com semântica de ownership, restrições de behavior e tipos de dados algébricos. O sistema de tipos é projetado para fornecer as mesmas garantias de segurança que o sistema de tipos do Rust, usando uma notação que prioriza clareza e compatibilidade com LLMs.

O verificador de tipos opera em quatro fases sequenciais:
1. **Registro** — Todas as definições de tipo, assinaturas de função e declarações de behavior são registradas no ambiente de tipos.
2. **Resolução de imports** — Os imports de módulos são resolvidos e os símbolos externos são vinculados.
3. **Vinculação de implementações** — As implementações de behavior (`impl Behavior for Type`) são registradas e verificadas quanto à coerência.
4. **Verificação de corpos** — Os corpos de função são verificados com inferência completa, usando o ambiente de tipos registrado.

Essa abordagem em fases permite referências futuras (uma função pode chamar outra função definida mais adiante no mesmo módulo) e suporta o modelo de compilação por consultas descrito na Seção 5.

---

## 3.2 Tipos Primitivos

O TML fornece um conjunto completo de tipos numéricos de largura fixa com nomes capitalizados e autodocumentados:

| Categoria | Tipos | Tamanho |
|-----------|-------|---------|
| Inteiros com sinal | `I8`, `I16`, `I32`, `I64` | 1, 2, 4, 8 bytes |
| Inteiros sem sinal | `U8`, `U16`, `U32`, `U64` | 1, 2, 4, 8 bytes |
| Ponto flutuante | `F32`, `F64` | 4, 8 bytes |
| Booleano | `Bool` | 1 byte |
| Fatia de string | `Str` | ponteiro + comprimento |
| Caractere | `Char` | 4 bytes (escalar Unicode) |
| Unidade | `Unit` | 0 bytes |

A convenção de nomenclatura é notável: o TML usa letras maiúsculas seguidas da largura em bits (`I32`, `F64`) em vez das abreviações minúsculas do Rust (`i32`, `f64`) ou da nomenclatura inconsistente do C (`int`, `float`, `long long`). Essa convenção:

1. Distingue visualmente tipos de variáveis (tipos são capitalizados, variáveis são minúsculas).
2. Comunica imediatamente a largura em bits sem exigir conhecimento de padrões específicos da plataforma.
3. Segue o padrão estabelecido por `Bool`, `Str`, `Char` — todos os tipos são identificadores capitalizados.

---

## 3.3 Inferência de Tipos

A inferência de tipos do TML é baseada no Algoritmo W (Hindley-Milner), estendida com:

- **Inferência de literais numéricos**: Literais inteiros padrão para `I32`, literais float para `F64`. Anotação explícita é necessária para outras larguras: `let x: I64 = 42`.
- **Inferência de tipo de retorno**: Os tipos de retorno de funções são inferidos a partir do corpo quando não explicitamente anotados.
- **Instanciação genérica**: Os parâmetros de tipo são inferidos a partir dos tipos de argumento nos locais de chamada.
- **Inferência de parâmetros de closure**: Os tipos de parâmetros de closure são inferidos a partir do contexto em que a closure é usada.

Exemplo de inferência progressiva:

```
let items = List.new()         // List[?T] — type parameter unknown
items.push(42)                 // List[I32] — inferred from literal
let doubled = items.map(do(x) x * 2)  // List[I32] — inferred through chain
```

O motor de inferência usa verificação de tipos bidirecional: a informação flui tanto da expressão para o tipo esperado (modo de verificação) quanto do tipo esperado para a expressão (modo de síntese). Isso habilita padrões como:

```
let result: Maybe[I64] = Just(42)  // 42 inferred as I64 from context
```

### 3.3.1 Comparação com Outros Sistemas de Inferência

| Linguagem | Nível de Inferência | Limitações |
|-----------|---------------------|------------|
| TML | HM completo com ownership | Tipos de retorno opcionais, generics inferidos |
| Rust | HM completo com tempos de vida | Tipos de retorno obrigatórios, tempos de vida às vezes explícitos |
| Go | Limitado (sintaxe `:=`) | Sem inferência de generic (até 1.18), tipos de retorno obrigatórios |
| C++ | `auto` + dedução de template | Regras de dedução complexas, SFINAE |
| Python | Nenhuma (tipagem em runtime) | Type hints opcionais, não aplicados por padrão |
| TypeScript | Estrutural + contextual | Inferência completa dentro de funções, anotações nas fronteiras |

A inferência do TML é mais próxima à do Rust em poder, mas difere em um aspecto crítico: **os tempos de vida são sempre inferidos**. Não existe sintaxe de anotação de tempo de vida no TML. Isso é discutido em detalhes na Seção 4.

---

## 3.4 Tipos de Dados Algébricos

O TML suporta tipos de dados algébricos através de suas declarações `type` e `enum`:

**Tipos produto (structs):**
```
type Point { x: F64, y: F64 }
type Pair[A, B] { first: A, second: B }
```

**Tipos soma (enums):**
```
enum Shape {
    Circle(F64),
    Rectangle(F64, F64),
    Triangle(F64, F64, F64),
}
```

**Correspondência de padrões** com verificação de exaustividade:
```
func area(shape: Shape) -> F64 {
    when shape {
        Circle(r) -> 3.14159 * r * r,
        Rectangle(w, h) -> w * h,
        Triangle(a, b, c) -> {
            let s = (a + b + c) / 2.0
            (s * (s - a) * (s - b) * (s - c)).sqrt()
        },
    }
}
```

O verificador de tipos aplica exaustividade: se uma nova variante for adicionada a `Shape`, todas as expressões `when` que correspondem a `Shape` precisam ser atualizadas. Isso é idêntico à verificação de exaustividade do Rust para expressões `match`.

### 3.4.1 Maybe[T] e Outcome[T, E]

Os tipos opcional e de erro do TML são tipos de dados algébricos com suporte especial do compilador:

```
enum Maybe[T] {
    Just(T),
    Nothing,
}

enum Outcome[T, E] {
    Ok(T),
    Err(E),
}
```

Esses tipos são integrados a recursos da linguagem:

- **Operador `!`** — Propaga erros: `let value = risky_operation()!` retorna `Err(e)` da função envolvente se a operação falhar.
- **Encadeamento opcional `?.`** — Propaga `Nothing`: `let name = user?.name` avalia como `Nothing` se `user` for `Nothing`.
- **Guards `let-else`** — `let Just(x) = maybe_value else { return Nothing }` fornece desempacotamento plano.
- **Recuperação `else`** — `let value = risky()! else default_value` fornece um valor de fallback.

Isso é mais extenso do que o operador `?` do Rust, que apenas propaga erros. O operador `?.` do TML (emprestado de JavaScript/TypeScript/Kotlin) adiciona encadeamento opcional, e `let-else` (também presente no Rust desde 1.65) fornece fluxo de controle plano para desempacotamento sequencial.

---

## 3.5 Behaviors (Traits)

O sistema de behavior do TML é semanticamente equivalente ao sistema de traits do Rust, mas usa terminologia e sintaxe diferentes:

```
behavior Display {
    func to_string(this) -> Str
}

behavior Ordered: Equal {
    func compare(this, other: ref This) -> Ordering
}
```

Propriedades-chave:

- **Restrições de behavior**: `func sort[T: Ordered](items: mut ref List[T])` — o parâmetro de tipo `T` deve implementar o behavior `Ordered`.
- **Cláusulas where**: `func merge[K, V](a: HashMap[K, V], b: HashMap[K, V]) -> HashMap[K, V] where K: Hash + Equal` — restrições complexas são expressas em cláusulas where.
- **Métodos padrão**: Behaviors podem fornecer implementações padrão que tipos podem sobrescrever.
- **Tipos associados**: Behaviors podem definir tipos associados resolvidos no momento da implementação.
- **Herança de behavior**: `behavior Ordered: Equal` — `Ordered` requer `Equal` como supertrait.

### 3.5.1 Derivação Automática

O TML fornece o decorator `@auto` para implementação automática de behavior:

```
@auto(equal, duplicate, debug, hash, order, default)
type Config {
    name: Str,
    version: I32,
    enabled: Bool,
}
```

Isso é equivalente ao `#[derive(PartialEq, Clone, Debug, Hash, Ord, Default)]` do Rust, mas com nomenclatura mais simples:

| TML @auto | Rust #[derive] | Behavior Gerado |
|-----------|----------------|-----------------|
| `equal` | `PartialEq`, `Eq` | Igualdade estrutural |
| `duplicate` | `Clone` | Cópia profunda |
| `debug` | `Debug` | Formatação de debug |
| `hash` | `Hash` | Cálculo de hash |
| `order` | `PartialOrd`, `Ord` | Ordenação de comparação |
| `default` | `Default` | Construção de valor padrão |

### 3.5.2 Objetos de Behavior (Despacho Dinâmico)

O TML suporta objetos de behavior para polimorfismo em runtime:

```
func print_all(items: List[dyn Display]) {
    for item in items {
        println(item.to_string())
    }
}
```

O tipo `dyn Display` é um ponteiro gordo contendo um ponteiro de dados e um ponteiro de vtable, idêntico à implementação `dyn Trait` do Rust. Isso habilita coleções heterogêneas e despacho em runtime ao custo de perder a otimização de monomorphização e inlining.

---

## 3.6 Sistema de Tipos Genéricos

### 3.6.1 Polimorfismo Paramétrico

Os generics do TML usam sintaxe de colchetes com monomorphização (especialização em tempo de compilação):

```
func max[T: Ordered](a: T, b: T) -> T {
    if a.compare(ref b) == Ordering.Greater {
        return a
    }
    return b
}
```

Em tempo de compilação, `max[I32]`, `max[F64]` e `max[Str]` são cada um compilados como funções separadas e especializadas — idêntico à estratégia de monomorphização do Rust e à instanciação de templates do C++.

### 3.6.2 Generics Constantes

O TML suporta parâmetros genéricos constantes em tempo de compilação:

```
type Array[T; N] {
    // Fixed-size array of N elements of type T
}

func sum_array[N](arr: ref Array[I32; N]) -> I32 { ... }
```

Isso habilita contêineres de tamanho fixo alocados na pilha sem alocação de heap, equivalente a `[T; N]` do Rust e `std::array<T, N>` do C++.

### 3.6.3 Comparação com Outros Sistemas Genéricos

| Recurso | TML | Rust | C++ | Go | Java |
|---------|-----|------|-----|----|------|
| Estratégia | Monomorphização | Monomorphização | Instanciação de template | Passagem de dicionário (gcshape stenciling) | Apagamento de tipo |
| Sintaxe | `[T]` | `<T>` | `<T>` | `[T]` | `<T>` |
| Restrições | `T: Behavior` | `T: Trait` | `concept` (C++20) | `~interface` | `extends/super` |
| Generics constantes | Sim | Sim (estável) | Sim (NTTP) | Não | Não |
| Especialização | Não | Parcial (nightly) | Completa (SFINAE, if constexpr) | Não | Não |
| Variádico | Não | Não (workaround com tupla) | Sim (parameter packs) | Não | Sim (varargs) |

O sistema genérico do TML é mais próximo ao do Rust: mesma estratégia de monomorphização, mesmas restrições de behavior/trait, mesmos generics constantes. A diferença principal é a sintaxe (colchetes vs colchetes angulares) e a ausência de parâmetros explícitos de tempo de vida.

---

## 3.7 Tipagem Estrutural vs Nominal

O TML usa **tipagem nominal**: dois tipos com campos idênticos são distintos se tiverem nomes diferentes. Este é o mesmo modelo do Rust e do C++, e difere da tipagem estrutural do Go para interfaces.

```
type Meters { value: F64 }
type Seconds { value: F64 }

// These are DIFFERENT types — cannot be mixed
let distance: Meters = Meters { value: 100.0 }
let time: Seconds = Seconds { value: 9.58 }
// distance + time  // COMPILE ERROR: type mismatch
```

No entanto, o sistema de behavior do TML usa **subtipagem estrutural** para objetos de behavior: qualquer tipo que implemente os métodos exigidos satisfaz uma restrição `dyn Behavior`, independentemente de declarar explicitamente a implementação. Isso é verificado em tempo de compilação pelo resolvedor de behaviors do verificador de tipos.

---

## 3.8 O Sistema de Impl

O TML usa blocos `impl` tanto para métodos inerentes quanto para implementações de behavior:

```
// Inherent methods
impl Point {
    func new(x: F64, y: F64) -> This { This { x, y } }
    func distance(this, other: ref Point) -> F64 { ... }
}

// Behavior implementation
impl Display for Point {
    func to_string(this) -> Str {
        `({this.x}, {this.y})`
    }
}
```

A palavra-chave `This` na posição de retorno refere-se ao tipo que está sendo implementado, evitando a necessidade de repetir o nome do tipo. Isso é equivalente a `Self` no Rust, mas usa uma palavra em inglês mais natural.

### 3.8.1 Regras de Coerência

O TML aplica coerência (a regra orphan): uma implementação de behavior `impl B for T` deve ser definida no módulo que define `B` ou no módulo que define `T`. Isso evita implementações conflitantes e é idêntico às regras de coerência do Rust.

---

## 3.9 Resumo

O sistema de tipos do TML alcança expressividade e segurança equivalentes ao Rust por meio de:

1. **Inferência de Hindley-Milner completa** com extensões de ownership — reduzindo o fardo de anotações.
2. **Tipos de dados algébricos** com correspondência de padrões exaustiva — prevenindo casos não tratados.
3. **Restrições de behavior** em generics — garantindo segurança de tipos sem overhead em runtime.
4. **Monomorphização** — generics de custo zero em runtime.
5. **Sem tempos de vida explícitos** — a inferência cuida de toda a análise de tempo de vida.

As inovações do sistema de tipos estão principalmente em nomenclatura e sintaxe, não em semântica: `behavior` para `trait`, `Maybe` para `Option`, `Outcome` para `Result`, `Duplicate` para `Clone`. Essas escolhas reduzem a curva de aprendizado e melhoram a precisão da geração de código por LLMs sem sacrificar nenhuma garantia de segurança.
