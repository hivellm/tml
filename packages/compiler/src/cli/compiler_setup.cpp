#include "compiler_setup.hpp"
#include "utils.hpp"
#include "tml/common.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <mutex>
#include <map>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace tml::cli {

// Thread-safe compilation mutex (per-file basis)
static std::mutex compilation_mutex;
static std::map<std::string, bool> compilation_in_progress;

#ifdef _WIN32
MSVCInfo find_msvc() {
    MSVCInfo info;
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

    std::string msvc_base;
    std::string msvc_ver;
    for (const auto& vs_path : vs_bases) {
        if (fs::exists(vs_path)) {
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
        std::string cl_x64 = msvc_path + "/bin/Hostx64/x64/cl.exe";
        std::string cl_x86 = msvc_path + "/bin/Hostx86/x86/cl.exe";
        if (fs::exists(cl_x64)) {
            info.cl_path = cl_x64;
        } else if (fs::exists(cl_x86)) {
            info.cl_path = cl_x86;
        }

        std::string inc = msvc_path + "/include";
        if (fs::exists(inc)) {
            info.includes.push_back(inc);
        }

        std::string lib_x64 = msvc_path + "/lib/x64";
        std::string lib_x86 = msvc_path + "/lib/x86";
        if (fs::exists(cl_x64) && fs::exists(lib_x64)) {
            info.libs.push_back(lib_x64);
        } else if (fs::exists(lib_x86)) {
            info.libs.push_back(lib_x86);
        }
    }

    std::string sdk_base = "C:/Program Files (x86)/Windows Kits/10";
    if (fs::exists(sdk_base + "/Include")) {
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

std::string find_clang() {
    std::string clang = "clang";
#ifdef _WIN32
    std::vector<std::string> clang_paths = {
        "F:/LLVM/bin/clang.exe",
        "C:/Program Files/LLVM/bin/clang.exe",
        "C:/LLVM/bin/clang.exe",
    };
    for (const auto& p : clang_paths) {
        if (fs::exists(p)) {
            return p;
        }
    }
#endif
    return clang;
}

std::string find_runtime() {
    std::vector<std::string> runtime_search = {
        "packages/compiler/runtime/essential.c",
        "runtime/essential.c",
        "../runtime/essential.c",
        "../../runtime/essential.c",
        "F:/Node/hivellm/tml/packages/compiler/runtime/essential.c",
    };
    for (const auto& rp : runtime_search) {
        if (fs::exists(rp)) {
            return to_forward_slashes(fs::absolute(rp).string());
        }
    }
    return "";
}

std::string ensure_runtime_compiled(const std::string& runtime_c_path, const std::string& clang, bool verbose) {
    fs::path c_path = runtime_c_path;
    fs::path obj_path = c_path.parent_path() / "essential";
#ifdef _WIN32
    obj_path += ".obj";
#else
    obj_path += ".o";
#endif

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
        std::string compile_cmd = clang + " -c -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops -o \"" +
                                  to_forward_slashes(obj_path.string()) + "\" \"" + to_forward_slashes(c_path.string()) + "\"";
        int ret = std::system(compile_cmd.c_str());
        if (ret != 0) {
            return runtime_c_path;
        }
    }

    return to_forward_slashes(obj_path.string());
}

std::string ensure_c_compiled(const std::string& c_path_str, const std::string& cache_dir,
                               const std::string& clang, bool verbose) {
    fs::path c_path = c_path_str;

    // Create cache directory if needed
    fs::path cache_path = cache_dir;
    fs::create_directories(cache_path);

    // Generate object file name from source file name
    std::string base_name = c_path.stem().string();
    fs::path obj_path = cache_path / base_name;
#ifdef _WIN32
    obj_path += ".obj";
#else
    obj_path += ".o";
#endif

    std::string obj_path_str = to_forward_slashes(obj_path.string());

    // Thread-safe check and compilation
    bool should_compile = false;
    {
        std::lock_guard<std::mutex> lock(compilation_mutex);

        // If another thread is compiling, wait by checking if file exists after lock
        bool needs_compile = !fs::exists(obj_path);
        if (!needs_compile) {
            auto c_time = fs::last_write_time(c_path);
            auto obj_time = fs::last_write_time(obj_path);
            needs_compile = (c_time > obj_time);
        }

        if (needs_compile && !compilation_in_progress[obj_path_str]) {
            compilation_in_progress[obj_path_str] = true;
            should_compile = true;
        }
    }

    if (should_compile) {
        if (verbose) {
            std::cout << "Compiling: " << c_path.filename().string() << " -> " << obj_path.filename().string() << "\n";
        }
        std::string compile_cmd = clang + " -c -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops -o \"" +
                                  obj_path_str + "\" \"" + to_forward_slashes(c_path.string()) + "\"";
        int ret = std::system(compile_cmd.c_str());

        // Mark compilation as done
        {
            std::lock_guard<std::mutex> lock(compilation_mutex);
            compilation_in_progress.erase(obj_path_str);
        }

        if (ret != 0) {
            // Fallback to returning .c path on compile failure
            return c_path_str;
        }
    } else {
        // Another thread compiled or is compiling - wait for file to exist
        int retries = 0;
        while (!fs::exists(obj_path) && retries < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            retries++;
        }
    }

    return obj_path_str;
}

}
