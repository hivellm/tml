TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Collection Methods
//!
//! This file previously implemented instance methods for collection types
//! (List, HashMap, Buffer). All collection types are now pure TML:
//!
//! - List[T]: see lib/std/src/collections/list.tml
//! - HashMap[K,V]: see lib/std/src/collections/hashmap.tml
//! - Buffer: see lib/std/src/collections/buffer.tml
//!
//! This function now always returns nullopt, letting the normal
//! impl dispatch handle all collection method calls.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_collection_method(const parser::MethodCallExpr& call,
                                      const std::string& receiver,
                                      const std::string& receiver_type_name,
                                      types::TypePtr receiver_type) -> std::optional<std::string> {
    (void)call;
    (void)receiver;
    (void)receiver_type_name;
    (void)receiver_type;
    // All collection types (List, HashMap, Buffer) are now pure TML.
    // Method calls go through normal impl dispatch.
    return std::nullopt;
}

} // namespace tml::codegen
