#include "types/module_metadata.hpp"

#include "types/env.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace tml::types {

namespace {

// Helper to escape JSON strings
std::string json_escape(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c == '"')
            result += "\\\"";
        else if (c == '\\')
            result += "\\\\";
        else if (c == '\n')
            result += "\\n";
        else if (c == '\t')
            result += "\\t";
        else
            result += c;
    }
    return result;
}

// Serialize a type to JSON string representation
std::string serialize_type(const TypePtr& type) {
    if (!type)
        return "\"Unit\"";

    if (type->is<PrimitiveType>()) {
        auto& prim = type->as<PrimitiveType>();
        switch (prim.kind) {
        case PrimitiveKind::I8:
            return "\"I8\"";
        case PrimitiveKind::I16:
            return "\"I16\"";
        case PrimitiveKind::I32:
            return "\"I32\"";
        case PrimitiveKind::I64:
            return "\"I64\"";
        case PrimitiveKind::I128:
            return "\"I128\"";
        case PrimitiveKind::U8:
            return "\"U8\"";
        case PrimitiveKind::U16:
            return "\"U16\"";
        case PrimitiveKind::U32:
            return "\"U32\"";
        case PrimitiveKind::U64:
            return "\"U64\"";
        case PrimitiveKind::U128:
            return "\"U128\"";
        case PrimitiveKind::F32:
            return "\"F32\"";
        case PrimitiveKind::F64:
            return "\"F64\"";
        case PrimitiveKind::Bool:
            return "\"Bool\"";
        case PrimitiveKind::Char:
            return "\"Char\"";
        case PrimitiveKind::Str:
            return "\"Str\"";
        }
    } else if (type->is<RefType>()) {
        auto& ref = type->as<RefType>();
        return "{\"ref\": " + serialize_type(ref.inner) +
               ", \"mut\": " + (ref.is_mut ? "true" : "false") + "}";
    } else if (type->is<NamedType>()) {
        auto& named = type->as<NamedType>();
        return "\"" + named.name + "\"";
    }

    return "\"Unknown\"";
}

// Deserialize type from JSON representation (simplified)
TypePtr deserialize_type(const std::string& type_str) {
    // Simple type deserialization
    if (type_str == "I8")
        return make_primitive(PrimitiveKind::I8);
    if (type_str == "I16")
        return make_primitive(PrimitiveKind::I16);
    if (type_str == "I32")
        return make_primitive(PrimitiveKind::I32);
    if (type_str == "I64")
        return make_primitive(PrimitiveKind::I64);
    if (type_str == "I128")
        return make_primitive(PrimitiveKind::I128);
    if (type_str == "U8")
        return make_primitive(PrimitiveKind::U8);
    if (type_str == "U16")
        return make_primitive(PrimitiveKind::U16);
    if (type_str == "U32")
        return make_primitive(PrimitiveKind::U32);
    if (type_str == "U64")
        return make_primitive(PrimitiveKind::U64);
    if (type_str == "U128")
        return make_primitive(PrimitiveKind::U128);
    if (type_str == "F32")
        return make_primitive(PrimitiveKind::F32);
    if (type_str == "F64")
        return make_primitive(PrimitiveKind::F64);
    if (type_str == "Bool")
        return make_primitive(PrimitiveKind::Bool);
    if (type_str == "Char")
        return make_primitive(PrimitiveKind::Char);
    if (type_str == "Str")
        return make_primitive(PrimitiveKind::Str);
    if (type_str == "Unit")
        return make_unit();

    // Named types
    if (type_str == "List")
        return std::make_shared<Type>(Type{NamedType{"List", "", {}}});
    if (type_str == "HashMap")
        return std::make_shared<Type>(Type{NamedType{"HashMap", "", {}}});
    if (type_str == "Buffer")
        return std::make_shared<Type>(Type{NamedType{"Buffer", "", {}}});

    return make_primitive(PrimitiveKind::I32); // fallback
}

} // anonymous namespace

