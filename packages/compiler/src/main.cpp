#include "tml/common.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include "tml/types/checker.hpp"
#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/format/formatter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <functional>
#ifndef _WIN32
#include <sys/wait.h>
#endif

using namespace tml;
namespace fs = std::filesystem;

// Helper to convert path to forward slashes (more portable for std::system on Windows)
std::string to_forward_slashes(const std::string& path) {
    std::string result = path;
    for (char& c : result) {
        if (c == '\\') c = '/';
    }
    return result;
}

#ifdef _WIN32
// MSVC detection result
struct MSVCInfo {
    std::string cl_path;           // Path to cl.exe
    std::vector<std::string> includes;  // Include paths
    std::vector<std::string> libs;      // Library paths
};

// Find Visual Studio and Windows SDK paths for cl.exe
MSVCInfo find_msvc() {
    MSVCInfo info;

    // Common MSVC installation paths
    std::vector<std::string> vs_bases = {
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2019/Professional/VC/Tools/MSVC",
        "C:/Program Files/Microsoft Visual Studio/2019/Enterprise/VC/Tools/MSVC",
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/MSVC",
    };

    // Find MSVC
    std::string msvc_base;
    std::string msvc_ver;
    for (const auto& vs_path : vs_bases) {
        if (fs::exists(vs_path)) {
            // Find newest version
            for (const auto& entry : fs::directory_iterator(vs_path)) {
                if (entry.is_directory()) {
                    std::string ver = entry.path().filename().string();
                    if (msvc_ver.empty() || ver > msvc_ver) {
                        msvc_ver = ver;
                        msvc_base = vs_path;
                    }
                }
            }
        }
    }

    if (!msvc_ver.empty()) {
        std::string msvc_path = msvc_base + "/" + msvc_ver;

        // Find cl.exe (prefer x64)
        std::string cl_x64 = msvc_path + "/bin/Hostx64/x64/cl.exe";
        std::string cl_x86 = msvc_path + "/bin/Hostx86/x86/cl.exe";
        if (fs::exists(cl_x64)) {
            info.cl_path = cl_x64;
        } else if (fs::exists(cl_x86)) {
            info.cl_path = cl_x86;
        }

        // Include path
        std::string inc = msvc_path + "/include";
        if (fs::exists(inc)) {
            info.includes.push_back(inc);
        }

        // Lib paths
        std::string lib_x64 = msvc_path + "/lib/x64";
        std::string lib_x86 = msvc_path + "/lib/x86";
        if (fs::exists(cl_x64) && fs::exists(lib_x64)) {
            info.libs.push_back(lib_x64);
        } else if (fs::exists(lib_x86)) {
            info.libs.push_back(lib_x86);
        }
    }

    // Find Windows SDK
    std::string sdk_base = "C:/Program Files (x86)/Windows Kits/10";
    if (fs::exists(sdk_base + "/Include")) {
        // Find newest version
        std::string sdk_ver;
        for (const auto& entry : fs::directory_iterator(sdk_base + "/Include")) {
            if (entry.is_directory()) {
                std::string ver = entry.path().filename().string();
                if (ver.find("10.") == 0 && (sdk_ver.empty() || ver > sdk_ver)) {
                    sdk_ver = ver;
                }
            }
        }
        if (!sdk_ver.empty()) {
            std::string inc_base = sdk_base + "/Include/" + sdk_ver;
            if (fs::exists(inc_base + "/ucrt")) info.includes.push_back(inc_base + "/ucrt");
            if (fs::exists(inc_base + "/shared")) info.includes.push_back(inc_base + "/shared");
            if (fs::exists(inc_base + "/um")) info.includes.push_back(inc_base + "/um");

            std::string lib_base = sdk_base + "/Lib/" + sdk_ver;
            bool use_x64 = info.cl_path.find("x64") != std::string::npos;
            std::string arch = use_x64 ? "x64" : "x86";
            if (fs::exists(lib_base + "/ucrt/" + arch)) info.libs.push_back(lib_base + "/ucrt/" + arch);
            if (fs::exists(lib_base + "/um/" + arch)) info.libs.push_back(lib_base + "/um/" + arch);
        }
    }

    return info;
}
#endif

