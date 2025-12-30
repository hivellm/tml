// MIR Optimization Benchmarks
//
// Tests to measure the effectiveness of MIR optimization passes.
// Includes:
// - Instruction count reduction
// - Block count reduction
// - Pass execution time
// - Language comparison (TML vs Rust vs C++ vs Go patterns)

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/common_subexpression_elimination.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/constant_propagation.hpp"
#include "mir/passes/copy_propagation.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/unreachable_code_elimination.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

namespace {

// Benchmark statistics
struct OptStats {
    size_t instructions_before = 0;
    size_t instructions_after = 0;
    size_t blocks_before = 0;
    size_t blocks_after = 0;
    double time_ms = 0.0;
    int passes_applied = 0;

    double instruction_reduction_percent() const {
        if (instructions_before == 0)
            return 0.0;
        return 100.0 * (1.0 - static_cast<double>(instructions_after) /
                                  static_cast<double>(instructions_before));
    }

    double block_reduction_percent() const {
        if (blocks_before == 0)
            return 0.0;
        return 100.0 *
               (1.0 - static_cast<double>(blocks_after) / static_cast<double>(blocks_before));
    }
};

// Count total instructions in a module
size_t count_instructions(const tml::mir::Module& module) {
    size_t count = 0;
    for (const auto& func : module.functions) {
        for (const auto& block : func.blocks) {
            count += block.instructions.size();
        }
    }
    return count;
}

// Count total blocks in a module
size_t count_blocks(const tml::mir::Module& module) {
    size_t count = 0;
    for (const auto& func : module.functions) {
        count += func.blocks.size();
    }
    return count;
}

class MirOptimizationBench : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;

    auto build_mir(const std::string& code) -> tml::mir::Module {
        source_ = std::make_unique<tml::lexer::Source>(tml::lexer::Source::from_string(code));
        tml::lexer::Lexer lexer(*source_);
        auto tokens = lexer.tokenize();

        tml::parser::Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("bench");
        EXPECT_TRUE(tml::is_ok(module_result));
        auto& module = std::get<tml::parser::Module>(module_result);

        tml::types::TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(tml::is_ok(env_result));
        auto& env = std::get<tml::types::TypeEnv>(env_result);

        tml::mir::MirBuilder builder(env);
        return builder.build(module);
    }

    auto run_optimization(tml::mir::Module& module, tml::mir::OptLevel level) -> OptStats {
        OptStats stats;
        stats.instructions_before = count_instructions(module);
        stats.blocks_before = count_blocks(module);

        auto start = std::chrono::high_resolution_clock::now();

        tml::mir::PassManager pm(level);
        pm.configure_standard_pipeline();
        stats.passes_applied = pm.run(module);

        auto end = std::chrono::high_resolution_clock::now();
        stats.time_ms =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        stats.instructions_after = count_instructions(module);
        stats.blocks_after = count_blocks(module);

        return stats;
    }

    void print_stats(const std::string& name, const OptStats& stats) {
        std::cout << "\n=== " << name << " ===\n";
        std::cout << "  Instructions: " << stats.instructions_before << " -> "
                  << stats.instructions_after << " (" << std::fixed << std::setprecision(1)
                  << stats.instruction_reduction_percent() << "% reduction)\n";
        std::cout << "  Blocks: " << stats.blocks_before << " -> " << stats.blocks_after << " ("
                  << stats.block_reduction_percent() << "% reduction)\n";
        std::cout << "  Passes applied: " << stats.passes_applied << "\n";
        std::cout << "  Time: " << std::setprecision(3) << stats.time_ms << " ms\n";
    }

    void print_comparison_header() {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "  LANGUAGE PATTERN COMPARISON: TML vs Rust vs C++ vs Go\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "\nThis benchmark compares equivalent code patterns across languages.\n";
        std::cout << "TML code is compiled and optimized; other languages shown for reference.\n\n";
    }

