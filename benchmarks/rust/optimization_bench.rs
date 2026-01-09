// Optimization Benchmarks - Rust
//
// Equivalent code to optimization_bench.tml for comparison.
// Compile with different optimization levels to compare:
//   rustc optimization_bench.rs -o opt_O0.exe          # Debug (no opts)
//   rustc -O optimization_bench.rs -o opt_O2.exe       # Release (O2)
//   rustc -C opt-level=3 optimization_bench.rs -o opt_O3.exe  # O3
//
// To see LLVM IR:
//   rustc --emit=llvm-ir optimization_bench.rs

#![allow(unused_variables)]
#![allow(dead_code)]

// ============================================================================
// 1. Constant Folding Benchmarks
// ============================================================================

pub fn constant_fold_integers() -> i32 {
    let a: i32 = 10 + 20 + 30 + 40 + 50;
    let b: i32 = 100 - 25 - 25 - 25 - 25;
    let c: i32 = 2 * 3 * 4 * 5;
    let d: i32 = 1000 / 10 / 10;
    let e: i32 = 17 % 5 + 10 % 3;
    a + b + c + d + e
}

pub fn constant_fold_nested() -> i32 {
    let result: i32 = ((1 + 2) * (3 + 4)) + ((5 - 1) * (6 - 2));
    result
}

pub fn constant_fold_floats() -> f64 {
    let a: f64 = 1.5 + 2.5 + 3.0;
    let b: f64 = 10.0 * 0.5 * 2.0;
    let c: f64 = 100.0 / 4.0 / 5.0;
    a + b + c
}

pub fn constant_fold_booleans() -> bool {
    let a: bool = true && true && true;
    let b: bool = false || false || true;
    let c: bool = !false;
    let d: bool = 5 > 3 && 10 == 10 && 7 < 8;
    a && b && c && d
}

pub fn constant_fold_bitwise() -> i32 {
    let a: i32 = 0xFF & 0x0F;          // 15
    let b: i32 = 0xF0 | 0x0F;          // 255
    let c: i32 = 0xFF ^ 0xAA;          // 85
    let d: i32 = 1 << 4;               // 16
    let e: i32 = 256 >> 4;             // 16
    a + b + c + d + e
}

// ============================================================================
// 2. Dead Code Elimination Benchmarks
// ============================================================================

pub fn dce_unused_variables() -> i32 {
    let used: i32 = 42;
    let unused1: i32 = 100 * 200 * 300;
    let unused2: i32 = 400 + 500 + 600;
    let unused3: i32 = unused1 + unused2;
    let unused4: i32 = unused3 * 2;
    let unused5: i32 = unused4 - unused1;
    used
}

pub fn dce_chained_dead_code() -> i32 {
    let live: i32 = 1;
    let dead_a: i32 = 10;
    let dead_b: i32 = dead_a + 20;
    let dead_c: i32 = dead_b * 2;
    let dead_d: i32 = dead_c - dead_a;
    let dead_e: i32 = dead_d + dead_b + dead_c;
    live
}

// ============================================================================
// 3. Constant If Condition
// ============================================================================

pub fn dce_if_true() -> i32 {
    if true {
        1
    } else {
        2
    }
}

pub fn dce_if_false() -> i32 {
    if false {
        1
    } else {
        2
    }
}

pub fn dce_nested_ifs() -> i32 {
    if true {
        if false {
            1
        } else {
            if true {
                3
            } else {
                4
            }
        }
    } else {
        5
    }
}

// ============================================================================
// 4. Short-Circuit Evaluation
// ============================================================================

fn expensive_bool_computation() -> bool {
    let mut sum: i32 = 0;
    for i in 0..1000 {
        sum += i;
    }
    sum > 0
}

pub fn short_circuit_and_false() -> bool {
    false && expensive_bool_computation()
}

pub fn short_circuit_or_true() -> bool {
    true || expensive_bool_computation()
}

// ============================================================================
// 5. Common Subexpression Elimination (CSE)
// ============================================================================

pub fn cse_simple_duplicates(x: i32, y: i32) -> i32 {
    let a: i32 = x + y;
    let b: i32 = x + y;
    let c: i32 = x + y;
    a + b + c
}

