/*
 * phase24k minimal reproducer for the residual essential.c × 5 = 0/5 SIGSEGV.
 *
 * Trigger pattern (3 lines, ~85% crash rate at 20 runs):
 *
 *   typedef void (*sig_t)(int);          // function-pointer typedef in main TU
 *   sig_t f(int sig, sig_t handler);     // function decl whose return type AND
 *                                        //   one parameter type both reference
 *                                        //   the typedef-name above
 *   int main(void) { return 0; }
 *
 * Bisection: this trigger lives at lines 41-42 of essential.c, where
 *   #include <signal.h>
 * brings the typedef+decl pair into the main TU through the preprocessor.
 *
 * What does NOT trigger:
 *   - typedef alone (v28: 0/20)
 *   - same typedef+decl behind an #include (v22: 1/20 — borderline noise)
 *   - typedef without a function decl that uses it (v28)
 *   - func decl that names a primitive return type and primitive params
 *
 * What DOES trigger (all 17/20+ at 20 runs):
 *   - typedef return type + typedef parameter (v18, v23, v24)
 *   - return type only (v25, v27, fewer crashes ~12-16/20)
 *   - parameter only (v26)
 *
 * This is the same Heap-borrow-drop class fixed in phase24c/24d/24e but for
 * the typedef name resolution path that was rewired in phase24b. Phase24b
 * fixed lower.tml call sites to take CTypeEnv by ref; this reproducer shows
 * a remaining aliasing path inside the parser/AST that the env-by-ref change
 * does not cover when the typedef name appears INLINED in a top-level
 * declarator.
 */

typedef void (*sig_t)(int);
sig_t f(int sig, sig_t handler);
int main(void) { return 0; }
