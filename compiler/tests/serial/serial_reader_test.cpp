//! # Round-trip tests for AST and TypeEnv binary deserializers.
//!
//! Standalone test binary (no GTest dependency). Manually constructs binary data
//! in the format produced by `lib/std/src/serial/ast.tml` and
//! `lib/std/src/serial/typeenv.tml`, feeds it to the C++ readers, and verifies
//! the deserialized structures.
//!
//! Build: cmake adds this as `serial_reader_test` target.
//! Run:   build/debug/bin/serial_reader_test.exe

#include "parser/ast.hpp"
#include "parser/ast_decls.hpp"
#include "parser/ast_exprs.hpp"
#include "parser/ast_types.hpp"
#include "serial/ast_reader.hpp"
#include "serial/typeenv_reader.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tml;

// ============================================================================
// Minimal test framework
// ============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                                     \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << ": " << (msg) << "\n";       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define TEST_EQ(a, b, msg)                                                                         \
    do {                                                                                           \
        if ((a) != (b)) {                                                                          \
            std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << ": " << (msg) << "\n"        \
                      << "    expected: " << (b) << "\n"                                           \
                      << "    got:      " << (a) << "\n";                                          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                                                               \
    do {                                                                                           \
        g_tests_run++;                                                                             \
        std::cout << "  " << #fn << " ... ";                                                       \
        try {                                                                                      \
            if (fn()) {                                                                            \
                g_tests_passed++;                                                                  \
                std::cout << "OK\n";                                                               \
            } else {                                                                               \
                g_tests_failed++;                                                                  \
            }                                                                                      \
        } catch (const std::exception& e) {                                                        \
            g_tests_failed++;                                                                      \
            std::cerr << "  EXCEPTION: " << e.what() << "\n";                                      \
        }                                                                                          \
    } while (0)

// ============================================================================
// BinaryBuilder — mirrors the TML BinaryWriter format
// ============================================================================

class BinaryBuilder {
public:
    void write_u8(uint8_t v) {
        buf_.push_back(v);
    }

    void write_u32_le(uint32_t v) {
        buf_.push_back(static_cast<uint8_t>(v & 0xFF));
        buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    void write_u64_le(uint64_t v) {
        for (int i = 0; i < 8; i++) {
            buf_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void write_bool(bool v) {
        buf_.push_back(v ? 1 : 0);
    }

    void write_varint(uint64_t v) {
        while (v >= 0x80) {
            buf_.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
            v >>= 7;
        }
        buf_.push_back(static_cast<uint8_t>(v));
    }

    void write_str(const std::string& s) {
        write_varint(s.size());
        buf_.insert(buf_.end(), s.begin(), s.end());
    }

    void write_source_location(uint32_t line, uint32_t col, uint32_t offset, uint32_t length) {
        write_u32_le(line);
        write_u32_le(col);
        write_u32_le(offset);
        write_u32_le(length);
    }

    void write_source_span(uint32_t line = 1, uint32_t col = 1, uint32_t offset = 0,
                           uint32_t length = 0) {
        write_source_location(line, col, offset, length);
        write_source_location(line, col, offset, length);
    }

    void write_visibility(uint8_t vis) {
        write_u8(vis);
    }

    void write_type_path(const std::vector<std::string>& segments) {
        write_varint(segments.size());
        for (const auto& s : segments) {
            write_str(s);
        }
        write_source_span();
    }

    void write_named_type(const std::vector<std::string>& path) {
        write_u8(0); // Named tag
        write_type_path(path);
        write_varint(0); // no type args
        write_source_span();
    }

    void write_empty_generic_params() {
        write_varint(0);
    }

    void write_func_param(const std::string& name, const std::vector<std::string>& type_path,
                          bool is_this = false) {
        write_str(name);
        write_named_type(type_path);
        write_bool(is_this);
        write_bool(false); // is_mut
        write_source_span();
    }

    void write_opt_type_none() {
        write_u8(0);
    }
    void write_opt_type_named(const std::vector<std::string>& path) {
        write_u8(1);
        write_named_type(path);
    }

    // Semantic type tags for TypeEnv
    void write_sem_primitive(uint8_t kind) {
        write_u8(0x00);
        write_u8(kind);
    }
    void write_sem_named(const std::string& name, const std::string& module_path) {
        write_u8(0x01);
        write_str(name);
        write_str(module_path);
        write_varint(0); // no type_args
    }
    void write_sem_ref(uint8_t inner_prim, bool is_mut) {
        write_u8(0x02);
        write_bool(is_mut);
        write_sem_primitive(inner_prim);
        write_varint(0); // lifetime: empty opt_str = Nothing
    }
    void write_sem_ptr(uint8_t inner_prim, bool is_mut) {
        write_u8(0x03);
        write_bool(is_mut);
        write_sem_primitive(inner_prim);
    }
    void write_sem_null() {
        write_u8(0x10);
    }

    void write_ast_header() {
        write_u32_le(0x41535420);
        write_u8(1);
        write_u8(0);
    }

    void write_typeenv_header() {
        write_u32_le(0x54454E56);
        write_u8(1);
        write_u8(0);
    }

    void write_empty_typeenv_sections(int count = 10) {
        for (int i = 0; i < count; i++) {
            write_varint(0);
        }
    }

    // Write a minimal FuncSig for TypeEnv (must match typeenv_reader.cpp read_func_sig)
    void write_func_sig_typeenv(const std::string& name, uint8_t return_prim) {
        write_str(name);
        write_varint(0);                  // no params
        write_sem_primitive(return_prim); // return type
        write_varint(0);                  // no type_params (str_list)
        write_bool(false);                // is_async
        write_u8(0);                      // stability = Unstable
        write_str("");                    // deprecated_message
        write_str("");                    // since_version
        write_varint(0);                  // no where_constraints
        write_bool(false);                // is_lowlevel
        write_bool(false);                // is_intrinsic
        write_varint(0);                  // extern_abi: opt_str empty = None
        write_varint(0);                  // extern_name: opt_str empty = None
        write_varint(0);                  // no link_libs (str_list)
        write_varint(0);                  // ffi_module: opt_str empty = None
        write_varint(0);                  // no const_params (const_generic_param_list)
    }

    [[nodiscard]] const std::vector<uint8_t>& data() const {
        return buf_;
    }

private:
    std::vector<uint8_t> buf_;
};

// ============================================================================
// AST Reader Tests (5.1 + 5.2)
// ============================================================================

bool test_ast_empty_module() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test_module");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(0);

    auto m = serial::read_ast(b.data(), "test.tml");
    TEST_EQ(m.name, std::string("test_module"), "module name");
    TEST_ASSERT(m.decls.empty(), "decls should be empty");
    TEST_ASSERT(m.module_docs.empty(), "docs should be empty");
    return true;
}

bool test_ast_module_with_docs() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("documented");
    b.write_varint(2);
    b.write_str("First line");
    b.write_str("Second line");
    b.write_source_span();
    b.write_varint(0);

    auto m = serial::read_ast(b.data(), "test.tml");
    TEST_EQ(m.name, std::string("documented"), "module name");
    TEST_EQ(m.module_docs.size(), size_t(2), "doc count");
    TEST_EQ(m.module_docs[0], std::string("First line"), "doc[0]");
    TEST_EQ(m.module_docs[1], std::string("Second line"), "doc[1]");
    return true;
}

bool test_ast_simple_struct() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(1);

    b.write_u8(1); // Struct tag
    b.write_str("Point");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(2);

    b.write_str("x");
    b.write_named_type({"I32"});
    b.write_visibility(1);
    b.write_source_span();

    b.write_str("y");
    b.write_named_type({"I32"});
    b.write_visibility(1);
    b.write_source_span();

    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    TEST_EQ(m.decls.size(), size_t(1), "decl count");

    auto& sd = std::get<parser::StructDecl>(m.decls[0]->kind);
    TEST_EQ(sd.name, std::string("Point"), "struct name");
    TEST_ASSERT(sd.vis == parser::Visibility::Public, "visibility");
    TEST_EQ(sd.fields.size(), size_t(2), "field count");
    TEST_EQ(sd.fields[0].name, std::string("x"), "field[0] name");
    TEST_EQ(sd.fields[1].name, std::string("y"), "field[1] name");

    TEST_ASSERT(sd.fields[0].type->is<parser::NamedType>(), "field type is NamedType");
    auto& xt = sd.fields[0].type->as<parser::NamedType>();
    TEST_EQ(xt.path.segments.size(), size_t(1), "type path segments");
    TEST_EQ(xt.path.segments[0], std::string("I32"), "type name");
    return true;
}

bool test_ast_simple_function() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(1);

    b.write_u8(0); // Func tag
    b.write_str("add");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(2);
    b.write_func_param("a", {"I32"});
    b.write_func_param("b", {"I32"});
    b.write_opt_type_named({"I32"});
    b.write_u8(0);       // no body
    b.write_bool(false); // is_async
    b.write_bool(false); // is_override
    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    TEST_EQ(m.decls.size(), size_t(1), "decl count");

    auto& fd = std::get<parser::FuncDecl>(m.decls[0]->kind);
    TEST_EQ(fd.name, std::string("add"), "func name");
    TEST_EQ(fd.params.size(), size_t(2), "param count");
    TEST_ASSERT(fd.return_type.has_value(), "has return type");
    TEST_ASSERT((*fd.return_type)->is<parser::NamedType>(), "return type is NamedType");
    TEST_EQ((*fd.return_type)->as<parser::NamedType>().path.segments[0], std::string("I32"),
            "return type");
    return true;
}

bool test_ast_source_span() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_location(10, 5, 100, 50);
    b.write_source_location(20, 15, 200, 0);
    b.write_varint(0);

    auto m = serial::read_ast(b.data(), "test.tml");
    TEST_EQ(m.span.start.line, uint32_t(10), "start line");
    TEST_EQ(m.span.start.column, uint32_t(5), "start column");
    TEST_EQ(m.span.start.offset, uint32_t(100), "start offset");
    TEST_EQ(m.span.start.length, uint32_t(50), "start length");
    TEST_EQ(m.span.end.line, uint32_t(20), "end line");
    TEST_EQ(m.span.end.column, uint32_t(15), "end column");
    TEST_EQ(std::string(m.span.start.file), std::string("test.tml"), "file_path injection");
    return true;
}

