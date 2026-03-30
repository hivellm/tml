/// JIT Engine unit tests.
///
/// Tests ORC JIT execution engine: creation, IR loading, runtime symbol
/// resolution, and in-process execution.
///
/// Only compiled and run when TML_HAS_JIT is defined (build with --jit).

#include <gtest/gtest.h>

#if TML_HAS_JIT

#include "backend/jit_engine.hpp"

using namespace tml::backend;

class JitEngineTest : public ::testing::Test {};

TEST_F(JitEngineTest, CreateSucceeds) {
    auto [result, engine] = JitEngine::create();
    ASSERT_TRUE(result.success) << result.error;
    ASSERT_NE(engine, nullptr);
}

TEST_F(JitEngineTest, PureIRReturns42) {
    constexpr const char* ir = R"ir(
define i32 @main(i32 %argc, ptr %argv) {
entry:
  ret i32 42
}
)ir";
    auto [cr, engine] = JitEngine::create();
    ASSERT_TRUE(cr.success) << cr.error;

    auto ar = engine->addModule(ir);
    ASSERT_TRUE(ar.success) << ar.error;

    auto er = engine->executeMain({});
    ASSERT_TRUE(er.success) << er.error;
    EXPECT_EQ(er.exit_code, 42);
}

TEST_F(JitEngineTest, InvalidIRReturnsError) {
    auto [cr, engine] = JitEngine::create();
    ASSERT_TRUE(cr.success) << cr.error;

    auto ar = engine->addModule("not valid IR $$$");
    EXPECT_FALSE(ar.success);
    EXPECT_FALSE(ar.error.empty());
}

TEST_F(JitEngineTest, RuntimePrintResolved) {
    constexpr const char* ir = R"ir(
@.str = private constant [6 x i8] c"hello\00"
declare void @print(ptr)
define i32 @main(i32 %argc, ptr %argv) {
entry:
  call void @print(ptr @.str)
  ret i32 0
}
)ir";
    auto [cr, engine] = JitEngine::create();
    ASSERT_TRUE(cr.success) << cr.error;

    auto ar = engine->addModule(ir);
    ASSERT_TRUE(ar.success) << ar.error;

    auto er = engine->executeMain({});
    ASSERT_TRUE(er.success) << er.error;
    EXPECT_EQ(er.exit_code, 0);
}

TEST_F(JitEngineTest, RuntimeAllocFree) {
    constexpr const char* ir = R"ir(
declare ptr @tml_alloc(i64)
declare void @tml_free(ptr)
define i32 @main(i32 %argc, ptr %argv) {
entry:
  %ptr = call ptr @tml_alloc(i64 64)
  call void @tml_free(ptr %ptr)
  ret i32 0
}
)ir";
    auto [cr, engine] = JitEngine::create();
    ASSERT_TRUE(cr.success) << cr.error;

    auto ar = engine->addModule(ir);
    ASSERT_TRUE(ar.success) << ar.error;

    auto er = engine->executeMain({});
    ASSERT_TRUE(er.success) << er.error;
    EXPECT_EQ(er.exit_code, 0);
}

TEST_F(JitEngineTest, PointerReadWrite) {
    constexpr const char* ir = R"ir(
declare ptr @tml_alloc(i64)
declare void @tml_free(ptr)
define i32 @main(i32 %argc, ptr %argv) {
entry:
  %ptr = call ptr @tml_alloc(i64 8)
  store i32 42, ptr %ptr
  %val = load i32, ptr %ptr
  call void @tml_free(ptr %ptr)
  ret i32 %val
}
)ir";
    auto [cr, engine] = JitEngine::create();
    ASSERT_TRUE(cr.success) << cr.error;

    auto ar = engine->addModule(ir);
    ASSERT_TRUE(ar.success) << ar.error;

    auto er = engine->executeMain({});
    ASSERT_TRUE(er.success) << er.error;
    EXPECT_EQ(er.exit_code, 42);
}

TEST_F(JitEngineTest, SelfTestPasses) {
    EXPECT_TRUE(JitEngine::selfTest());
}

#else // !TML_HAS_JIT

TEST(JitEngineDisabledTest, SkipWhenJITDisabled) {
    GTEST_SKIP() << "JIT not enabled (build with --jit)";
}

#endif // TML_HAS_JIT
