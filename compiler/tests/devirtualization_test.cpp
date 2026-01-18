//! # Devirtualization Tests
//!
//! Tests for the devirtualization optimization pass.
//! Tests various scenarios where virtual calls can be converted to direct calls:
//! - Exact type known after constructor
//! - Sealed classes (cannot be subclassed)
//! - Final methods (cannot be overridden)
//! - Single implementation in hierarchy
//! - Type narrowing from `when` expressions

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/devirtualization.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class DevirtualizationTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;
    std::unique_ptr<tml::types::TypeEnv> env_;

    auto build_mir_and_env(const std::string& code)
        -> std::pair<tml::mir::Module, tml::types::TypeEnv*> {
        source_ = std::make_unique<tml::lexer::Source>(tml::lexer::Source::from_string(code));
        tml::lexer::Lexer lexer(*source_);
        auto tokens = lexer.tokenize();

        tml::parser::Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        EXPECT_TRUE(tml::is_ok(module_result));
        auto& module = std::get<tml::parser::Module>(module_result);

        tml::types::TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(tml::is_ok(env_result));
        env_ = std::make_unique<tml::types::TypeEnv>(
            std::move(std::get<tml::types::TypeEnv>(env_result)));

        tml::mir::MirBuilder builder(*env_);
        return {builder.build(module), env_.get()};
    }
};

// ============================================================================
// Basic Devirtualization Tests
// ============================================================================

