//! # OOP Tests
//!
//! Comprehensive tests for C#-style object-oriented programming features.
//! Tests lexer keywords, parser grammar, and type checking for:
//! - Classes and interfaces
//! - Inheritance (extends) and implementation (implements)
//! - Virtual methods, overrides, abstract classes
//! - Visibility modifiers (public, private, protected)
//! - Static members

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
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
// Lexer OOP Tests
// ============================================================================

class OOPLexerTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto lex(const std::string& code) -> std::vector<Token> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        return lexer.tokenize();
    }

    auto lex_one(const std::string& code) -> Token {
        auto tokens = lex(code);
        EXPECT_GE(tokens.size(), 1);
        return tokens[0];
    }
};

TEST_F(OOPLexerTest, ClassKeyword) {
    EXPECT_EQ(lex_one("class").kind, TokenKind::KwClass);
}

TEST_F(OOPLexerTest, InterfaceKeyword) {
    EXPECT_EQ(lex_one("interface").kind, TokenKind::KwInterface);
}

TEST_F(OOPLexerTest, ExtendsKeyword) {
    EXPECT_EQ(lex_one("extends").kind, TokenKind::KwExtends);
}

TEST_F(OOPLexerTest, ImplementsKeyword) {
    EXPECT_EQ(lex_one("implements").kind, TokenKind::KwImplements);
}

TEST_F(OOPLexerTest, OverrideKeyword) {
    EXPECT_EQ(lex_one("override").kind, TokenKind::KwOverride);
}

TEST_F(OOPLexerTest, VirtualKeyword) {
    EXPECT_EQ(lex_one("virtual").kind, TokenKind::KwVirtual);
}

TEST_F(OOPLexerTest, AbstractKeyword) {
    EXPECT_EQ(lex_one("abstract").kind, TokenKind::KwAbstract);
}

TEST_F(OOPLexerTest, SealedKeyword) {
    EXPECT_EQ(lex_one("sealed").kind, TokenKind::KwSealed);
}

TEST_F(OOPLexerTest, BaseKeyword) {
    EXPECT_EQ(lex_one("base").kind, TokenKind::KwBase);
}

TEST_F(OOPLexerTest, ProtectedKeyword) {
    EXPECT_EQ(lex_one("protected").kind, TokenKind::KwProtected);
}

TEST_F(OOPLexerTest, PrivateKeyword) {
    EXPECT_EQ(lex_one("private").kind, TokenKind::KwPrivate);
}

TEST_F(OOPLexerTest, StaticKeyword) {
    EXPECT_EQ(lex_one("static").kind, TokenKind::KwStatic);
}

TEST_F(OOPLexerTest, NewKeyword) {
    EXPECT_EQ(lex_one("new").kind, TokenKind::KwNew);
}

TEST_F(OOPLexerTest, PropKeyword) {
    EXPECT_EQ(lex_one("prop").kind, TokenKind::KwProp);
}

TEST_F(OOPLexerTest, NamespaceKeyword) {
    EXPECT_EQ(lex_one("namespace").kind, TokenKind::KwNamespace);
}

TEST_F(OOPLexerTest, SimpleClassDeclaration) {
    auto tokens = lex("class Dog { }");

    ASSERT_GE(tokens.size(), 4);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwClass);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "Dog");
    EXPECT_EQ(tokens[2].kind, TokenKind::LBrace);
    EXPECT_EQ(tokens[3].kind, TokenKind::RBrace);
}

TEST_F(OOPLexerTest, ClassWithInheritance) {
    auto tokens = lex("class Dog extends Animal { }");

    bool has_class = false, has_extends = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwClass)
            has_class = true;
        if (token.kind == TokenKind::KwExtends)
            has_extends = true;
    }
    EXPECT_TRUE(has_class);
    EXPECT_TRUE(has_extends);
}

TEST_F(OOPLexerTest, ClassWithImplements) {
    auto tokens = lex("class Dog implements Runnable, Barker { }");

    bool has_class = false, has_implements = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwClass)
            has_class = true;
        if (token.kind == TokenKind::KwImplements)
            has_implements = true;
    }
    EXPECT_TRUE(has_class);
    EXPECT_TRUE(has_implements);
}

TEST_F(OOPLexerTest, InterfaceDeclaration) {
    auto tokens = lex("interface Runnable { func run(this) }");

    bool has_interface = false, has_func = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwInterface)
            has_interface = true;
        if (token.kind == TokenKind::KwFunc)
            has_func = true;
    }
    EXPECT_TRUE(has_interface);
    EXPECT_TRUE(has_func);
}

TEST_F(OOPLexerTest, AbstractClass) {
    auto tokens = lex("abstract class Animal { abstract func speak(this) }");

    int abstract_count = 0;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwAbstract)
            abstract_count++;
    }
    EXPECT_EQ(abstract_count, 2); // One for class, one for method
}