// Pre-compile runtime to object file if needed (returns path to .obj/.o)
std::string ensure_runtime_compiled(const std::string& runtime_c_path, const std::string& clang, bool verbose) {
    fs::path c_path = runtime_c_path;
    fs::path obj_path = c_path.parent_path() / "tml_runtime";
#ifdef _WIN32
    obj_path += ".obj";
#else
    obj_path += ".o";
#endif

    // Check if object file exists and is newer than source
    bool needs_compile = !fs::exists(obj_path);
    if (!needs_compile) {
        auto c_time = fs::last_write_time(c_path);
        auto obj_time = fs::last_write_time(obj_path);
        needs_compile = (c_time > obj_time);
    }

    if (needs_compile) {
        if (verbose) {
            std::cout << "Pre-compiling runtime: " << c_path << "\n";
        }
        // Use -O3 -march=native for SIMD and aggressive optimizations
        std::string compile_cmd = clang + " -c -O3 -march=native -mtune=native -ffast-math -fomit-frame-pointer -funroll-loops -o \"" + to_forward_slashes(obj_path.string()) +
                                  "\" \"" + to_forward_slashes(c_path.string()) + "\"";
        int ret = std::system(compile_cmd.c_str());
        if (ret != 0) {
            // Fall back to compiling source directly
            return runtime_c_path;
        }
    }

    return to_forward_slashes(obj_path.string());
}

void print_usage() {
    std::cout << "TML Compiler " << VERSION << "\n\n";
    std::cout << "Usage: tml <command> [options] [files]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  build     Compile the project\n";
    std::cout << "  run       Build and run the project\n";
    std::cout << "  check     Type-check without generating code\n";
    std::cout << "  test      Run tests\n";
    std::cout << "  fmt       Format source files\n";
    std::cout << "  new       Create a new project\n";
    std::cout << "  lex       Tokenize a file (debug)\n";
    std::cout << "  parse     Parse a file (debug)\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --help, -h       Show this help\n";
    std::cout << "  --version, -V    Show version\n";
    std::cout << "  --release        Build with optimizations\n";
    std::cout << "  --verbose        Show detailed output\n";
}

void print_version() {
    std::cout << "tml " << VERSION << "\n";
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

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
            std::cout << "  " << token.span.start.line << ":"
                      << token.span.start.column << " "
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
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
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
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
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
                    if (i > 0) std::cout << ", ";
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

    // Lex
    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Type check
    types::TypeChecker checker;
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
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
        (void)env; // Will use later for more verbose output
    } else {
        std::cout << "check: " << path << " ok\n";
    }

    return 0;
}

int run_build(const std::string& path, bool verbose, bool emit_c_only) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Lex
    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Type check
    types::TypeChecker checker;
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Generate LLVM IR
    codegen::LLVMGenOptions options;
    options.emit_comments = verbose;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": codegen error: "
                      << error.message << "\n";
        }
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Output path - use absolute paths for reliability
    fs::path input_path = fs::absolute(path);
    fs::path ll_output = input_path.parent_path() / (module_name + ".ll");
    fs::path exe_output = input_path.parent_path() / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    // Write LLVM IR file
    std::ofstream ll_file(ll_output);
    if (!ll_file) {
        std::cerr << "error: Cannot write to " << ll_output << "\n";
        return 1;
    }
    ll_file << llvm_ir;
    ll_file.close();

    if (verbose) {
        std::cout << "Generated: " << ll_output << "\n";
    }

    if (emit_c_only) {  // Reuse flag for emit-ir-only
        std::cout << "build: " << ll_output << "\n";
        return 0;
    }

    // Compile LLVM IR with clang
    std::string clang = "clang";
#ifdef _WIN32
    // Try common LLVM installation paths
    std::vector<std::string> clang_paths = {
        "F:/LLVM/bin/clang.exe",
        "C:/Program Files/LLVM/bin/clang.exe",
        "C:/LLVM/bin/clang.exe",
    };
    for (const auto& p : clang_paths) {
        if (fs::exists(p)) {
            clang = p;
            break;
        }
    }
