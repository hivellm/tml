# Link flag accumulation is manual and module-registry-driven
**Source**: manual
**Date**: 2026-03-15
**Tags**: build, testing, tech-debt, linking
compile_suite() in testing_compile.cpp manually checks registry->has_module("std::net"), registry->has_module("std::sqlite"), etc. to decide which vcpkg libraries to link. This grows linearly with each new stdlib module and is not centralized. The same logic must be duplicated between test compilation and regular build paths. Should be refactored into a centralized module→link-flags mapping.