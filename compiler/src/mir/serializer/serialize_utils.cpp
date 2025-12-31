// MIR Serialization Convenience Functions

#include "serializer_internal.hpp"

#include <fstream>
#include <sstream>

namespace tml::mir {

// ============================================================================
// Convenience Functions
// ============================================================================

auto serialize_binary(const Module& module) -> std::vector<uint8_t> {
    std::ostringstream oss(std::ios::binary);
    MirBinaryWriter writer(oss);
    writer.write_module(module);
    std::string data = oss.str();
    return std::vector<uint8_t>(data.begin(), data.end());
}

auto deserialize_binary(const std::vector<uint8_t>& data) -> Module {
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    MirBinaryReader reader(iss);
    return reader.read_module();
}

auto serialize_text(const Module& module, SerializeOptions options) -> std::string {
    std::ostringstream oss;
    MirTextWriter writer(oss, options);
    writer.write_module(module);
    return oss.str();
}

auto deserialize_text(const std::string& text) -> Module {
    std::istringstream iss(text);
    MirTextReader reader(iss);
    return reader.read_module();
}

auto write_mir_file(const Module& module, const std::string& path, bool binary) -> bool {
    std::ofstream file(path, binary ? std::ios::binary : std::ios::out);
    if (!file)
        return false;

    if (binary) {
        MirBinaryWriter writer(file);
        writer.write_module(module);
    } else {
        MirTextWriter writer(file);
        writer.write_module(module);
    }

    return true;
}

auto read_mir_file(const std::string& path) -> Module {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Module{};
    }

    // Check magic to determine format
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.seekg(0);

    if (magic == MIR_MAGIC) {
        MirBinaryReader reader(file);
        return reader.read_module();
    } else {
        // Try text format
        std::stringstream ss;
        ss << file.rdbuf();
        return deserialize_text(ss.str());
    }
}

} // namespace tml::mir