#endif

    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    // Find runtime - check known locations
    std::string build_runtime_path;
    std::vector<std::string> build_runtime_search = {
        "runtime/tml_runtime.c",
        "../runtime/tml_runtime.c",
        "../../runtime/tml_runtime.c",
        "F:/Node/hivellm/tml/packages/compiler/runtime/tml_runtime.c",
    };
    for (const auto& rp : build_runtime_search) {
        if (fs::exists(rp)) {
            build_runtime_path = to_forward_slashes(fs::absolute(rp).string());
            break;
        }
    }

    // On Windows, cmd.exe has issues with leading quotes - don't quote clang path
    // Use -O3 for aggressive LLVM optimizations, -march=native for SIMD
    // -flto for link-time optimization, -fomit-frame-pointer for faster calls
    std::string compile_cmd = clang + " -O3 -march=native -mtune=native -ffast-math -fomit-frame-pointer -funroll-loops -o \"" + exe_path + "\" \"" + ll_path + "\"";

    // Add runtime if found (use pre-compiled object if possible)
    if (!build_runtime_path.empty()) {
        std::string runtime_to_link = ensure_runtime_compiled(build_runtime_path, clang, verbose);
        compile_cmd += " \"" + runtime_to_link + "\"";
        if (verbose) {
            std::cout << "Including runtime: " << runtime_to_link << "\n";
        }
    }

    if (verbose) {
        std::cout << "Running: " << compile_cmd << "\n";
    }

    int ret = std::system(compile_cmd.c_str());
    if (ret != 0) {
        std::cerr << "error: LLVM compilation failed\n";
        return 1;
    }

    // Clean up .ll file
    fs::remove(ll_output);

    std::cout << "build: " << exe_path << "\n";
    return 0;
}

int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Lex
    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Type check
    types::TypeChecker checker;
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Generate LLVM IR
    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": codegen error: "
                      << error.message << "\n";
        }
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Create temp directory for build artifacts
    fs::path temp_dir = fs::temp_directory_path() / ("tml_run_" + std::to_string(std::hash<std::string>{}(path)));
    fs::create_directories(temp_dir);

    fs::path ll_output = temp_dir / (module_name + ".ll");
    fs::path exe_output = temp_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    // Write LLVM IR file
    std::ofstream ll_file(ll_output);
    if (!ll_file) {
        std::cerr << "error: Cannot write to " << ll_output << "\n";
        return 1;
    }
    ll_file << llvm_ir;
    ll_file.close();

    if (verbose) {
        std::cout << "Generated: " << ll_output << "\n";
    }

    // Find clang compiler
    std::string clang = "clang";
#ifdef _WIN32
    std::vector<std::string> clang_paths = {
        "F:/LLVM/bin/clang.exe",
        "C:/Program Files/LLVM/bin/clang.exe",
        "C:/LLVM/bin/clang.exe",
    };
    for (const auto& p : clang_paths) {
        if (fs::exists(p)) {
            clang = p;
            break;
        }
    }
