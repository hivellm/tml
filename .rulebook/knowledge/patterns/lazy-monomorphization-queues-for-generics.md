# Lazy Monomorphization Queues for Generics

**Category**: codegen
**Tags**: codegen, generics, monomorphization, performance

## Description

Use pending queues (pending_func_keys_, pending_class_keys_, require_struct_instantiation, require_enum_instantiation) to defer generic type instantiation until actually needed. Avoids O(n²) scanning of all generic types. Each instantiation is triggered by usage, not declaration.

## When to Use

When generating IR for generic types and functions. The queue pattern ensures each monomorphization happens exactly once and only when referenced.
