/*
 * Determinism-corpus fixture (phase24i gate, tracked since phase25a).
 * Function-like macro, single parameter. Part of the 7-repro set whose
 * 10x determinism went 7/70 -> 70/70 when CollectedArgs switched to a
 * flat List[PpToken] + offset table (v0.3.50).
 *
 * Run: tml cc compiler-tml/tests/native/repros/macro_r0.c --emit=ast
 */
#define M(x) f(x)
int main() { M(1); return 0; }
