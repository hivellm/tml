/*
 * Determinism-corpus fixture (phase24i gate, tracked since phase25a).
 * Fully variadic macro with __VA_ARGS__ expansion.
 *
 * Run: tml cc compiler-tml/tests/native/repros/macro_r1.c --emit=ast
 */
#define M(...) f(__VA_ARGS__)
int main() { M(1, 2); return 0; }