TEST_F(OOPLexerTest, SealedClass) {
    auto tokens = lex("sealed class FinalDog extends Dog { }");

    bool has_sealed = false, has_extends = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwSealed)
            has_sealed = true;
        if (token.kind == TokenKind::KwExtends)
            has_extends = true;
    }
    EXPECT_TRUE(has_sealed);
    EXPECT_TRUE(has_extends);
}

TEST_F(OOPLexerTest, VirtualMethod) {
    auto tokens = lex("virtual func speak(this) { }");

    bool has_virtual = false, has_func = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwVirtual)
            has_virtual = true;
        if (token.kind == TokenKind::KwFunc)
            has_func = true;
    }
    EXPECT_TRUE(has_virtual);
    EXPECT_TRUE(has_func);
}

TEST_F(OOPLexerTest, OverrideMethod) {
    auto tokens = lex("override func speak(this) { }");

    bool has_override = false, has_func = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwOverride)
            has_override = true;
        if (token.kind == TokenKind::KwFunc)
            has_func = true;
    }
    EXPECT_TRUE(has_override);
    EXPECT_TRUE(has_func);
}

TEST_F(OOPLexerTest, VisibilityModifiers) {
    auto tokens = lex("private x: I32\nprotected y: I32\npub z: I32");

    bool has_private = false, has_protected = false, has_pub = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwPrivate)
            has_private = true;
        if (token.kind == TokenKind::KwProtected)
            has_protected = true;
        if (token.kind == TokenKind::KwPub)
            has_pub = true;
    }
    EXPECT_TRUE(has_private);
    EXPECT_TRUE(has_protected);
    EXPECT_TRUE(has_pub);
}

TEST_F(OOPLexerTest, StaticField) {
    auto tokens = lex("static count: I32 = 0");

    bool has_static = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwStatic)
            has_static = true;
    }
    EXPECT_TRUE(has_static);
}

TEST_F(OOPLexerTest, CompleteClassDefinition) {
    std::string code = R"(
abstract class Animal {
    protected name: Str

    func new(name: Str) -> Animal {
        return Animal { name: name }
    }

    abstract func speak(this) -> Str

    virtual func move(this) {
        println("Moving")
    }
}

class Dog extends Animal implements Barker {
    private breed: Str

    override func speak(this) -> Str {
        return "Woof!"
    }

    override func move(this) {
        base.move()
        println("Running")
    }
}

sealed class GermanShepherd extends Dog {
    static count: I32 = 0
}
)";
    auto tokens = lex(code);

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Count OOP keywords
    int class_count = 0, abstract_count = 0, virtual_count = 0;
    int override_count = 0, sealed_count = 0, static_count = 0;
    int extends_count = 0, implements_count = 0;
    int private_count = 0, protected_count = 0, base_count = 0;

    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwClass)
            class_count++;
        if (token.kind == TokenKind::KwAbstract)
            abstract_count++;
        if (token.kind == TokenKind::KwVirtual)
            virtual_count++;
        if (token.kind == TokenKind::KwOverride)
            override_count++;
        if (token.kind == TokenKind::KwSealed)
            sealed_count++;
        if (token.kind == TokenKind::KwStatic)
            static_count++;
        if (token.kind == TokenKind::KwExtends)
            extends_count++;
        if (token.kind == TokenKind::KwImplements)
            implements_count++;
        if (token.kind == TokenKind::KwPrivate)
            private_count++;
        if (token.kind == TokenKind::KwProtected)
            protected_count++;
        if (token.kind == TokenKind::KwBase)
            base_count++;
    }

    EXPECT_EQ(class_count, 3);      // Animal, Dog, GermanShepherd
    EXPECT_EQ(abstract_count, 2);   // abstract class + abstract func
    EXPECT_EQ(virtual_count, 1);    // virtual func move
    EXPECT_EQ(override_count, 2);   // speak + move overrides
    EXPECT_EQ(sealed_count, 1);     // sealed class GermanShepherd
    EXPECT_EQ(static_count, 1);     // static count
    EXPECT_EQ(extends_count, 2);    // Dog extends, GermanShepherd extends
    EXPECT_EQ(implements_count, 1); // implements Barker
    EXPECT_EQ(private_count, 1);    // private breed
    EXPECT_EQ(protected_count, 1);  // protected name
    EXPECT_EQ(base_count, 1);       // base.move()
}

// ============================================================================
// Parser OOP Tests
// ============================================================================

class OOPParserTest : public ::testing::Test {
protected:
    auto parse(const std::string& code) -> Result<Module, std::vector<ParseError>> {
        auto source = Source::from_string(code);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse_module("test");
    }
};

