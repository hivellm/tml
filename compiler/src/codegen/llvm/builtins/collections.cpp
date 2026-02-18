//! # LLVM IR Generator - Collection Builtins
//!
//! All collection types (List, HashMap, Buffer) are now pure TML.
//! This function always returns nullopt.
//!
//! - List[T]: see lib/std/src/collections/list.tml
//! - HashMap[K,V]: see lib/std/src/collections/hashmap.tml
//! - Buffer: see lib/std/src/collections/buffer.tml

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_collections(const std::string& fn_name,
                                            const parser::CallExpr& call)
    -> std::optional<std::string> {
    (void)fn_name;
    (void)call;
    // All collection builtins removed — now pure TML
    return std::nullopt;
}

} // namespace tml::codegen