bool test_ast_invalid_magic() {
    BinaryBuilder b;
    b.write_u32_le(0xDEADBEEF);
    b.write_u8(1);
    b.write_u8(0);

    try {
        serial::read_ast(b.data(), "test.tml");
        TEST_ASSERT(false, "should have thrown");
    } catch (const std::runtime_error&) {
        // expected
    }
    return true;
}

bool test_ast_wrong_major_version() {
    BinaryBuilder b;
    b.write_u32_le(0x41535420);
    b.write_u8(99);
    b.write_u8(0);

    try {
        serial::read_ast(b.data(), "test.tml");
        TEST_ASSERT(false, "should have thrown");
    } catch (const std::runtime_error&) {
        // expected
    }
    return true;
}

bool test_ast_multiple_declarations() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("multi");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(2);

    // struct Color { r: I32 }
    b.write_u8(1);
    b.write_str("Color");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(1);
    b.write_str("r");
    b.write_named_type({"I32"});
    b.write_visibility(1);
    b.write_source_span();
    b.write_source_span();

    // func get_r(c: Color) -> I32
    b.write_u8(0);
    b.write_str("get_r");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(1);
    b.write_func_param("c", {"Color"});
    b.write_opt_type_named({"I32"});
    b.write_u8(0);
    b.write_bool(false);
    b.write_bool(false);
    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    TEST_EQ(m.decls.size(), size_t(2), "decl count");
    TEST_ASSERT(std::holds_alternative<parser::StructDecl>(m.decls[0]->kind), "first is struct");
    TEST_EQ(std::get<parser::StructDecl>(m.decls[0]->kind).name, std::string("Color"),
            "struct name");
    TEST_ASSERT(std::holds_alternative<parser::FuncDecl>(m.decls[1]->kind), "second is func");
    TEST_EQ(std::get<parser::FuncDecl>(m.decls[1]->kind).name, std::string("get_r"), "func name");
    return true;
}

