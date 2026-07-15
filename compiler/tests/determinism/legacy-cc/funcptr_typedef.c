/*
 * Determinism-corpus fixture (phase24h gate, tracked since phase25a).
 *
 * Function-pointer typedef plus a static variable that uses the typedef
 * name — exercises typedef registration AND resolution through
 * base_to_ctype (the phase24c/24e HashMap-borrow path) in one file.
 *
 * Run: tml cc compiler-tml/tests/native/repros/funcptr_typedef.c --emit=ast
 */
typedef void (*hook_fn)(const char*);
static hook_fn my_hook = 0;
