/*
 * Determinism-corpus fixture (phase24h gate, tracked since phase25a).
 *
 * Parenthesized pointer declarator. Same Heap-borrow-drop class as
 * sig_alone.c: `leaf = inner.decl` in cp_parse_direct bitwise-copied a
 * Shared payload and both copies dropped it. Fixed 30/30 by phase24h.
 *
 * Run: tml cc compiler-tml/tests/native/repros/int_p.c --emit=ast
 */
int (*p);
