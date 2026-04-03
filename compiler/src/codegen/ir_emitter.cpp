TML_MODULE("codegen_x86")

//! # Typed IR Emission Helper — Implementation
//!
//! Implements the IREmitter methods declared in ir_emitter.hpp.
//! Each method validates its inputs (e.g., type != "void" for loads)
//! and emits the corresponding LLVM IR instruction.

#include "codegen/ir_emitter.hpp"

namespace tml::codegen {

void IREmitter::emit_load_to(const std::string& result_reg, const std::string& type,
                             const std::string& ptr_reg, bool is_volatile, int align) {
    // Can't load void or unit types — they have no value representation.
    assert(type != "void" && "Cannot load void type");
    assert(type != "{}" && "Cannot load unit type {}");
    assert(!type.empty() && "Cannot load empty type");
    assert(!result_reg.empty() && "Load requires a result register");

    std::string volatile_kw = is_volatile ? "volatile " : "";
    std::string line = result_reg + " = load " + volatile_kw + type + ", ptr " + ptr_reg;
    if (align > 0) {
        line += ", align " + std::to_string(align);
    }
    emit_line(line);
}

auto IREmitter::emit_load(const std::string& type, const std::string& ptr_reg, bool is_volatile,
                          int align) -> std::string {
    std::string reg = fresh_reg();
    emit_load_to(reg, type, ptr_reg, is_volatile, align);
    return reg;
}

void IREmitter::emit_store(const std::string& type, const std::string& value,
                           const std::string& ptr_reg, bool is_volatile, int align) {
    // Can't store void — it has no value representation.
    assert(type != "void" && "Cannot store void type");
    assert(!type.empty() && "Cannot store empty type");

    // Unit type "{}" is zero-sized. Skip the store entirely — there's no data to write.
    if (type == "{}") {
        emit_line("; skip store of {} (unit type)");
        return;
    }

    std::string volatile_kw = is_volatile ? "volatile " : "";
    std::string line = "store " + volatile_kw + type + " " + value + ", ptr " + ptr_reg;
    if (align > 0) {
        line += ", align " + std::to_string(align);
    }
    emit_line(line);
}

void IREmitter::emit_alloca_to(const std::string& result_reg, const std::string& type, int align) {
    // Can't alloca void — it has no size.
    assert(type != "void" && "Cannot alloca void type");
    assert(!type.empty() && "Cannot alloca empty type");
    assert(!result_reg.empty() && "Alloca requires a result register");

    std::string line = result_reg + " = alloca " + type;
    if (align > 0) {
        line += ", align " + std::to_string(align);
    }
    emit_line(line);
}

auto IREmitter::emit_alloca(const std::string& type, int align) -> std::string {
    std::string reg = fresh_reg();
    emit_alloca_to(reg, type, align);
    return reg;
}

void IREmitter::emit_gep_to(const std::string& result_reg, const std::string& base_type,
                            const std::string& ptr_reg,
                            const std::vector<std::pair<std::string, std::string>>& typed_indices,
                            bool inbounds) {
    assert(!base_type.empty() && "GEP requires a base type");
    assert(!typed_indices.empty() && "GEP requires at least one index");
    assert(!result_reg.empty() && "GEP requires a result register");

    std::string line = result_reg + " = getelementptr ";
    if (inbounds) {
        line += "inbounds ";
    }
    line += base_type + ", ptr " + ptr_reg;
    for (const auto& [idx_type, idx_val] : typed_indices) {
        line += ", " + idx_type + " " + idx_val;
    }
    emit_line(line);
}

auto IREmitter::emit_gep(const std::string& base_type, const std::string& ptr_reg,
                         const std::vector<std::pair<std::string, std::string>>& typed_indices,
                         bool inbounds) -> std::string {
    std::string reg = fresh_reg();
    emit_gep_to(reg, base_type, ptr_reg, typed_indices, inbounds);
    return reg;
}

auto IREmitter::emit_call(const std::string& ret_type, const std::string& func_name,
                          const std::string& args_str) -> std::string {
    assert(!ret_type.empty() && "Call requires a return type");
    assert(!func_name.empty() && "Call requires a function name");

    if (ret_type == "void") {
        emit_line("call void @" + func_name + "(" + args_str + ")");
        return "";
    }

    std::string reg = fresh_reg();
    emit_line(reg + " = call " + ret_type + " @" + func_name + "(" + args_str + ")");
    return reg;
}

void IREmitter::emit_memcpy(const std::string& dst, const std::string& src, const std::string& size,
                            bool is_volatile) {
    std::string vol = is_volatile ? "true" : "false";
    emit_line("call void @llvm.memcpy.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " + size +
              ", i1 " + vol + ")");
}

void IREmitter::emit_memset(const std::string& dst, const std::string& val, const std::string& size,
                            bool is_volatile) {
    std::string vol = is_volatile ? "true" : "false";
    emit_line("call void @llvm.memset.p0.i64(ptr " + dst + ", i8 " + val + ", i64 " + size +
              ", i1 " + vol + ")");
}

void IREmitter::emit_memmove(const std::string& dst, const std::string& src,
                             const std::string& size, bool is_volatile) {
    std::string vol = is_volatile ? "true" : "false";
    emit_line("call void @llvm.memmove.p0.p0.i64(ptr " + dst + ", ptr " + src + ", i64 " + size +
              ", i1 " + vol + ")");
}

} // namespace tml::codegen
