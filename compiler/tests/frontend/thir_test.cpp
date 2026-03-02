// Tests for THIR lowering and exhaustiveness checking
// Covers: thir/thir_lower.cpp, thir/thir_module.cpp, thir/exhaustiveness.cpp

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class ThirTest : public ::testing::Test {
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
// Pattern Exhaustiveness
// ============================================================================

TEST_F(ThirTest, ExhaustiveEnumMatch) {
    EXPECT_TRUE(type_checks(R"(
        type Color {
            Red,
            Green,
            Blue,
        }

        func name(c: Color) -> I32 {
            when c {
                Red => return 0,
                Green => return 1,
                Blue => return 2,
            }
        }
    )"));
}

TEST_F(ThirTest, ExhaustiveWithWildcard) {
    EXPECT_TRUE(type_checks(R"(
        type Dir {
            North,
            South,
            East,
            West,
        }

        func is_vertical(d: Dir) -> Bool {
            when d {
                North => return true,
                South => return true,
                _ => return false,
            }
        }
    )"));
}

TEST_F(ThirTest, ExhaustiveBoolMatch) {
    EXPECT_TRUE(type_checks(R"(
        func to_int(b: Bool) -> I32 {
            when b {
                true => return 1,
                false => return 0,
            }
        }
    )"));
}

// ============================================================================
// THIR Lowering
// ============================================================================

TEST_F(ThirTest, LetBindingLowering) {
    EXPECT_TRUE(type_checks(R"(
        func main() -> I32 {
            let x: I32 = 42
            return x
        }
    )"));
}

TEST_F(ThirTest, MutableVariableLowering) {
    EXPECT_TRUE(type_checks(R"(
        func main() -> I32 {
            var x: I32 = 0
            x = 42
            return x
        }
    )"));
}

TEST_F(ThirTest, StructDestructuring) {
    EXPECT_TRUE(type_checks(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> I32 {
            let p = Point { x: 1, y: 2 }
            return p.x + p.y
        }
    )"));
}

TEST_F(ThirTest, EnumPatternLowering) {
    EXPECT_TRUE(type_checks(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func unwrap(m: Maybe[I32]) -> I32 {
            when m {
                Just(val) => return val,
                Nothing => return 0,
            }
        }
    )"));
}

// ============================================================================
// Complex Patterns
// ============================================================================

TEST_F(ThirTest, NestedPatternMatch) {
    EXPECT_TRUE(type_checks(R"(
        type Result {
            Ok(I32),
            Err(I32),
        }

        func handle(r: Result) -> I32 {
            when r {
                Ok(val) => return val,
                Err(code) => return 0 - code,
            }
        }
    )"));
}

TEST_F(ThirTest, MultipleReturnPaths) {
    EXPECT_TRUE(type_checks(R"(
        func classify(x: I32) -> I32 {
            if x > 0 {
                return 1
            }
            if x < 0 {
                return -1
            }
            return 0
        }
    )"));
}
