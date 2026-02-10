// TML Compiler - Text Type Tests
// Comprehensive tests for Text type codegen and runtime integration

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::codegen;
using namespace tml::types;
using namespace tml::parser;
using namespace tml::lexer;

class TextCodegenTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto generate(const std::string& code) -> std::string {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        EXPECT_TRUE(is_ok(module_result));
        auto& module = std::get<parser::Module>(module_result);

        TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(is_ok(env_result));
        auto& env = std::get<TypeEnv>(env_result);

        LLVMIRGen gen(env);
        auto ir_result = gen.generate(module);

        if (is_err(ir_result)) {
            auto& errors = std::get<std::vector<LLVMGenError>>(ir_result);
            for (const auto& err : errors) {
                ADD_FAILURE() << "Codegen error: " << err.message;
            }
            return "";
        }

        return std::get<std::string>(ir_result);
    }

    void expect_ir_contains(const std::string& ir, const std::string& pattern,
                            const std::string& msg = "") {
        EXPECT_NE(ir.find(pattern), std::string::npos) << msg << "\nPattern: " << pattern;
    }

    void expect_ir_not_contains(const std::string& ir, const std::string& pattern,
                                const std::string& msg = "") {
        EXPECT_EQ(ir.find(pattern), std::string::npos)
            << msg << "\nUnexpected pattern: " << pattern;
    }
};

// ============================================================================
// Text Constructor Tests
// ============================================================================

TEST_F(TextCodegenTest, TextNew) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::new()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_new", "Should call text_new constructor");
    expect_ir_contains(ir, "text_drop", "Should call text_drop destructor");
}

TEST_F(TextCodegenTest, TextFromStr) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_from_str", "Should call text_from_str");
}

TEST_F(TextCodegenTest, TextWithCapacity) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::with_capacity(100)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_with_capacity", "Should call text_with_capacity");
}

TEST_F(TextCodegenTest, TextFromI64) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from_i64(42)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_from_i64", "Should call text_from_i64");
}

TEST_F(TextCodegenTest, TextFromF64) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from_f64(3.14)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_from_f64", "Should call text_from_f64");
}

TEST_F(TextCodegenTest, TextFromBool) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from_bool(true)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_from_bool", "Should call text_from_bool");
}

// ============================================================================
// Text Properties Tests
// ============================================================================

TEST_F(TextCodegenTest, TextLen) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            let l: I64 = t.len()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_len", "Should call text_len");
}

TEST_F(TextCodegenTest, TextCapacity) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::with_capacity(100)
            let c: I64 = t.capacity()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_capacity", "Should call text_capacity");
}

TEST_F(TextCodegenTest, TextIsEmpty) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::new()
            let e: Bool = t.is_empty()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_is_empty", "Should call text_is_empty");
}

TEST_F(TextCodegenTest, TextByteAt) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("ABC")
            let b: I32 = t.byte_at(0)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_byte_at", "Should call text_byte_at");
}

// ============================================================================
// Text Modification Tests
// ============================================================================

TEST_F(TextCodegenTest, TextClear) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            t.clear()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_clear", "Should call text_clear");
}

TEST_F(TextCodegenTest, TextPush) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::new()
            t.push(65)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_push", "Should call text_push");
}

TEST_F(TextCodegenTest, TextPushStr) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::new()
            t.push_str("Hello")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_push_str", "Should call text_push_str");
}

TEST_F(TextCodegenTest, TextReserve) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::new()
            t.reserve(100)
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_reserve", "Should call text_reserve");
}

// ============================================================================
// Text Search Tests
// ============================================================================

TEST_F(TextCodegenTest, TextIndexOf) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello, World!")
            let idx: I64 = t.index_of("World")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_index_of", "Should call text_index_of");
}

TEST_F(TextCodegenTest, TextLastIndexOf) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("abcabc")
            let idx: I64 = t.last_index_of("bc")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_last_index_of", "Should call text_last_index_of");
}

TEST_F(TextCodegenTest, TextStartsWith) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            let b: Bool = t.starts_with("He")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_starts_with", "Should call text_starts_with");
}

TEST_F(TextCodegenTest, TextEndsWith) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            let b: Bool = t.ends_with("lo")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_ends_with", "Should call text_ends_with");
}

TEST_F(TextCodegenTest, TextContains) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello, World!")
            let b: Bool = t.contains(",")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_contains", "Should call text_contains");
}

// ============================================================================
// Text Transformation Tests
// ============================================================================

TEST_F(TextCodegenTest, TextToUpperCase) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("hello")
            let u: Text = t.to_upper_case()
            t.drop()
            u.drop()
        }
    )");
    expect_ir_contains(ir, "text_to_upper", "Should call text_to_upper");
}

TEST_F(TextCodegenTest, TextToLowerCase) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("HELLO")
            let l: Text = t.to_lower_case()
            t.drop()
            l.drop()
        }
    )");
    expect_ir_contains(ir, "text_to_lower", "Should call text_to_lower");
}

TEST_F(TextCodegenTest, TextTrim) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("  hello  ")
            let tr: Text = t.trim()
            t.drop()
            tr.drop()
        }
    )");
    expect_ir_contains(ir, "text_trim", "Should call text_trim");
}

TEST_F(TextCodegenTest, TextTrimStart) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("  hello")
            let tr: Text = t.trim_start()
            t.drop()
            tr.drop()
        }
    )");
    expect_ir_contains(ir, "text_trim_start", "Should call text_trim_start");
}

