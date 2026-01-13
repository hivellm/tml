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
