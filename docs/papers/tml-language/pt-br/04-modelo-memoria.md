# 4. Modelo de Memória

## 4.1 Visão Geral

O TML adota um modelo de ownership em tempo de compilação derivado do sistema de ownership do Rust. Em vez de depender de um coletor de lixo (como Go e Java fazem) ou exigir gerenciamento manual de memória (como C e C++ fazem), o compilador do TML prova estaticamente a correção da memória através de um borrow checker em tempo de compilação, com zero overhead em runtime pelas próprias garantias de segurança.

O invariante central é simples: todo valor em um programa TML tem exatamente um dono em qualquer ponto no tempo. Quando a variável dona sai do escopo, o valor é descartado automaticamente via RAII (Resource Acquisition Is Initialization), sem pausa de coleta de lixo, sem chamada explícita de free e sem possibilidade de double-free.

O que distingue o TML do Rust é a sintaxe superficial: o TML usa palavras-chave em inglês (`ref`, `mut ref`, `lowlevel`) em vez de símbolos (`&`, `&mut`, `unsafe`), e o TML infere todos os tempos de vida sem exigir anotação explícita.

---

## 4.2 Sistema de Ownership

### 4.2.1 A Regra do Dono Único

O TML aplica três regras em tempo de compilação:

1. **Todo valor tem exatamente um dono.** Um valor é vinculado a exatamente uma variável em qualquer ponto.
2. **Quando o dono sai do escopo, o valor é descartado.** Destrutores executam deterministicamente na saída do escopo.
3. **O ownership pode ser transferido (movido).** Após uma movimentação, a variável de origem é permanentemente invalidada.

```
let a = List.new()    // 'a' owns the list
let b = a             // ownership moved to 'b'
// a.push(1)          // COMPILE ERROR: 'a' has been moved
b.push(1)             // OK: 'b' owns the list
```

### 4.2.2 Semântica de Movimentação

Por padrão, atribuição e passagem de argumentos de função movem valores:

```
func consume(list: List[I32]) { ... }

let items = List.of(1, 2, 3)
consume(items)          // 'items' is moved into the function
// items.len()          // COMPILE ERROR: 'items' has been moved
```

Tipos que implementam o behavior `Copy` (tipos pequenos, alocados na pilha, como inteiros e booleanos) são implicitamente copiados em vez de movidos. Tipos que implementam `Duplicate` podem ser explicitamente duplicados:

```
let a = List.of(1, 2, 3)
let b = a.duplicate()   // deep copy; both 'a' and 'b' are valid
```

---

## 4.3 Referências

O sistema de referências do TML é semanticamente idêntico ao do Rust, mas usa sintaxe de palavras-chave:

| TML | Rust | Significado |
|-----|------|-------------|
| `ref T` | `&T` | Referência compartilhada (imutável) |
| `mut ref T` | `&mut T` | Referência exclusiva (mutável) |

### 4.3.1 Regras de Empréstimo

O borrow checker aplica duas regras:

1. **Múltiplas referências compartilhadas OU uma referência mutável** — nunca ambas simultaneamente.
2. **Referências não devem sobreviver ao referenciado** — sem ponteiros pendurados.

```
func length(s: ref Str) -> I64 {   // borrows 's' immutably
    return s.len()
}

func append(s: mut ref List[I32], value: I32) {  // borrows 's' mutably
    s.push(value)
}

let items = List.of(1, 2, 3)
let len = length(ref items)     // shared borrow — OK
append(mut ref items, 4)        // mutable borrow — OK (no other borrows active)
```

### 4.3.2 Justificativa da Sintaxe de Palavras-Chave

A escolha de `ref` e `mut ref` em vez de `&` e `&mut` é uma decisão de legibilidade:

- `ref List[I32]` lê como "referência a List de I32" — inglês natural.
- `&Vec<i32>` lê como "e-comercial Vec colchete-angular i32 colchete-angular" — sopa de símbolos.

Para LLMs, a sintaxe de palavras-chave tem uma vantagem adicional: a palavra "ref" ativa associações semânticas com "reference" a partir dos dados de treinamento em linguagem natural, enquanto `&` requer conhecimento específico da linguagem sobre se significa "referência" (Rust), "endereço-de" (C), "AND bitwise" (na maioria das linguagens) ou "concatenação de strings" (em algumas linguagens).

---

## 4.4 Sem Tempos de Vida Explícitos

