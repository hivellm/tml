#include "cmd_debug.hpp"

#include "tml/common.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include "tml/types/checker.hpp"
#include "tml/types/module.hpp"
#include "utils.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

int run_lex(const std::string& path, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (verbose) {
        std::cout << "Tokens (" << tokens.size() << "):\n";
        for (const auto& token : tokens) {
            std::cout << "  " << token.span.start.line << ":" << token.span.start.column << " "
                      << lexer::token_kind_to_string(token.kind);
            if (token.kind == lexer::TokenKind::Identifier ||
                token.kind == lexer::TokenKind::IntLiteral ||
                token.kind == lexer::TokenKind::FloatLiteral ||
                token.kind == lexer::TokenKind::StringLiteral) {
                std::cout << " `" << token.lexeme << "`";
            }
            std::cout << "\n";
        }
    }

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":" << error.span.start.column
                      << ": error: " << error.message << "\n";
        }
        return 1;
    }

    if (!verbose) {
        std::cout << "Lexed " << tokens.size() << " tokens from " << path << "\n";
    }
    return 0;
}

int run_parse(const std::string& path, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":" << error.span.start.column
                      << ": error: " << error.message << "\n";
        }
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":" << error.span.start.column
                      << ": error: " << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(result);

    if (verbose) {
        std::cout << "Module: " << module.name << "\n";
        std::cout << "Declarations: " << module.decls.size() << "\n";
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                std::cout << "  func " << func.name << "(";
                for (size_t i = 0; i < func.params.size(); ++i) {
                    if (i > 0)
                        std::cout << ", ";
                    const auto& param = func.params[i];
                    if (param.pattern && param.pattern->is<parser::IdentPattern>()) {
                        std::cout << param.pattern->as<parser::IdentPattern>().name;
                    } else {
                        std::cout << "_";
                    }
                }
                std::cout << ")\n";
            } else if (decl->is<parser::StructDecl>()) {
                const auto& s = decl->as<parser::StructDecl>();
                std::cout << "  type " << s.name << " { ... }\n";
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& e = decl->as<parser::EnumDecl>();
                std::cout << "  type " << e.name << " = ...\n";
            } else if (decl->is<parser::TraitDecl>()) {
                const auto& t = decl->as<parser::TraitDecl>();
                std::cout << "  behavior " << t.name << " { ... }\n";
            } else if (decl->is<parser::ImplDecl>()) {
                std::cout << "  impl ...\n";
            }
        }
    } else {
        std::cout << "Parsed " << module.decls.size() << " declarations from " << path << "\n";
    }

    return 0;
}

int run_check(const std::string& path, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":" << error.span.start.column
                      << ": error: " << error.message << "\n";
        }
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":" << error.span.start.column
                      << ": error: " << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Initialize module registry for test module
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":" << error.span.start.column
                      << ": error: " << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    if (verbose) {
        const auto& env = std::get<types::TypeEnv>(check_result);
        std::cout << "Type check passed for " << path << "\n";
        std::cout << "Module: " << module.name << "\n";
        std::cout << "Declarations: " << module.decls.size() << "\n";
        (void)env;
    } else {
        std::cout << "check: " << path << " ok\n";
    }

    return 0;
}

} // namespace tml::cli