bool test_ast_qualified_type_path() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(1);

    b.write_u8(1);
    b.write_str("Wrapper");
    b.write_visibility(0);
    b.write_empty_generic_params();
    b.write_varint(1);
    b.write_str("data");
    b.write_named_type({"std", "collections", "List"});
    b.write_visibility(0);
    b.write_source_span();
    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    auto& sd = std::get<parser::StructDecl>(m.decls[0]->kind);
    TEST_ASSERT(sd.vis == parser::Visibility::Private, "private visibility");

    auto& ft = sd.fields[0].type->as<parser::NamedType>();
    TEST_EQ(ft.path.segments.size(), size_t(3), "3 segments");
    TEST_EQ(ft.path.segments[0], std::string("std"), "seg[0]");
    TEST_EQ(ft.path.segments[1], std::string("collections"), "seg[1]");
    TEST_EQ(ft.path.segments[2], std::string("List"), "seg[2]");
    return true;
}

bool test_ast_reference_type() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(1);

    b.write_u8(1);
    b.write_str("R");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(1);
    b.write_str("val");
    b.write_u8(1); // Ref tag
    b.write_named_type({"I32"});
    b.write_bool(false);
    b.write_source_span();
    b.write_visibility(1);
    b.write_source_span();
    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    auto& sd = std::get<parser::StructDecl>(m.decls[0]->kind);
    TEST_ASSERT(sd.fields[0].type->is<parser::RefType>(), "is RefType");
    TEST_ASSERT(!sd.fields[0].type->as<parser::RefType>().is_mut, "not mut");
    return true;
}

