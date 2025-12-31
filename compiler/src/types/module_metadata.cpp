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

    // Serialize structs
    json << "  \"structs\": [\n";
    bool first_struct = true;
    for (const auto& [name, struct_def] : module.structs) {
        if (!first_struct)
            json << ",\n";
        first_struct = false;

        json << "    {\n";
        json << "      \"name\": \"" << json_escape(name) << "\",\n";
        json << "      \"type_params\": [";
        for (size_t i = 0; i < struct_def.type_params.size(); ++i) {
            if (i > 0)
                json << ", ";
            json << "\"" << json_escape(struct_def.type_params[i]) << "\"";
        }
        json << "],\n";
        json << "      \"fields\": [\n";
        for (size_t i = 0; i < struct_def.fields.size(); ++i) {
            if (i > 0)
                json << ",\n";
            json << "        {\"name\": \"" << json_escape(struct_def.fields[i].first)
                 << "\", \"type\": " << serialize_type(struct_def.fields[i].second) << "}";
        }
        json << "\n      ]\n";
        json << "    }";
    }
    json << "\n  ],\n";

    // Serialize enums
    json << "  \"enums\": [\n";
    bool first_enum = true;
    for (const auto& [name, enum_def] : module.enums) {
        if (!first_enum)
            json << ",\n";
        first_enum = false;

        json << "    {\n";
        json << "      \"name\": \"" << json_escape(name) << "\",\n";
        json << "      \"type_params\": [";
        for (size_t i = 0; i < enum_def.type_params.size(); ++i) {
            if (i > 0)
                json << ", ";
            json << "\"" << json_escape(enum_def.type_params[i]) << "\"";
        }
        json << "],\n";
        json << "      \"variants\": [\n";
        for (size_t i = 0; i < enum_def.variants.size(); ++i) {
            if (i > 0)
                json << ",\n";
            const auto& [variant_name, payload_types] = enum_def.variants[i];
            json << "        {\"name\": \"" << json_escape(variant_name) << "\", \"payload\": [";
            for (size_t j = 0; j < payload_types.size(); ++j) {
                if (j > 0)
                    json << ", ";
                json << serialize_type(payload_types[j]);
            }
            json << "]}";
        }
        json << "\n      ]\n";
        json << "    }";
    }
    json << "\n  ],\n";

    json << "  \"type_aliases\": []\n";
    json << "}\n";

    return json.str();
}

// Simple JSON parser helper
namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& content) : content_(content), pos_(0) {}

    void skip_ws() {
        while (pos_ < content_.size() && std::isspace(content_[pos_]))
            pos_++;
    }

    bool match(char c) {
        skip_ws();
        if (pos_ < content_.size() && content_[pos_] == c) {
            pos_++;
            return true;
        }
        return false;
    }

    bool expect(char c) {
        skip_ws();
        if (pos_ < content_.size() && content_[pos_] == c) {
            pos_++;
            return true;
        }
        return false;
    }

    std::string parse_string() {
        skip_ws();
        if (!expect('"'))
            return "";
        std::string result;
        while (pos_ < content_.size() && content_[pos_] != '"') {
            if (content_[pos_] == '\\' && pos_ + 1 < content_.size()) {
                pos_++;
                switch (content_[pos_]) {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                default:
                    result += content_[pos_];
                    break;
                }
            } else {
                result += content_[pos_];
            }
            pos_++;
        }
        expect('"');
        return result;
    }

    bool parse_bool() {
        skip_ws();
        if (content_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return true;
        } else if (content_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return false;
        }
        return false;
    }

    std::vector<std::string> parse_string_array() {
        std::vector<std::string> result;
        skip_ws();
        if (!expect('['))
            return result;
        skip_ws();
        if (match(']'))
            return result;
        do {
            result.push_back(parse_string());
            skip_ws();
        } while (match(','));
        expect(']');
        return result;
    }

    // Skip a JSON value (for fields we don't care about)
    void skip_value() {
        skip_ws();
        if (pos_ >= content_.size())
            return;
        char c = content_[pos_];
        if (c == '"') {
            parse_string();
        } else if (c == '[') {
            pos_++;
            int depth = 1;
            while (pos_ < content_.size() && depth > 0) {
                if (content_[pos_] == '[')
                    depth++;
                else if (content_[pos_] == ']')
                    depth--;
                else if (content_[pos_] == '"') {
                    pos_++;
                    while (pos_ < content_.size() && content_[pos_] != '"') {
                        if (content_[pos_] == '\\')
                            pos_++;
                        pos_++;
                    }
                }
                pos_++;
            }
        } else if (c == '{') {
            pos_++;
            int depth = 1;
            while (pos_ < content_.size() && depth > 0) {
                if (content_[pos_] == '{')
                    depth++;
                else if (content_[pos_] == '}')
                    depth--;
                else if (content_[pos_] == '"') {
                    pos_++;
                    while (pos_ < content_.size() && content_[pos_] != '"') {
                        if (content_[pos_] == '\\')
                            pos_++;
                        pos_++;
                    }
                }
                pos_++;
            }
        } else {
            // Number or boolean
            while (pos_ < content_.size() && !std::isspace(content_[pos_]) &&
                   content_[pos_] != ',' && content_[pos_] != '}' && content_[pos_] != ']') {
                pos_++;
            }
        }
    }

    size_t pos() const {
        return pos_;
    }
    const std::string& content() const {
        return content_;
    }

private:
    const std::string& content_;
    size_t pos_;
};

} // anonymous namespace