auto ModuleMetadata::serialize(const Module& module) -> std::string {
    std::ostringstream json;

    json << "{\n";
    json << "  \"name\": \"" << json_escape(module.name) << "\",\n";
    json << "  \"file_path\": \"" << json_escape(module.file_path) << "\",\n";
    json << "  \"functions\": [\n";

    // Serialize functions
    bool first_func = true;
    for (const auto& [name, func_sig] : module.functions) {
        if (!first_func)
            json << ",\n";
        first_func = false;

        json << "    {\n";
        json << "      \"name\": \"" << json_escape(name) << "\",\n";
        json << "      \"params\": [";

        for (size_t i = 0; i < func_sig.params.size(); ++i) {
            if (i > 0)
                json << ", ";
            json << serialize_type(func_sig.params[i]);
        }

        json << "],\n";
        json << "      \"return_type\": " << serialize_type(func_sig.return_type) << ",\n";
        json << "      \"is_async\": " << (func_sig.is_async ? "true" : "false") << ",\n";
        json << "      \"is_lowlevel\": " << (func_sig.is_lowlevel ? "true" : "false") << ",\n";
        json << "      \"stability\": \""
             << (func_sig.stability == StabilityLevel::Stable     ? "Stable"
                 : func_sig.stability == StabilityLevel::Unstable ? "Unstable"
                                                                  : "Deprecated")
             << "\"";

        // FFI fields
        if (func_sig.extern_abi.has_value()) {
            json << ",\n      \"extern_abi\": \"" << json_escape(*func_sig.extern_abi) << "\"";
        }
        if (func_sig.extern_name.has_value()) {
            json << ",\n      \"extern_name\": \"" << json_escape(*func_sig.extern_name) << "\"";
        }
        if (!func_sig.link_libs.empty()) {
            json << ",\n      \"link_libs\": [";
            for (size_t i = 0; i < func_sig.link_libs.size(); ++i) {
                if (i > 0)
                    json << ", ";
                json << "\"" << json_escape(func_sig.link_libs[i]) << "\"";
            }
            json << "]";
        }
        json << "\n";
        json << "    }";
    }

    json << "\n  ],\n";
    json << "  \"structs\": [],\n"; // TODO: implement struct serialization
    json << "  \"enums\": [],\n";   // TODO: implement enum serialization
    json << "  \"type_aliases\": []\n";
    json << "}\n";

    return json.str();
}

auto ModuleMetadata::deserialize(const std::string& json_content) -> std::optional<Module> {
    // Simple JSON parsing - in production, use a proper JSON library
    // For now, this is a placeholder that returns empty module
    // TODO: Implement proper JSON parsing

    Module module;

    // Very basic parsing to extract module name
    size_t name_pos = json_content.find("\"name\":");
    if (name_pos != std::string::npos) {
        size_t quote1 = json_content.find("\"", name_pos + 7);
        size_t quote2 = json_content.find("\"", quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            module.name = json_content.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }

    // For now, return empty module - proper JSON parsing needed
    std::cerr << "[METADATA] Warning: JSON deserialization not fully implemented yet\n";

    return module;
}

auto ModuleMetadata::load_from_file(const std::filesystem::path& meta_file)
    -> std::optional<Module> {
    std::ifstream file(meta_file);
    if (!file) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    return deserialize(content);
}

bool ModuleMetadata::save_to_file(const Module& module, const std::filesystem::path& meta_file) {
    // Create directory if it doesn't exist
    std::filesystem::create_directories(meta_file.parent_path());

    std::ofstream file(meta_file);
    if (!file) {
        std::cerr << "[METADATA] Failed to create: " << meta_file << "\n";
        return false;
    }

    file << serialize(module);
    file.close();

    std::cerr << "[METADATA] Saved: " << meta_file << "\n";
    return true;
}

auto ModuleMetadata::get_metadata_path(const std::string& module_path) -> std::filesystem::path {
    // "core::mem" -> "lib/core/compiled/mem.tml.meta"
    if (module_path.substr(0, 6) == "core::") {
        std::string module_name = module_path.substr(6);
        return std::filesystem::path("lib") / "core" / "compiled" / (module_name + ".tml.meta");
    }
    // "std::math" -> "lib/std/compiled/math.tml.meta"
    else if (module_path.substr(0, 5) == "std::") {
        std::string module_name = module_path.substr(5);
        return std::filesystem::path("lib") / "std" / "compiled" / (module_name + ".tml.meta");
    } else if (module_path == "test") {
        return std::filesystem::path("lib") / "test" / "compiled" / "test.tml.meta";
    }

    // User modules
    return std::filesystem::path("tml_modules") / "compiled" / (module_path + ".tml.meta");
}

auto ModuleMetadata::get_object_path(const std::string& module_path) -> std::filesystem::path {
    // "core::mem" -> "lib/core/compiled/mem.o" (or .obj on Windows)
    std::string obj_ext;
#ifdef _WIN32
    obj_ext = ".obj";
#else
    obj_ext = ".o";
#endif

    if (module_path.substr(0, 6) == "core::") {
        std::string module_name = module_path.substr(6);
        return std::filesystem::path("lib") / "core" / "compiled" / (module_name + obj_ext);
    } else if (module_path.substr(0, 5) == "std::") {
        std::string module_name = module_path.substr(5);
        return std::filesystem::path("lib") / "std" / "compiled" / (module_name + obj_ext);
    } else if (module_path == "test") {
        return std::filesystem::path("lib") / "test" / "compiled" / ("test" + obj_ext);
    }

    return std::filesystem::path("tml_modules") / "compiled" / (module_path + obj_ext);
}

bool ModuleMetadata::has_compiled_metadata(const std::string& module_path) {
    auto meta_path = get_metadata_path(module_path);
    return std::filesystem::exists(meta_path);
}

} // namespace tml::types