bool test_ast_pointer_type() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(1);

    b.write_u8(1);
    b.write_str("P");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(1);
    b.write_str("p");
    b.write_u8(2); // Ptr tag
    b.write_named_type({"I32"});
    b.write_bool(true);
    b.write_source_span();
    b.write_visibility(1);
    b.write_source_span();
    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    auto& sd = std::get<parser::StructDecl>(m.decls[0]->kind);
    TEST_ASSERT(sd.fields[0].type->is<parser::PtrType>(), "is PtrType");
    TEST_ASSERT(sd.fields[0].type->as<parser::PtrType>().is_mut, "is mut");
    return true;
}

bool test_ast_tuple_type() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("test");
    b.write_varint(0);
    b.write_source_span();
    b.write_varint(1);

    b.write_u8(1);
    b.write_str("T");
    b.write_visibility(1);
    b.write_empty_generic_params();
    b.write_varint(1);
    b.write_str("t");
    b.write_u8(5); // Tuple tag
    b.write_varint(2);
    b.write_named_type({"I32"});
    b.write_named_type({"Bool"});
    b.write_source_span();
    b.write_visibility(1);
    b.write_source_span();
    b.write_source_span();

    auto m = serial::read_ast(b.data(), "test.tml");
    auto& sd = std::get<parser::StructDecl>(m.decls[0]->kind);
    TEST_ASSERT(sd.fields[0].type->is<parser::TupleType>(), "is TupleType");
    TEST_EQ(sd.fields[0].type->as<parser::TupleType>().elements.size(), size_t(2), "2 elements");
    return true;
}

// ============================================================================
// TypeEnv Reader Tests (5.3)
// ============================================================================

bool test_typeenv_empty() {
    BinaryBuilder b;
    b.write_typeenv_header();
    b.write_empty_typeenv_sections();

    auto env = serial::read_typeenv(b.data());
    TEST_ASSERT(!env.lookup_struct("NonExistent").has_value(), "no phantom structs");
    return true;
}

bool test_typeenv_invalid_magic() {
    BinaryBuilder b;
    b.write_u32_le(0xDEADBEEF);
    b.write_u8(1);
    b.write_u8(0);

    try {
        serial::read_typeenv(b.data());
        TEST_ASSERT(false, "should have thrown");
    } catch (const std::runtime_error&) {
        // expected
    }
    return true;
}

bool test_typeenv_single_struct() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(1);
    b.write_str("Point");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(1);
    b.write_str("x");
    b.write_sem_primitive(2); // I32
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);

    b.write_empty_typeenv_sections(9);

    auto env = serial::read_typeenv(b.data());
    auto sd = env.lookup_struct("Point");
    TEST_ASSERT(sd.has_value(), "Point found");
    TEST_EQ(sd->name, std::string("Point"), "name");
    TEST_EQ(sd->fields.size(), size_t(1), "1 field");
    TEST_EQ(sd->fields[0].name, std::string("x"), "field name");
    TEST_ASSERT(!sd->is_interior_mutable, "not interior_mutable");
    TEST_ASSERT(!sd->is_union, "not union");
    TEST_ASSERT(!sd->is_simd, "not simd");

    auto* pt = std::get_if<types::PrimitiveType>(&sd->fields[0].type->kind);
    TEST_ASSERT(pt != nullptr, "field type is primitive");
    TEST_ASSERT(pt->kind == types::PrimitiveKind::I32, "field type is I32");
    return true;
}

