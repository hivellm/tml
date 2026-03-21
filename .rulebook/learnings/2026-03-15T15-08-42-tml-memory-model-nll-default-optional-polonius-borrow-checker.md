# TML memory model: NLL default + optional Polonius borrow checker
**Source**: manual
**Date**: 2026-03-15
**Tags**: language, borrow-checker, memory, ownership
Borrow checker has two modes: NLL (default, borrows end at last use) and Polonius (--polonius flag, Datalog-style constraint solving, strictly more permissive). Copy types implicitly copied (all numerics, Bool, Char, tuples/arrays of Copy). Move types transferred (String, List, Map, user structs). Interior mutability via Cell[T] (copy types only), RefCell[T] (runtime checks), Mutex[T], RwLock[T]. Smart pointers: Heap[T]=Box, Shared[T]=Rc, Sync[T]=Arc, Weak[T]. Lifetime annotations rarely needed — use 'life' keyword when compiler can't infer (multiple ref params, no 'this').