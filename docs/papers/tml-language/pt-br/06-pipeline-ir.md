# 6. Pipeline de Representação Intermediária em Múltiplas Camadas

## 6.1 Visão Geral

O TML emprega um pipeline de representação intermediária com cinco camadas — uma pilha de IR excepcionalmente profunda em comparação com a maioria dos compiladores. O pipeline é:

```
Source (.tml)
    |
    v
  AST          Faithful syntax tree
    |
    v
  HIR          Typed, desugared, monomorphized
    |
    v
  THIR         Coercions inserted, methods resolved, operators desugared
    |
    v
  MIR          SSA form, basic blocks, explicit control flow
    |
    v
  LLVM IR      Machine-level, target-specific
    |
    v
  Object Code  Native binary
```

Cada camada serve a um propósito distinto, e a informação é progressivamente rebaixada desde a semântica de alto nível da linguagem até as operações de nível de máquina. Essa abordagem em múltiplas camadas oferece diversas vantagens sobre pipelines mais rasos: as transformações de cada camada são isoladas, tornando-as mais fáceis de testar e depurar; os passos de otimização podem ter como alvo a camada onde são mais eficazes; e o compilador pode emitir saídas de diagnóstico em qualquer camada para depuração.

### 6.1.1 Comparação com Outros Compiladores

| Compilador | Camadas de IR | Pipeline |
|------------|--------------|----------|
| TML | 5 (AST, HIR, THIR, MIR, LLVM IR) | Rebaixamento progressivo profundo |
| Rust (rustc) | 5 (AST, HIR, THIR, MIR, LLVM IR) | Muito similar ao TML |
| Clang | 2 (Clang AST, LLVM IR) | Raso, rebaixamento direto |
| GCC | 3 (GENERIC, GIMPLE, RTL) | Profundidade média, duas camadas de otimização |
| Go | 2 (AST, SSA) | Raso, backend personalizado |
| Zig | 3 (ZIR, AIR, Código de Máquina ou LLVM IR) | Médio, auto-hospedado |
| V | 1 (AST para C) | Modelo transpilador |

O pipeline do TML é arquiteturalmente mais próximo ao do Rust — não por coincidência, pois ambas as linguagens têm complexidade semântica similar (ownership, generics, tipos algébricos, closures) que se beneficia do rebaixamento progressivo.

---

## 6.2 AST (Árvore de Sintaxe Abstrata)

A AST é produzida pelo analisador e representa a estrutura sintática exata do código-fonte. Ela preserva todos os detalhes sintáticos, incluindo posições de código-fonte, comentários e açúcar sintático.

**Propriedades principais:**
- Sem tipagem: expressões não carregam informações de tipo.
- Açúcar preservado: `var x = 5` é representado como está (não desaçucarado para `let mut`).
- Fiel ao código-fonte: posições de código-fonte permitem mensagens de erro precisas e recursos de IDE.

O analisador usa um **analisador Pratt** para expressões (subida de precedência) e **descida recursiva** para declarações. Essa combinação é comum em compiladores modernos; trata a precedência de operadores de forma limpa enquanto mantém a análise de declarações direta.

---

## 6.3 HIR (Representação Intermediária de Alto Nível)

O HIR é a primeira representação semanticamente enriquecida. É produzido pelo construtor HIR (`hir_builder.cpp`) a partir da AST verificada de tipos.

### 6.3.1 Transformações

| AST | HIR | Transformação |
|-----|-----|--------------|
| `var x = 5` | `let mut x: I32 = 5` | Desaçucaramento + anotação de tipo |
| `Just(x)` | `Maybe[I32]::Just(x)` com variant_index=0 | Resolução de tipo + índice |
| `point.x` | `point.x` com field_index=0, type=F64 | Resolução de campo |
| `do(x) x + n` | Closure com capturas `[n: I32]` | Análise de captura |
| `for item in list {}` | Chamadas de protocolo de iterador | Desaçucaramento para `.into_iter()` + loop |
| `items.map(do(x) x * 2)` | `map[I32, I32]` monomorphizado | Instanciação de generic |

### 6.3.2 Monomorphização

O TML realiza monomorphização no nível HIR: funções e tipos genéricos são instanciados com argumentos de tipo concretos. Por exemplo, `List[I32]` e `List[Str]` se tornam tipos concretos separados no HIR, cada um com seus próprios métodos.

Esta é uma escolha arquitetural com tradeoffs:

- **Vantagem**: Passos downstream (construção MIR, geração de código) trabalham inteiramente com tipos concretos, simplificando sua implementação.
- **Vantagem**: Habilita otimização por instanciação — `sort[I32]` pode ser otimizado de forma diferente de `sort[Str]`.
- **Desvantagem**: Aumento no tamanho do código — N instanciações produzem N cópias de cada função.
- **Comparação**: Rust também faz monomorphização, aproximadamente na mesma etapa. O Go usa passagem de dicionário (gcshape stenciling) para evitar duplicação de código ao custo de overhead em runtime.

