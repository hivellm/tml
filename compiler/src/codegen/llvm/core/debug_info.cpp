TML_MODULE("codegen_x86")

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

    // ---- Pointer / reference types → DIDerivedType ----
    if (llvm_type == "ptr" || (!llvm_type.empty() && llvm_type.back() == '*')) {
        // Reserve the type_id in the map early to prevent infinite recursion
        // when the base type circularly references this pointer type.
        type_debug_info_[type_name] = type_id;

        // Determine the base type name by stripping pointer/ref indicators
        std::string base_type_name = type_name;
        if (base_type_name.starts_with("mut ref ")) {
            base_type_name = base_type_name.substr(8);
        } else if (base_type_name.starts_with("ref ")) {
            base_type_name = base_type_name.substr(4);
        } else if (base_type_name.starts_with("*")) {
            base_type_name = base_type_name.substr(1);
        }

        // Get or create the pointee type debug info
        int base_type_id = 0;
        if (!base_type_name.empty() && base_type_name != type_name) {
            // Use i64 as a fallback LLVM type for the base — the actual layout
            // is determined by the pointee, but we need a valid type string.
            base_type_id = get_or_create_type_debug_info(base_type_name, "i64");
        }

        meta << "!" << type_id << " = !DIDerivedType("
             << "tag: DW_TAG_pointer_type, "
             << "name: \"" << type_name << "\", "
             << "baseType: " << (base_type_id > 0 ? "!" + std::to_string(base_type_id) : "null")
             << ", size: 64)\n";
        debug_metadata_.push_back(meta.str());

        return type_id;
    }

    // ---- Struct / enum / class / union types → DICompositeType ----
    if (llvm_type.starts_with("%struct.") || llvm_type.starts_with("%enum.") ||
        llvm_type.starts_with("%class.") || llvm_type.starts_with("%union.")) {

        // Reserve the type_id in the map early — member DIDerivedType nodes
        // reference the parent composite via `scope: !<type_id>`, so this
        // must be registered before we recurse into field types.
        type_debug_info_[type_name] = type_id;

        // Extract struct name from LLVM type: "%struct.List__I32" → "List__I32"
        std::string struct_name = llvm_type.substr(llvm_type.find('.') + 1);

        // Look up field info from the struct_fields_ registry
        auto fields_it = struct_fields_.find(struct_name);

        // Create DIDerivedType(DW_TAG_member) for each field
        std::vector<int> member_ids;
        int offset_bits = 0;

        if (fields_it != struct_fields_.end()) {
            for (const auto& field : fields_it->second) {
                int member_type_id = get_or_create_type_debug_info(field.name, field.llvm_type);
                int member_id = fresh_debug_id();

                // Determine field size in bits from LLVM type
                int field_size = 64; // default for ptr, i64, structs
                if (field.llvm_type == "i1" || field.llvm_type == "i8") {
                    field_size = 8;
                } else if (field.llvm_type == "i16") {
                    field_size = 16;
                } else if (field.llvm_type == "i32" || field.llvm_type == "float") {
                    field_size = 32;
                } else if (field.llvm_type == "i64" || field.llvm_type == "double" ||
                           field.llvm_type == "ptr" ||
                           field.llvm_type.find('*') != std::string::npos) {
                    field_size = 64;
                } else if (field.llvm_type == "i128") {
                    field_size = 128;
                }

                std::ostringstream m;
                m << "!" << member_id << " = !DIDerivedType("
                  << "tag: DW_TAG_member, "
                  << "name: \"" << field.name << "\", "
                  << "scope: !" << type_id << ", "
                  << "file: !" << file_id_ << ", "
                  << "baseType: !" << member_type_id << ", "
                  << "size: " << field_size << ", "
                  << "offset: " << offset_bits << ")\n";
                debug_metadata_.push_back(m.str());
                member_ids.push_back(member_id);
                offset_bits += field_size;
            }
        }

        // Create the elements tuple referencing all member nodes
        int elements_id = fresh_debug_id();
        {
            std::ostringstream m;
            m << "!" << elements_id << " = !{";
            for (size_t i = 0; i < member_ids.size(); i++) {
                if (i > 0)
                    m << ", ";
                m << "!" << member_ids[i];
            }
            m << "}\n";
            debug_metadata_.push_back(m.str());
        }

        // Choose the DWARF tag based on the LLVM type prefix
        std::string tag = "DW_TAG_structure_type";
        if (llvm_type.starts_with("%enum.") || llvm_type.starts_with("%union.")) {
            tag = "DW_TAG_union_type";
        }

        meta << "!" << type_id << " = distinct !DICompositeType("
             << "tag: " << tag << ", "
             << "name: \"" << type_name << "\", "
             << "scope: !" << file_id_ << ", "
             << "file: !" << file_id_ << ", "
             << "size: " << offset_bits << ", "
             << "elements: !" << elements_id << ")\n";
        debug_metadata_.push_back(meta.str());

        return type_id;
    }

    // ---- Primitive / fallback types → DIBasicType ----

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
    } else {
        // Default to 64-bit for unknown types
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

int LLVMIRGen::create_lexical_block(uint32_t line, uint32_t column) {
    if (!options_.emit_debug_info || current_scope_id_ == 0) {
        return 0;
    }

    // Push current scope onto the stack so pop_debug_scope() can restore it
    debug_scope_stack_.push_back(current_scope_id_);

    int block_id = fresh_debug_id();
    std::ostringstream meta;
    meta << "!" << block_id << " = distinct !DILexicalBlock("
         << "scope: !" << current_scope_id_ << ", "
         << "file: !" << file_id_ << ", "
         << "line: " << line << ", "
         << "column: " << column << ")\n";
    debug_metadata_.push_back(meta.str());

    current_scope_id_ = block_id;
    return block_id;
}

void LLVMIRGen::pop_debug_scope() {
    if (!options_.emit_debug_info || debug_scope_stack_.empty()) {
        return;
    }
    current_scope_id_ = debug_scope_stack_.back();
    debug_scope_stack_.pop_back();
}

} // namespace tml::codegen
