// MIR Text Writer Implementation

#include "serializer_internal.hpp"

namespace tml::mir {

// ============================================================================
// MirTextWriter Implementation
// ============================================================================

MirTextWriter::MirTextWriter(std::ostream& out, SerializeOptions options)
    : out_(out), options_(options) {}

void MirTextWriter::write_module(const Module& module) {
    MirPrinter printer(!options_.compact);
    out_ << printer.print_module(module);
}

} // namespace tml::mir
