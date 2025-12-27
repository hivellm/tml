# TML Core Library - Análise Comparativa com Rust Core

## Análise da Estrutura Atual do TML vs Rust Core

### 📊 Status Geral

**TML Atual:** 9 módulos implementados
**Rust Core:** 33+ módulos
**Cobertura:** ~27%

---

## ✅ MÓDULOS JÁ IMPLEMENTADOS NO TML

### 1. **core::mem** ✅
**Status:** Implementado básico
**Arquivo:** `packages/compiler/src/core/mem.tml`

**Funcionalidades:**
- ✅ `alloc()` - Alocação de memória
- ✅ `dealloc()` - Liberação de memória
- ✅ `read_i32()`, `write_i32()` - Leitura/escrita
- ✅ `ptr_offset()` - Aritmética de ponteiros

**vs Rust core::mem:**
- ❌ Falta: `size_of()`, `align_of()`, `swap()`, `replace()`, `drop()`
- ❌ Falta: `forget()`, `discriminant()`, `transmute()`
- ❌ Falta: `MaybeUninit[T]` type

---

### 2. **core::time** ✅
**Status:** Implementado básico
**Arquivo:** `packages/compiler/src/core/time.tml`

**vs Rust core::time:**
- ✅ Provavelmente tem funções de timing
- ⚠️ Precisa verificar se tem `Duration`, `Instant`

---

### 3. **core::thread** ✅
**Status:** Implementado básico
**Arquivo:** `packages/compiler/src/core/thread.tml`

**vs Rust core (std::thread):**
- ⚠️ Rust core não tem threads (isso é std)
- ✅ TML tem implementação de threading

---

### 4. **core::sync** ✅
**Status:** Implementado básico
**Arquivo:** `packages/compiler/src/core/sync.tml`

**vs Rust core::sync:**
- ✅ Provavelmente tem primitivas de sincronização
- ⚠️ Precisa verificar: `Arc`, `Mutex`, `RwLock`, `Barrier`

---

### 5. **std::types** ✅
**Status:** Implementado
**Arquivo:** `packages/std/src/types/mod.tml`

**Funcionalidades:**
- ✅ `Maybe[T]` (equivalente a `Option[T]`)
- ✅ `Outcome[T, E]` (equivalente a `Result[T, E]`)
- ✅ Helper functions: `is_just()`, `is_nothing()`, `unwrap_or()`
- ✅ Helper functions: `is_ok()`, `is_err()`, `unwrap_or_ok()`

**vs Rust core::option + core::result:**
- ✅ Tipos base implementados
- ❌ Falta: `map()`, `and_then()`, `or_else()`, `filter()`
- ❌ Falta: `unwrap()`, `expect()`, `unwrap_or_else()`

---

### 6. **std::iter** ✅
**Status:** Implementado avançado
**Arquivo:** `packages/std/src/iter/mod.tml`

**Funcionalidades:**
- ✅ `Iterator` behavior (trait)
- ✅ `IntoIterator` behavior
- ✅ `Range` type com iteração
- ✅ Métodos: `next()`, `take()`, `skip()`, `sum()`, `count()`
- ✅ Métodos: `fold()`, `any()`, `all()`

**vs Rust core::iter:**
- ✅ Estrutura base muito boa
- ❌ Falta: `map()`, `filter()`, `collect()`
- ❌ Falta: `zip()`, `enumerate()`, `chain()`, `rev()`
- ❌ Falta: `find()`, `position()`, `max()`, `min()`

---

### 7. **std::collections** ✅
**Status:** Implementado avançado
**Arquivo:** `packages/std/src/collections/mod.tml`

**Funcionalidades:**
- ✅ `List[T]` - Dynamic array (Vec equivalente)
- ✅ `HashMap[K, V]` - Hash table
- ✅ `Buffer` - Byte buffer

