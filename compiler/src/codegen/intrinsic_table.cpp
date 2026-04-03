TML_MODULE("codegen_x86")

//! # Intrinsic Dispatch Table — Implementation
//!
//! Static table mapping intrinsic function names to IntrinsicKind + metadata.
//! Built once on first lookup call; subsequent lookups are O(1) hash probes.

#include "codegen/intrinsic_table.hpp"

#include <unordered_map>

namespace tml::codegen {

/// The canonical table of all intrinsic function names recognized by the MIR
/// codegen's emit_call_inst(). Aliases (e.g., "volatile_read" for PtrReadVolatile)
/// are separate entries so the lookup remains a single hash probe.
static const std::unordered_map<std::string, IntrinsicInfo>& intrinsic_table() {
    static const std::unordered_map<std::string, IntrinsicInfo> table = {
        // ---- Pointer operations ----
        {"ptr_read", {IntrinsicKind::PtrRead, 1, true}},
        {"ptr_write", {IntrinsicKind::PtrWrite, 2, false}},
        {"ptr_offset", {IntrinsicKind::PtrOffset, 2, true}},
        {"ptr_read_volatile", {IntrinsicKind::PtrReadVolatile, 1, true}},
        {"volatile_read", {IntrinsicKind::PtrReadVolatile, 1, true}},
        {"ptr_write_volatile", {IntrinsicKind::PtrWriteVolatile, 2, false}},
        {"volatile_write", {IntrinsicKind::PtrWriteVolatile, 2, false}},
        {"ptr_read_unaligned", {IntrinsicKind::PtrReadUnaligned, 1, true}},
        {"ptr_write_unaligned", {IntrinsicKind::PtrWriteUnaligned, 2, false}},

        // ---- Memory management ----
        {"mem_free", {IntrinsicKind::MemFree, 1, false}},

        // ---- Bulk memory operations ----
        {"copy_nonoverlapping", {IntrinsicKind::CopyNonOverlapping, 3, false}},
        {"memcpy", {IntrinsicKind::Memcpy, 3, false}},
        {"mem_copy", {IntrinsicKind::Memcpy, 3, false}},
        {"memmove", {IntrinsicKind::Memmove, 3, false}},
        {"mem_move", {IntrinsicKind::Memmove, 3, false}},
        {"copy", {IntrinsicKind::Memmove, 3, false}},
        {"memset", {IntrinsicKind::Memset, 2, false}},
        {"mem_set", {IntrinsicKind::Memset, 3, false}},
        {"mem_zero", {IntrinsicKind::MemZero, 2, false}},
        {"write_bytes", {IntrinsicKind::Memset, 3, false}},

        // ---- Math: single-arg LLVM intrinsics ----
        {"sqrt", {IntrinsicKind::Sqrt, 1, true}},
        {"sin", {IntrinsicKind::Sin, 1, true}},
        {"cos", {IntrinsicKind::Cos, 1, true}},
        {"log", {IntrinsicKind::Log, 1, true}},
        {"exp", {IntrinsicKind::Exp, 1, true}},
        {"floor", {IntrinsicKind::Floor, 1, true}},
        {"ceil", {IntrinsicKind::Ceil, 1, true}},
        {"round", {IntrinsicKind::Round, 1, true}},
        {"trunc", {IntrinsicKind::Trunc, 1, true}},
        {"fabs", {IntrinsicKind::Fabs, 1, true}},

        // ---- Math: two-arg LLVM intrinsics ----
        {"pow", {IntrinsicKind::Pow, 1, true}},
        {"minnum", {IntrinsicKind::Minnum, 1, true}},
        {"maxnum", {IntrinsicKind::Maxnum, 1, true}},
        {"copysign", {IntrinsicKind::Copysign, 1, true}},

        // ---- Math: three-arg LLVM intrinsics ----
        {"fma", {IntrinsicKind::Fma, 1, true}},

        // ---- Conversion ----
        {"to_string", {IntrinsicKind::ToString, 1, true}},
        {"debug_string", {IntrinsicKind::DebugString, 1, true}},

        // ---- Misc ----
        {"black_box", {IntrinsicKind::BlackBox, 1, true}},
        {"black_box_i64", {IntrinsicKind::BlackBoxI64, 1, true}},
        {"black_box_f64", {IntrinsicKind::BlackBoxF64, 1, true}},
        {"store_byte", {IntrinsicKind::StoreByte, 3, false}},
    };
    return table;
}

std::optional<IntrinsicInfo> lookup_intrinsic(const std::string& func_name) {
    const auto& table = intrinsic_table();
    auto it = table.find(func_name);
    if (it != table.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace tml::codegen
