//! # Token list deserializer (phase12f hybrid pipeline)
//!
//! Reads a binary `TokenizeResult` produced by a TML lexer stage. Format
//! defined in `docs/specs/33-SERIAL-FORMAT.md` "Token List Format".
//! Magic: `0x544D4C4C` ("TMLL").

#ifndef TML_SERIAL_TOKEN_READER_HPP
#define TML_SERIAL_TOKEN_READER_HPP

#include "query/query_key.hpp"

#include <cstdint>
#include <vector>

namespace tml::serial {

/// Deserialize a token list blob produced by a TML lexer stage subprocess.
/// Returns a populated `TokenizeResult` (with `success=true`) on success,
/// or a result with `success=false` and one or more entries in `errors` on
/// any I/O / format error.
query::TokenizeResult read_tokens(const std::vector<uint8_t>& bytes);

} // namespace tml::serial

#endif