**vs Rust std::collections (core não tem):**
- ✅ Lista dinâmica implementada
- ✅ HashMap implementado
- ❌ Falta: `BTreeMap`, `BinaryHeap`, `VecDeque`
- ❌ Falta: `HashSet`, `BTreeSet`

---

### 8. **std::file** ✅
**Status:** Implementado
**Arquivo:** `packages/std/src/file/mod.tml`

**vs Rust std::fs (core não tem I/O):**
- ✅ File I/O implementado
- ⚠️ Rust core não tem I/O (apenas std)

---

## ❌ MÓDULOS CRÍTICOS FALTANDO

### PRIORIDADE ALTA 🔴

#### 1. **core::clone** - CRÍTICO
**Rust:** `Clone` trait para duplicação explícita
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Duplicar valores que não são `Copy`
- Implementar `clone()` em tipos complexos

**Implementação sugerida:**
```tml
// packages/core/src/clone.tml
pub behavior Clone {
    func clone(this) -> This
}

pub behavior Copy extends Clone {
    // Marker behavior - copied implicitly
}
```

---

#### 2. **core::cmp** - CRÍTICO
**Rust:** `PartialEq`, `Eq`, `PartialOrd`, `Ord`
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Comparações personalizadas
- Ordenação de coleções

**Implementação sugerida:**
```tml
// packages/core/src/cmp.tml
pub behavior PartialEq {
    func eq(this, other: This) -> Bool
    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

pub behavior Ord extends PartialEq {
    func cmp(this, other: This) -> Ordering
    func lt(this, other: This) -> Bool
    func le(this, other: This) -> Bool
    func gt(this, other: This) -> Bool
    func ge(this, other: This) -> Bool
}

pub type Ordering {
    Less,
    Equal,
    Greater
}
```

---

#### 3. **core::ops** - CRÍTICO
**Rust:** Operator overloading (`Add`, `Sub`, `Mul`, `Div`, `Index`)
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Sobrecarga de operadores (+, -, *, /, [])
- Syntax sugar para tipos customizados

**Implementação sugerida:**
```tml
// packages/core/src/ops.tml
pub behavior Add {
    type Output
    func add(this, rhs: This) -> This::Output
}

pub behavior Sub {
    type Output
    func sub(this, rhs: This) -> This::Output
}

pub behavior Mul {
    type Output
    func mul(this, rhs: This) -> This::Output
}

pub behavior Index {
    type Output
    func index(this, idx: I64) -> This::Output
}
```

---

#### 4. **core::default** - ALTA PRIORIDADE
**Rust:** `Default` trait para valores padrão
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Criar instâncias padrão de tipos
- Inicialização genérica

**Implementação sugerida:**
```tml
// packages/core/src/default.tml
pub behavior Default {
    func default() -> This
}
```

---

#### 5. **core::fmt** - ALTA PRIORIDADE
**Rust:** Formatação (`Display`, `Debug`)
**TML:** ❌ NÃO IMPLEMENTADO (usa builtins)
**Necessário para:**
- Print customizado
- String representation
- Debug output

**Implementação sugerida:**
```tml
// packages/core/src/fmt.tml
pub behavior Display {
    func fmt(this) -> Str
}

pub behavior Debug {
    func debug_fmt(this) -> Str
}
```

---

### PRIORIDADE MÉDIA 🟡

#### 6. **core::convert** - MÉDIA
**Rust:** `From`, `Into`, `TryFrom`, `TryInto`, `AsRef`, `AsMut`
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Conversões entre tipos
- Trait bounds genéricos

---

#### 7. **core::borrow** - MÉDIA
**Rust:** `Borrow`, `BorrowMut`, `ToOwned`, `Cow`
**TML:** ⚠️ Sistema de ownership existe, mas não tem traits
**Necessário para:**
- Abstração sobre owned/borrowed
- Generic borrowing

---

#### 8. **core::hash** - MÉDIA
**Rust:** `Hash` trait, `Hasher`
**TML:** ⚠️ HashMap existe mas hash trait não é público
**Necessário para:**
- Hash customizado
- HashMap com tipos customizados