A diferença mais significativa em relação ao modelo de memória do Rust é a ausência completa de anotações de tempo de vida explícitas no TML. Em Rust, relacionamentos complexos de referências exigem que o programador anote os tempos de vida:

```rust
// Rust: explicit lifetimes required
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
```

No TML, a função equivalente não requer anotação de tempo de vida:

```
func longest(x: ref Str, y: ref Str) -> ref Str {
    if x.len() > y.len() then x else y
}
```

### 4.4.1 Como Isso Funciona

O borrow checker do TML implementa análise de Non-Lexical Lifetimes (NLL) com inferência estendida:

1. **Regras de elisão de tempo de vida** (similares às do Rust) tratam automaticamente padrões comuns.
2. **Inferência de região** determina relacionamentos de tempo de vida a partir do grafo de fluxo de controle.
3. **Resolução de restrições** prova que todas as referências são válidas sem exigir anotações do programador.

### 4.4.2 Tradeoffs

| Aspecto | TML (sem tempos de vida) | Rust (tempos de vida explícitos) |
|---------|--------------------------|----------------------------------|
| Simplicidade | Sintaxe dramaticamente mais simples | Anotações de tempo de vida adicionam ruído |
| Expressividade | Não pode expressar alguns padrões complexos | Controle completo sobre relacionamentos de tempo de vida |
| Curva de aprendizado | Muito menor — sem sintaxe de tempo de vida para aprender | Os tempos de vida são a parte mais íngreme da curva do Rust |
| Structs auto-referenciais | Limitado — requer Pin | Possível com tempos de vida explícitos |
| Precisão de LLMs | Maior — sem erros de tempo de vida possíveis | LLMs frequentemente geram tempos de vida incorretos |

O tradeoff é intencional: o TML sacrifica uma pequena quantidade de expressividade (tipos auto-referenciais, relacionamentos complexos de tempo de vida) em troca de sintaxe dramaticamente mais simples. A grande maioria do código do mundo real não requer anotações explícitas de tempo de vida mesmo em Rust (a elisão de tempo de vida trata a maioria dos casos), portanto a abordagem do TML cobre o caso comum enquanto simplifica significativamente a linguagem.

---

## 4.5 Ponteiros Inteligentes

O TML fornece três tipos de ponteiros inteligentes com nomes autodocumentados:

### 4.5.1 Heap[T] (Rust: Box<T>)

Alocação de heap com ownership exclusivo. O valor é desalocado quando o `Heap` sai do escopo.

```
let value = Heap.new(42)         // allocates I32 on the heap
let large = Heap.new(Matrix.identity(1000))  // large value on heap
```

O nome `Heap` descreve ONDE o valor vive. O `Box` do Rust é uma metáfora que precisa ser aprendida.

### 4.5.2 Shared[T] (Rust: Rc<T>)

Ownership compartilhado com contagem de referências. Múltiplos ponteiros `Shared` podem apontar para o mesmo valor. O valor é desalocado quando o último `Shared` é descartado.

```
let a = Shared.new(Config { name: "default" })
let b = a.duplicate()    // both 'a' and 'b' point to same data
// value freed when both 'a' and 'b' go out of scope
```

O nome `Shared` descreve COMO o ownership funciona. O `Rc` (Reference Counted) do Rust é uma abreviação do mecanismo de implementação.

### 4.5.3 Sync[T] (Rust: Arc<T>)

Ownership compartilhado com contagem de referências atômicas. Versão thread-safe de `Shared`.

```
let config = Sync.new(AppConfig.load())
// Can be sent to multiple threads safely
spawn(do() { config.read() })
```

O nome `Sync` descreve a PROPRIEDADE DE SEGURANÇA. O `Arc` (Atomically Reference Counted) do Rust descreve o mecanismo de implementação.

---

## 4.6 Blocos lowlevel

O TML usa `lowlevel` em vez de `unsafe` para blocos que ignoram o borrow checker:

```
lowlevel {
    let raw = mem_alloc(size)
    ptr_write(raw, value)
    let result = ptr_read[I32](raw)
    mem_free(raw)
}
```

### 4.6.1 Intrínsecos Disponíveis

| Intrínseco | Propósito |
|-----------|----------|
| `mem_alloc(size)` | Alocar memória bruta |
| `mem_free(ptr)` | Liberar memória alocada |
| `ptr_read[T](ptr)` | Ler um valor de um ponteiro bruto |
| `ptr_write(ptr, value)` | Escrever um valor em um ponteiro bruto |
| `ptr_offset(ptr, offset)` | Aritmética de ponteiros |
| `copy_nonoverlapping(src, dst, count)` | Cópia de memória em massa |

