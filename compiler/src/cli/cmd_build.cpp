// cmd_build.cpp - Entry point for build commands
// Implementation split into builder/ directory for maintainability:
//   - builder/helpers.cpp      - Shared utilities (hashing, diagnostics, etc.)
//   - builder/build.cpp        - run_build() and run_build_ex()
//   - builder/run.cpp          - run_run() and run_run_quiet()
//   - builder/run_profiled.cpp - run_run_profiled() with phase timing

#include "cmd_build.hpp"

// All implementations are in builder/*.cpp files
// This file exists only to document the module structure
// and maintain backwards compatibility with the include path.

namespace tml::cli {

// Function implementations are in:
// - run_build()        -> builder/build.cpp
// - run_build_ex()     -> builder/build.cpp
// - run_run()          -> builder/run.cpp
// - run_run_quiet()    -> builder/run.cpp
// - run_run_profiled() -> builder/run_profiled.cpp

} // namespace tml::cli