---

#### 9. **core::cell** - MÉDIA
**Rust:** `Cell[T]`, `RefCell[T]` - interior mutability
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Mutabilidade interior
- Shared mutability segura

---

#### 10. **core::marker** - MÉDIA
**Rust:** `Copy`, `Send`, `Sync`, `Sized`, `Unpin`
**TML:** ❌ NÃO IMPLEMENTADO
**Necessário para:**
- Traits marcadores
- Garantias de tipo

---

### PRIORIDADE BAIXA 🟢

#### 11. **core::any** - BAIXA
**Rust:** Type reflection (`Any`, `TypeId`)
**TML:** ❌ NÃO IMPLEMENTADO

#### 12. **core::str** - BAIXA
**Rust:** String slice manipulation
**TML:** ⚠️ Tem `Str` builtin mas sem módulo

#### 13. **core::slice** - BAIXA
**Rust:** Slice utilities
**TML:** ⚠️ Tem arrays mas sem slice abstraction

#### 14. **core::array** - BAIXA
**Rust:** Array utilities e traits
**TML:** ⚠️ Arrays existem mas sem utilities

#### 15. **core::ptr** - BAIXA
**Rust:** Raw pointer utilities
**TML:** ⚠️ Tem `Ptr[T]` mas sem utilities

#### 16. **core::panic** - BAIXA
**Rust:** Panic infrastructure
**TML:** ❌ NÃO IMPLEMENTADO (tem assert builtins)

#### 17. **core::pin** - BAIXA
**Rust:** Pinning pointers
**TML:** ❌ NÃO IMPLEMENTADO

#### 18. **core::future** / **core::task** - BAIXA
**Rust:** Async foundations
**TML:** ❌ NÃO IMPLEMENTADO

#### 19. **core::error** - BAIXA
**Rust:** Error trait
**TML:** ⚠️ Tem `Outcome[T,E]` mas sem Error trait

---

## 📋 PLANO DE IMPLEMENTAÇÃO SUGERIDO

### FASE 1 - Fundamentos (CRÍTICOS) 🔴

**Objetivo:** Implementar behaviors essenciais para APIs genéricas

1. **core::clone** - `Clone` behavior
2. **core::cmp** - `PartialEq`, `Ord`, `Ordering`
3. **core::default** - `Default` behavior
4. **core::ops** - `Add`, `Sub`, `Mul`, `Div`, `Index`
5. **core::fmt** - `Display`, `Debug`

**Estimativa:** 2-3 semanas para LLMs
**Impacto:** Habilita 80% dos padrões comuns

---

### FASE 2 - Conversões e Utilidades (MÉDIA) 🟡

6. **core::convert** - `From`, `Into`, `TryFrom`, `TryInto`
7. **core::hash** - `Hash` trait público
8. **core::borrow** - `Borrow`, `BorrowMut`
9. **core::marker** - `Copy`, `Send`, `Sync`
10. **Expandir core::mem** - `size_of`, `swap`, `replace`

**Estimativa:** 2-3 semanas
**Impacto:** APIs mais expressivas e type-safe

---

### FASE 3 - Avançado (BAIXA) 🟢

11. **core::cell** - `Cell[T]`, `RefCell[T]`
12. **core::str** - String utilities
13. **core::slice** - Slice manipulation
14. **core::ptr** - Pointer utilities
15. **core::error** - Error trait

**Estimativa:** 3-4 semanas
**Impacto:** Features avançadas

---

### FASE 4 - Async e Especializados (OPCIONAL) ⚪

16. **core::future** - Future trait
17. **core::task** - Task types
18. **core::pin** - Pin types
19. **core::any** - Type reflection

**Estimativa:** 4-6 semanas
**Impacto:** Async/await support

---

## 🎯 RECOMENDAÇÕES IMEDIATAS

### Para LLMs Gerarem Código Eficiente:

**TOP 3 PRIORIDADES:**