### 6.3.3 Análise de Captura de Closures

O construtor HIR analisa closures para determinar quais variáveis elas capturam e como (por valor, por referência ou por referência mutável). Essa informação é codificada na representação HIR de closures:

```
// Source
let factor = 2
let doubled = items.map(do(x) x * factor)

// HIR representation
Closure {
    params: [x: I32],
    captures: [factor: I32, by_value],
    body: Mul(x, factor),
    return_type: I32,
}
```

A análise de captura determina a classificação do tipo da closure: `Fn` (capturas imutáveis), `FnMut` (capturas mutáveis) ou `FnOnce` (capturas movidas). Isso é idêntico à classificação de closures do Rust.

---

## 6.4 THIR (HIR Tipado)

O THIR é uma adição relativamente recente ao pipeline do TML, inserida entre HIR e MIR. Ele realiza transformações que requerem informações de tipo completas e resolução de traits.

### 6.4.1 Transformações

**Inserção de coerção implícita:**
```
// HIR: add(a: I32, b: I64) — type mismatch
// THIR: add(CoercionExpr(a, I32 -> I64), b) — coercion made explicit
```

**Desaçucaramento de operadores para chamadas de método:**
```
// HIR: a + b  (where a: Point, b: Point)
// THIR: a.add(b)  (resolved to impl Add for Point)
```

**Resolução de métodos via solucionador de traits:**
```
// HIR: items.sort()  (which sort? could be from multiple behaviors)
// THIR: <List[I32] as Sortable>::sort(items)  (resolved to specific impl)
```

**Verificação de exaustividade de padrões:**
O THIR verifica que as expressões `when` cobrem todas as variantes possíveis do tipo correspondido. Padrões faltando produzem um erro em tempo de compilação.

### 6.4.2 Por Que o THIR Existe

O THIR existe porque certas transformações requerem tanto informações de tipo QUANTO resultados de resolução de traits, que não estão disponíveis durante a construção do HIR. Especificamente:

1. **Inserção de coerção** requer conhecer o tipo de destino, que depende de como a expressão é usada — informação que flui bidirecionalmente e só é totalmente resolvida após a verificação de tipos.
2. **Desaçucaramento de operadores** requer saber qual implementação de `Add`/`Sub`/etc. chamar, determinada pelo solucionador de traits.
3. **Resolução de métodos** na presença de múltiplos behaviors aplicáveis requer a análise de coerência e especificidade do solucionador de traits.

O THIR do Rust serve a um propósito similar, embora as transformações específicas difiram em detalhes.

---

## 6.5 MIR (Representação Intermediária de Nível Médio)

O MIR é o principal alvo de otimização e análise. É uma representação em forma SSA com blocos básicos, terminadores explícitos e valores tipados.

### 6.5.1 Estrutura

Uma função MIR consiste em:
- **Blocos básicos**: Sequências de instruções seguidas por um terminador.
- **Instruções**: Operações que produzem valores (aritmética, carregamentos, armazenamentos, chamadas).
- **Terminadores**: Operações de fluxo de controle (branch, branch condicional, retorno, switch).
- **Identificadores de valor**: Todo valor computado tem um identificador único `%N`.

```
func @max_I32(%0: i32, %1: i32) -> i32 {
  bb0:
    %2 = cmp.gt %0, %1
    br.cond %2, bb1, bb2

  bb1:
    ret %0

  bb2:
    ret %1
}
```

### 6.5.2 Caminhos Duais de Construção MIR

O TML mantém de forma única **dois caminhos paralelos** para construção do MIR:

**Caminho A: HIR para MIR (legado)**
- Arquivos: `hir_mir_builder.cpp`, `builder/hir_expr.cpp`, `builder/hir_expr_control.cpp`
- Entrada: HirModule
- Status: Maduro, trata todos os recursos da linguagem, usado para compilação de produção

**Caminho B: THIR para MIR (novo)**
- Arquivos: `thir_mir_builder.cpp`, `thir_mir_builder_expr.cpp`
- Entrada: ThirModule
- Status: Em desenvolvimento, suporta um subconjunto crescente de recursos da linguagem

A justificativa para caminhos duplos é migração: o Caminho B eventualmente substituirá o Caminho A, mas a transição é incremental. O Caminho A ignora THIR completamente (realizando coerções e resolução de métodos durante a construção MIR), enquanto o Caminho B recebe um THIR totalmente resolvido e realiza um rebaixamento mais limpo e fundamentado.

Isso é análogo à própria migração do Rust de geração de código baseada em AST para geração de código baseada em MIR, que levou vários anos e foi feita incrementalmente.

### 6.5.3 Instruções MIR Principais

