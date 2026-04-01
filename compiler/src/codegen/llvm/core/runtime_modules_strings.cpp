TML_MODULE("codegen_x86")

//! # LLVM IR Generator - String Constants Codegen
//!
//! This file emits global string literal constants into LLVM IR.
//! Extracted from runtime_modules.cpp to keep individual files manageable.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Emits all string literal constants collected during codegen as LLVM global constants.
void LLVMIRGen::emit_string_constants() {
    if (string_literals_.empty())
        return;

    emit_line("; String constants");
    for (const auto& [name, value] : string_literals_) {
        // Escape the string and add null terminator
        std::string escaped;
        for (char c : value) {
            if (c == '\n')
                escaped += "\\0A";
            else if (c == '\r')
                escaped += "\\0D";
            else if (c == '\t')
                escaped += "\\09";
            else if (c == '\0')
                escaped += "\\00";
            else if (c == '\\')
                escaped += "\\5C";
            else if (c == '"')
                escaped += "\\22";
            else if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E)
                escaped += "\\" + std::string(1, "0123456789ABCDEF"[(unsigned char)c >> 4]) +
                           std::string(1, "0123456789ABCDEF"[(unsigned char)c & 0xF]);
            else
                escaped += c;
        }
        escaped += "\\00";

        emit_line(name + " = private constant [" + std::to_string(value.size() + 1) + " x i8] c\"" +
                  escaped + "\"");
    }
    emit_line("");
}

} // namespace tml::codegen