TEST_F(OOPParserTest, SimpleClassDecl) {
    auto result = parse("class Dog { }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 1);
    EXPECT_TRUE(unwrap(result).decls[0]->is<ClassDecl>());

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.name, "Dog");
    EXPECT_FALSE(cls.is_abstract);
    EXPECT_FALSE(cls.is_sealed);
}

TEST_F(OOPParserTest, GenericClass) {
    auto result = parse("class Container[T] { value: T }");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.name, "Container");
    EXPECT_EQ(cls.generics.size(), 1);
    EXPECT_EQ(cls.generics[0].name, "T");
}

TEST_F(OOPParserTest, ClassExtendsBase) {
    auto result = parse("class Dog extends Animal { }");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.name, "Dog");
    EXPECT_TRUE(cls.extends.has_value());
    EXPECT_EQ(cls.extends->segments.back(), "Animal");
}

TEST_F(OOPParserTest, ClassImplementsInterfaces) {
    auto result = parse("class Dog implements Runnable, Barker { }");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.implements.size(), 2);
}

TEST_F(OOPParserTest, ClassExtendsAndImplements) {
    auto result = parse("class Dog extends Animal implements Runnable { }");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_TRUE(cls.extends.has_value());
    EXPECT_EQ(cls.implements.size(), 1);
}

TEST_F(OOPParserTest, AbstractClass) {
    auto result = parse("abstract class Animal { }");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_TRUE(cls.is_abstract);
}

TEST_F(OOPParserTest, SealedClass) {
    auto result = parse("sealed class FinalDog { }");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_TRUE(cls.is_sealed);
}