bool test_typeenv_multiple_structs() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(2);

    // Point { x: I32, y: I32 }
    b.write_str("Point");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(2);
    b.write_str("x");
    b.write_sem_primitive(2);
    b.write_bool(false);
    b.write_str("y");
    b.write_sem_primitive(2);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);

    // Color { r: U8, g: U8, b: U8 }
    b.write_str("Color");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(3);
    b.write_str("r");
    b.write_sem_primitive(5);
    b.write_bool(false);
    b.write_str("g");
    b.write_sem_primitive(5);
    b.write_bool(false);
    b.write_str("b");
    b.write_sem_primitive(5);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);

    b.write_empty_typeenv_sections(9);

    auto env = serial::read_typeenv(b.data());
    auto point = env.lookup_struct("Point");
    TEST_ASSERT(point.has_value(), "Point found");
    TEST_EQ(point->fields.size(), size_t(2), "Point has 2 fields");

    auto color = env.lookup_struct("Color");
    TEST_ASSERT(color.has_value(), "Color found");
    TEST_EQ(color->fields.size(), size_t(3), "Color has 3 fields");
    TEST_EQ(color->fields[0].name, std::string("r"), "first field name");
    return true;
}

bool test_typeenv_single_enum() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(0); // structs

    b.write_varint(1); // 1 enum
    b.write_str("Maybe");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(2);

    b.write_str("Just");
    b.write_varint(1);
    b.write_sem_primitive(2);

    b.write_str("Nothing");
    b.write_varint(0);

    b.write_bool(false);
    b.write_str("U32");
    b.write_varint(2);
    b.write_u64_le(0);
    b.write_u64_le(1);

    b.write_empty_typeenv_sections(8);

    auto env = serial::read_typeenv(b.data());
    auto ed = env.lookup_enum("Maybe");
    TEST_ASSERT(ed.has_value(), "Maybe found");
    TEST_EQ(ed->variants.size(), size_t(2), "2 variants");
    TEST_EQ(ed->variants[0].first, std::string("Just"), "variant[0] name");
    TEST_EQ(ed->variants[0].second.size(), size_t(1), "Just has 1 payload");
    TEST_EQ(ed->variants[1].first, std::string("Nothing"), "variant[1] name");
    TEST_ASSERT(ed->variants[1].second.empty(), "Nothing has no payload");
    TEST_ASSERT(!ed->is_flags, "not flags");
    TEST_EQ(ed->discriminant_values.size(), size_t(2), "2 discriminants");
    TEST_EQ(ed->discriminant_values[0], uint64_t(0), "disc[0]");
    TEST_EQ(ed->discriminant_values[1], uint64_t(1), "disc[1]");
    return true;
}

bool test_typeenv_single_function() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(0); // structs
    b.write_varint(0); // enums
    b.write_varint(0); // behaviors

    b.write_varint(1);                    // 1 function GROUP
    b.write_str("main");                  // group name
    b.write_varint(1);                    // 1 overload in this group
    b.write_func_sig_typeenv("main", 15); // the FuncSig

    b.write_empty_typeenv_sections(6);

    // Should not throw
    auto env = serial::read_typeenv(b.data());
    (void)env;
    return true;
}

bool test_typeenv_named_type_in_field() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(1);
    b.write_str("Wrapper");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(1);
    b.write_str("inner");
    b.write_sem_named("Point", "test");
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);

    b.write_empty_typeenv_sections(9);

    auto env = serial::read_typeenv(b.data());
    auto sd = env.lookup_struct("Wrapper");
    TEST_ASSERT(sd.has_value(), "Wrapper found");
    auto* nt = std::get_if<types::NamedType>(&sd->fields[0].type->kind);
    TEST_ASSERT(nt != nullptr, "field is NamedType");
    TEST_EQ(nt->name, std::string("Point"), "named type name");
    TEST_EQ(nt->module_path, std::string("test"), "named type module_path");
    return true;
}

bool test_typeenv_ref_and_ptr_types() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(1);
    b.write_str("Mixed");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(2);

    b.write_str("r");
    b.write_sem_ref(2, false); // ref I32
    b.write_bool(false);

    b.write_str("p");
    b.write_sem_ptr(3, true); // *mut I64
    b.write_bool(false);

    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);

    b.write_empty_typeenv_sections(9);

    auto env = serial::read_typeenv(b.data());
    auto sd = env.lookup_struct("Mixed");
    TEST_ASSERT(sd.has_value(), "Mixed found");
    TEST_EQ(sd->fields.size(), size_t(2), "2 fields");

    auto* rf = std::get_if<types::RefType>(&sd->fields[0].type->kind);
    TEST_ASSERT(rf != nullptr, "field[0] is RefType");
    TEST_ASSERT(!rf->is_mut, "ref not mut");

    auto* pf = std::get_if<types::PtrType>(&sd->fields[1].type->kind);
    TEST_ASSERT(pf != nullptr, "field[1] is PtrType");
    TEST_ASSERT(pf->is_mut, "ptr is mut");
    return true;
}

