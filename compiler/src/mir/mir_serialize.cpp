// MIR Serialization
//
// This file provides serialization and deserialization of MIR modules.
// The implementation is split into modular components in the serializer/ directory:
//
// - serializer/serializer_internal.hpp - Common types and tags
// - serializer/binary_writer.cpp       - Binary format writer (~350 lines)
// - serializer/binary_reader.cpp       - Binary format reader (~400 lines)
// - serializer/text_writer.cpp         - Text format writer (~20 lines)
// - serializer/text_reader.cpp         - Text format reader (~400 lines)
// - serializer/serialize_utils.cpp     - Convenience functions (~70 lines)
//
// This file is kept minimal and serves as documentation for the module structure.

#include "mir/mir_serialize.hpp"

// All implementation is in the serializer/ subdirectory