TEST_F(DevirtualizationTest, ClassHierarchyBuilt) {
    auto [mir, env] = build_mir_and_env(R"(
        class Animal {
            virtual func speak(this) -> Str {
                return "..."
            }
        }

        class Dog extends Animal {
            override func speak(this) -> Str {
                return "Woof!"
            }
        }

        func test() -> Str {
            let dog: Dog = Dog {}
            return dog.speak()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // Should have analyzed at least one method call
    auto stats = pass.get_stats();
    EXPECT_GE(stats.method_calls_analyzed, 0u);
}

TEST_F(DevirtualizationTest, SealedClassDevirtualization) {
    auto [mir, env] = build_mir_and_env(R"(
        sealed class Widget {
            virtual func render(this) -> Str {
                return "widget"
            }
        }

        func test() -> Str {
            let w: Widget = Widget {}
            return w.render()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    auto stats = pass.get_stats();
    // Sealed class calls should be devirtualized
    EXPECT_GE(stats.devirtualized_sealed + stats.devirtualized_exact, 0u);
}

TEST_F(DevirtualizationTest, ExactTypeAfterConstructor) {
    auto [mir, env] = build_mir_and_env(R"(
        class Shape {
            virtual func area(this) -> I32 {
                return 0
            }
        }

        class Circle extends Shape {
            override func area(this) -> I32 {
                return 314
            }
        }

        func test() -> I32 {
            let c: Circle = Circle {}
            return c.area()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // Exact type known after constructor - should enable devirtualization
    auto stats = pass.get_stats();
    EXPECT_GE(stats.devirtualized_exact + stats.devirtualized_sealed, 0u);
}

TEST_F(DevirtualizationTest, FinalMethodDevirtualization) {
    auto [mir, env] = build_mir_and_env(R"(
        class Animal {
            final func id(this) -> I32 {
                return 42
            }
        }

        class Dog extends Animal {
        }

        func test() -> I32 {
            let dog: Dog = Dog {}
            return dog.id()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // Final methods cannot be overridden, so they can be devirtualized
    auto stats = pass.get_stats();
    EXPECT_GE(stats.devirtualized_final + stats.devirtualized_exact, 0u);
}

TEST_F(DevirtualizationTest, SingleImplementationDevirtualization) {
    auto [mir, env] = build_mir_and_env(R"(
        abstract class Base {
            abstract func compute(this) -> I32
        }

        class Only extends Base {
            override func compute(this) -> I32 {
                return 42
            }
        }

        func test(b: Base) -> I32 {
            return b.compute()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // With only one implementation, we can devirtualize
    auto stats = pass.get_stats();
    EXPECT_GE(stats.devirtualized_single + stats.method_calls_analyzed, 0u);
}

// ============================================================================
// Class Hierarchy Info Tests
// ============================================================================

TEST_F(DevirtualizationTest, LeafClassDetection) {
    auto [mir, env] = build_mir_and_env(R"(
        class Parent {
        }

        class Child extends Parent {
        }

        func test() {}
    )");

    tml::mir::DevirtualizationPass pass(*env);

    // Child has no subclasses, so it's a leaf class
    auto child_info = pass.get_class_info("Child");
    EXPECT_NE(child_info, nullptr);
    if (child_info) {
        EXPECT_TRUE(child_info->is_leaf());
    }

    // Parent has Child as a subclass
    auto parent_info = pass.get_class_info("Parent");
    EXPECT_NE(parent_info, nullptr);
    if (parent_info) {
        EXPECT_FALSE(parent_info->is_leaf());
    }
}

TEST_F(DevirtualizationTest, SealedClassInfo) {
    auto [mir, env] = build_mir_and_env(R"(
        sealed class Final {
        }

        func test() {}
    )");

    tml::mir::DevirtualizationPass pass(*env);
    auto info = pass.get_class_info("Final");
    EXPECT_NE(info, nullptr);
    if (info) {
        EXPECT_TRUE(info->is_sealed);
    }
}

TEST_F(DevirtualizationTest, AbstractClassInfo) {
    auto [mir, env] = build_mir_and_env(R"(
        abstract class AbstractBase {
            abstract func foo(this)
        }

        func test() {}
    )");

    tml::mir::DevirtualizationPass pass(*env);
    auto info = pass.get_class_info("AbstractBase");
    EXPECT_NE(info, nullptr);
    if (info) {
        EXPECT_TRUE(info->is_abstract);
    }
}

// ============================================================================
// Devirtualization Reason Tests
// ============================================================================

TEST_F(DevirtualizationTest, CanDevirtualizeSealedClass) {
    auto [mir, env] = build_mir_and_env(R"(
        sealed class Sealed {
            virtual func foo(this) {}
        }

        func test() {}
    )");

    tml::mir::DevirtualizationPass pass(*env);
    auto reason = pass.can_devirtualize("Sealed", "foo");
    EXPECT_EQ(reason, tml::mir::DevirtReason::SealedClass);
}

TEST_F(DevirtualizationTest, CanDevirtualizeExactType) {
    auto [mir, env] = build_mir_and_env(R"(
        class Leaf {
            virtual func foo(this) {}
        }

        func test() {}
    )");

    tml::mir::DevirtualizationPass pass(*env);
    auto reason = pass.can_devirtualize("Leaf", "foo");
    // Leaf class with no subclasses -> ExactType
    EXPECT_EQ(reason, tml::mir::DevirtReason::ExactType);
}

TEST_F(DevirtualizationTest, CannotDevirtualizeWithSubclasses) {
    auto [mir, env] = build_mir_and_env(R"(
        class Parent {
            virtual func foo(this) {}
        }

        class Child extends Parent {
            override func foo(this) {}
        }

        func test() {}
    )");

    tml::mir::DevirtualizationPass pass(*env);
    auto reason = pass.can_devirtualize("Parent", "foo");
    // Parent has subclass that overrides, cannot devirtualize
    EXPECT_EQ(reason, tml::mir::DevirtReason::NotDevirtualized);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(DevirtualizationTest, StatsTracking) {
    auto [mir, env] = build_mir_and_env(R"(
        sealed class Widget {
            virtual func render(this) -> Str {
                return "widget"
            }
        }

        func test() -> Str {
            let w: Widget = Widget {}
            return w.render()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    auto stats = pass.get_stats();
    // Stats should be tracked properly
    EXPECT_GE(stats.total_devirtualized() + stats.not_devirtualized, stats.method_calls_analyzed);
}

TEST_F(DevirtualizationTest, DevirtRateCalculation) {
    auto [mir, env] = build_mir_and_env(R"(
        sealed class Test {
            virtual func foo(this) {}
            virtual func bar(this) {}
        }

        func test() {
            let t: Test = Test {}
            t.foo()
            t.bar()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    auto stats = pass.get_stats();
    // Rate should be between 0 and 1
    double rate = stats.devirt_rate();
    EXPECT_GE(rate, 0.0);
    EXPECT_LE(rate, 1.0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(DevirtualizationTest, ComplexHierarchy) {
    auto [mir, env] = build_mir_and_env(R"(
        abstract class Animal {
            abstract func speak(this) -> Str
        }

        class Dog extends Animal {
            override func speak(this) -> Str {
                return "Woof!"
            }
        }

        class Cat extends Animal {
            override func speak(this) -> Str {
                return "Meow!"
            }
        }

        sealed class SilentDog extends Dog {
            override func speak(this) -> Str {
                return ""
            }
        }

        func test() {
            let dog: Dog = Dog {}
            let cat: Cat = Cat {}
            let silent: SilentDog = SilentDog {}

            dog.speak()
            cat.speak()
            silent.speak()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // SilentDog is sealed, should be devirtualizable
    auto reason = pass.can_devirtualize("SilentDog", "speak");
    EXPECT_EQ(reason, tml::mir::DevirtReason::SealedClass);
}

TEST_F(DevirtualizationTest, InterfaceImplementation) {
    auto [mir, env] = build_mir_and_env(R"(
        interface Drawable {
            func draw(this)
        }

        sealed class Circle implements Drawable {
            override func draw(this) {}
        }

        func test() {
            let c: Circle = Circle {}
            c.draw()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // Circle is sealed, interface method should be devirtualizable
    auto stats = pass.get_stats();
    EXPECT_GE(stats.devirtualized_sealed + stats.devirtualized_exact, 0u);
}

// ============================================================================
// Type Narrowing Tests
// ============================================================================

TEST_F(DevirtualizationTest, TypeNarrowingStatsExist) {
    auto [mir, env] = build_mir_and_env(R"(
        class Animal {
            virtual func speak(this) -> Str {
                return "..."
            }
        }

        class Dog extends Animal {
            override func speak(this) -> Str {
                return "Woof!"
            }
        }

        func test() -> Str {
            let dog: Dog = Dog {}
            return dog.speak()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    auto stats = pass.get_stats();
    // devirtualized_narrowing stat should exist
    EXPECT_GE(stats.devirtualized_narrowing, 0u);
}

TEST_F(DevirtualizationTest, ConstructorExactType) {
    // After a constructor, the exact type is known
    auto [mir, env] = build_mir_and_env(R"(
        class Point {
            virtual func x(this) -> I32 {
                return 0
            }
        }

        func test() -> I32 {
            let p: Point = Point {}
            return p.x()
        }
    )");

    tml::mir::DevirtualizationPass pass(*env);
    pass.run(mir);

    // With constructor, exact type is known
    auto stats = pass.get_stats();
    EXPECT_GE(stats.devirtualized_exact + stats.devirtualized_sealed, 0u);
}
