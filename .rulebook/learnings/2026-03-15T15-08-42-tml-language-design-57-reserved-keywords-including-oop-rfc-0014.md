# TML language design: 57 reserved keywords including OOP (RFC-0014)
**Source**: manual
**Date**: 2026-03-15
**Tags**: language, syntax, design, keywords
TML has 57 reserved keywords including C#-style OOP support (class, interface, extends, implements, override, virtual, abstract, sealed, namespace, base, protected, private, static, new, prop). This coexists with the primary struct+behavior model. Key syntax choices: 'do(x) expr' for closures, '!' for error propagation (not '?'), 'when' for pattern matching (not 'match'), 'to'/'through' for ranges (exclusive/inclusive), mandatory type annotations on all declarations, LL(1) grammar with single-token lookahead. Operator precedence: 'or' lowest, member access/calls highest. No macros — decorators replace them with quote/splice syntax.