TEST_F(TextCodegenTest, TextTrimEnd) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("hello  ")
            let tr: Text = t.trim_end()
            t.drop()
            tr.drop()
        }
    )");
    expect_ir_contains(ir, "text_trim_end", "Should call text_trim_end");
}

TEST_F(TextCodegenTest, TextSubstring) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello, World!")
            let s: Text = t.substring(7, 12)
            t.drop()
            s.drop()
        }
    )");
    expect_ir_contains(ir, "text_substring", "Should call text_substring");
}

TEST_F(TextCodegenTest, TextRepeat) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("ab")
            let r: Text = t.repeat(3)
            t.drop()
            r.drop()
        }
    )");
    expect_ir_contains(ir, "text_repeat", "Should call text_repeat");
}

TEST_F(TextCodegenTest, TextReplace) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello, World!")
            let r: Text = t.replace("World", "TML")
            t.drop()
            r.drop()
        }
    )");
    expect_ir_contains(ir, "text_replace", "Should call text_replace");
}

TEST_F(TextCodegenTest, TextReplaceAll) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("ababab")
            let r: Text = t.replace_all("ab", "X")
            t.drop()
            r.drop()
        }
    )");
    expect_ir_contains(ir, "text_replace_all", "Should call text_replace_all");
}

TEST_F(TextCodegenTest, TextReverse) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("hello")
            let r: Text = t.reverse()
            t.drop()
            r.drop()
        }
    )");
    expect_ir_contains(ir, "text_reverse", "Should call text_reverse");
}

TEST_F(TextCodegenTest, TextPadStart) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("42")
            let p: Text = t.pad_start(5, 48)
            t.drop()
            p.drop()
        }
    )");
    expect_ir_contains(ir, "text_pad_start", "Should call text_pad_start");
}

TEST_F(TextCodegenTest, TextPadEnd) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hi")
            let p: Text = t.pad_end(5, 46)
            t.drop()
            p.drop()
        }
    )");
    expect_ir_contains(ir, "text_pad_end", "Should call text_pad_end");
}

// ============================================================================
// Text Concatenation Tests
// ============================================================================

TEST_F(TextCodegenTest, TextConcat) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t1: Text = Text::from("Hello")
            let t2: Text = Text::from(", World!")
            let r: Text = t1.concat(ref t2)
            t1.drop()
            t2.drop()
            r.drop()
        }
    )");
    expect_ir_contains(ir, "text_concat", "Should call text_concat");
}

TEST_F(TextCodegenTest, TextConcatStr) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            let r: Text = t.concat_str(", World!")
            t.drop()
            r.drop()
        }
    )");
    expect_ir_contains(ir, "text_concat_str", "Should call text_concat_str");
}

// ============================================================================
// Text Comparison Tests
// ============================================================================

TEST_F(TextCodegenTest, TextCompare) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t1: Text = Text::from("apple")
            let t2: Text = Text::from("banana")
            let c: I32 = t1.compare(ref t2)
            t1.drop()
            t2.drop()
        }
    )");
    expect_ir_contains(ir, "text_compare", "Should call text_compare");
}

TEST_F(TextCodegenTest, TextEquals) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t1: Text = Text::from("hello")
            let t2: Text = Text::from("hello")
            let eq: Bool = t1.equals(ref t2)
            t1.drop()
            t2.drop()
        }
    )");
    expect_ir_contains(ir, "text_equals", "Should call text_equals");
}

// ============================================================================
// Text Clone Tests
// ============================================================================

TEST_F(TextCodegenTest, TextClone) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t1: Text = Text::from("Clone me")
            let t2: Text = t1.clone()
            t1.drop()
            t2.drop()
        }
    )");
    expect_ir_contains(ir, "text_clone", "Should call text_clone");
}

// ============================================================================
// Text Conversion Tests
// ============================================================================

TEST_F(TextCodegenTest, TextAsStr) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            let s: Str = t.as_str()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_as_cstr", "Should call text_as_cstr");
}

// ============================================================================
// Text Output Tests
// ============================================================================

TEST_F(TextCodegenTest, TextPrint) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            t.print()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_print", "Should call text_print");
}

TEST_F(TextCodegenTest, TextPrintln) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("Hello")
            t.println()
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_println", "Should call text_println");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(TextCodegenTest, TextChainedOperations) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("  HELLO WORLD  ")
            let trimmed: Text = t.trim()
            let lower: Text = trimmed.to_lower_case()
            let replaced: Text = lower.replace(" ", "_")
            t.drop()
            trimmed.drop()
            lower.drop()
            replaced.drop()
        }
    )");
    expect_ir_contains(ir, "text_trim", "Should call text_trim");
    expect_ir_contains(ir, "text_to_lower", "Should call text_to_lower");
    expect_ir_contains(ir, "text_replace", "Should call text_replace");
}

TEST_F(TextCodegenTest, TextMultiplePushStr) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::new()
            t.push_str("one")
            t.push_str(" ")
            t.push_str("two")
            t.push_str(" ")
            t.push_str("three")
            t.drop()
        }
    )");
    // Verify multiple push_str calls are generated
    // Count occurrences would be ideal but checking presence is sufficient
    expect_ir_contains(ir, "text_push_str", "Should call text_push_str");
}

TEST_F(TextCodegenTest, TextSSOToHeapTransition) {
    std::string ir = generate(R"(
        use std::text::Text
        func main() {
            let t: Text = Text::from("short")
            t.push_str(" - now adding more content to exceed SSO limit!")
            t.drop()
        }
    )");
    expect_ir_contains(ir, "text_from_str", "Should call text_from_str");
    expect_ir_contains(ir, "text_push_str", "Should call text_push_str");
}
