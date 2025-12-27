#include "rlib.hpp"

#include "utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// For SHA256 hashing
#ifdef _WIN32
// clang-format off
#include <windows.h>  // Must come before wincrypt.h
#include <wincrypt.h>
// clang-format on
#pragma comment(lib, "advapi32.lib")
#else
#include <openssl/sha.h>
#endif

namespace tml::cli {

namespace {

/**
 * Escape JSON string
 */
std::string json_escape(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (c < 0x20) {
                result += "\\u";
                result += "0000"; // Simple placeholder for low control chars
            } else {
                result += c;
            }
        }
    }

    return result;
}

/**
 * Simple JSON value extraction (key: "value")
 */
std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos)
        return "";

    // Find the opening quote after the colon
    pos = json.find("\"", pos + search.length());
    if (pos == std::string::npos)
        return "";
    pos++; // Skip opening quote

    // Find the closing quote
    size_t end = json.find("\"", pos);
    if (end == std::string::npos)
        return "";

    return json.substr(pos, end - pos);
}

/**
 * Extract boolean value from JSON
 */
bool extract_json_bool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos)
        return false;

    pos = json.find(":", pos);
    if (pos == std::string::npos)
        return false;

    // Skip whitespace
    pos++;
    while (pos < json.length() && std::isspace(json[pos]))
        pos++;

    return json.substr(pos, 4) == "true";
}

/**
 * Execute command and capture output
 */
std::string exec_command(const std::string& cmd) {
    std::string result;
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe)
        return result;

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    _pclose(pipe);
    return result;
}

} // anonymous namespace

// ============================================================================
// RlibMetadata Implementation
// ============================================================================

std::optional<RlibExport> RlibMetadata::find_export(const std::string& name) const {
    for (const auto& module : modules) {
        for (const auto& exp : module.exports) {
            if (exp.name == name) {
                return exp;
            }
        }
    }
    return std::nullopt;
}

std::vector<RlibExport> RlibMetadata::get_all_exports() const {
    std::vector<RlibExport> result;
    for (const auto& module : modules) {
        for (const auto& exp : module.exports) {
            if (exp.is_public) {
                result.push_back(exp);
            }
        }
    }
    return result;
}

