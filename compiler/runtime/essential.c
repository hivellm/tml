// TML Runtime - Essential Functions (IO only)
// Other functions are in separate files: string.c, mem.c, time.c, math.c, etc.

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Panic Catching State (for @should_panic tests)
// Uses a callback approach because setjmp/longjmp requires the setjmp call
// to remain on the stack while the test runs.
// ============================================================================

static jmp_buf tml_panic_jmp_buf;
static int32_t tml_catching_panic = 0;
static char tml_panic_msg[1024] = {0};

// print(message: Str) -> Unit
void print(const char* message) {
    if (message)
        printf("%s", message);
}

// println(message: Str) -> Unit
void println(const char* message) {
    if (message)
        printf("%s\n", message);
    else
        printf("\n");
}

// panic(message: Str) -> Never
void panic(const char* message) {
    // If we're in panic catching mode, save the message and longjmp back
    if (tml_catching_panic) {
        if (message) {
            snprintf(tml_panic_msg, sizeof(tml_panic_msg), "%s", message);
        } else {
            tml_panic_msg[0] = '\0';
        }
        longjmp(tml_panic_jmp_buf, 1);
    }

    // Normal panic behavior - print message and exit
    fprintf(stderr, "panic: %s\n", message ? message : "(null)");
    exit(1);
}

// assert(condition: Bool, message: Str) -> Unit
void assert_tml(int32_t condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message ? message : "(no message)");
        exit(1);
    }
}

// Type-specific print variants (for polymorphic print)
void print_i32(int32_t n) {
    printf("%d", n);
}
void print_i64(int64_t n) {
    printf("%lld", (long long)n);
}
void print_f32(float n) {
    printf("%g", n);
}
void print_f64(double n) {
    printf("%g", n);
}
void print_bool(int32_t b) {
    printf("%s", b ? "true" : "false");
}
void print_char(int32_t c) {
    printf("%c", (char)c);
}

// ============================================================================
// Panic Catching Functions (for @should_panic tests)
// Uses a callback approach: LLVM IR passes a function pointer to tml_run_should_panic()
// which keeps setjmp on the stack while the test runs.
// ============================================================================

// Callback type for test functions (void -> void)
typedef void (*tml_test_fn)(void);

// Run a test function that should panic
// Returns: 1 if the test panicked (success), 0 if it didn't panic (failure)
int32_t tml_run_should_panic(tml_test_fn test_fn) {
    tml_panic_msg[0] = '\0';
    tml_catching_panic = 1;

    if (setjmp(tml_panic_jmp_buf) == 0) {
        // First time through - run the test
        test_fn();
        // If we get here, test didn't panic
        tml_catching_panic = 0;
        return 0; // Failure - expected panic but didn't get one
    } else {
        // Got here via longjmp - panic was caught
        tml_catching_panic = 0;
        return 1; // Success - panic was caught
    }
}

// Get the last panic message (valid after tml_run_should_panic returned 1)
const char* tml_get_panic_message(void) {
    return tml_panic_msg;
}

// Check if the panic message contains expected substring
int32_t tml_panic_message_contains(const char* expected) {
    if (!expected || !expected[0]) {
        // No expected message specified - any panic is fine
        return 1;
    }
    return strstr(tml_panic_msg, expected) != NULL;
}