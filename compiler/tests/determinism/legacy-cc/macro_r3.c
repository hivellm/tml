/*
 * Determinism-corpus fixture (phase24i gate, tracked since phase25a).
 * GCC `, ##__VA_ARGS__` comma-elision with EMPTY variadic list (the
 * comma must be dropped).
 *
 * Run: tml cc compiler-tml/tests/native/repros/macro_r3.c --emit=ast
 */
#define M(x, ...) f(x, ##__VA_ARGS__)
int main() { M(1); return 0; }