| Categoria | Instruções | Descrição |
|-----------|------------|-----------|
| Constantes | `const.i32`, `const.f64`, `const.str`, `const.bool` | Valores literais |
| Aritmética | `add`, `sub`, `mul`, `div`, `rem`, `neg` | Operações numéricas |
| Comparação | `cmp.eq`, `cmp.ne`, `cmp.lt`, `cmp.gt`, `cmp.le`, `cmp.ge` | Comparações |
| Lógica | `and`, `or`, `not` | Operações booleanas |
| Memória | `alloca`, `load`, `store`, `gep` | Alocação de pilha e acesso |
| Agregados | `struct_create`, `struct_extract`, `enum_create`, `enum_discriminant` | Tipos compostos |
| Chamadas | `call`, `call_indirect` | Invocação de função |
| Casts | `cast`, `bitcast`, `trunc`, `zext`, `sext` | Conversões de tipo |

### 6.5.4 Otimização

O MIR passa por 52 passos de otimização (detalhados na Seção 7), organizados em um pipeline cuidadosamente ordenado. O passo mais crítico é **mem2reg**, que promove alocações de pilha para registradores SSA — essencial porque o construtor MIR conservadoramente aloca valores na pilha, e mem2reg remove a indireção desnecessária.

---

## 6.6 Geração de LLVM IR

A etapa final de rebaixamento converte MIR otimizado em texto LLVM IR, que é então analisado e compilado pela biblioteca LLVM embutida.

### 6.6.1 MirCodegen

A classe `MirCodegen` (`mir_codegen.cpp`) percorre o módulo MIR e emite texto LLVM IR. Responsabilidades principais:

- **Rebaixamento de tipos**: Tipos TML para tipos LLVM (`I32` para `i32`, `Str` para `ptr`, structs para tipos struct LLVM).
- **Emissão de funções**: Funções MIR para definições de função LLVM com convenções de chamada corretas.
- **Rebaixamento de instruções**: Instruções MIR para instruções LLVM (a maioria são mapeamentos 1:1).
- **Tratamento de ABI**: Convenção sret para retornos de struct grandes, byval para parâmetros de valor.
- **Declarações de runtime**: Declarações de função do runtime C para chamadas FFI.

### 6.6.2 Despacho de Chamadas de Método

Um detalhe importante de implementação: no caminho MIR, chamadas de método são representadas como instruções `CallInst` regulares (não uma `MethodCallInst` separada). O método é resolvido para uma função concreta durante o rebaixamento THIR ou construção HIR-para-MIR, e quando chega ao MIR, é simplesmente uma chamada de função com o receptor como primeiro argumento. Isso simplifica a otimização MIR (sem tratamento especial para chamadas de método) e corresponde à representação MIR do Rust.

### 6.6.3 Metodologia Rust-como-Referência

O TML emprega uma abordagem sistemática para avaliar a qualidade do IR: a metodologia Rust-como-Referência. Como TML e Rust têm semântica similar e ambos têm LLVM como alvo, código TML e Rust equivalentes deveriam produzir LLVM IR comparável. Quando a saída do TML diverge significativamente (mais instruções, layouts de tipo piores, alocações desnecessárias), isso indica um bug de geração de código ou uma oportunidade de otimização.

Essa metodologia é descrita em detalhes na Seção 7.

---

## 6.7 Recurso de Camadas de Debug

Um aspecto único do pipeline de IR do TML é sua integração com a infraestrutura de testes por meio do recurso de **debug_layers**. Quando um teste falha, o compilador pode ser instruído a emitir HIR, MIR e LLVM IR para a função com falha, juntamente com dicas de diagnóstico que identificam qual camada contém o bug:

```
Test FAILED: test_sort_list

=== HIR ===
[HIR output showing type-resolved, desugared code]
Diagnosis: HIR types look correct

=== MIR ===
[MIR output showing SSA form with basic blocks]
Diagnosis: MIR control flow correct, but %7 has unexpected type

=== LLVM IR ===
[LLVM IR output]
Diagnosis: Type mismatch at call instruction — likely MIR codegen bug
```

Esse recurso é projetado especificamente para depuração assistida por LLMs: um agente de IA pode ler todas as três camadas de IR, compará-las e localizar o bug em uma fase específica de compilação. Nenhuma outra linguagem fornece esse nível de depuração de IR integrada em seu framework de testes.

---

## 6.8 Resumo

O pipeline de IR com cinco camadas do TML fornece:

1. **Rebaixamento progressivo** — Cada camada remove um nível de abstração, tornando as transformações isoladas e testáveis.
2. **Otimização no nível certo** — Otimizações cientes de tipos no nível MIR, otimizações de nível de máquina no nível LLVM.
3. **Poder de diagnóstico** — Saída de IR em cada camada habilita localização precisa de bugs.
4. **Caminho de migração** — Caminhos duais de construção MIR habilitam melhoria incremental sem quebrar código existente.
5. **Preservação semântica** — Cada rebaixamento preserva a semântica, habilitando verificação formal do pipeline.

A profundidade do pipeline é justificada pela complexidade semântica da linguagem: ownership, generics, behaviors, closures e tipos de dados algébricos todos requerem tratamento cuidadoso que se beneficia de representações intermediárias. Linguagens mais simples (Go, C) podem se dar ao luxo de pipelines mais rasos porque têm menos construções semânticas para rebaixar.