pub fn cse_complex(a: i32, b: i32, c: i32) -> i32 {
    let expr1: i32 = a * b + c;
    let expr2: i32 = a * b + c;
    let expr3: i32 = (a * b) + c;
    expr1 + expr2 + expr3
}

#[derive(Clone, Copy)]
struct Point {
    x: i32,
    y: i32,
}

pub fn cse_field_access(p: Point) -> i32 {
    p.x * p.x + p.y * p.y
}

// ============================================================================
// 6. Copy Propagation
// ============================================================================

pub fn copy_propagation_simple(input: i32) -> i32 {
    let a: i32 = input;
    let b: i32 = a;
    let c: i32 = b;
    let d: i32 = c;
    d + d
}

// ============================================================================
// 7. Combined Optimizations
// ============================================================================

pub fn combined_optimizations(x: i32) -> i32 {
    // Constant folding
    let const_expr: i32 = 10 + 20 + 30;

    // Dead code
    let unused: i32 = 999;

    // Constant if
    if true {
        // CSE opportunity
        let a: i32 = x + const_expr;
        let b: i32 = x + const_expr;
        a + b
    } else {
        // Dead branch
        unused
    }
}

pub fn real_world_calculation(width: i32, height: i32) -> i32 {
    let w: i32 = width;
    let h: i32 = height;
    let perimeter: i32 = 2 * (w + h);  // May be unused
    let area: i32 = w * h;
    area
}

// ============================================================================
// 8. Loop Optimizations
// ============================================================================

pub fn loop_invariant(n: i32) -> i32 {
    let constant: i32 = 10 * 20;  // Should be computed once before loop
    let mut sum: i32 = 0;
    for _ in 0..n {
        sum += constant;
    }
    sum
}

pub fn loop_with_dead_code(n: i32) -> i32 {
    let mut sum: i32 = 0;
    for i in 0..n {
        let unused: i32 = i * 1000;  // Dead code in loop body
        sum += i;
    }
    sum
}

// ============================================================================
// 9. Function-level Optimizations (inlining targets)
// ============================================================================

#[inline(never)]
fn add(a: i32, b: i32) -> i32 {
    a + b
}

pub fn inline_candidate(x: i32, y: i32) -> i32 {
    let sum1: i32 = add(x, y);
    let sum2: i32 = add(sum1, x);
    let sum3: i32 = add(sum2, y);
    sum3
}

// ============================================================================
// 10. Struct Optimizations
// ============================================================================

struct Rectangle {
    width: i32,
    height: i32,
}

pub fn struct_optimization() -> i32 {
    let r = Rectangle { width: 10, height: 20 };
    let unused_rect = Rectangle { width: 100, height: 200 };  // Dead
    r.width * r.height
}

// ============================================================================
// Benchmark Runner
// ============================================================================

pub fn run_all_benchmarks() -> i32 {
    let mut total: i32 = 0;

    total += constant_fold_integers();
    total += constant_fold_nested();
    total += if constant_fold_booleans() { 1 } else { 0 };
    total += constant_fold_bitwise();

    total += dce_unused_variables();
    total += dce_chained_dead_code();

    total += dce_if_true();
    total += dce_if_false();
    total += dce_nested_ifs();

    total += if short_circuit_and_false() { 1 } else { 0 };
    total += if short_circuit_or_true() { 1 } else { 0 };

    total += cse_simple_duplicates(5, 10);
    total += cse_complex(2, 3, 4);

    let p = Point { x: 3, y: 4 };
    total += cse_field_access(p);

    total += copy_propagation_simple(7);
    total += combined_optimizations(100);
    total += real_world_calculation(50, 30);

    total += loop_invariant(10);
    total += loop_with_dead_code(10);

    total += inline_candidate(1, 2);
    total += struct_optimization();

    total
}

fn main() {
    println!("Rust Optimization Benchmark");
    println!("============================");
    println!();
    println!("Result: {}", run_all_benchmarks());
    println!();
    println!("To compare optimization levels:");
    println!("  rustc optimization_bench.rs -o opt_debug.exe");
    println!("  rustc -O optimization_bench.rs -o opt_release.exe");
    println!();
    println!("To see LLVM IR:");
    println!("  rustc --emit=llvm-ir -O optimization_bench.rs");
}
