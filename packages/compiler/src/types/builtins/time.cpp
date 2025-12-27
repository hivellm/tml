// Builtin time functions
#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_time() {
    SourceSpan builtin_span{};

    // ============ Time Reading ============

    // time_ms() -> I32 - Current time in milliseconds (for benchmarking)
    functions_["time_ms"].push_back(
        FuncSig{"time_ms", {}, make_primitive(PrimitiveKind::I32), {}, false, builtin_span});

    // time_us() -> I64 - Current time in microseconds
    functions_["time_us"].push_back(
        FuncSig{"time_us", {}, make_primitive(PrimitiveKind::I64), {}, false, builtin_span});

    // time_ns() -> I64 - Current time in nanoseconds
    functions_["time_ns"].push_back(
        FuncSig{"time_ns", {}, make_primitive(PrimitiveKind::I64), {}, false, builtin_span});

    // ============ Sleep ============

    // sleep_ms(ms: I32) -> Unit - Sleep for milliseconds
    functions_["sleep_ms"].push_back(FuncSig{
        "sleep_ms", {make_primitive(PrimitiveKind::I32)}, make_unit(), {}, false, builtin_span});

    // sleep_us(us: I64) -> Unit - Sleep for microseconds
    functions_["sleep_us"].push_back(FuncSig{
        "sleep_us", {make_primitive(PrimitiveKind::I64)}, make_unit(), {}, false, builtin_span});

    // ============ Elapsed Time ============

    // elapsed_ms(start: I32) -> I32 - Calculate elapsed milliseconds
    functions_["elapsed_ms"].push_back(FuncSig{"elapsed_ms",
                                               {make_primitive(PrimitiveKind::I32)},
                                               make_primitive(PrimitiveKind::I32),
                                               {},
                                               false,
                                               builtin_span});

    // elapsed_us(start: I64) -> I64 - Calculate elapsed microseconds
    functions_["elapsed_us"].push_back(FuncSig{"elapsed_us",
                                               {make_primitive(PrimitiveKind::I64)},
                                               make_primitive(PrimitiveKind::I64),
                                               {},
                                               false,
                                               builtin_span});

    // elapsed_ns(start: I64) -> I64 - Calculate elapsed nanoseconds
    functions_["elapsed_ns"].push_back(FuncSig{"elapsed_ns",
                                               {make_primitive(PrimitiveKind::I64)},
                                               make_primitive(PrimitiveKind::I64),
                                               {},
                                               false,
                                               builtin_span});
}

} // namespace tml::types
