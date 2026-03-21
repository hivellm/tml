# Stride-Based Generic Element Storage in Collections

**Category**: stdlib
**Tags**: stdlib, collections, generics, memory

## Description

List[T] uses a 32-byte header + contiguous data buffer with stride-based generic element storage. Elements are accessed via ptr_offset with stride calculated from element size. This avoids the need for C++ template-style monomorphization of the data structure itself — one implementation handles all element types via runtime stride arithmetic.

## When to Use

When implementing generic collections in pure TML. The stride pattern allows a single implementation to handle any element type T without codegen monomorphization of the container structure itself.