    void print_language_comparison(const std::string& pattern_name, const std::string& tml_code,
                                   const std::string& rust_code, const std::string& cpp_code,
                                   const std::string& go_code, const OptStats& stats) {
        std::cout << "\n" << std::string(60, '-') << "\n";
        std::cout << "Pattern: " << pattern_name << "\n";
        std::cout << std::string(60, '-') << "\n";

        std::cout << "\n[TML] (Optimized)\n";
        std::cout << tml_code << "\n";
        std::cout << "  -> " << stats.instructions_before << " -> " << stats.instructions_after
                  << " instructions (" << std::fixed << std::setprecision(1)
                  << stats.instruction_reduction_percent() << "% reduction)\n";

        std::cout << "\n[Rust] (Reference)\n";
        std::cout << rust_code << "\n";

        std::cout << "\n[C++] (Reference)\n";
        std::cout << cpp_code << "\n";

        std::cout << "\n[Go] (Reference)\n";
        std::cout << go_code << "\n";
    }
};

// ============================================================================
// Constant Folding Benchmarks
// ============================================================================

TEST_F(MirOptimizationBench, ConstantFoldingArithmetic) {
    auto mir = build_mir(R"(
        func compute() -> I32 {
            let a: I32 = 10 + 20
            let b: I32 = 100 - 50
            let c: I32 = 6 * 7
            let d: I32 = 100 / 4
            let e: I32 = 17 % 5
            return a + b + c + d + e
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Constant Folding - Arithmetic", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, ConstantFoldingChained) {
    auto mir = build_mir(R"(
        func deep_fold() -> I32 {
            let a: I32 = ((1 + 2) * 3 + 4) * 5
            let b: I32 = (10 - 5) * (20 - 10)
            let c: I32 = a + b
            return c
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Constant Folding - Chained", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, ConstantFoldingBitwise) {
    // Note: 'and'/'or' are logical operators (bool), use '&'/'|' for bitwise
    auto mir = build_mir(R"(
        func bitwise_fold() -> I32 {
            let a: I32 = 0xFF & 0x0F
            let b: I32 = 0xF0 | 0x0F
            let c: I32 = 0xFF xor 0xAA
            let d: I32 = 1 shl 4
            let e: I32 = 256 shr 4
            return a + b + c + d + e
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Constant Folding - Bitwise", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 0.0);
}

// ============================================================================
// Dead Code Elimination Benchmarks
// ============================================================================

TEST_F(MirOptimizationBench, DeadCodeUnusedVariables) {
    auto mir = build_mir(R"(
        func with_dead_code() -> I32 {
            let used: I32 = 42
            let unused1: I32 = 100
            let unused2: I32 = 200
            let unused3: I32 = 300
            let unused4: I32 = 400
            let unused5: I32 = 500
            return used
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("DCE - Unused Variables", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 30.0);
}

TEST_F(MirOptimizationBench, DeadCodeComplexExpressions) {
    auto mir = build_mir(R"(
        func complex_dead_code() -> I32 {
            let result: I32 = 42
            let dead1: I32 = 1 + 2 + 3 + 4 + 5
            let dead2: I32 = dead1 * 2
            let dead3: I32 = dead2 + dead1
            return result
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("DCE - Complex Dead Expressions", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 40.0);
}

TEST_F(MirOptimizationBench, DeadCodeChainedDependencies) {
    auto mir = build_mir(R"(
        func chained_dead() -> I32 {
            let live: I32 = 1
            let dead_a: I32 = 10
            let dead_b: I32 = dead_a + 20
            let dead_c: I32 = dead_b * 2
            let dead_d: I32 = dead_c - dead_a
            let dead_e: I32 = dead_d + dead_b + dead_c
            return live
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("DCE - Chained Dead Dependencies", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 70.0);
}

// ============================================================================
// Common Subexpression Elimination Benchmarks
// ============================================================================

TEST_F(MirOptimizationBench, CSESimpleDuplicates) {
    auto mir = build_mir(R"(
        func cse_test(x: I32, y: I32) -> I32 {
            let a: I32 = x + y
            let b: I32 = x + y
            let c: I32 = x + y
            return a + b + c
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("CSE - Simple Duplicates", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, CSEComplexExpressions) {
    auto mir = build_mir(R"(
        func cse_complex(a: I32, b: I32, c: I32) -> I32 {
            let expr1: I32 = a * b + c
            let expr2: I32 = a * b + c
            let expr3: I32 = (a * b) + c
            return expr1 + expr2 + expr3
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("CSE - Complex Expressions", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

// ============================================================================
// Combined Optimization Benchmarks
// ============================================================================

TEST_F(MirOptimizationBench, CombinedOptimizations) {
    auto mir = build_mir(R"(
        func combined_test(x: I32) -> I32 {
            let const_expr: I32 = 10 + 20 + 30
            let unused: I32 = 999
            let result: I32 = x + const_expr
            let also_unused: I32 = unused + 1
            return result
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Combined Optimizations", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 20.0);
}

TEST_F(MirOptimizationBench, RealWorldPattern) {
    auto mir = build_mir(R"(
        func calculate_area(width: I32, height: I32) -> I32 {
            let w: I32 = width
            let h: I32 = height
            let perimeter: I32 = 2 * (w + h)
            let area: I32 = w * h
            return area
        }

        func main() -> I32 {
            let result: I32 = calculate_area(10, 20)
            return result
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Real-World Pattern", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

// ============================================================================
// Language Comparison Benchmarks: TML vs Rust vs C++ vs Go
// ============================================================================

TEST_F(MirOptimizationBench, LangCompare_ConstantFolding) {
    print_comparison_header();

    const std::string tml_code = R"(
func sum_constants() -> I32 {
    let a: I32 = 10 + 20 + 30
    let b: I32 = 5 * 8
    return a + b
})";

    const std::string rust_code = R"(
fn sum_constants() -> i32 {
    let a: i32 = 10 + 20 + 30;
    let b: i32 = 5 * 8;
    a + b
})";

    const std::string cpp_code = R"(
int sum_constants() {
    int a = 10 + 20 + 30;
    int b = 5 * 8;
    return a + b;
})";

    const std::string go_code = R"(
func sumConstants() int32 {
    a := int32(10 + 20 + 30)
    b := int32(5 * 8)
    return a + b
})";

    auto mir = build_mir(R"(
        func sum_constants() -> I32 {
            let a: I32 = 10 + 20 + 30
            let b: I32 = 5 * 8
            return a + b
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Constant Folding", tml_code, rust_code, cpp_code, go_code, stats);

    // All languages should optimize this to a single constant
    std::cout << "\n  Expected: All compilers fold to single constant (100)\n";
    EXPECT_GT(stats.instruction_reduction_percent(), 50.0);
}

TEST_F(MirOptimizationBench, LangCompare_DeadCodeElimination) {
    const std::string tml_code = R"(
func dead_code_test(x: I32) -> I32 {
    let unused: I32 = 42 * 100
    let also_unused: I32 = unused + 1
    return x
})";

    const std::string rust_code = R"(
fn dead_code_test(x: i32) -> i32 {
    let unused: i32 = 42 * 100;
    let also_unused: i32 = unused + 1;
    x  // Rust warns about unused variables
})";

    const std::string cpp_code = R"(
int dead_code_test(int x) {
    int unused = 42 * 100;
    int also_unused = unused + 1;
    return x;  // C++ may warn with -Wunused
})";

    const std::string go_code = R"(
func deadCodeTest(x int32) int32 {
    // Go REQUIRES all variables to be used!
    // This code would not compile in Go.
    // unused := int32(42 * 100)
    return x
})";

    auto mir = build_mir(R"(
        func dead_code_test(x: I32) -> I32 {
            let unused: I32 = 42 * 100
            let also_unused: I32 = unused + 1
            return x
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Dead Code Elimination", tml_code, rust_code, cpp_code, go_code,
                              stats);

    std::cout << "\n  Note: Go enforces no unused variables at compile time!\n";
    EXPECT_GT(stats.instruction_reduction_percent(), 50.0);
}

TEST_F(MirOptimizationBench, LangCompare_LoopInvariant) {
    const std::string tml_code = R"(
func loop_invariant(n: I32) -> I32 {
    let constant: I32 = 10 * 20
    let mut sum: I32 = 0
    let mut i: I32 = 0
    loop {
        if i >= n { break }
        sum = sum + constant
        i = i + 1
    }
    return sum
})";

    const std::string rust_code = R"(
fn loop_invariant(n: i32) -> i32 {
    let constant = 10 * 20;  // Hoisted
    let mut sum = 0;
    for _ in 0..n {
        sum += constant;
    }
    sum
})";

    const std::string cpp_code = R"(
int loop_invariant(int n) {
    const int constant = 10 * 20;  // Hoisted
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += constant;
    }
    return sum;
})";

    const std::string go_code = R"(
func loopInvariant(n int32) int32 {
    constant := int32(10 * 20)  // Hoisted by compiler
    var sum int32 = 0
    for i := int32(0); i < n; i++ {
        sum += constant
    }
    return sum
})";

    auto mir = build_mir(R"(
        func loop_invariant(n: I32) -> I32 {
            let constant: I32 = 10 * 20
            let mut sum: I32 = 0
            let mut i: I32 = 0
            loop {
                if i >= n { break }
                sum = sum + constant
                i = i + 1
            }
            return sum
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Loop Invariant Code Motion", tml_code, rust_code, cpp_code, go_code,
                              stats);

    std::cout << "\n  Note: constant = 200 should be computed once before loop\n";
    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, LangCompare_StructFieldAccess) {
    // TML uses 'type' keyword for structs (not 'struct')
    const std::string tml_code = R"(
type Point { x: I32, y: I32 }

func distance_squared(p: Point) -> I32 {
    return p.x * p.x + p.y * p.y
})";

    const std::string rust_code = R"(
struct Point { x: i32, y: i32 }

fn distance_squared(p: Point) -> i32 {
    p.x * p.x + p.y * p.y
})";

    const std::string cpp_code = R"(
struct Point { int x, y; };

int distance_squared(Point p) {
    return p.x * p.x + p.y * p.y;
})";

    const std::string go_code = R"(
type Point struct { X, Y int32 }

func distanceSquared(p Point) int32 {
    return p.X * p.X + p.Y * p.Y
})";

    auto mir = build_mir(R"(
        type Point {
            x: I32,
            y: I32,
        }

        func distance_squared(p: Point) -> I32 {
            return p.x * p.x + p.y * p.y
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Struct Field Access", tml_code, rust_code, cpp_code, go_code, stats);

    std::cout << "\n  CSE opportunity: p.x and p.y accessed twice\n";
    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, LangCompare_Fibonacci) {
    const std::string tml_code = R"(
func fib(n: I32) -> I32 {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
})";

    const std::string rust_code = R"(
fn fib(n: i32) -> i32 {
    if n <= 1 { return n; }
    fib(n - 1) + fib(n - 2)
})";

    const std::string cpp_code = R"(
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
})";

    const std::string go_code = R"(
func fib(n int32) int32 {
    if n <= 1 { return n }
    return fib(n-1) + fib(n-2)
})";

    auto mir = build_mir(R"(
        func fib(n: I32) -> I32 {
            if n <= 1 { return n }
            return fib(n - 1) + fib(n - 2)
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Recursive Fibonacci", tml_code, rust_code, cpp_code, go_code, stats);

    std::cout << "\n  Note: Tail-call optimization not yet implemented\n";
    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, LangCompare_ArraySum) {
    const std::string tml_code = R"(
func sum_array(arr: [I32; 5]) -> I32 {
    let mut sum: I32 = 0
    let mut i: I32 = 0
    loop {
        if i >= 5 { break }
        sum = sum + arr[i]
        i = i + 1
    }
    return sum
})";

    const std::string rust_code = R"(
fn sum_array(arr: [i32; 5]) -> i32 {
    arr.iter().sum()
    // Or: arr.iter().fold(0, |acc, x| acc + x)
})";

    const std::string cpp_code = R"(
int sum_array(int arr[5]) {
    int sum = 0;
    for (int i = 0; i < 5; ++i) {
        sum += arr[i];
    }
    return sum;
    // Or: std::accumulate(arr, arr+5, 0)
})";

    const std::string go_code = R"(
func sumArray(arr [5]int32) int32 {
    var sum int32 = 0
    for _, v := range arr {
        sum += v
    }
    return sum
})";

    auto mir = build_mir(R"(
        func sum_five() -> I32 {
            let mut sum: I32 = 0
            let mut i: I32 = 0
            loop {
                if i >= 5 { break }
                sum = sum + i
                i = i + 1
            }
            return sum
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Array/Loop Sum", tml_code, rust_code, cpp_code, go_code, stats);

    std::cout << "\n  Rust: Uses iterator + SIMD when possible\n";
    std::cout << "  Go: Range-based loops are idiomatic\n";
    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, LangCompare_ErrorHandling) {
    const std::string tml_code = R"(
func divide(a: I32, b: I32) -> Outcome[I32, Str] {
    if b == 0 {
        return Err("division by zero")
    }
    return Ok(a / b)
})";

    const std::string rust_code = R"(
fn divide(a: i32, b: i32) -> Result<i32, &'static str> {
    if b == 0 {
        return Err("division by zero");
    }
    Ok(a / b)
})";

    const std::string cpp_code = R"(
// C++23 std::expected or custom Result type
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return std::unexpected("division by zero");
    }
    return a / b;
})";

    const std::string go_code = R"(
func divide(a, b int32) (int32, error) {
    if b == 0 {
        return 0, errors.New("division by zero")
    }
    return a / b, nil
})";

    auto mir = build_mir(R"(
        func safe_divide(a: I32, b: I32) -> I32 {
            if b == 0 {
                return 0
            }
            return a / b
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_language_comparison("Error Handling Pattern", tml_code, rust_code, cpp_code, go_code,
                              stats);

    std::cout << "\n  TML/Rust: Zero-cost Result/Outcome types\n";
    std::cout << "  Go: Multiple return values for errors\n";
    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

// ============================================================================
// Optimization Level Comparison
// ============================================================================

TEST_F(MirOptimizationBench, CompareOptLevels) {
    const std::string code = R"(
        func optimize_me(a: I32, b: I32) -> I32 {
            let c1: I32 = 5 + 10
            let c2: I32 = 20 - 5
            let unused: I32 = 999
            let result: I32 = a + b + c1 + c2
            let also_unused: I32 = result + unused
            return result
        }
    )";

    std::cout << "\n=== Optimization Level Comparison ===\n";

    // O0 - No optimization
    {
        auto mir = build_mir(code);
        auto stats = run_optimization(mir, tml::mir::OptLevel::O0);
        std::cout << "O0: " << stats.instructions_before << " instructions (baseline)\n";
        EXPECT_EQ(stats.passes_applied, 0);
    }

    // O1 - Basic optimizations
    {
        auto mir = build_mir(code);
        auto stats = run_optimization(mir, tml::mir::OptLevel::O1);
        std::cout << "O1: " << stats.instructions_after << " instructions (" << std::fixed
                  << std::setprecision(1) << stats.instruction_reduction_percent()
                  << "% reduction)\n";
    }

    // O2 - Standard optimizations
    {
        auto mir = build_mir(code);
        auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
        std::cout << "O2: " << stats.instructions_after << " instructions ("
                  << stats.instruction_reduction_percent() << "% reduction)\n";
    }

    // O3 - Aggressive optimizations
    {
        auto mir = build_mir(code);
        auto stats = run_optimization(mir, tml::mir::OptLevel::O3);
        std::cout << "O3: " << stats.instructions_after << " instructions ("
                  << stats.instruction_reduction_percent() << "% reduction)\n";
    }
}

// ============================================================================
// Scalability Benchmarks
// ============================================================================

TEST_F(MirOptimizationBench, ScalabilityManyVariables) {
    std::ostringstream code;
    code << "func many_vars() -> I32 {\n";

    for (int i = 0; i < 50; ++i) {
        code << "    let unused" << i << ": I32 = " << i << "\n";
    }

    code << "    let result: I32 = 42\n";
    code << "    return result\n";
    code << "}\n";

    auto mir = build_mir(code.str());
    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Scalability - 50 Unused Variables", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 80.0);
}

TEST_F(MirOptimizationBench, ScalabilityDeepNesting) {
    auto mir = build_mir(R"(
        func nested() -> I32 {
            let a: I32 = 1 + 2
            let b: I32 = a + 3
            let c: I32 = b + 4
            let d: I32 = c + 5
            let e: I32 = d + 6
            let f: I32 = e + 7
            let g: I32 = f + 8
            let h: I32 = g + 9
            let i: I32 = h + 10
            let j: I32 = i + 11
            return j
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Scalability - Deep Nesting", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, ScalabilityManyFunctions) {
    std::ostringstream code;

    for (int i = 0; i < 20; ++i) {
        code << "func fn" << i << "() -> I32 {\n";
        code << "    let unused: I32 = " << (i * 10) << "\n";
        code << "    return " << i << "\n";
        code << "}\n\n";
    }

    auto mir = build_mir(code.str());
    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Scalability - 20 Functions", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 30.0);
}

TEST_F(MirOptimizationBench, ScalabilityLargeFunction) {
    std::ostringstream code;
    code << "func large_function() -> I32 {\n";

    // Create a function with many computations
    for (int i = 0; i < 30; ++i) {
        code << "    let v" << i << ": I32 = " << i << " + " << (i + 1) << "\n";
    }

    // Only use a few of them
    code << "    return v0 + v10 + v20\n";
    code << "}\n";

    auto mir = build_mir(code.str());
    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Scalability - Large Function (30 vars, 3 used)", stats);

    EXPECT_GT(stats.instruction_reduction_percent(), 70.0);
}

// ============================================================================
// Algorithm Pattern Benchmarks
// ============================================================================

TEST_F(MirOptimizationBench, AlgorithmBubbleSort) {
    auto mir = build_mir(R"(
        func bubble_pass(a: I32, b: I32) -> I32 {
            if a > b {
                return b
            }
            return a
        }

        func sort_two(x: I32, y: I32) -> I32 {
            let min: I32 = bubble_pass(x, y)
            let max: I32 = bubble_pass(y, x)
            let unused: I32 = min + max
            return min
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Algorithm - Bubble Sort Pattern", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, AlgorithmFactorial) {
    auto mir = build_mir(R"(
        func factorial(n: I32) -> I32 {
            if n <= 1 {
                return 1
            }
            return n * factorial(n - 1)
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Algorithm - Factorial", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, AlgorithmGCD) {
    auto mir = build_mir(R"(
        func gcd(a: I32, b: I32) -> I32 {
            if b == 0 {
                return a
            }
            return gcd(b, a % b)
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Algorithm - GCD (Euclidean)", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

TEST_F(MirOptimizationBench, AlgorithmPower) {
    auto mir = build_mir(R"(
        func power(base: I32, exp: I32) -> I32 {
            if exp == 0 {
                return 1
            }
            if exp == 1 {
                return base
            }
            let half: I32 = power(base, exp / 2)
            if exp % 2 == 0 {
                return half * half
            }
            return base * half * half
        }
    )");

    auto stats = run_optimization(mir, tml::mir::OptLevel::O2);
    print_stats("Algorithm - Fast Power", stats);

    EXPECT_GE(stats.instruction_reduction_percent(), 0.0);
}

// ============================================================================
// Summary Report
// ============================================================================

TEST_F(MirOptimizationBench, SummaryReport) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "    MIR Optimization Effectiveness     \n";
    std::cout << "========================================\n";
    std::cout << "\nOptimization passes available:\n";
    std::cout << "  - Constant Folding\n";
    std::cout << "  - Constant Propagation\n";
    std::cout << "  - Common Subexpression Elimination\n";
    std::cout << "  - Copy Propagation\n";
    std::cout << "  - Dead Code Elimination\n";
    std::cout << "  - Unreachable Code Elimination\n";
    std::cout << "\nOptimization levels:\n";
    std::cout << "  O0: No optimization\n";
    std::cout << "  O1: Constant folding + propagation\n";
    std::cout << "  O2: O1 + CSE, copy prop, DCE, UCE\n";
    std::cout << "  O3: O2 + second optimization round\n";
    std::cout << "\nLanguage comparison notes:\n";
    std::cout << "  TML: Rust-inspired with cleaner syntax\n";
    std::cout << "  Rust: Zero-cost abstractions, borrow checker\n";
    std::cout << "  C++: Maximum control, complex optimizations\n";
    std::cout << "  Go: Simplicity, fast compilation, GC\n";
    std::cout << "========================================\n\n";

    SUCCEED();
}

} // namespace
