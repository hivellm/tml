/*
 * Determinism-corpus fixture (phase24i gate, tracked since phase25a).
 * Function-like macro, two parameters.
 *
 * Run: tml cc compiler-tml/tests/native/repros/macro_r0a.c --emit=ast
 */
#define M(x, y) f(x, y)
int main() { M(1, 2); return 0; }
