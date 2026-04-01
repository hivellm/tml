//! # OOP Advanced Tests
//!
//! Advanced OOP tests split from oop_test.cpp for build-time management.
//! Covers: Class Hierarchy Analysis, Virtual Call Inlining,
//! Dead Method Elimination, Escape Analysis, @value and @pool class validation.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/dead_method_elimination.hpp"
#include "mir/passes/devirtualization.hpp"
#include "mir/passes/escape_analysis.hpp"
#include "mir/passes/inlining.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"
#include "types/type.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::lexer;
using namespace tml::parser;
using namespace tml::types;

// ============================================================================
// Class Hierarchy Analysis (CHA) Tests
// ============================================================================

class ClassHierarchyAnalysisTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> std::optional<TypeEnv> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module))
            return std::nullopt;

        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result))
            return std::nullopt;
        return std::move(std::get<TypeEnv>(result));
    }
};

TEST_F(ClassHierarchyAnalysisTest, BuildHierarchyBasic) {
    auto env_opt = check(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }
        }

        class Dog extends Animal {
            breed: Str

            new(n: Str, b: Str) {
                this.name = n
                this.breed = b
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    // Access class hierarchy by querying can_devirtualize
    // which internally builds the hierarchy
    (void)pass.can_devirtualize("Dog", "some_method");

    auto* dog_info = pass.get_class_info("Dog");
    ASSERT_NE(dog_info, nullptr);
    EXPECT_EQ(dog_info->name, "Dog");
    EXPECT_TRUE(dog_info->base_class.has_value());
    EXPECT_EQ(*dog_info->base_class, "Animal");
    EXPECT_TRUE(dog_info->is_leaf()); // No subclasses

    auto* animal_info = pass.get_class_info("Animal");
    ASSERT_NE(animal_info, nullptr);
    EXPECT_EQ(animal_info->name, "Animal");
    EXPECT_FALSE(animal_info->base_class.has_value()); // No base class
    EXPECT_FALSE(animal_info->is_leaf());              // Has subclasses
    EXPECT_TRUE(animal_info->subclasses.count("Dog") > 0);
}

TEST_F(ClassHierarchyAnalysisTest, SealedClassDetection) {
    auto env_opt = check(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }
        }

        sealed class FinalDog extends Animal {
            new(n: Str) {
                this.name = n
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    auto* final_dog_info = pass.get_class_info("FinalDog");
    ASSERT_NE(final_dog_info, nullptr);
    EXPECT_TRUE(final_dog_info->is_sealed);
    EXPECT_TRUE(final_dog_info->can_devirtualize());

    auto* animal_info = pass.get_class_info("Animal");
    ASSERT_NE(animal_info, nullptr);
    EXPECT_FALSE(animal_info->is_sealed);
}

TEST_F(ClassHierarchyAnalysisTest, AbstractClassDetection) {
    auto env_opt = check(R"(
        abstract class Shape {
            x: I32

            abstract func area(this) -> I32
        }

        class Circle extends Shape {
            radius: I32

            new(r: I32) {
                this.x = 0
                this.radius = r
            }

            override func area(this) -> I32 {
                return 3 * this.radius * this.radius
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    auto* shape_info = pass.get_class_info("Shape");
    ASSERT_NE(shape_info, nullptr);
    EXPECT_TRUE(shape_info->is_abstract);

    auto* circle_info = pass.get_class_info("Circle");
    ASSERT_NE(circle_info, nullptr);
    EXPECT_FALSE(circle_info->is_abstract);
}

TEST_F(ClassHierarchyAnalysisTest, TransitiveSubclasses) {
    auto env_opt = check(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }
        }

        class Dog extends Animal {
            new(n: Str) {
                this.name = n
            }
        }

        class GermanShepherd extends Dog {
            new(n: Str) {
                this.name = n
            }
        }

        class Labrador extends Dog {
            new(n: Str) {
                this.name = n
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    auto* animal_info = pass.get_class_info("Animal");
    ASSERT_NE(animal_info, nullptr);

    // Direct subclasses of Animal
    EXPECT_EQ(animal_info->subclasses.size(), 1u); // Just Dog
    EXPECT_TRUE(animal_info->subclasses.count("Dog") > 0);

    // Transitive subclasses of Animal (includes GermanShepherd and Labrador)
    EXPECT_EQ(animal_info->all_subclasses.size(), 3u);
    EXPECT_TRUE(animal_info->all_subclasses.count("Dog") > 0);
    EXPECT_TRUE(animal_info->all_subclasses.count("GermanShepherd") > 0);
    EXPECT_TRUE(animal_info->all_subclasses.count("Labrador") > 0);

    auto* dog_info = pass.get_class_info("Dog");
    ASSERT_NE(dog_info, nullptr);
    EXPECT_EQ(dog_info->subclasses.size(), 2u); // GermanShepherd, Labrador
    EXPECT_EQ(dog_info->all_subclasses.size(), 2u);

    // Leaf classes
    auto* gs_info = pass.get_class_info("GermanShepherd");
    ASSERT_NE(gs_info, nullptr);
    EXPECT_TRUE(gs_info->is_leaf());

    auto* lab_info = pass.get_class_info("Labrador");
    ASSERT_NE(lab_info, nullptr);
    EXPECT_TRUE(lab_info->is_leaf());
}

TEST_F(ClassHierarchyAnalysisTest, InterfaceTracking) {
    auto env_opt = check(R"(
        interface Runnable {
            func run(this)
        }

        interface Speakable {
            func speak(this) -> Str
        }

        class Dog implements Runnable, Speakable {
            name: Str

            new(n: Str) {
                this.name = n
            }

            func run(this) {
                print("Running\n")
            }

            func speak(this) -> Str {
                return "Woof"
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    auto* dog_info = pass.get_class_info("Dog");
    ASSERT_NE(dog_info, nullptr);
    EXPECT_EQ(dog_info->interfaces.size(), 2u);
}

TEST_F(ClassHierarchyAnalysisTest, CanDevirtualizeSealedClass) {
    auto env_opt = check(R"(
        sealed class Counter {
            value: I32

            new(v: I32) {
                this.value = v
            }

            virtual func increment(this) {
                this.value = this.value + 1
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    auto reason = pass.can_devirtualize("Counter", "increment");
    EXPECT_EQ(reason, tml::mir::DevirtReason::SealedClass);
}

TEST_F(ClassHierarchyAnalysisTest, CanDevirtualizeLeafClass) {
    auto env_opt = check(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }

            virtual func speak(this) -> Str {
                return "..."
            }
        }

        class Dog extends Animal {
            new(n: Str) {
                this.name = n
            }

            override func speak(this) -> Str {
                return "Woof"
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    // Dog is a leaf class (no subclasses)
    auto reason = pass.can_devirtualize("Dog", "speak");
    EXPECT_EQ(reason, tml::mir::DevirtReason::ExactType);
}

TEST_F(ClassHierarchyAnalysisTest, CanDevirtualizeFinalMethod) {
    auto env_opt = check(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }

            virtual func speak(this) -> Str {
                return "..."
            }
        }

        class Dog extends Animal {
            new(n: Str) {
                this.name = n
            }

            sealed override func speak(this) -> Str {
                return "Woof"
            }
        }

        class Cat extends Animal {
            new(n: Str) {
                this.name = n
            }

            override func speak(this) -> Str {
                return "Meow"
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    // Dog::speak is sealed (final), so calls through Dog can be devirtualized
    auto reason = pass.can_devirtualize("Dog", "speak");
    EXPECT_EQ(reason, tml::mir::DevirtReason::FinalMethod);

    // Cat::speak is not sealed, so it's devirtualized as ExactType (leaf class)
    auto reason2 = pass.can_devirtualize("Cat", "speak");
    EXPECT_EQ(reason2, tml::mir::DevirtReason::ExactType);
}

TEST_F(ClassHierarchyAnalysisTest, FinalMethodInheritance) {
    auto env_opt = check(R"(
        class Base {
            value: I32

            new() {
                this.value = 0
            }

            virtual func compute(this) -> I32 {
                return 42
            }
        }

        class Derived extends Base {
            new() {
                this.value = 1
            }

            sealed override func compute(this) -> I32 {
                return 100
            }
        }

        class MoreDerived extends Derived {
            new() {
                this.value = 2
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    // Derived has a sealed override - it should be detected as final
    auto* derived_info = pass.get_class_info("Derived");
    ASSERT_NE(derived_info, nullptr);
    EXPECT_TRUE(derived_info->is_method_final("compute"));

    // MoreDerived inherits the sealed compute method from Derived
    // The method is final and cannot be overridden
    auto reason1 = pass.can_devirtualize("Derived", "compute");
    EXPECT_EQ(reason1, tml::mir::DevirtReason::FinalMethod);

    auto reason2 = pass.can_devirtualize("MoreDerived", "compute");
    EXPECT_EQ(reason2, tml::mir::DevirtReason::FinalMethod);
}

TEST_F(ClassHierarchyAnalysisTest, CannotDevirtualizePolymorphic) {
    auto env_opt = check(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }

            virtual func speak(this) -> Str {
                return "..."
            }
        }

        class Dog extends Animal {
            new(n: Str) {
                this.name = n
            }

            override func speak(this) -> Str {
                return "Woof"
            }
        }

        class Cat extends Animal {
            new(n: Str) {
                this.name = n
            }

            override func speak(this) -> Str {
                return "Meow"
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    // Animal has multiple implementations (Dog, Cat), cannot devirtualize
    auto reason = pass.can_devirtualize("Animal", "speak");
    EXPECT_EQ(reason, tml::mir::DevirtReason::NotDevirtualized);
}

TEST_F(ClassHierarchyAnalysisTest, NonVirtualMethodDevirtualization) {
    auto env_opt = check(R"(
        class Counter {
            value: I32

            new() {
                this.value = 0
            }

            func increment(this) {
                this.value = this.value + 1
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DevirtualizationPass pass(env);

    // Non-virtual methods don't need devirtualization
    auto reason = pass.can_devirtualize("Counter", "increment");
    EXPECT_EQ(reason, tml::mir::DevirtReason::NoOverride);
}

// ============================================================================
// Virtual Call Inlining Tests
// ============================================================================

class VirtualCallInliningTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto build_mir(const std::string& code) -> std::optional<std::pair<TypeEnv, tml::mir::Module>> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module))
            return std::nullopt;

        TypeChecker checker;
        auto env_result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(env_result))
            return std::nullopt;
        auto env = std::move(std::get<TypeEnv>(env_result));

        tml::mir::MirBuilder builder(env);
        auto mir = builder.build(std::get<parser::Module>(module));
        return std::make_pair(std::move(env), std::move(mir));
    }
};

TEST_F(VirtualCallInliningTest, DevirtualizedCallGetsBonus) {
    // Test that devirtualized calls receive threshold bonus
    tml::mir::InliningOptions opts;
    opts.base_threshold = 250;
    opts.devirt_bonus = 100;
    opts.devirt_exact_bonus = 150;
    opts.devirt_sealed_bonus = 120;
    opts.prioritize_devirt = true;

    EXPECT_EQ(opts.devirt_bonus, 100);
    EXPECT_EQ(opts.devirt_exact_bonus, 150);
    EXPECT_EQ(opts.devirt_sealed_bonus, 120);
}

TEST_F(VirtualCallInliningTest, ConstructorInliningOptions) {
    // Test constructor inlining configuration
    tml::mir::InliningOptions opts;
    opts.constructor_bonus = 200;
    opts.base_constructor_bonus = 250;
    opts.prioritize_constructors = true;

    EXPECT_EQ(opts.constructor_bonus, 200);
    EXPECT_EQ(opts.base_constructor_bonus, 250);
    EXPECT_TRUE(opts.prioritize_constructors);
}

TEST_F(VirtualCallInliningTest, InlineCostAnalysis) {
    // Test the InlineCost struct
    tml::mir::InlineCost cost;
    cost.instruction_cost = 100;
    cost.call_overhead_saved = 20;
    cost.threshold = 200;

    // Net cost is 100 - 20 = 80, which is <= 200, so should inline
    EXPECT_EQ(cost.net_cost(), 80);
    EXPECT_TRUE(cost.should_inline());

    // Increase instruction cost to exceed threshold
    cost.instruction_cost = 300;
    // Net cost is now 300 - 20 = 280, which is > 200
    EXPECT_EQ(cost.net_cost(), 280);
    EXPECT_FALSE(cost.should_inline());
}

TEST_F(VirtualCallInliningTest, InlineDecisionEnum) {
    // Test that inline decisions are properly defined
    auto inline_decision = tml::mir::InlineDecision::Inline;
    EXPECT_EQ(inline_decision, tml::mir::InlineDecision::Inline);

    auto no_inline = tml::mir::InlineDecision::NoInline;
    EXPECT_EQ(no_inline, tml::mir::InlineDecision::NoInline);

    auto always = tml::mir::InlineDecision::AlwaysInline;
    EXPECT_EQ(always, tml::mir::InlineDecision::AlwaysInline);

    auto never = tml::mir::InlineDecision::NeverInline;
    EXPECT_EQ(never, tml::mir::InlineDecision::NeverInline);
}

TEST_F(VirtualCallInliningTest, InliningStatsInitialization) {
    // Test that statistics are properly initialized
    tml::mir::InliningStats stats;

    EXPECT_EQ(stats.calls_analyzed, 0u);
    EXPECT_EQ(stats.calls_inlined, 0u);
    EXPECT_EQ(stats.devirt_calls_analyzed, 0u);
    EXPECT_EQ(stats.devirt_calls_inlined, 0u);
    EXPECT_EQ(stats.constructor_calls_analyzed, 0u);
    EXPECT_EQ(stats.constructor_calls_inlined, 0u);
}

TEST_F(VirtualCallInliningTest, InliningPassCreation) {
    // Test that the inlining pass can be created with custom options
    tml::mir::InliningOptions opts;
    opts.base_threshold = 500;
    opts.devirt_bonus = 200;

    tml::mir::InliningPass pass(opts);
    EXPECT_EQ(pass.name(), "Inlining");

    // Statistics should be empty before running
    auto stats = pass.get_stats();
    EXPECT_EQ(stats.calls_analyzed, 0u);
}

TEST_F(VirtualCallInliningTest, AlwaysInlinePassCreation) {
    // Test that the always-inline pass can be created
    tml::mir::AlwaysInlinePass pass;
    EXPECT_EQ(pass.name(), "AlwaysInline");

    // Statistics should be empty before running
    auto stats = pass.get_stats();
    EXPECT_EQ(stats.calls_analyzed, 0u);
}

// ============================================================================
// Dead Method Elimination Tests
// ============================================================================

class DeadMethodEliminationTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> std::optional<TypeEnv> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module))
            return std::nullopt;

        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result))
            return std::nullopt;
        return std::move(std::get<TypeEnv>(result));
    }
};

TEST_F(DeadMethodEliminationTest, StatsInitialization) {
    // Test that statistics are properly initialized
    tml::mir::DeadMethodStats stats;

    EXPECT_EQ(stats.total_methods, 0u);
    EXPECT_EQ(stats.entry_points, 0u);
    EXPECT_EQ(stats.reachable_methods, 0u);
    EXPECT_EQ(stats.unreachable_methods, 0u);
    EXPECT_EQ(stats.methods_eliminated, 0u);
    EXPECT_EQ(stats.virtual_methods, 0u);
    EXPECT_EQ(stats.dead_virtual_methods, 0u);

    // Elimination rate should be 0 for empty stats
    EXPECT_EQ(stats.elimination_rate(), 0.0);
}

TEST_F(DeadMethodEliminationTest, EliminationRate) {
    // Test the elimination rate calculation
    tml::mir::DeadMethodStats stats;
    stats.total_methods = 10;
    stats.methods_eliminated = 3;

    // 3/10 = 0.3
    EXPECT_DOUBLE_EQ(stats.elimination_rate(), 0.3);

    stats.methods_eliminated = 10;
    EXPECT_DOUBLE_EQ(stats.elimination_rate(), 1.0);
}

TEST_F(DeadMethodEliminationTest, MethodInfoStruct) {
    // Test the MethodInfo struct
    tml::mir::MethodInfo info;
    info.full_name = "Dog_speak";
    info.class_name = "Dog";
    info.method_name = "speak";
    info.is_virtual = true;
    info.is_entry_point = false;
    info.is_reachable = true;

    EXPECT_EQ(info.full_name, "Dog_speak");
    EXPECT_EQ(info.class_name, "Dog");
    EXPECT_EQ(info.method_name, "speak");
    EXPECT_TRUE(info.is_virtual);
    EXPECT_FALSE(info.is_entry_point);
    EXPECT_TRUE(info.is_reachable);
}

TEST_F(DeadMethodEliminationTest, PassCreation) {
    auto env_opt = check(R"(
        class Dog {
            name: Str

            new(n: Str) {
                this.name = n
            }

            virtual func speak(this) -> Str {
                return "Woof"
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DeadMethodEliminationPass pass(env);

    EXPECT_EQ(pass.name(), "DeadMethodElimination");

    // Statistics should be empty before running
    auto stats = pass.get_stats();
    EXPECT_EQ(stats.total_methods, 0u);
    EXPECT_EQ(stats.entry_points, 0u);
}

TEST_F(DeadMethodEliminationTest, GetDeadMethodsEmpty) {
    auto env_opt = check(R"(
        class Dog {
            name: Str

            new(n: Str) {
                this.name = n
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DeadMethodEliminationPass pass(env);

    // Before running, get_dead_methods should return empty
    auto dead = pass.get_dead_methods();
    EXPECT_TRUE(dead.empty());
}

TEST_F(DeadMethodEliminationTest, ReachabilityQueryBeforeRun) {
    auto env_opt = check(R"(
        class Dog {
            name: Str

            new(n: Str) {
                this.name = n
            }
        }
    )");
    ASSERT_TRUE(env_opt.has_value());
    auto& env = *env_opt;

    tml::mir::DeadMethodEliminationPass pass(env);

    // Before running, method should not be marked reachable
    EXPECT_FALSE(pass.is_method_reachable("Dog_new"));
    EXPECT_FALSE(pass.is_method_reachable("nonexistent"));
}

// ============================================================================
// Escape Analysis Tests
// ============================================================================

class OOPEscapeAnalysisTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> std::optional<TypeEnv> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module))
            return std::nullopt;

        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result))
            return std::nullopt;
        return std::move(std::get<TypeEnv>(result));
    }
};

TEST_F(OOPEscapeAnalysisTest, EscapeStateEnum) {
    // Test EscapeState enum values
    auto no_escape = tml::mir::EscapeState::NoEscape;
    auto arg_escape = tml::mir::EscapeState::ArgEscape;
    auto return_escape = tml::mir::EscapeState::ReturnEscape;
    auto global_escape = tml::mir::EscapeState::GlobalEscape;
    auto unknown = tml::mir::EscapeState::Unknown;

    EXPECT_EQ(no_escape, tml::mir::EscapeState::NoEscape);
    EXPECT_EQ(arg_escape, tml::mir::EscapeState::ArgEscape);
    EXPECT_EQ(return_escape, tml::mir::EscapeState::ReturnEscape);
    EXPECT_EQ(global_escape, tml::mir::EscapeState::GlobalEscape);
    EXPECT_EQ(unknown, tml::mir::EscapeState::Unknown);
}

TEST_F(OOPEscapeAnalysisTest, EscapeInfoStruct) {
    // Test EscapeInfo struct and its helper methods
    tml::mir::EscapeInfo info;

    // Default state is Unknown
    EXPECT_EQ(info.state, tml::mir::EscapeState::Unknown);
    EXPECT_FALSE(info.may_alias_heap);
    EXPECT_FALSE(info.may_alias_global);
    EXPECT_FALSE(info.is_stack_promotable);
    EXPECT_FALSE(info.is_class_instance);
    EXPECT_TRUE(info.class_name.empty());

    // Unknown state means it escapes (conservative)
    EXPECT_TRUE(info.escapes());

    // NoEscape does not escape
    info.state = tml::mir::EscapeState::NoEscape;
    EXPECT_FALSE(info.escapes());

    // ArgEscape escapes
    info.state = tml::mir::EscapeState::ArgEscape;
    EXPECT_TRUE(info.escapes());

    // ReturnEscape escapes
    info.state = tml::mir::EscapeState::ReturnEscape;
    EXPECT_TRUE(info.escapes());

    // GlobalEscape escapes
    info.state = tml::mir::EscapeState::GlobalEscape;
    EXPECT_TRUE(info.escapes());
}

TEST_F(OOPEscapeAnalysisTest, EscapeInfoClassInstance) {
    // Test EscapeInfo with class instance tracking
    tml::mir::EscapeInfo info;
    info.is_class_instance = true;
    info.class_name = "Dog";
    info.state = tml::mir::EscapeState::NoEscape;
    info.is_stack_promotable = true;

    EXPECT_TRUE(info.is_class_instance);
    EXPECT_EQ(info.class_name, "Dog");
    EXPECT_FALSE(info.escapes());
    EXPECT_TRUE(info.is_stack_promotable);
}

TEST_F(OOPEscapeAnalysisTest, StatsInitialization) {
    // Test that statistics are properly initialized
    tml::mir::EscapeAnalysisPass::Stats stats;

    EXPECT_EQ(stats.total_allocations, 0u);
    EXPECT_EQ(stats.no_escape, 0u);
    EXPECT_EQ(stats.arg_escape, 0u);
    EXPECT_EQ(stats.return_escape, 0u);
    EXPECT_EQ(stats.global_escape, 0u);
    EXPECT_EQ(stats.stack_promotable, 0u);

    // Class instance statistics
    EXPECT_EQ(stats.class_instances, 0u);
    EXPECT_EQ(stats.class_instances_no_escape, 0u);
    EXPECT_EQ(stats.class_instances_promotable, 0u);
    EXPECT_EQ(stats.method_call_escapes, 0u);
    EXPECT_EQ(stats.field_store_escapes, 0u);
}

TEST_F(OOPEscapeAnalysisTest, PassCreation) {
    // Test that the escape analysis pass can be created
    tml::mir::EscapeAnalysisPass pass;

    EXPECT_EQ(pass.name(), "EscapeAnalysis");

    // Statistics should be empty before running
    auto stats = pass.get_stats();
    EXPECT_EQ(stats.total_allocations, 0u);
    EXPECT_EQ(stats.class_instances, 0u);
}

TEST_F(OOPEscapeAnalysisTest, QueryBeforeRun) {
    // Test querying escape info before analysis runs
    tml::mir::EscapeAnalysisPass pass;

    // Before running, all queries should return default Unknown state
    auto info = pass.get_escape_info(tml::mir::ValueId(42));
    EXPECT_EQ(info.state, tml::mir::EscapeState::Unknown);
    EXPECT_TRUE(info.escapes());

    // can_stack_promote should return false for unknown values
    EXPECT_FALSE(pass.can_stack_promote(tml::mir::ValueId(42)));

    // get_stack_promotable should return empty before running
    auto promotable = pass.get_stack_promotable();
    EXPECT_TRUE(promotable.empty());
}

TEST_F(OOPEscapeAnalysisTest, StackPromotionPassCreation) {
    // Test that the stack promotion pass can be created
    tml::mir::EscapeAnalysisPass escape_pass;
    tml::mir::StackPromotionPass promo_pass(escape_pass);

    EXPECT_EQ(promo_pass.name(), "StackPromotion");

    // Statistics should be empty before running
    auto stats = promo_pass.get_stats();
    EXPECT_EQ(stats.allocations_promoted, 0u);
    EXPECT_EQ(stats.bytes_saved, 0u);
}

// ============================================================================
// @value Class Validation Tests
// ============================================================================

class ValueClassValidationTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;
    std::optional<TypeEnv> last_env_;
    std::vector<TypeError> last_errors_;

    auto check(const std::string& code) -> bool {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module)) {
            last_env_ = std::nullopt;
            last_errors_.clear();
            return false;
        }
        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result)) {
            last_env_ = std::nullopt;
            last_errors_ = std::get<std::vector<TypeError>>(result);
            return false;
        }
        last_env_ = std::move(std::get<TypeEnv>(result));
        last_errors_.clear();
        return true;
    }
};

TEST_F(ValueClassValidationTest, ValidValueClass) {
    // A valid @value class with no virtual methods
    bool success = check(R"(
        @value
        class Point {
            private x: I32
            private y: I32

            func get_x(this) -> I32 {
                this.x
            }
        }
    )");
    EXPECT_TRUE(success);
    EXPECT_TRUE(last_errors_.empty());
    if (last_env_) {
        auto class_def = last_env_->lookup_class("Point");
        ASSERT_TRUE(class_def.has_value());
        EXPECT_TRUE(class_def->is_value);
        EXPECT_TRUE(class_def->is_sealed); // @value implies sealed
    }
}

TEST_F(ValueClassValidationTest, ValueClassCannotHaveVirtualMethods) {
    // @value classes cannot have virtual methods
    bool success = check(R"(
        @value
        class BadValue {
            virtual func foo(this) -> I32 { 42 }
        }
    )");
    EXPECT_FALSE(success);
    EXPECT_FALSE(last_errors_.empty());
    bool found_error = false;
    for (const auto& err : last_errors_) {
        if (err.message.find("cannot have virtual method") != std::string::npos) {
            found_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_error) << "Expected error about virtual methods in @value class";
}

TEST_F(ValueClassValidationTest, ValueClassCannotBeAbstract) {
    // @value classes cannot be abstract
    bool success = check(R"(
        @value
        abstract class BadAbstractValue {
            abstract func foo(this) -> I32
        }
    )");
    EXPECT_FALSE(success);
    EXPECT_FALSE(last_errors_.empty());
    bool found_error = false;
    for (const auto& err : last_errors_) {
        if (err.message.find("cannot be abstract") != std::string::npos) {
            found_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_error) << "Expected error about @value class being abstract";
}

TEST_F(ValueClassValidationTest, ValueClassCanExtendValueClass) {
    // @value classes can extend other @value classes
    bool success = check(R"(
        @value
        class Base {
            private x: I32
        }

        @value
        class Derived extends Base {
            private y: I32
        }
    )");
    EXPECT_TRUE(success);
    EXPECT_TRUE(last_errors_.empty());
}

TEST_F(ValueClassValidationTest, ValueClassCannotExtendNonValueClass) {
    // @value classes cannot extend non-value classes
    bool success = check(R"(
        class RegularClass {
            private x: I32
        }

        @value
        class BadDerived extends RegularClass {
            private y: I32
        }
    )");
    EXPECT_FALSE(success);
    EXPECT_FALSE(last_errors_.empty());
    bool found_error = false;
    for (const auto& err : last_errors_) {
        if (err.message.find("cannot extend non-value class") != std::string::npos) {
            found_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_error) << "Expected error about extending non-value class";
}

TEST_F(ValueClassValidationTest, ValueClassCanImplementInterfaces) {
    // @value classes can implement interfaces
    bool success = check(R"(
        interface IAddable {
            func add(this, other: I32) -> I32
        }

        @value
        class Counter implements IAddable {
            private value: I32

            func add(this, other: I32) -> I32 {
                this.value + other
            }
        }
    )");
    EXPECT_TRUE(success);
    EXPECT_TRUE(last_errors_.empty());
}

// =============================================================================
// @pool Class Validation Tests
// =============================================================================

class PoolClassValidationTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;
    std::optional<TypeEnv> last_env_;
    std::vector<TypeError> last_errors_;

    auto check(const std::string& code) -> bool {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module)) {
            last_env_ = std::nullopt;
            last_errors_.clear();
            return false;
        }
        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result)) {
            last_env_ = std::nullopt;
            last_errors_ = std::get<std::vector<TypeError>>(result);
            return false;
        }
        last_env_ = std::move(std::get<TypeEnv>(result));
        last_errors_.clear();
        return true;
    }
};

TEST_F(PoolClassValidationTest, ValidPoolClass) {
    // A valid @pool class
    bool success = check(R"(
        @pool
        class PooledEntity {
            private id: I32

            func get_id(this) -> I32 {
                this.id
            }
        }
    )");
    EXPECT_TRUE(success);
    EXPECT_TRUE(last_errors_.empty());
}

TEST_F(PoolClassValidationTest, PoolClassCannotBeAbstract) {
    // @pool classes cannot be abstract
    bool success = check(R"(
        @pool
        abstract class BadPooledAbstract {
            abstract func foo(this) -> I32
        }
    )");
    EXPECT_FALSE(success);
    EXPECT_FALSE(last_errors_.empty());
    EXPECT_TRUE(last_errors_[0].message.find("cannot be abstract") != std::string::npos);
}

TEST_F(PoolClassValidationTest, PoolAndValueMutuallyExclusive) {
    // @pool and @value cannot be combined
    bool success = check(R"(
        @pool
        @value
        class BadCombined {
            private x: I32
        }
    )");
    EXPECT_FALSE(success);
    EXPECT_FALSE(last_errors_.empty());
    EXPECT_TRUE(last_errors_[0].message.find("mutually exclusive") != std::string::npos);
}

TEST_F(PoolClassValidationTest, PoolClassCanHaveVirtualMethods) {
    // @pool classes CAN have virtual methods (unlike @value)
    bool success = check(R"(
        @pool
        class PooledWithVirtual {
            private x: I32

            virtual func process(this) -> I32 {
                this.x
            }
        }
    )");
    EXPECT_TRUE(success);
    EXPECT_TRUE(last_errors_.empty());
}

TEST_F(PoolClassValidationTest, PoolClassCanExtendNonPoolClass) {
    // @pool classes can extend non-pool classes
    bool success = check(R"(
        class BaseEntity {
            private id: I32
        }

        @pool
        class PooledEntity extends BaseEntity {
            private data: I32
        }
    )");
    EXPECT_TRUE(success);
    EXPECT_TRUE(last_errors_.empty());
}
