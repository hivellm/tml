# New Syntax Features

Este documento descreve as novas funcionalidades de sintaxe adicionadas ao TML.

## Summary

✅ **Implementado e Testado:**
- While loops (`while condition { }`)
- Operador ternário (`condition ? true_val : false_val`)

⚠️ **Implementado mas com bugs:**
- Foreach sobre coleções (bug pré-existente nas funções de collection runtime)

## While Loops

Traditional `while` loop syntax is now supported as an alternative to the `loop { if condition then break }` pattern.

### Syntax

```tml
while condition {
    // body
}
```

### Examples

```tml
// Count from 0 to 4
let mut count: I32 = 0
while count < 5 {
    print(count)
    count = count + 1
}

// Sum numbers until reaching 10
let mut sum: I32 = 0
let mut i: I32 = 0
while i < 5 {
    sum = sum + i
    i = i + 1
}
```

### Features

- **Break and Continue**: Work as expected inside while loops
- **Nested Loops**: While loops can be nested
- **Condition**: Must evaluate to `Bool` type

## Ternary Operator

JavaScript-style ternary conditional operator for inline conditional expressions.

### Syntax

```tml
condition ? true_value : false_value
```

### Examples

```tml
// Find maximum of two numbers
let x: I32 = 10
let y: I32 = 20
let max: I32 = x > y ? x : y

// Find minimum
let min: I32 = x < y ? x : y

// Nested ternary (find max of three)
let a: I32 = 5
let b: I32 = 10
let c: I32 = 15
let max3: I32 = a > b ? (a > c ? a : c) : (b > c ? b : c)

// With expressions
let result: I32 = x > 3 ? x * 2 : x + 10
```

### Features

- **Type Safety**: Both branches must return the same type
- **Nestable**: Ternary operators can be nested
- **Right-Associative**: Groups right-to-left like most languages
- **Expression**: Returns a value, can be used anywhere an expression is expected

### Precedence

The ternary operator has very low precedence, just above assignment operators. This means:

```tml
let x: I32 = a > b ? c + d : e * f  // Equivalent to: a > b ? (c + d) : (e * f)
```

## Combining While and Ternary

```tml
let mut i: I32 = 0
let mut evens: I32 = 0
let mut odds: I32 = 0

while i < 10 {
    let is_even: Bool = i % 2 == 0
    evens = is_even ? evens + 1 : evens
    odds = is_even ? odds : odds + 1
    i = i + 1
}
```

## Implementation Notes

### While Loops
- Implemented using LLVM basic blocks with conditional branches
- Condition evaluated at loop start
- Body only executes if condition is true

### Ternary Operator
- Uses temporary alloca for result storage to handle nested ternaries
- Converts `Bool` (i1) to i32 automatically
- Generates clean LLVM IR with proper control flow

## Testing

All new features have comprehensive tests in `packages/compiler/tests/tml/compiler/control_flow.test.tml`:

- `test_while_loop()` - Basic while loop functionality
- `test_while_with_break()` - While with break statement
- `test_ternary_basic()` - Basic ternary usage
- `test_ternary_nested()` - Nested ternary expressions
- `test_ternary_with_expressions()` - Ternary with complex expressions
- `test_while_and_ternary_combined()` - Both features working together

All tests pass successfully (11/13 tests passing overall, 2 unrelated pre-existing failures in collections).

## Foreach sobre Coleções (EXPERIMENTAL)

### Status: ⚠️ Implementado mas com bugs de runtime

Suporte para iteração sobre coleções List, HashMap e Buffer foi parcialmente implementado, mas está bloqueado por bugs pré-existentes nas funções runtime de collections.

### Sintaxe

```tml
for item in collection {
    // body
}
```

### Exemplo (não funcional devido a bugs de runtime)

```tml
let list: List = list_create(5)
list_push(list, 10)
list_push(list, 20)
list_push(list, 30)

let mut sum: I32 = 0
for item in list {
    sum = sum + item
}

list_destroy(list)
```

### O que foi implementado

1. **Codegen**: Geração de código LLVM para iterar sobre collections
   - Chama `list_len()` para obter tamanho
   - Itera de 0 até tamanho-1
   - Chama `list_get(index)` para cada elemento
   - Bind do elemento à variável do loop

2. **Type Checker**: Aceita tipos `List`, `HashMap`, `Buffer` e `Vec` em for loops
   - Valida que o tipo iterado é uma coleção válida
   - Infere tipo do elemento como I32

3. **Parser**: Já existia suporte para `for pattern in expr { body }`

### Problemas conhecidos

- **Runtime crash**: As funções de collection (`list_len`, `list_get`, etc.) estão causando crashes
- **Bug pré-existente**: O teste `collections.test.tml` também falha com o mesmo erro
- **Necessita debugging**: O runtime de collections precisa ser corrigido antes que foreach funcione

### Arquivos modificados

- `src/codegen/llvm_ir_gen_control.cpp` - gen_for() estendido para collections
- `src/types/checker.cpp` - check_for() aceita NamedType (List, HashMap, etc.)
- `tests/tml/compiler/foreach.test.tml` - Testes criados (não passam por bugs de runtime)

### Próximos passos

1. Debugar e corrigir bugs no runtime de collections (`tml_collections.c`)
2. Testar foreach com List
3. Adicionar suporte específico para HashMap (iteração sobre pares key-value)
4. Adicionar suporte para Buffer
5. Considerar adicionar Set quando implementado