#endif

    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        std::cerr << "error: clang not found.\n";
        std::cerr << "Please install LLVM/clang\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    if (verbose) {
        std::cout << "Using compiler: " << clang << "\n";
    }

    // Compile LLVM IR with clang
    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    // Find runtime - check known locations
    std::string runtime_path;
    std::vector<std::string> runtime_search_paths = {
        "runtime/tml_runtime.c",
        "../runtime/tml_runtime.c",
        "../../runtime/tml_runtime.c",
        "F:/Node/hivellm/tml/packages/compiler/runtime/tml_runtime.c",
    };
    for (const auto& rp : runtime_search_paths) {
        if (fs::exists(rp)) {
            runtime_path = to_forward_slashes(fs::absolute(rp).string());
            break;
        }
    }

    // On Windows, cmd.exe has issues with leading quotes - don't quote clang path
    // Use -O3 for aggressive LLVM optimizations, -march=native for SIMD
    // -flto for link-time optimization, -fomit-frame-pointer for faster calls
    std::string compile_cmd = clang + " -O3 -march=native -mtune=native -ffast-math -fomit-frame-pointer -funroll-loops -o \"" + exe_path + "\" \"" + ll_path + "\"";

    // Add runtime if found (use pre-compiled object if possible)
    if (!runtime_path.empty()) {
        std::string runtime_to_link = ensure_runtime_compiled(runtime_path, clang, verbose);
        compile_cmd += " \"" + runtime_to_link + "\"";
        if (verbose) {
            std::cout << "Including runtime: " << runtime_to_link << "\n";
        }
    }

    if (verbose) {
        std::cout << "Compiling: " << compile_cmd << "\n";
    }

    int compile_ret = std::system(compile_cmd.c_str());
    if (compile_ret != 0) {
        std::cerr << "error: LLVM compilation failed\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    // Build command with args
    std::string run_cmd = "\"" + exe_path + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }

    if (verbose) {
        std::cout << "Running: " << run_cmd << "\n";
    }

    // Execute
    int run_ret = std::system(run_cmd.c_str());

    // Cleanup temp files
    if (!verbose) {
        fs::remove_all(temp_dir);
    } else {
        std::cout << "Temp files kept at: " << temp_dir << "\n";
    }

    // Return the program's exit code
#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

int run_fmt(const std::string& path, bool check_only, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Lex
    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Format
    format::FormatOptions options;
    format::Formatter formatter(options);
    auto formatted = formatter.format(module);

    if (check_only) {
        // Check if file would change
        if (formatted != source_code) {
            std::cerr << path << " would be reformatted\n";
            return 1;
        }
        if (verbose) {
            std::cout << path << " is correctly formatted\n";
        }
        return 0;
    }

    // Write back to file
    std::ofstream out(path);
    if (!out) {
        std::cerr << "error: Cannot write to " << path << "\n";
        return 1;
    }
    out << formatted;
    out.close();

    if (verbose) {
        std::cout << "Formatted " << path << "\n";
    } else {
        std::cout << "fmt: " << path << "\n";
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];
    bool verbose = false;

    // Check for global flags
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v") {
            verbose = true;
        }
    }

    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    if (command == "--version" || command == "-V") {
        print_version();
        return 0;
    }

    if (command == "lex") {
        if (argc < 3) {
            std::cerr << "Usage: tml lex <file.tml> [--verbose]\n";
            return 1;
        }
        return run_lex(argv[2], verbose);
    }

    if (command == "parse") {
        if (argc < 3) {
            std::cerr << "Usage: tml parse <file.tml> [--verbose]\n";
            return 1;
        }
        return run_parse(argv[2], verbose);
    }

    if (command == "check") {
        if (argc < 3) {
            std::cerr << "Usage: tml check <file.tml> [--verbose]\n";
            return 1;
        }
        return run_check(argv[2], verbose);
    }

    if (command == "build") {
        if (argc < 3) {
            std::cerr << "Usage: tml build <file.tml> [--emit-ir] [--verbose]\n";
            return 1;
        }
        bool emit_ir_only = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--emit-ir" || std::string(argv[i]) == "--emit-c") {
                emit_ir_only = true;
            }
        }
        return run_build(argv[2], verbose, emit_ir_only);
    }

    if (command == "fmt") {
        if (argc < 3) {
            std::cerr << "Usage: tml fmt <file.tml> [--check] [--verbose]\n";
            return 1;
        }
        bool check_only = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--check") {
                check_only = true;
            }
        }
        return run_fmt(argv[2], check_only, verbose);
    }

    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Usage: tml run <file.tml> [args...] [--verbose]\n";
            return 1;
        }
        // Collect args to pass to the program
        std::vector<std::string> program_args;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg != "--verbose" && arg != "-v") {
                program_args.push_back(arg);
            }
        }
        return run_run(argv[2], program_args, verbose);
    }

    if (command == "test" || command == "new") {
        std::cerr << "Error: '" << command << "' command not yet implemented\n";
        std::cerr << "The compiler is still in early development.\n";
        return 1;
    }

    std::cerr << "Error: Unknown command '" << command << "'\n";
    std::cerr << "Run 'tml --help' for usage information.\n";
    return 1;
}