bool test_typeenv_struct_flags() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(2);

    // Cell (interior_mutable=true)
    b.write_str("Cell");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(1);
    b.write_str("value");
    b.write_sem_primitive(2);
    b.write_bool(false);
    b.write_bool(true);  // interior_mutable
    b.write_bool(false); // union
    b.write_bool(false); // simd

    // MyUnion (is_union=true)
    b.write_str("MyUnion");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(1);
    b.write_str("data");
    b.write_sem_primitive(3);
    b.write_bool(false);
    b.write_bool(false); // interior_mutable
    b.write_bool(true);  // union
    b.write_bool(false); // simd

    b.write_empty_typeenv_sections(9);

    auto env = serial::read_typeenv(b.data());

    auto cell = env.lookup_struct("Cell");
    TEST_ASSERT(cell.has_value(), "Cell found");
    TEST_ASSERT(cell->is_interior_mutable, "Cell is interior_mutable");
    TEST_ASSERT(!cell->is_union, "Cell not union");

    auto u = env.lookup_struct("MyUnion");
    TEST_ASSERT(u.has_value(), "MyUnion found");
    TEST_ASSERT(!u->is_interior_mutable, "MyUnion not interior_mutable");
    TEST_ASSERT(u->is_union, "MyUnion is union");
    return true;
}

bool test_typeenv_null_type() {
    BinaryBuilder b;
    b.write_typeenv_header();

    b.write_varint(1);
    b.write_str("NullField");
    b.write_varint(0);
    b.write_varint(0);
    b.write_varint(1);
    b.write_str("x");
    b.write_sem_null();
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);
    b.write_bool(false);

    b.write_empty_typeenv_sections(9);

    auto env = serial::read_typeenv(b.data());
    auto sd = env.lookup_struct("NullField");
    TEST_ASSERT(sd.has_value(), "NullField found");
    TEST_ASSERT(sd->fields[0].type == nullptr, "null type field is nullptr");
    return true;
}

bool test_typeenv_all_primitive_kinds() {
    struct PrimEntry {
        uint8_t idx;
        types::PrimitiveKind kind;
    };
    const PrimEntry prims[] = {
        {0, types::PrimitiveKind::I8},     {1, types::PrimitiveKind::I16},
        {2, types::PrimitiveKind::I32},    {3, types::PrimitiveKind::I64},
        {4, types::PrimitiveKind::I128},   {5, types::PrimitiveKind::U8},
        {6, types::PrimitiveKind::U16},    {7, types::PrimitiveKind::U32},
        {8, types::PrimitiveKind::U64},    {9, types::PrimitiveKind::U128},
        {10, types::PrimitiveKind::F32},   {11, types::PrimitiveKind::F64},
        {12, types::PrimitiveKind::Bool},  {13, types::PrimitiveKind::Char},
        {14, types::PrimitiveKind::Str},   {15, types::PrimitiveKind::Unit},
        {16, types::PrimitiveKind::Never},
    };

    for (const auto& p : prims) {
        BinaryBuilder b;
        b.write_typeenv_header();

        b.write_varint(1);
        b.write_str("PrimTest");
        b.write_varint(0);
        b.write_varint(0);
        b.write_varint(1);
        b.write_str("f");
        b.write_sem_primitive(p.idx);
        b.write_bool(false);
        b.write_bool(false);
        b.write_bool(false);
        b.write_bool(false);

        b.write_empty_typeenv_sections(9);

        auto env = serial::read_typeenv(b.data());
        auto sd = env.lookup_struct("PrimTest");
        TEST_ASSERT(sd.has_value(), "PrimTest found for idx");
        auto* pt = std::get_if<types::PrimitiveType>(&sd->fields[0].type->kind);
        TEST_ASSERT(pt != nullptr, "field is primitive");
        TEST_ASSERT(pt->kind == p.kind, "primitive kind matches");
    }
    return true;
}