std::string RlibMetadata::to_json() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format_version\": \"" << json_escape(format_version) << "\",\n";

    // Library section
    oss << "  \"library\": {\n";
    oss << "    \"name\": \"" << json_escape(library.name) << "\",\n";
    oss << "    \"version\": \"" << json_escape(library.version) << "\",\n";
    oss << "    \"tml_version\": \"" << json_escape(library.tml_version) << "\"\n";
    oss << "  },\n";

    // Modules section
    oss << "  \"modules\": [\n";
    for (size_t i = 0; i < modules.size(); ++i) {
        const auto& mod = modules[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << json_escape(mod.name) << "\",\n";
        oss << "      \"file\": \"" << json_escape(mod.file) << "\",\n";
        oss << "      \"hash\": \"" << json_escape(mod.hash) << "\",\n";
        oss << "      \"exports\": [\n";

        for (size_t j = 0; j < mod.exports.size(); ++j) {
            const auto& exp = mod.exports[j];
            oss << "        {\n";
            oss << "          \"name\": \"" << json_escape(exp.name) << "\",\n";
            oss << "          \"symbol\": \"" << json_escape(exp.symbol) << "\",\n";
            oss << "          \"type\": \"" << json_escape(exp.type) << "\",\n";
            oss << "          \"public\": " << (exp.is_public ? "true" : "false") << "\n";
            oss << "        }";
            if (j < mod.exports.size() - 1)
                oss << ",";
            oss << "\n";
        }

        oss << "      ]\n";
        oss << "    }";
        if (i < modules.size() - 1)
            oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    // Dependencies section
    oss << "  \"dependencies\": [\n";
    for (size_t i = 0; i < dependencies.size(); ++i) {
        const auto& dep = dependencies[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << json_escape(dep.name) << "\",\n";
        oss << "      \"version\": \"" << json_escape(dep.version) << "\",\n";
        oss << "      \"hash\": \"" << json_escape(dep.hash) << "\"\n";
        oss << "    }";
        if (i < dependencies.size() - 1)
            oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";

    oss << "}\n";
    return oss.str();
}

RlibMetadata RlibMetadata::from_json(const std::string& json_str) {
    RlibMetadata metadata;

    // Parse top-level fields
    metadata.format_version = extract_json_string(json_str, "format_version");

    // Parse library section
    metadata.library.name = extract_json_string(json_str, "name");
    metadata.library.version = extract_json_string(json_str, "version");
    metadata.library.tml_version = extract_json_string(json_str, "tml_version");

    // For simplicity, this is a basic parser
    // A full implementation would use a proper JSON library
    // For now, we'll parse the essential fields

    return metadata;
}

// ============================================================================
// File Hashing
// ============================================================================

std::string calculate_file_hash(const fs::path& file_path) {
    if (!fs::exists(file_path)) {
        return "";
    }

#ifdef _WIN32
    // Windows: Use CryptoAPI
    HANDLE hFile = CreateFileW(file_path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        CloseHandle(hFile);
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return "";
    }

    BYTE buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        if (!CryptHashData(hHash, buffer, bytesRead, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            CloseHandle(hFile);
            return "";
        }
    }

    BYTE hash[32];
    DWORD hashLen = sizeof(hash);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return "";
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);

    // Convert to hex string
    std::ostringstream oss;
    oss << "sha256:";
    for (DWORD i = 0; i < hashLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();

#else
    // Linux/macOS: Use OpenSSL
    std::ifstream file(file_path, std::ios::binary);
    if (!file)
        return "";

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    std::ostringstream oss;
    oss << "sha256:";
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
#endif
}

// ============================================================================
// RLIB Creation
// ============================================================================

RlibResult create_rlib(const std::vector<fs::path>& object_files, const RlibMetadata& metadata,
                       const fs::path& output_rlib, const RlibCreateOptions& options) {
    // Create temporary directory for intermediate files
    fs::path temp_dir = fs::temp_directory_path() / "tml_rlib_temp";
    fs::create_directories(temp_dir);

    try {
        // Write metadata.json
        fs::path metadata_file = temp_dir / "metadata.json";
        std::ofstream metadata_out(metadata_file);
        if (!metadata_out) {
            return {false, "Failed to create metadata file", 1};
        }
        metadata_out << metadata.to_json();
        metadata_out.close();

        // Write exports.txt
        fs::path exports_file = temp_dir / "exports.txt";
        std::ofstream exports_out(exports_file);
        if (!exports_out) {
            return {false, "Failed to create exports file", 1};
        }
        for (const auto& exp : metadata.get_all_exports()) {
            exports_out << exp.symbol << "\n";
        }
        exports_out.close();

        // Build lib.exe command
        std::ostringstream cmd;

#ifdef _WIN32
        cmd << options.archiver << " /OUT:\"" << output_rlib.string() << "\"";

        // Add object files
        for (const auto& obj : object_files) {
            cmd << " \"" << obj.string() << "\"";
        }

        // Add metadata and exports
        cmd << " \"" << metadata_file.string() << "\"";
        cmd << " \"" << exports_file.string() << "\"";

        if (!options.verbose) {
            cmd << " /NOLOGO";
        }
#else
        // Linux/macOS: use ar
        cmd << "ar rcs \"" << output_rlib.string() << "\"";

        for (const auto& obj : object_files) {
            cmd << " \"" << obj.string() << "\"";
        }

        cmd << " \"" << metadata_file.string() << "\"";
        cmd << " \"" << exports_file.string() << "\"";
#endif

        if (options.verbose) {
            std::cout << "Creating RLIB: " << cmd.str() << "\n";
        }

        int result = std::system(cmd.str().c_str());

        // Cleanup temp directory
        fs::remove_all(temp_dir);

        if (result != 0) {
            return {false, "Failed to create RLIB archive", result};
        }

        return {true, "RLIB created successfully", 0};

    } catch (const std::exception& e) {
        fs::remove_all(temp_dir);
        return {false, std::string("Error: ") + e.what(), 1};
    }
}

// ============================================================================
// RLIB Reading
// ============================================================================

bool extract_rlib_member(const fs::path& rlib_file, const std::string& member_name,
                         const fs::path& output_path) {
    if (!fs::exists(rlib_file)) {
        return false;
    }

#ifdef _WIN32
    // Windows: Use lib.exe to extract
    std::string cmd = "lib.exe /NOLOGO /EXTRACT:\"" + member_name + "\" /OUT:\"" +
                      output_path.string() + "\" \"" + rlib_file.string() + "\" 2>nul";

    int result = std::system(cmd.c_str());
    return result == 0 && fs::exists(output_path);
#else
    // Linux/macOS: Use ar to extract
    std::string cmd = "ar p \"" + rlib_file.string() + "\" \"" + member_name + "\" > \"" +
                      output_path.string() + "\"";

    int result = std::system(cmd.c_str());
    return result == 0 && fs::exists(output_path);
#endif
}

std::optional<RlibMetadata> read_rlib_metadata(const fs::path& rlib_file) {
    if (!fs::exists(rlib_file)) {
        return std::nullopt;
    }

    // Create temporary directory
    fs::path temp_dir = fs::temp_directory_path() / "tml_rlib_read";
    fs::create_directories(temp_dir);

    fs::path metadata_file = temp_dir / "metadata.json";

    try {
        // Extract metadata.json
        if (!extract_rlib_member(rlib_file, "metadata.json", metadata_file)) {
            fs::remove_all(temp_dir);
            return std::nullopt;
        }

        // Read metadata
        std::ifstream metadata_in(metadata_file);
        if (!metadata_in) {
            fs::remove_all(temp_dir);
            return std::nullopt;
        }

        std::string json_str((std::istreambuf_iterator<char>(metadata_in)),
                             std::istreambuf_iterator<char>());
        metadata_in.close();

        RlibMetadata metadata = RlibMetadata::from_json(json_str);

        // Cleanup
        fs::remove_all(temp_dir);

        return metadata;

    } catch (const std::exception&) {
        fs::remove_all(temp_dir);
        return std::nullopt;
    }
}

std::vector<fs::path> extract_rlib_objects(const fs::path& rlib_file, const fs::path& temp_dir) {
    std::vector<fs::path> result;

    if (!fs::exists(rlib_file)) {
        return result;
    }

    fs::create_directories(temp_dir);

    // Read metadata to get list of object files
    auto metadata = read_rlib_metadata(rlib_file);
    if (!metadata) {
        return result;
    }

    // Extract each object file
    for (const auto& module : metadata->modules) {
        fs::path output_path = temp_dir / module.file;

        if (extract_rlib_member(rlib_file, module.file, output_path)) {
            result.push_back(output_path);
        }
    }

    return result;
}

std::vector<std::string> list_rlib_members(const fs::path& rlib_file) {
    std::vector<std::string> result;

    if (!fs::exists(rlib_file)) {
        return result;
    }

#ifdef _WIN32
    // Windows: Use lib.exe to list members
    std::string cmd = "lib.exe /NOLOGO /LIST \"" + rlib_file.string() + "\"";
    std::string output = exec_command(cmd);

    // Parse output
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            result.push_back(line);
        }
    }
#else
    // Linux/macOS: Use ar to list members
    std::string cmd = "ar t \"" + rlib_file.string() + "\"";
    std::string output = exec_command(cmd);

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            result.push_back(line);
        }
    }
#endif

    return result;
}

bool validate_rlib(const fs::path& rlib_file) {
    if (!fs::exists(rlib_file)) {
        return false;
    }

    // Check if metadata.json exists
    auto members = list_rlib_members(rlib_file);
    bool has_metadata = std::find(members.begin(), members.end(), "metadata.json") != members.end();

    if (!has_metadata) {
        return false;
    }

    // Try to read metadata
    auto metadata = read_rlib_metadata(rlib_file);
    if (!metadata) {
        return false;
    }

    // Check format version
    if (metadata->format_version != "1.0") {
        return false;
    }

    // Check that all modules exist in archive
    for (const auto& module : metadata->modules) {
        bool has_module = std::find(members.begin(), members.end(), module.file) != members.end();
        if (!has_module) {
            return false;
        }
    }

    return true;
}

} // namespace tml::cli