### 4.6.2 Filosofia de Nomenclatura

A palavra "unsafe" carrega conotações morais de irresponsabilidade. Esse enquadramento desencoraja o uso mesmo quando operações de baixo nível são a ferramenta correta — por exemplo, ao implementar uma estrutura de dados de alto desempenho. "Lowlevel" é descritivamente preciso: o código opera em um nível mais baixo de abstração, ignorando as garantias de segurança do sistema de tipos. Não é inerentemente errado; simplesmente requer mais cuidado.

Essa escolha de nomenclatura reduz a barreira psicológica para usar operações de baixo nível quando genuinamente necessárias, ao mesmo tempo que claramente marca o código como necessitando de revisão adicional.

---

## 4.7 Mutabilidade Interior

O TML fornece os mesmos primitivos de mutabilidade interior que o Rust:

| TML | Rust | Propósito |
|-----|------|----------|
| `Cell[T]` | `Cell<T>` | Mutabilidade interior baseada em Copy |
| `RefCell[T]` | `RefCell<T>` | Empréstimo verificado em runtime |
| `OnceCell[T]` | `OnceCell<T>` | Inicialização lazy write-once |
| `LazyCell[T]` | `LazyCell<T>` | Computação lazy com cache |
| `UnsafeCell[T]` | `UnsafeCell<T>` | Fundação para toda mutabilidade interior |

---

## 4.8 Segurança de Concorrência

O TML aplica segurança de thread através de marker behaviors e primitivos de sincronização:

- **Behavior `Send`**: Tipos que podem ser transferidos entre threads.
- **Behavior `SyncSafe`**: Tipos que podem ser compartilhados entre threads via referências.
- **`Mutex[T]`**: Exclusão mútua com proteção de dados (os dados ficam dentro do mutex).
- **`RwLock[T]`**: Lock de leitor-escritor.
- **`Sync[T]`**: Contagem de referências atômica para ownership compartilhado thread-safe.
- **Tipos atômicos**: `AtomicI32`, `AtomicI64`, `AtomicBool`, `AtomicPtr` para programação lock-free.
- **Canais MPSC**: Passagem de mensagens multi-produtor, consumidor-único.

O modelo é idêntico ao do Rust: o sistema de tipos previne corridas de dados em tempo de compilação. `Mutex[T]` envolve os dados protegidos, garantindo que o acesso só seja possível enquanto o lock é mantido.

---

## 4.9 Matriz de Comparação

| Aspecto | TML | Rust | C++ | Go | Swift | Zig |
|---------|-----|------|-----|----|-------|-----|
| Modelo de memória | Ownership | Ownership | RAII + manual | GC | ARC | Manual |
| Segurança em tempo de compilação | Sim (borrow checker) | Sim (borrow checker) | Parcial (somente RAII) | Não (GC trata) | Parcial (ARC) | Não |
| Overhead em runtime | Zero | Zero | Zero (RAII) | Pausas de GC | Contagem ARC | Zero |
| Sintaxe de referência | `ref T` / `mut ref T` | `&T` / `&mut T` | `T&` / `const T&` | Ponteiros | Implícito | `*T` |
| Anotações de tempo de vida | Nunca (inferidas) | Às vezes explícitas | N/A | N/A | N/A | N/A |
| Sintaxe unsafe | `lowlevel {}` | `unsafe {}` | Sempre unsafe | `unsafe` | Implícito | `@import("std")` |
| Ponteiros inteligentes | `Heap`/`Shared`/`Sync` | `Box`/`Rc`/`Arc` | `unique_ptr`/`shared_ptr` | N/A (GC) | N/A (ARC) | Manual |
| Prevenção de corrida de dados | Tempo de compilação | Tempo de compilação | Nenhuma nativa | Runtime (race detector) | Runtime (Sendable) | Nenhuma nativa |

O modelo de memória do TML é o mais próximo ao do Rust entre todas as linguagens. As diferenças são puramente sintáticas (palavras-chave vs símbolos, tempos de vida inferidos vs explícitos) em vez de semânticas. Ambas as linguagens fornecem as mesmas garantias de segurança com o mesmo princípio de abstração de custo zero.