// ============================================================================
// Query Integration Tests (5.4)
// ============================================================================

// Test a function with body: verifies deep recursive AST deserialization
// including Block → Return → Literal expression nesting.
bool test_ast_func_with_body() {
    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("compile_test"); // module name
    b.write_varint(0);           // doc_comments
    b.write_source_span();       // module span

    // 1 declaration: FuncDecl
    b.write_varint(1);
    b.write_u8(0); // DeclTag::Func

    // read_func_decl_body format:
    b.write_str("main");   // name
    b.write_visibility(1); // Public
    b.write_empty_generic_params();
    b.write_varint(0);               // 0 params
    b.write_opt_type_named({"I32"}); // return type

    // Body: u8(1) = present, then an Expr
    b.write_u8(1);

    // BlockExpr (tag=18): varint(stmts_count), stmts, opt_expr(trailing), span
    b.write_u8(18);    // ExprTag::Block
    b.write_varint(0); // 0 stmts

    // Trailing expr: u8(1)=present + a ReturnExpr
    b.write_u8(1);
    // ReturnExpr (tag=19): opt_expr(value), span
    b.write_u8(19);
    // opt_expr for return value: u8(1)=present + LiteralExpr
    b.write_u8(1);
    // LiteralExpr (tag=0): read_literal_kind + span
    b.write_u8(0); // ExprTag::Literal
    // LiteralKind::Int = 0: u8 tag + u64_le value
    b.write_u8(0);         // LitKind::Int
    b.write_u64_le(0);     // value = 0
    b.write_source_span(); // LiteralExpr span
    // (end of LiteralExpr)
    b.write_source_span(); // ReturnExpr span
    // (end of ReturnExpr as trailing)
    b.write_source_span(); // BlockExpr span

    b.write_bool(false);   // is_async
    b.write_bool(false);   // is_override
    b.write_source_span(); // FuncDecl span

    auto mod = serial::read_ast(b.data(), "compile_test.tml");

    TEST_EQ(mod.name, std::string("compile_test"), "module name");
    TEST_EQ(mod.decls.size(), size_t(1), "1 decl");

    auto* fd = std::get_if<parser::FuncDecl>(&mod.decls[0]->kind);
    TEST_ASSERT(fd != nullptr, "decl is FuncDecl");
    TEST_EQ(fd->name, std::string("main"), "func name");
    TEST_ASSERT(fd->return_type.has_value(), "has return type");
    TEST_ASSERT(fd->body.has_value(), "has body");

    // Verify body has trailing expression
    const auto& block = fd->body.value();
    TEST_ASSERT(block.expr.has_value(), "has trailing expr");

    auto* ret = std::get_if<parser::ReturnExpr>(&block.expr.value()->kind);
    TEST_ASSERT(ret != nullptr, "trailing is ReturnExpr");
    TEST_ASSERT(ret->value.has_value(), "return has value");

    auto* lit = std::get_if<parser::LiteralExpr>(&ret->value.value()->kind);
    TEST_ASSERT(lit != nullptr, "return value is LiteralExpr");

    return true;
}

// ============================================================================
// Performance Tests (6.1 + 6.2)
// ============================================================================

// Build a large AST binary blob with N struct declarations, each having M fields.
// Then time the deserialization.
bool test_perf_ast_large_module() {
    using Clock = std::chrono::high_resolution_clock;

    const int num_structs = 200;
    const int fields_per_struct = 10;

    BinaryBuilder b;
    b.write_ast_header();
    b.write_str("perf_module");
    b.write_varint(0);           // docs
    b.write_source_span();       // module span
    b.write_varint(num_structs); // decl count

    for (int s = 0; s < num_structs; s++) {
        std::string name = "Struct" + std::to_string(s);
        b.write_u8(1); // DeclTag::Struct
        b.write_str(name);
        b.write_visibility(1); // Public
        b.write_empty_generic_params();
        b.write_varint(fields_per_struct);
        for (int f = 0; f < fields_per_struct; f++) {
            std::string fname = "field_" + std::to_string(f);
            b.write_str(fname);
            b.write_named_type({"I64"});
            b.write_visibility(0);
            b.write_source_span();
        }
        b.write_source_span();
    }

    const auto& data = b.data();
    const size_t data_size = data.size();

    // Warm up
    serial::read_ast(data, "perf.tml");

    // Time 100 iterations
    const int iterations = 100;
    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        serial::read_ast(data, "perf.tml");
    }
    auto t1 = Clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::cout << "    AST: " << num_structs << " structs × " << fields_per_struct
              << " fields = " << data_size << " bytes, " << iterations << " reads in " << us
              << " µs (" << (us / iterations) << " µs/iter)\n";

    // Pass if deserialization takes < 10ms per iteration (generous bound)
    TEST_ASSERT(us / iterations < 10000, "AST deser < 10ms per iter");
    return true;
}

