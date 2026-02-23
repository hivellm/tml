TML_MODULE("compiler")

#include "codegen/codegen_backend.hpp"

#include "codegen/llvm/llvm_codegen_backend.hpp"

#ifdef TML_HAS_CRANELIFT_BACKEND
#include "codegen/cranelift/cranelift_codegen_backend.hpp"
#endif

#include <stdexcept>

namespace tml::codegen {

auto create_backend(BackendType type) -> std::unique_ptr<CodegenBackend> {
    switch (type) {
    case BackendType::LLVM:
        return std::make_unique<LLVMCodegenBackend>();
    case BackendType::Cranelift:
#ifdef TML_HAS_CRANELIFT_BACKEND
        return std::make_unique<CraneliftCodegenBackend>();
#else
        throw std::runtime_error(
            "Cranelift backend is not available. Rebuild with -DTML_USE_CRANELIFT_BACKEND=ON");
#endif
    }
    throw std::runtime_error("Unknown backend type");
}

auto default_backend_type() -> BackendType {
    return BackendType::LLVM;
}

} // namespace tml::codegen