auto ModuleMetadata::deserialize(const std::string& json_content) -> std::optional<Module> {
    Module module;
    JsonParser parser(json_content);

    if (!parser.expect('{'))
        return std::nullopt;

    while (true) {
        parser.skip_ws();
        if (parser.match('}'))
            break;

        std::string key = parser.parse_string();
        if (!parser.expect(':')) {
            parser.skip_value();
            parser.match(',');
            continue;
        }

        if (key == "name") {
            module.name = parser.parse_string();
        } else if (key == "file_path") {
            module.file_path = parser.parse_string();
        } else if (key == "functions") {
            // Parse functions array
            if (!parser.expect('[')) {
                parser.skip_value();
                parser.match(',');
                continue;
            }

            while (!parser.match(']')) {
                if (!parser.expect('{')) {
                    parser.skip_value();
                    parser.match(',');
                    continue;
                }

                FuncSig sig;
                while (!parser.match('}')) {
                    std::string func_key = parser.parse_string();
                    parser.expect(':');

                    if (func_key == "name") {
                        sig.name = parser.parse_string();
                    } else if (func_key == "params") {
                        auto param_strs = parser.parse_string_array();
                        for (const auto& ps : param_strs) {
                            sig.params.push_back(deserialize_type(ps));
                        }
                    } else if (func_key == "return_type") {
                        std::string type_str = parser.parse_string();
                        sig.return_type = deserialize_type(type_str);
                    } else if (func_key == "is_async") {
                        sig.is_async = parser.parse_bool();
                    } else if (func_key == "is_lowlevel") {
                        sig.is_lowlevel = parser.parse_bool();
                    } else if (func_key == "stability") {
                        std::string stab = parser.parse_string();
                        if (stab == "Stable")
                            sig.stability = StabilityLevel::Stable;
                        else if (stab == "Deprecated")
                            sig.stability = StabilityLevel::Deprecated;
                        else
                            sig.stability = StabilityLevel::Unstable;
                    } else if (func_key == "extern_abi") {
                        sig.extern_abi = parser.parse_string();
                    } else if (func_key == "extern_name") {
                        sig.extern_name = parser.parse_string();
                    } else if (func_key == "link_libs") {
                        sig.link_libs = parser.parse_string_array();
                    } else {
                        parser.skip_value();
                    }
                    parser.match(',');
                }

                if (!sig.name.empty()) {
                    module.functions[sig.name] = sig;
                }
                parser.match(',');
            }
        } else if (key == "structs") {
            // Parse structs array
            if (!parser.expect('[')) {
                parser.skip_value();
                parser.match(',');
                continue;
            }

            while (!parser.match(']')) {
                if (!parser.expect('{')) {
                    parser.skip_value();
                    parser.match(',');
                    continue;
                }

                StructDef def;
                while (!parser.match('}')) {
                    std::string struct_key = parser.parse_string();
                    parser.expect(':');

                    if (struct_key == "name") {
                        def.name = parser.parse_string();
                    } else if (struct_key == "type_params") {
                        def.type_params = parser.parse_string_array();
                    } else if (struct_key == "fields") {
                        // Parse fields array
                        if (parser.expect('[')) {
                            while (!parser.match(']')) {
                                if (parser.expect('{')) {
                                    std::string field_name;
                                    TypePtr field_type;
                                    while (!parser.match('}')) {
                                        std::string field_key = parser.parse_string();
                                        parser.expect(':');
                                        if (field_key == "name") {
                                            field_name = parser.parse_string();
                                        } else if (field_key == "type") {
                                            std::string type_str = parser.parse_string();
                                            field_type = deserialize_type(type_str);
                                        } else {
                                            parser.skip_value();
                                        }
                                        parser.match(',');
                                    }
                                    if (!field_name.empty()) {
                                        def.fields.emplace_back(field_name, field_type);
                                    }
                                }
                                parser.match(',');
                            }
                        }
                    } else {
                        parser.skip_value();
                    }
                    parser.match(',');
                }

                if (!def.name.empty()) {
                    module.structs[def.name] = def;
                }
                parser.match(',');
            }
        } else if (key == "enums") {
            // Parse enums array
            if (!parser.expect('[')) {
                parser.skip_value();
                parser.match(',');
                continue;
            }

            while (!parser.match(']')) {
                if (!parser.expect('{')) {
                    parser.skip_value();
                    parser.match(',');
                    continue;
                }

                EnumDef def;
                while (!parser.match('}')) {
                    std::string enum_key = parser.parse_string();
                    parser.expect(':');

                    if (enum_key == "name") {
                        def.name = parser.parse_string();
                    } else if (enum_key == "type_params") {
                        def.type_params = parser.parse_string_array();
                    } else if (enum_key == "variants") {
                        // Parse variants array
                        if (parser.expect('[')) {
                            while (!parser.match(']')) {
                                if (parser.expect('{')) {
                                    std::string variant_name;
                                    std::vector<TypePtr> payload_types;
                                    while (!parser.match('}')) {
                                        std::string var_key = parser.parse_string();
                                        parser.expect(':');
                                        if (var_key == "name") {
                                            variant_name = parser.parse_string();
                                        } else if (var_key == "payload") {
                                            auto type_strs = parser.parse_string_array();
                                            for (const auto& ts : type_strs) {
                                                payload_types.push_back(deserialize_type(ts));
                                            }
                                        } else {
                                            parser.skip_value();
                                        }
                                        parser.match(',');
                                    }
                                    if (!variant_name.empty()) {
                                        def.variants.emplace_back(variant_name, payload_types);
                                    }
                                }
                                parser.match(',');
                            }
                        }
                    } else {
                        parser.skip_value();
                    }
                    parser.match(',');
                }

                if (!def.name.empty()) {
                    module.enums[def.name] = def;
                }
                parser.match(',');
            }
        } else {
            parser.skip_value();
        }
        parser.match(',');
    }

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
