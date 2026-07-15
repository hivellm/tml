/*
 * Determinism-corpus fixture (phase24i gate, tracked since phase25a).
 * Realistic diagnostic-macro shape (the essential.c log.h pattern that
 * originally exposed the phase24i bug class).
 *
 * Run: tml cc compiler-tml/tests/native/repros/macro_r5.c --emit=ast
 */
#define RT_FATAL(m, f, ...) g(1, m, f, ##__VA_ARGS__)
int main() { RT_FATAL("a", "%s", "b"); return 0; }
