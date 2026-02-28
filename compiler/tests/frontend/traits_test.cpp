// Tests for trait solving
// Covers: traits/solver.cpp, traits/solver_builtins.cpp

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class TraitSolverTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;

    auto type_checks(const std::string& code) -> bool {
        source_ = std::make_unique<tml::lexer::Source>(tml::lexer::Source::from_string(code));
        tml::lexer::Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        tml::parser::Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        if (tml::is_err(module_result))
            return false;
        auto& module = std::get<tml::parser::Module>(module_result);
        tml::types::TypeChecker checker;
        auto env_result = checker.check_module(module);
        return tml::is_ok(env_result);
    }
};

// ============================================================================
// Basic Behavior Implementation
// ============================================================================

TEST_F(TraitSolverTest, SimpleBehaviorImpl) {
    EXPECT_TRUE(type_checks(R"(
        behavior Greetable {
            func greet(self) -> Str
        }

        struct Person {
            name: Str,
        }

        impl Greetable for Person {
            func greet(self) -> Str {
                return self.name
            }
        }
    )"));
}

TEST_F(TraitSolverTest, MultipleBehaviorImpls) {
    EXPECT_TRUE(type_checks(R"(
        behavior Printable {
            func to_string(self) -> Str
        }

        behavior Comparable {
            func compare(self, other: Self) -> I32
        }

        struct Num {
            val: I32,
        }

        impl Printable for Num {
            func to_string(self) -> Str {
                return "num"
            }
        }

        impl Comparable for Num {
            func compare(self, other: Num) -> I32 {
                return self.val - other.val
            }
        }
    )"));
}

// ============================================================================
// Behavior Constraints
// ============================================================================

TEST_F(TraitSolverTest, GenericWithConstraint) {
    EXPECT_TRUE(type_checks(R"(
        behavior Addable {
            func add(self, other: Self) -> Self
        }

        func double[T: Addable](x: T) -> T {
            return x.add(x)
        }
    )"));
}

TEST_F(TraitSolverTest, MultipleConstraints) {
    EXPECT_TRUE(type_checks(R"(
        behavior Display {
            func display(self) -> Str
        }

        behavior Numeric {
            func zero() -> Self
        }

        func show_zero[T: Display + Numeric]() -> Str {
            let z = T.zero()
            return z.display()
        }
    )"));
}

// ============================================================================
// Builtin Behavior Resolution
// ============================================================================

TEST_F(TraitSolverTest, DerivePartialEqResolution) {
    EXPECT_TRUE(type_checks(R"(
        @derive(PartialEq)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> Bool {
            let a = Point { x: 1, y: 2 }
            let b = Point { x: 1, y: 2 }
            return a == b
        }
    )"));
}

// ============================================================================
// Behavior Default Methods
// ============================================================================

TEST_F(TraitSolverTest, BehaviorWithDefaultMethod) {
    EXPECT_TRUE(type_checks(R"(
        behavior Describable {
            func name(self) -> Str

            func describe(self) -> Str {
                return self.name()
            }
        }

        struct Item {
            label: Str,
        }

        impl Describable for Item {
            func name(self) -> Str {
                return self.label
            }
        }
    )"));
}

// ============================================================================
// Missing Impl Detection
// ============================================================================

TEST_F(TraitSolverTest, MissingMethodDetected) {
    EXPECT_FALSE(type_checks(R"(
        behavior Required {
            func must_implement(self) -> I32
        }

        struct Empty {
            x: I32,
        }

        impl Required for Empty {
            // Missing must_implement!
        }
    )"));
}