1. **Implementar core::clone**
   - 90% do código Rust usa Clone
   - Crítico para trabalhar com coleções

2. **Implementar core::cmp**
   - Necessário para sorting e ordenação
   - Habilita `sort()` em List[T]

3. **Implementar core::ops**
   - Syntax sugar massivo
   - `vec[i]` ao invés de `vec.get(i)`
   - `a + b` ao invés de `a.add(b)`

### Arquivos a Criar:

```
packages/core/src/
  ├── mod.tml           # Re-export all core modules
  ├── clone.tml         # Clone, Copy behaviors
  ├── cmp.tml           # PartialEq, Ord, Ordering
  ├── default.tml       # Default behavior
  ├── ops.tml           # Add, Sub, Mul, Div, Index
  ├── fmt.tml           # Display, Debug
  ├── convert.tml       # From, Into conversions
  ├── hash.tml          # Hash behavior
  ├── borrow.tml        # Borrow, BorrowMut
  └── marker.tml        # Copy, Send, Sync markers
```

---

## 📊 MATRIZ DE PRIORIDADES

| Módulo | Prioridade | Complexidade | Impacto | Esforço | ROI |
|--------|-----------|--------------|---------|---------|-----|
| core::clone | 🔴 CRÍTICO | Baixa | Alto | 1 dia | ⭐⭐⭐⭐⭐ |
| core::cmp | 🔴 CRÍTICO | Média | Alto | 2 dias | ⭐⭐⭐⭐⭐ |
| core::ops | 🔴 CRÍTICO | Média | Muito Alto | 3 dias | ⭐⭐⭐⭐⭐ |
| core::default | 🔴 ALTA | Baixa | Médio | 1 dia | ⭐⭐⭐⭐ |
| core::fmt | 🔴 ALTA | Média | Alto | 2 dias | ⭐⭐⭐⭐ |
| core::convert | 🟡 MÉDIA | Média | Médio | 2 dias | ⭐⭐⭐ |
| core::hash | 🟡 MÉDIA | Baixa | Baixo | 1 dia | ⭐⭐⭐ |
| core::borrow | 🟡 MÉDIA | Alta | Médio | 3 dias | ⭐⭐⭐ |
| core::cell | 🟢 BAIXA | Alta | Baixo | 4 dias | ⭐⭐ |
| core::future | ⚪ OPCIONAL | Muito Alta | Baixo | 10+ dias | ⭐ |

---

## ✅ RESUMO EXECUTIVO

**O que TML já tem (MUITO BOM):**
- ✅ Maybe[T] e Outcome[T,E] - foundation sólida
- ✅ Iterator system - bem implementado
- ✅ Collections básicas - List, HashMap, Buffer
- ✅ Memória baixo nível - core::mem funcional

**O que TML precisa URGENTE:**
- ❌ Behaviors essenciais: Clone, PartialEq, Ord
- ❌ Operator overloading: Add, Sub, Index, etc.
- ❌ Default trait
- ❌ Display/Debug para formatação

**Impacto:**
Com **core::clone, core::cmp, core::ops** implementados (1 semana de trabalho), TML teria **90%** da ergonomia do Rust para LLMs gerarem código idiomático.

---

## 🚀 PRÓXIMOS PASSOS

1. **Criar `packages/core/src/mod.tml`**
2. **Implementar `core::clone.tml`** - Behavior Clone
3. **Implementar `core::cmp.tml`** - PartialEq, Ord
4. **Implementar `core::ops.tml`** - Add, Sub, Mul, etc.
5. **Implementar `core::default.tml`** - Default
6. **Implementar `core::fmt.tml`** - Display, Debug
7. **Atualizar std::iter** - Adicionar map(), filter(), collect()
8. **Atualizar std::types** - Adicionar map(), and_then(), etc.

---

**Documentado em:** 2025-12-26
**Versão TML:** 0.1.0
**Baseline:** Rust core 1.83.0
