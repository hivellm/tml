#ifndef TML_TYPES_MODULE_METADATA_HPP
#define TML_TYPES_MODULE_METADATA_HPP

#include "tml/types/module.hpp"

#include <filesystem>
#include <string>

namespace tml::types {

// Module metadata serialization/deserialization
class ModuleMetadata {
public:
    // Serialize module to .tml.meta JSON format
    static auto serialize(const Module& module) -> std::string;

    // Deserialize .tml.meta file to Module
    static auto deserialize(const std::string& json_content) -> std::optional<Module>;

    // Load module from .tml.meta file
    static auto load_from_file(const std::filesystem::path& meta_file) -> std::optional<Module>;

    // Save module to .tml.meta file
    static bool save_to_file(const Module& module, const std::filesystem::path& meta_file);

    // Get compiled metadata path for a module
    // Example: "std::math" -> "packages/std/compiled/math.tml.meta"
    static auto get_metadata_path(const std::string& module_path) -> std::filesystem::path;

    // Get compiled object file path for a module
    // Example: "std::math" -> "packages/std/compiled/math.o"
    static auto get_object_path(const std::string& module_path) -> std::filesystem::path;

    // Check if compiled metadata exists
    static bool has_compiled_metadata(const std::string& module_path);
};

} // namespace tml::types

#endif // TML_TYPES_MODULE_METADATA_HPP
