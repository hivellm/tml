#include <algorithm>
#include <cctype>
#include <sstream>
#include <tml/codegen/c_header_gen.hpp>

namespace tml::codegen {

CHeaderGen::CHeaderGen(const types::TypeEnv& env, CHeaderGenOptions options)
    : env_(env), options_(std::move(options)) {}

std::string CHeaderGen::gen_guard_name(const std::string& module_name) {
    std::string guard =
        options_.guard_prefix.empty() ? "TML_" + module_name + "_H" : options_.guard_prefix + "_H";

    // Convert to uppercase and replace invalid characters
    std::transform(guard.begin(), guard.end(), guard.begin(), [](unsigned char c) -> char {
        return std::isalnum(c) ? static_cast<char>(std::toupper(c)) : '_';
    });

    return guard;
}

std::string CHeaderGen::map_type_to_c(const parser::TypePtr& type) {
    if (!type)
        return "void";

    // Handle named types (I32, U32, F64, etc.)
    if (auto named = std::get_if<parser::NamedType>(&type->kind)) {
        if (named->path.segments.size() == 1) {
            const std::string& type_name = named->path.segments[0];

            // Map primitive types
            if (type_name == "I8")
                return "int8_t";
            if (type_name == "I16")
                return "int16_t";
            if (type_name == "I32")
                return "int32_t";
            if (type_name == "I64")
                return "int64_t";
            if (type_name == "U8")
                return "uint8_t";
            if (type_name == "U16")
                return "uint16_t";
            if (type_name == "U32")
                return "uint32_t";
            if (type_name == "U64")
                return "uint64_t";
            if (type_name == "F32")
                return "float";
            if (type_name == "F64")
                return "double";
            if (type_name == "Bool")
                return "bool";
            if (type_name == "Str")
                return "const char*";

            // Return the name as-is for custom types
            return type_name;
        }
    }

    // Handle reference types (ref T -> T*)
    if (auto ref = std::get_if<parser::RefType>(&type->kind)) {
        std::string inner = map_type_to_c(ref->inner);
        return inner + "*";
    }

    // Handle pointer types (*T -> T*)
    if (auto ptr = std::get_if<parser::PtrType>(&type->kind)) {
        std::string inner = map_type_to_c(ptr->inner);
        return inner + "*";
    }

    // Unsupported types return void*
    return "void*";
}

// Note: We don't need the types::TypePtr overload for C header generation
// since we work with the parser AST directly, not the type-checked types

std::string CHeaderGen::gen_func_decl(const parser::FuncDecl& func) {
    // Skip non-public functions
    if (func.vis != parser::Visibility::Public) {
        return "";
    }

    // Map return type
    std::string ret_type = "void";
    if (func.return_type.has_value()) {
        ret_type = map_type_to_c(*func.return_type);
    }

    // Build parameter list
    std::ostringstream params;
    if (func.params.empty()) {
        params << "void";
    } else {
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (i > 0)
                params << ", ";
            std::string param_type = map_type_to_c(func.params[i].type);

            // Extract parameter name from pattern
            std::string param_name = "arg" + std::to_string(i);
            if (auto ident = std::get_if<parser::IdentPattern>(&func.params[i].pattern->kind)) {
                param_name = ident->name;
            }

            params << param_type << " " << param_name;
        }
    }

    // Generate function declaration with tml_ prefix
    std::ostringstream decl;
    decl << ret_type << " tml_" << func.name << "(" << params.str() << ");";

    return decl.str();
}

CHeaderGenResult CHeaderGen::generate(const parser::Module& module) {
    CHeaderGenResult result;
    std::ostringstream header;

    // Generate include guard start
    if (options_.add_include_guards) {
        std::string guard = gen_guard_name(module.name);
        header << "#ifndef " << guard << "\n";
        header << "#define " << guard << "\n\n";
    }

    // Add necessary includes
    header << "#include <stdint.h>\n";
    header << "#include <stdbool.h>\n\n";

    // Add extern "C" wrapper if requested
    if (options_.add_extern_c) {
        header << "#ifdef __cplusplus\n";
        header << "extern \"C\" {\n";
        header << "#endif\n\n";
    }

    // Add comment
    header << "// TML library: " << module.name << "\n";
    header << "// Auto-generated C header for FFI\n\n";

    // Generate function declarations for all public functions
    bool has_functions = false;
    for (const auto& decl : module.decls) {
        if (auto func = std::get_if<parser::FuncDecl>(&decl->kind)) {
            std::string func_decl = gen_func_decl(*func);
            if (!func_decl.empty()) {
                header << func_decl << "\n";
                has_functions = true;
            }
        }
    }

    if (!has_functions) {
        result.success = false;
        result.error_message = "No public functions found in module";
        return result;
    }

    // Close extern "C" wrapper
    if (options_.add_extern_c) {
        header << "\n#ifdef __cplusplus\n";
        header << "}\n";
        header << "#endif\n";
    }

    // Close include guard
    if (options_.add_include_guards) {
        std::string guard = gen_guard_name(module.name);
        header << "\n#endif // " << guard << "\n";
    }

    result.success = true;
    result.header_content = header.str();
    return result;
}

} // namespace tml::codegen