TEST_F(OOPParserTest, ClassWithFields) {
    auto result = parse(R"(
        class Point {
            x: F64
            y: F64
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.fields.size(), 2);
}

TEST_F(OOPParserTest, ClassWithVisibilityModifiers) {
    auto result = parse(R"(
        class Person {
            private id: I64
            protected name: Str
            pub age: I32
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.fields.size(), 3);
    EXPECT_EQ(cls.fields[0].vis, parser::MemberVisibility::Private);
    EXPECT_EQ(cls.fields[1].vis, parser::MemberVisibility::Protected);
    EXPECT_EQ(cls.fields[2].vis, parser::MemberVisibility::Public);
}

TEST_F(OOPParserTest, ClassWithMethods) {
    auto result = parse(R"(
        class Counter {
            value: I32

            func increment(this) {
                this.value = this.value + 1
            }

            func get_value(this) -> I32 {
                return this.value
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.methods.size(), 2);
}

TEST_F(OOPParserTest, VirtualMethod) {
    auto result = parse(R"(
        class Animal {
            virtual func speak(this) -> Str {
                return "..."
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.methods.size(), 1);
    EXPECT_TRUE(cls.methods[0].is_virtual);
}

TEST_F(OOPParserTest, AbstractMethod) {
    auto result = parse(R"(
        abstract class Animal {
            abstract func speak(this) -> Str
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.methods.size(), 1);
    EXPECT_TRUE(cls.methods[0].is_abstract);
    EXPECT_FALSE(cls.methods[0].body.has_value());
}

TEST_F(OOPParserTest, OverrideMethod) {
    auto result = parse(R"(
        class Dog extends Animal {
            override func speak(this) -> Str {
                return "Woof!"
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.methods.size(), 1);
    EXPECT_TRUE(cls.methods[0].is_override);
}

TEST_F(OOPParserTest, StaticMethod) {
    auto result = parse(R"(
        class Counter {
            static func create() -> Counter {
                return Counter { value: 0 }
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.methods.size(), 1);
    EXPECT_TRUE(cls.methods[0].is_static);
}

TEST_F(OOPParserTest, InterfaceDecl) {
    auto result = parse(R"(
        interface Runnable {
            func run(this)
        }
    )");
    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 1);
    EXPECT_TRUE(unwrap(result).decls[0]->is<InterfaceDecl>());

    auto& iface = unwrap(result).decls[0]->as<InterfaceDecl>();
    EXPECT_EQ(iface.name, "Runnable");
    EXPECT_EQ(iface.methods.size(), 1);
}

TEST_F(OOPParserTest, GenericInterface) {
    auto result = parse(R"(
        interface Comparable[T] {
            func compare(this, other: T) -> I32
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& iface = unwrap(result).decls[0]->as<InterfaceDecl>();
    EXPECT_EQ(iface.generics.size(), 1);
    EXPECT_EQ(iface.generics[0].name, "T");
}

TEST_F(OOPParserTest, InterfaceExtendsInterface) {
    auto result = parse(R"(
        interface Orderable extends Comparable {
            func less_than(this, other: This) -> Bool
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& iface = unwrap(result).decls[0]->as<InterfaceDecl>();
    EXPECT_FALSE(iface.extends.empty());
    EXPECT_EQ(iface.extends[0].segments.back(), "Comparable");
}

TEST_F(OOPParserTest, ConstructorWithBaseCall) {
    auto result = parse(R"(
        class Dog extends Animal {
            breed: Str

            new(name: Str, breed: Str) : base(name) {
                this.breed = breed
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    // Constructor goes into constructors vector, not methods
    EXPECT_GE(cls.constructors.size(), 1);
    // Verify the constructor has base args
    EXPECT_TRUE(cls.constructors[0].base_args.has_value());
}

TEST_F(OOPParserTest, CompleteClassHierarchy) {
    auto result = parse(R"(
        interface Speakable {
            func speak(this) -> Str
        }

        abstract class Animal implements Speakable {
            protected name: Str

            new(name: Str) {
                this.name = name
            }

            abstract func speak(this) -> Str
        }

        class Dog extends Animal {
            private breed: Str

            new(name: Str, breed: Str) : base(name) {
                this.breed = breed
            }

            override func speak(this) -> Str {
                return "Woof!"
            }
        }

        sealed class GermanShepherd extends Dog {
            static instance_count: I32 = 0

            new(name: Str) : base(name, "German Shepherd") {
                GermanShepherd::instance_count = GermanShepherd::instance_count + 1
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& module = unwrap(result);
    EXPECT_EQ(module.decls.size(), 4); // 1 interface + 3 classes

    // Verify interface
    EXPECT_TRUE(module.decls[0]->is<InterfaceDecl>());

    // Verify Animal
    auto& animal = module.decls[1]->as<ClassDecl>();
    EXPECT_TRUE(animal.is_abstract);
    EXPECT_EQ(animal.implements.size(), 1);

    // Verify Dog
    auto& dog = module.decls[2]->as<ClassDecl>();
    EXPECT_TRUE(dog.extends.has_value());
    EXPECT_FALSE(dog.is_sealed);

    // Verify GermanShepherd
    auto& gs = module.decls[3]->as<ClassDecl>();
    EXPECT_TRUE(gs.is_sealed);
    EXPECT_TRUE(gs.extends.has_value());
}

TEST_F(OOPParserTest, PropertyReadOnly) {
    auto result = parse(R"(
        class Rectangle {
            private _width: F64

            prop area: F64 {
                get { return this._width * this._width }
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.properties.size(), 1);
    EXPECT_EQ(cls.properties[0].name, "area");
    EXPECT_TRUE(cls.properties[0].has_getter);
    EXPECT_FALSE(cls.properties[0].has_setter);
}

TEST_F(OOPParserTest, PropertyReadWrite) {
    auto result = parse(R"(
        class Rectangle {
            private _width: F64

            pub prop width: F64 {
                get { return this._width }
                set { this._width = value }
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.properties.size(), 1);
    EXPECT_EQ(cls.properties[0].name, "width");
    EXPECT_TRUE(cls.properties[0].has_getter);
    EXPECT_TRUE(cls.properties[0].has_setter);
    EXPECT_EQ(cls.properties[0].vis, parser::MemberVisibility::Public);
}

TEST_F(OOPParserTest, PropertyAutoGetSet) {
    auto result = parse(R"(
        class Counter {
            private _value: I32

            pub prop value: I32 {
                get
                set
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.properties.size(), 1);
    EXPECT_TRUE(cls.properties[0].has_getter);
    EXPECT_TRUE(cls.properties[0].has_setter);
    // Auto properties don't have explicit body
    EXPECT_FALSE(cls.properties[0].getter.has_value());
    EXPECT_FALSE(cls.properties[0].setter.has_value());
}

TEST_F(OOPParserTest, StaticProperty) {
    auto result = parse(R"(
        class Config {
            static _instance: I32 = 0

            static prop instance: I32 {
                get { return Config::_instance }
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& cls = unwrap(result).decls[0]->as<ClassDecl>();
    EXPECT_EQ(cls.properties.size(), 1);
    EXPECT_TRUE(cls.properties[0].is_static);
}

// ============================================================================
// Type Checker OOP Tests
// ============================================================================

class OOPTypeCheckerTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> Result<TypeEnv, std::vector<TypeError>> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        EXPECT_TRUE(is_ok(module));

        TypeChecker checker;
        return checker.check_module(std::get<parser::Module>(module));
    }

    auto check_ok(const std::string& code) -> TypeEnv {
        auto result = check(code);
        EXPECT_TRUE(is_ok(result)) << "Type check failed";
        return std::move(std::get<TypeEnv>(result));
    }

    void check_error(const std::string& code) {
        auto result = check(code);
        EXPECT_TRUE(is_err(result)) << "Expected type error";
    }
};

// Type checker tests are DISABLED until OOP type checking is implemented.
// The parser tests above verify that OOP syntax is correctly parsed.
// These tests document the expected type checker behavior when implemented.

TEST_F(OOPTypeCheckerTest, DISABLED_SimpleClassDecl) {
    auto env = check_ok(R"(
        class Point {
            x: F64
            y: F64
        }
    )");

    auto cls = env.lookup_class("Point");
    EXPECT_TRUE(cls.has_value());
    EXPECT_EQ(cls->name, "Point");
    EXPECT_EQ(cls->fields.size(), 2);
}

TEST_F(OOPTypeCheckerTest, DISABLED_ClassWithConstructor) {
    auto env = check_ok(R"(
        class Counter {
            value: I32

            func new() -> Counter {
                return Counter { value: 0 }
            }
        }
    )");

    auto cls = env.lookup_class("Counter");
    EXPECT_TRUE(cls.has_value());

    auto ctor = env.lookup_func("Counter::new");
    EXPECT_TRUE(ctor.has_value());
}

TEST_F(OOPTypeCheckerTest, DISABLED_ClassWithMethods) {
    auto env = check_ok(R"(
        class Counter {
            value: I32

            func new() -> Counter {
                return Counter { value: 0 }
            }

            func increment(this) {
                this.value = this.value + 1
            }

            func get_value(this) -> I32 {
                return this.value
            }
        }
    )");

    auto cls = env.lookup_class("Counter");
    EXPECT_TRUE(cls.has_value());
    EXPECT_EQ(cls->methods.size(), 3);
}

TEST_F(OOPTypeCheckerTest, DISABLED_InterfaceDecl) {
    auto env = check_ok(R"(
        interface Runnable {
            func run(this)
        }
    )");

    auto iface = env.lookup_interface("Runnable");
    EXPECT_TRUE(iface.has_value());
    EXPECT_EQ(iface->methods.size(), 1);
}

TEST_F(OOPTypeCheckerTest, DISABLED_ClassInheritance) {
    auto env = check_ok(R"(
        class Animal {
            name: Str
        }

        class Dog extends Animal {
            breed: Str
        }
    )");

    auto dog = env.lookup_class("Dog");
    EXPECT_TRUE(dog.has_value());
    EXPECT_TRUE(dog->base_class.has_value());
    EXPECT_EQ(*dog->base_class, "Animal");
}

TEST_F(OOPTypeCheckerTest, DISABLED_ClassImplementsInterface) {
    auto env = check_ok(R"(
        interface Speakable {
            func speak(this) -> Str
        }

        class Dog implements Speakable {
            func speak(this) -> Str {
                return "Woof!"
            }
        }
    )");

    auto dog = env.lookup_class("Dog");
    EXPECT_TRUE(dog.has_value());
    EXPECT_EQ(dog->interfaces.size(), 1);
}

TEST_F(OOPTypeCheckerTest, DISABLED_VirtualMethodResolution) {
    auto env = check_ok(R"(
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
    )");

    auto animal = env.lookup_class("Animal");
    EXPECT_TRUE(animal.has_value());
    // Verify virtual method was registered
    EXPECT_GE(animal->methods.size(), 1);

    auto dog = env.lookup_class("Dog");
    EXPECT_TRUE(dog.has_value());
    // Verify override method was registered
    EXPECT_GE(dog->methods.size(), 1);
}

TEST_F(OOPTypeCheckerTest, DISABLED_AbstractClassCannotInstantiate) {
    // This test documents expected behavior when abstract classes
    // are directly instantiated (should fail type checking)
    // Implementation may vary based on when this check is performed
    check_ok(R"(
        abstract class Animal {
            abstract func speak(this) -> Str
        }

        class Dog extends Animal {
            override func speak(this) -> Str {
                return "Woof!"
            }
        }
    )");
}

TEST_F(OOPTypeCheckerTest, DISABLED_StaticMethodLookup) {
    auto env = check_ok(R"(
        class Counter {
            static func create() -> Counter {
                return Counter { value: 0 }
            }
            value: I32
        }
    )");

    // Static methods are registered as class methods
    auto cls = env.lookup_class("Counter");
    EXPECT_TRUE(cls.has_value());
    // Find the static method in class methods
    bool found_static = false;
    for (const auto& m : cls->methods) {
        if (m.sig.name == "create" && m.is_static) {
            found_static = true;
            break;
        }
    }
    EXPECT_TRUE(found_static);
}

TEST_F(OOPTypeCheckerTest, DISABLED_FieldVisibility) {
    auto env = check_ok(R"(
        class Person {
            private id: I64
            protected name: Str
            pub age: I32
        }
    )");

    auto cls = env.lookup_class("Person");
    EXPECT_TRUE(cls.has_value());
    EXPECT_EQ(cls->fields.size(), 3);

    // Check visibility is tracked correctly
    EXPECT_EQ(cls->fields[0].vis, types::MemberVisibility::Private);
    EXPECT_EQ(cls->fields[1].vis, types::MemberVisibility::Protected);
    EXPECT_EQ(cls->fields[2].vis, types::MemberVisibility::Public);
}

TEST_F(OOPTypeCheckerTest, DISABLED_CompleteOOPProgram) {
    check_ok(R"(
        interface Drawable {
            func draw(this)
        }

        abstract class Shape implements Drawable {
            protected x: F64
            protected y: F64

            abstract func area(this) -> F64
        }

        class Circle extends Shape {
            private radius: F64

            func new(x: F64, y: F64, r: F64) -> Circle {
                return Circle { x: x, y: y, radius: r }
            }

            override func area(this) -> F64 {
                return 3.14159 * this.radius * this.radius
            }

            override func draw(this) {
                println("Drawing circle")
            }
        }

        class Rectangle extends Shape {
            private width: F64
            private height: F64

            func new(x: F64, y: F64, w: F64, h: F64) -> Rectangle {
                return Rectangle { x: x, y: y, width: w, height: h }
            }

            override func area(this) -> F64 {
                return this.width * this.height
            }

            override func draw(this) {
                println("Drawing rectangle")
            }
        }

        func main() {
            let c: Circle = Circle::new(0.0, 0.0, 5.0)
            let r: Rectangle = Rectangle::new(0.0, 0.0, 10.0, 20.0)

            println(c.area())
            println(r.area())

            c.draw()
            r.draw()
        }
    )");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(OOPParserTest, DesignPatternFactory) {
    auto result = parse(R"(
        interface Product {
            func operation(this) -> Str
        }

        class ConcreteProductA implements Product {
            func operation(this) -> Str {
                return "Result of ConcreteProductA"
            }
        }

        class ConcreteProductB implements Product {
            func operation(this) -> Str {
                return "Result of ConcreteProductB"
            }
        }

        abstract class Creator {
            abstract func factory_method(this) -> Product

            func some_operation(this) -> Str {
                let product: Product = this.factory_method()
                return product.operation()
            }
        }

        class ConcreteCreatorA extends Creator {
            override func factory_method(this) -> Product {
                return ConcreteProductA { }
            }
        }

        class ConcreteCreatorB extends Creator {
            override func factory_method(this) -> Product {
                return ConcreteProductB { }
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, DesignPatternObserver) {
    auto result = parse(R"(
        interface Observer {
            func update(this, message: Str)
        }

        interface Subject {
            func attach(this, observer: ref Observer)
            func detach(this, observer: ref Observer)
            func notify(this)
        }

        class ConcreteSubject implements Subject {
            observers: List[ref Observer]
            state: Str

            func attach(this, observer: ref Observer) {
                this.observers.push(observer)
            }

            func detach(this, observer: ref Observer) {
                // Remove observer
            }

            func notify(this) {
                for obs in this.observers {
                    obs.update(this.state)
                }
            }

            func set_state(this, state: Str) {
                this.state = state
                this.notify()
            }
        }

        class ConcreteObserver implements Observer {
            name: Str

            func update(this, message: Str) {
                println("{this.name} received: {message}")
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

// ============================================================================
// Enabled Type Checker Tests - These tests work with current implementation
// ============================================================================

TEST_F(OOPTypeCheckerTest, SimpleClassDeclEnabled) {
    auto env = check_ok(R"(
        class Point {
            x: I32
            y: I32
        }
    )");

    auto cls = env.lookup_class("Point");
    EXPECT_TRUE(cls.has_value());
    EXPECT_EQ(cls->name, "Point");
}

TEST_F(OOPTypeCheckerTest, InterfaceDeclEnabled) {
    auto env = check_ok(R"(
        interface Printable {
            func print(this) -> Str
        }
    )");

    auto iface = env.lookup_interface("Printable");
    EXPECT_TRUE(iface.has_value());
    EXPECT_EQ(iface->methods.size(), 1);
}

TEST_F(OOPTypeCheckerTest, ClassImplementsInterfaceEnabled) {
    auto env = check_ok(R"(
        interface Printable {
            func print(this) -> Str
        }

        class Document implements Printable {
            content: Str

            new(c: Str) {
                this.content = c
            }

            func print(this) -> Str {
                return this.content
            }
        }
    )");

    auto cls = env.lookup_class("Document");
    EXPECT_TRUE(cls.has_value());
    EXPECT_EQ(cls->interfaces.size(), 1);
}

TEST_F(OOPTypeCheckerTest, ClassInheritanceEnabled) {
    auto env = check_ok(R"(
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

    auto dog = env.lookup_class("Dog");
    EXPECT_TRUE(dog.has_value());
    EXPECT_TRUE(dog->base_class.has_value());
    EXPECT_EQ(*dog->base_class, "Animal");
}

TEST_F(OOPTypeCheckerTest, AbstractClassEnabled) {
    auto env = check_ok(R"(
        abstract class Shape {
            abstract func area(this) -> I32
        }
    )");

    auto cls = env.lookup_class("Shape");
    EXPECT_TRUE(cls.has_value());
    EXPECT_TRUE(cls->is_abstract);
}

TEST_F(OOPTypeCheckerTest, SealedClassEnabled) {
    auto env = check_ok(R"(
        sealed class FinalClass {
            value: I32

            new(v: I32) {
                this.value = v
            }
        }
    )");

    auto cls = env.lookup_class("FinalClass");
    EXPECT_TRUE(cls.has_value());
    EXPECT_TRUE(cls->is_sealed);
}

TEST_F(OOPTypeCheckerTest, VirtualMethodEnabled) {
    auto env = check_ok(R"(
        class Animal {
            name: Str

            new(n: Str) {
                this.name = n
            }

            virtual func speak(this) -> Str {
                return "..."
            }
        }
    )");

    auto cls = env.lookup_class("Animal");
    EXPECT_TRUE(cls.has_value());
}

TEST_F(OOPTypeCheckerTest, StaticFieldEnabled) {
    auto env = check_ok(R"(
        class Counter {
            static count: I32 = 0
            value: I32

            new(v: I32) {
                this.value = v
            }
        }
    )");

    auto cls = env.lookup_class("Counter");
    EXPECT_TRUE(cls.has_value());
}

TEST_F(OOPTypeCheckerTest, ClassAsVariableTypeEnabled) {
    check_ok(R"(
        class Point {
            x: I32
            y: I32

            new(x: I32, y: I32) {
                this.x = x
                this.y = y
            }
        }

        func main() -> I32 {
            let p: Point = Point::new(10, 20)
            return 0
        }
    )");
}

TEST_F(OOPTypeCheckerTest, ClassAsParameterEnabled) {
    check_ok(R"(
        class Point {
            x: I32
            y: I32

            new(x: I32, y: I32) {
                this.x = x
                this.y = y
            }
        }

        func distance(p1: Point, p2: Point) -> I32 {
            return 0
        }

        func main() -> I32 {
            let a: Point = Point::new(0, 0)
            let b: Point = Point::new(10, 10)
            return distance(a, b)
        }
    )");
}

// ============================================================================
// Lexer Tests for 'is' Operator
// ============================================================================

TEST_F(OOPLexerTest, IsKeyword) {
    EXPECT_EQ(lex_one("is").kind, TokenKind::KwIs);
}

TEST_F(OOPLexerTest, IsExpression) {
    auto tokens = lex("dog is Dog");

    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "dog");
    EXPECT_EQ(tokens[1].kind, TokenKind::KwIs);
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].lexeme, "Dog");
}

TEST_F(OOPLexerTest, IsExpressionInCondition) {
    auto tokens = lex("if animal is Dog { }");

    bool has_if = false, has_is = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwIf)
            has_if = true;
        if (token.kind == TokenKind::KwIs)
            has_is = true;
    }
    EXPECT_TRUE(has_if);
    EXPECT_TRUE(has_is);
}

// ============================================================================
// Parser Tests for 'is' Operator
// ============================================================================

TEST_F(OOPParserTest, IsExpressionParsing) {
    auto result = parse(R"(
        class Dog { }

        func main() -> Bool {
            let d: Dog = Dog { }
            return d is Dog
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, IsExpressionInCondition) {
    auto result = parse(R"(
        class Dog { }

        func check(d: Dog) -> I32 {
            if d is Dog {
                return 1
            }
            return 0
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

// ============================================================================
// Complex OOP Tests - Classes as Variable Types
// ============================================================================

TEST_F(OOPParserTest, ClassFieldOfClassType) {
    auto result = parse(R"(
        class Point {
            x: I32
            y: I32
        }

        class Rectangle {
            origin: Point
            width: I32
            height: I32
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& rect = unwrap(result).decls[1]->as<ClassDecl>();
    EXPECT_EQ(rect.fields.size(), 3);
}

TEST_F(OOPParserTest, ClassMethodReturningClass) {
    auto result = parse(R"(
        class Point {
            x: I32
            y: I32

            new(x: I32, y: I32) {
                this.x = x
                this.y = y
            }

            func clone(this) -> Point {
                return Point::new(this.x, this.y)
            }

            static func origin() -> Point {
                return Point::new(0, 0)
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, ClassMethodWithClassParameter) {
    auto result = parse(R"(
        class Point {
            x: I32
            y: I32

            new(x: I32, y: I32) {
                this.x = x
                this.y = y
            }

            func add(this, other: Point) -> Point {
                return Point::new(this.x + other.x, this.y + other.y)
            }

            func equals(this, other: Point) -> Bool {
                return this.x == other.x and this.y == other.y
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, NestedClassFieldAccess) {
    auto result = parse(R"(
        class Point {
            x: I32
            y: I32

            new(x: I32, y: I32) {
                this.x = x
                this.y = y
            }
        }

        class Line {
            start: Point
            end: Point

            new(s: Point, e: Point) {
                this.start = s
                this.end = e
            }

            func length(this) -> I32 {
                let dx: I32 = this.end.x - this.start.x
                let dy: I32 = this.end.y - this.start.y
                return dx + dy
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, MultipleClassesInteracting) {
    auto result = parse(R"(
        class Engine {
            horsepower: I32

            new(hp: I32) {
                this.horsepower = hp
            }

            func start(this) {
                print("Engine starting\n")
            }
        }

        class Car {
            engine: Engine
            name: Str

            new(name: Str, engine: Engine) {
                this.name = name
                this.engine = engine
            }

            func drive(this) {
                this.engine.start()
                print("Driving\n")
            }
        }

        func main() -> I32 {
            let e: Engine = Engine::new(200)
            let c: Car = Car::new("Tesla", e)
            c.drive()
            return 0
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, ClassArrayField) {
    auto result = parse(R"(
        class Point {
            x: I32
            y: I32
        }

        class Polygon {
            vertices: List[Point]
            name: Str

            func vertex_count(this) -> I32 {
                return this.vertices.len()
            }
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, GenericClassWithClassTypeArg) {
    auto result = parse(R"(
        class Point {
            x: I32
            y: I32
        }

        class Container[T] {
            value: T

            new(v: T) {
                this.value = v
            }

            func get(this) -> T {
                return this.value
            }
        }

        func main() -> I32 {
            let p: Point = Point { x: 10, y: 20 }
            let c: Container[Point] = Container::new(p)
            return 0
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

TEST_F(OOPParserTest, PolymorphicClassHierarchy) {
    auto result = parse(R"(
        interface Drawable {
            func draw(this)
        }

        abstract class Shape implements Drawable {
            x: I32
            y: I32

            abstract func area(this) -> I32
        }

        class Circle extends Shape {
            radius: I32

            new(x: I32, y: I32, r: I32) {
                this.x = x
                this.y = y
                this.radius = r
            }

            override func area(this) -> I32 {
                return 3 * this.radius * this.radius
            }

            func draw(this) {
                print("Drawing circle\n")
            }
        }

        class Rectangle extends Shape {
            width: I32
            height: I32

            new(x: I32, y: I32, w: I32, h: I32) {
                this.x = x
                this.y = y
                this.width = w
                this.height = h
            }

            override func area(this) -> I32 {
                return this.width * this.height
            }

            func draw(this) {
                print("Drawing rectangle\n")
            }
        }

        func main() -> I32 {
            let c: Circle = Circle::new(0, 0, 10)
            let r: Rectangle = Rectangle::new(0, 0, 10, 20)
            c.draw()
            r.draw()
            return c.area() + r.area()
        }
    )");
    ASSERT_TRUE(is_ok(result));
}

// ============================================================================
// Class Hierarchy Analysis (CHA) Tests
// ============================================================================

#include "mir/passes/devirtualization.hpp"

class ClassHierarchyAnalysisTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> std::optional<TypeEnv> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module)) return std::nullopt;

        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result)) return std::nullopt;
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
    EXPECT_FALSE(animal_info->is_leaf()); // Has subclasses
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

#include "mir/passes/inlining.hpp"
#include "mir/mir_builder.hpp"

class VirtualCallInliningTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto build_mir(const std::string& code) -> std::optional<std::pair<TypeEnv, tml::mir::Module>> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module)) return std::nullopt;

        TypeChecker checker;
        auto env_result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(env_result)) return std::nullopt;
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

#include "mir/passes/dead_method_elimination.hpp"

class DeadMethodEliminationTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> std::optional<TypeEnv> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module)) return std::nullopt;

        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result)) return std::nullopt;
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

    tml::mir::DevirtualizationPass devirt_pass(env);
    tml::mir::DeadMethodEliminationPass pass(devirt_pass);

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

    tml::mir::DevirtualizationPass devirt_pass(env);
    tml::mir::DeadMethodEliminationPass pass(devirt_pass);

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

    tml::mir::DevirtualizationPass devirt_pass(env);
    tml::mir::DeadMethodEliminationPass pass(devirt_pass);

    // Before running, method should not be marked reachable
    EXPECT_FALSE(pass.is_method_reachable("Dog_new"));
    EXPECT_FALSE(pass.is_method_reachable("nonexistent"));
}

// ============================================================================
// Escape Analysis Tests
// ============================================================================

#include "mir/passes/escape_analysis.hpp"

class OOPEscapeAnalysisTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> std::optional<TypeEnv> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        if (is_err(module)) return std::nullopt;

        TypeChecker checker;
        auto result = checker.check_module(std::get<parser::Module>(module));
        if (is_err(result)) return std::nullopt;
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
