---
name: TML Reserved Keywords
description: Keywords that cannot be used as identifiers in TML — discovered during multi_executor implementation
type: feedback
---

`base` is a reserved keyword in TML (`KwBase` in lexer_core.cpp). Using it as a variable name
produces cascading "Expected pattern" parse errors that are extremely misleading.

**How to apply:** Never use `base` as a variable or field name. Use `slot_addr`, `addr`, `ptr_addr`, etc. instead.

Also confirmed NOT keywords: `fn`, `offset`, `callback`. These are safe to use as identifiers.

To check if a name is a keyword: `grep '"name"' compiler/src/lexer/lexer_core.cpp`
