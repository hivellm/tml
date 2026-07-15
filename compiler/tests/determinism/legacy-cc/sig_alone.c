/*
 * Determinism-corpus fixture (phase24h/24m/24n gate, tracked since phase25a).
 *
 * Function-pointer typedef alone. Historically SIGSEGV'd in cc_driver
 * cleanup in 50-80% of runs (Heap-borrow-drop double-free on the
 * CDeclarator::Pointer(Shared[CDeclarator]) payload); fixed to 30/30 by
 * phase24h (manual `impl Duplicate for CDeclarator`). Kept in the corpus
 * so the fix cannot silently regress.
 *
 * Run: tml cc compiler-tml/tests/native/repros/sig_alone.c --emit=ast
 */
typedef void (*sig_t)(int);
