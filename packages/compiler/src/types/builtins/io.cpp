// Builtin IO functions: print, println, panic
#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_io() {
    SourceSpan builtin_span{};

    // print(message: Str) -> Unit
    functions_["print"].push_back(FuncSig{"print",
                                          {make_primitive(PrimitiveKind::Str)},
                                          make_unit(),
                                          {},
                                          false,
                                          builtin_span,
                                          StabilityLevel::Stable,
                                          "",
                                          "1.0"});

    // println(message: Str) -> Unit
    functions_["println"].push_back(FuncSig{"println",
                                            {make_primitive(PrimitiveKind::Str)},
                                            make_unit(),
                                            {},
                                            false,
                                            builtin_span,
                                            StabilityLevel::Stable,
                                            "",
                                            "1.0"});

    // panic(message: Str) -> Never
    functions_["panic"].push_back(FuncSig{"panic",
                                          {make_primitive(PrimitiveKind::Str)},
                                          make_unit(), // TODO: Should be Never type
                                          {},
                                          false,
                                          builtin_span,
                                          StabilityLevel::Stable,
                                          "",
                                          "1.0"});

    // assert(condition: Bool, message: Str) -> Unit
    functions_["assert"].push_back(
        FuncSig{"assert",
                {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Str)},
                make_unit(),
                {},
                false,
                builtin_span,
                StabilityLevel::Stable,
                "",
                "1.0"});
}

} // namespace tml::types