// Build a large TypeEnv binary blob with N struct definitions.
bool test_perf_typeenv_large() {
    using Clock = std::chrono::high_resolution_clock;

    const int num_structs = 200;
    const int fields_per_struct = 5;

    BinaryBuilder b;
    b.write_typeenv_header();
    b.write_varint(num_structs); // struct count

    for (int s = 0; s < num_structs; s++) {
        std::string name = "Type" + std::to_string(s);
        b.write_str(name);
        b.write_varint(0); // type_params
        b.write_varint(0); // constraints
        b.write_varint(fields_per_struct);
        for (int f = 0; f < fields_per_struct; f++) {
            std::string fname = "f" + std::to_string(f);
            b.write_str(fname);
            b.write_sem_primitive(2 + (f % 4)); // I32, I64, F32, F64
            b.write_bool(false);                // is_optional
        }
        b.write_bool(false); // interior_mutable
        b.write_bool(false); // is_union
        b.write_bool(false); // is_simd
    }

    b.write_varint(0); // enums
    b.write_varint(0); // behaviors
    b.write_varint(0); // functions
    b.write_empty_typeenv_sections(6);

    const auto& data = b.data();
    const size_t data_size = data.size();

    // Warm up
    serial::read_typeenv(data);

    // Time 100 iterations
    const int iterations = 100;
    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        serial::read_typeenv(data);
    }
    auto t1 = Clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::cout << "    TypeEnv: " << num_structs << " structs × " << fields_per_struct
              << " fields = " << data_size << " bytes, " << iterations << " reads in " << us
              << " µs (" << (us / iterations) << " µs/iter)\n";

    // Pass if deserialization takes < 10ms per iteration
    TEST_ASSERT(us / iterations < 10000, "TypeEnv deser < 10ms per iter");
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== AST Reader Tests (5.1 + 5.2) ===\n";
    RUN_TEST(test_ast_empty_module);
    RUN_TEST(test_ast_module_with_docs);
    RUN_TEST(test_ast_simple_struct);
    RUN_TEST(test_ast_simple_function);
    RUN_TEST(test_ast_source_span);
    RUN_TEST(test_ast_invalid_magic);
    RUN_TEST(test_ast_wrong_major_version);
    RUN_TEST(test_ast_multiple_declarations);
    RUN_TEST(test_ast_qualified_type_path);
    RUN_TEST(test_ast_reference_type);
    RUN_TEST(test_ast_pointer_type);
    RUN_TEST(test_ast_tuple_type);

    std::cout << "\n=== TypeEnv Reader Tests (5.3) ===\n";
    RUN_TEST(test_typeenv_empty);
    RUN_TEST(test_typeenv_invalid_magic);
    RUN_TEST(test_typeenv_single_struct);
    RUN_TEST(test_typeenv_multiple_structs);
    RUN_TEST(test_typeenv_single_enum);
    RUN_TEST(test_typeenv_single_function);
    RUN_TEST(test_typeenv_named_type_in_field);
    RUN_TEST(test_typeenv_ref_and_ptr_types);
    RUN_TEST(test_typeenv_struct_flags);
    RUN_TEST(test_typeenv_null_type);
    RUN_TEST(test_typeenv_all_primitive_kinds);

    std::cout << "\n=== Query Integration Tests (5.4) ===\n";
    RUN_TEST(test_ast_func_with_body);

    std::cout << "\n=== Performance Tests (6.1 + 6.2) ===\n";
    RUN_TEST(test_perf_ast_large_module);
    RUN_TEST(test_perf_typeenv_large);

    std::cout << "\n=== Results ===\n";
    std::cout << "  Total:  " << g_tests_run << "\n";
    std::cout << "  Passed: " << g_tests_passed << "\n";
    std::cout << "  Failed: " << g_tests_failed << "\n";

    return g_tests_failed > 0 ? 1 : 0;
}
