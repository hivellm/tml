# Proposal: phase0w_integer-literal-u32-u64-inference

## Why

Literais inteiros defaultam para I64. Quando usados em construção de struct
com campos `U32`/`U64`/`I32`/`U8`, forçam cast explícito em toda inicialização:

```tml
// Atual (verbose)
TableKey { table_id: 1 as U32, pk: 100 as U64 }

// Esperado (como Rust)
TableKey { table_id: 1, pk: 100 }
```

Problemas:
- Verboso em código que usa tipos não-padrão (DB keys, protocol fields, etc.)
- UzDB reportou isso como fricção constante
- Rust, Swift, C#, Zig — todos inferem literais do contexto do campo

Source: UzDB feedback letter, P3-10.

## What Changes

1. **Type inference propaga tipo esperado para literais** em struct construction:
   - Campo tem tipo `U32` → literal vira `U32`
   - Campo tem tipo `U64` → literal vira `U64`
   - Campo tem tipo `U8` → literal vira `U8` (com check de range)
   - Validação: literal fora de range do tipo → erro de compilação

2. **Também aplica em:**
   - Argumentos de função (`fn f(x: U32)` — `f(1)` infere U32)
   - Return positions (`-> U32 { 1 }` — 1 vira U32)
   - Binary ops (`x: U32 = 1 + 2` — ambos inferem U32)
   - Tuple fields com tipo conhecido
   - Array elements com tipo declarado

3. **Fallback para I64** quando não há contexto (inalterado)

4. **Type checker location:**
   - `compiler/src/types/infer.cpp`
   - `compiler/src/types/checker/check_expr.cpp`
   - Adicionar `expected_literal_type_` hint que flui do pai

5. **Testes:**
   - Struct literal com campos U8/U16/U32/U64/I8/I16/I32
   - Range check: `U8 { x: 300 }` deve falhar (overflow)
   - Tuple com tipo anotado
   - Function call args
   - Return inference

## Impact

- Affected specs: language/type-inference
- Affected code: `compiler/src/types/infer.cpp`, `compiler/src/types/checker/check_expr.cpp`, `compiler/src/codegen/llvm/expr/literal.cpp` (possível)
- Breaking change: NO (código existente com casts continua válido)
- User benefit: Remove verbosidade em código com tipos numéricos não-padrão. Melhor ergonomia. P3.
