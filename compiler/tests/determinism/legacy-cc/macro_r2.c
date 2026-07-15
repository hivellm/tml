/*
 * Determinism-corpus fixture (phase24i gate, tracked since phase25a).
 * Named parameter + variadic tail.
 *
 * Run: tml cc compiler-tml/tests/native/repros/macro_r2.c --emit=ast
 */
#define M(x, ...) f(x, __VA_ARGS__)
int main() { M(1, 2, 3); return 0; }
