//! # LLVM IR Generator - Debug Information
//!
//! This file generates DWARF debug metadata for source-level debugging.
//!
//! ## Purpose
//!
//! When compiled with `-g` or `--debug`, the compiler emits LLVM debug
//! metadata that maps generated code back to TML source locations.
//!
//! ## DWARF Metadata
//!
//! | Metadata Type    | Purpose                           |
//! |------------------|-----------------------------------|
//! | `DIFile`         | Source file reference             |
//! | `DICompileUnit`  | Compilation unit                  |
//! | `DISubprogram`   | Function debug info               |
//! | `DILocation`     | Source line/column mapping        |
//! | `DIBasicType`    | Primitive type debug info         |
//!
//! ## Key Methods
//!
//! | Method                  | Purpose                        |
//! |-------------------------|--------------------------------|
//! | `emit_debug_info_header`| Emit file and compile unit    |
//! | `emit_debug_info_footer`| Emit all collected metadata   |
//! | `emit_function_debug`   | Emit function subprogram      |
//! | `emit_debug_location`   | Emit source location marker   |

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "version_generated.hpp"

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::codegen {

int LLVMIRGen::fresh_debug_id() {
    return debug_metadata_counter_++;
}

void LLVMIRGen::emit_debug_info_header() {
    if (!options_.emit_debug_info || options_.source_file.empty()) {
        return;
    }

    // Get file info
    fs::path source_path(options_.source_file);
    std::string filename = source_path.filename().string();
    std::string directory = source_path.parent_path().string();
    if (directory.empty()) {
        directory = ".";
    }

    // Allocate metadata IDs
    file_id_ = fresh_debug_id();
    compile_unit_id_ = fresh_debug_id();

    // Add named metadata that must appear at module level
    // Note: The actual metadata nodes are emitted in footer

    // Store metadata to emit at end of file
    std::ostringstream meta;

    // File reference
    meta << "!" << file_id_ << " = !DIFile(filename: \"" << filename << "\", directory: \""
         << directory << "\")\n";
    debug_metadata_.push_back(meta.str());
    meta.str("");

    // Compile unit
    meta << "!" << compile_unit_id_ << " = distinct !DICompileUnit("
         << "language: DW_LANG_C_plus_plus, " // Use C++ for now, TML would need custom DWARF
         << "file: !" << file_id_ << ", "
         << "producer: \"TML Compiler " << VERSION << "\", "
         << "isOptimized: " << (CompilerOptions::optimization_level > 0 ? "true" : "false") << ", "
         << "runtimeVersion: 0, "
         << "emissionKind: FullDebug, "
         << "splitDebugInlining: false"
         << ")\n";
    debug_metadata_.push_back(meta.str());
}

void LLVMIRGen::emit_debug_info_footer() {
    if (!options_.emit_debug_info || debug_metadata_.empty()) {
        return;
    }

    emit_line("");
    emit_line("; Debug Information");

    // Emit all collected debug metadata
    for (const auto& meta : debug_metadata_) {
        emit(meta);
    }

    // Emit module flags for debug info
    emit_line("");
    emit_line("!llvm.dbg.cu = !{!" + std::to_string(compile_unit_id_) + "}");

    // Module flags reference named metadata that we define inline
    int version_id = fresh_debug_id();
    int dwarf_id = fresh_debug_id();
    emit_line("!llvm.module.flags = !{!" + std::to_string(version_id) + ", !" +
              std::to_string(dwarf_id) + "}");
    emit_line("!" + std::to_string(version_id) + " = !{i32 2, !\"Debug Info Version\", i32 3}");
    emit_line("!" + std::to_string(dwarf_id) + " = !{i32 2, !\"Dwarf Version\", i32 4}");
}

int LLVMIRGen::create_function_debug_scope(const std::string& func_name, uint32_t line,
                                           [[maybe_unused]] uint32_t column) {
    if (!options_.emit_debug_info) {
        return 0;
    }

    int scope_id = fresh_debug_id();
    int type_id = fresh_debug_id();

    std::ostringstream meta;

    // Function type (simplified - void return, no params shown)
    meta << "!" << type_id << " = !DISubroutineType(types: !{})\n";
    debug_metadata_.push_back(meta.str());
    meta.str("");

    // Function debug info
    meta << "!" << scope_id << " = distinct !DISubprogram("
         << "name: \"" << func_name << "\", "
         << "scope: !" << file_id_ << ", "
         << "file: !" << file_id_ << ", "
         << "line: " << line << ", "
         << "type: !" << type_id << ", "
         << "scopeLine: " << line << ", "
         << "spFlags: DISPFlagDefinition, "
         << "unit: !" << compile_unit_id_ << ")\n";
    debug_metadata_.push_back(meta.str());

    func_debug_scope_[func_name] = scope_id;
    current_scope_id_ = scope_id;

    return scope_id;
}

std::string LLVMIRGen::get_debug_location(uint32_t line, uint32_t column) {
    if (!options_.emit_debug_info || current_scope_id_ == 0) {
        return "";
    }

    // Create inline debug location reference
    int loc_id = fresh_debug_id();

    std::ostringstream meta;
    meta << "!" << loc_id << " = !DILocation("
         << "line: " << line << ", "
         << "column: " << column << ", "
         << "scope: !" << current_scope_id_ << ")\n";
    debug_metadata_.push_back(meta.str());

    return ", !dbg !" + std::to_string(loc_id);
}

int LLVMIRGen::create_debug_location(uint32_t line, uint32_t column) {
    if (!options_.emit_debug_info || current_scope_id_ == 0) {
        return 0;
    }

    int loc_id = fresh_debug_id();

    std::ostringstream meta;
    meta << "!" << loc_id << " = !DILocation("
         << "line: " << line << ", "
         << "column: " << column << ", "
         << "scope: !" << current_scope_id_ << ")\n";
    debug_metadata_.push_back(meta.str());

    current_debug_loc_id_ = loc_id;
    return loc_id;
}

std::string LLVMIRGen::get_debug_loc_suffix() {
    if (!options_.emit_debug_info || current_debug_loc_id_ == 0) {
        return "";
    }
    return ", !dbg !" + std::to_string(current_debug_loc_id_);
}

int LLVMIRGen::get_or_create_type_debug_info(const std::string& type_name,
                                             const std::string& llvm_type) {
    if (!options_.emit_debug_info) {
        return 0;
    }

    // Check if we already have debug info for this type
    auto it = type_debug_info_.find(type_name);
    if (it != type_debug_info_.end()) {
        return it->second;
    }

    int type_id = fresh_debug_id();
    std::ostringstream meta;

    // Determine size and encoding based on LLVM type
    int size_bits = 0;
    std::string encoding;

    if (llvm_type == "i1") {
        size_bits = 8; // Bool is stored as i8
        encoding = "DW_ATE_boolean";
    } else if (llvm_type == "i8") {
        size_bits = 8;
        encoding = "DW_ATE_signed";
    } else if (llvm_type == "i16") {
        size_bits = 16;
        encoding = "DW_ATE_signed";
    } else if (llvm_type == "i32") {
        size_bits = 32;
        encoding = "DW_ATE_signed";
    } else if (llvm_type == "i64") {
        size_bits = 64;
        encoding = "DW_ATE_signed";
    } else if (llvm_type == "i128") {
        size_bits = 128;
        encoding = "DW_ATE_signed";
    } else if (llvm_type == "float") {
        size_bits = 32;
        encoding = "DW_ATE_float";
    } else if (llvm_type == "double") {
        size_bits = 64;
        encoding = "DW_ATE_float";
    } else if (llvm_type == "i8*" || llvm_type.find("*") != std::string::npos) {
        size_bits = 64; // Pointer size (64-bit)
        encoding = "DW_ATE_address";
    } else {
        // Default to 64-bit for unknown types (structs, etc.)
        size_bits = 64;
        encoding = "DW_ATE_signed";
    }

    meta << "!" << type_id << " = !DIBasicType("
         << "name: \"" << type_name << "\", "
         << "size: " << size_bits << ", "
         << "encoding: " << encoding << ")\n";
    debug_metadata_.push_back(meta.str());

    type_debug_info_[type_name] = type_id;
    return type_id;
}

int LLVMIRGen::create_local_variable_debug_info(const std::string& var_name,
                                                const std::string& llvm_type, uint32_t line,
                                                uint32_t arg_no) {
    if (!options_.emit_debug_info || current_scope_id_ == 0) {
        return 0;
    }

    // Get or create type debug info
    int type_id = get_or_create_type_debug_info(llvm_type, llvm_type);

    int var_id = fresh_debug_id();
    std::ostringstream meta;

    if (arg_no > 0) {
        // Function parameter
        meta << "!" << var_id << " = !DILocalVariable("
             << "name: \"" << var_name << "\", "
             << "arg: " << arg_no << ", "
             << "scope: !" << current_scope_id_ << ", "
             << "file: !" << file_id_ << ", "
             << "line: " << line << ", "
             << "type: !" << type_id << ")\n";
    } else {
        // Local variable
        meta << "!" << var_id << " = !DILocalVariable("
             << "name: \"" << var_name << "\", "
             << "scope: !" << current_scope_id_ << ", "
             << "file: !" << file_id_ << ", "
             << "line: " << line << ", "
             << "type: !" << type_id << ")\n";
    }
    debug_metadata_.push_back(meta.str());

    var_debug_info_[var_name] = var_id;
    return var_id;
}

void LLVMIRGen::emit_debug_declare(const std::string& alloca_reg, int var_debug_id, int loc_id) {
    if (!options_.emit_debug_info || var_debug_id == 0) {
        return;
    }

    // Emit llvm.dbg.declare intrinsic call
    // This tells the debugger where the variable is stored
    emit_line("  call void @llvm.dbg.declare(metadata ptr " + alloca_reg + ", metadata !" +
              std::to_string(var_debug_id) + ", metadata !DIExpression()), !dbg !" +
              std::to_string(loc_id));
}

} // namespace tml::codegen
