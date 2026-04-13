# Proposal: Wildcard and Grouped Imports

## Why

TML files currently require one `use` statement per imported symbol. A file like `mir_lower.tml` has 30+ import lines just from `compiler::mir::inst`. This is:
1. Extremely verbose — 30 lines of boilerplate before any code
2. Error-prone — forgetting one import causes a confusing "Undefined variable" error
3. Painful to maintain — adding a new type to a module means updating every file that imports from it

Every modern language solves this: Rust has `use module::{A, B, C}` and `use module::*`, Go has `import "package"` (imports all exports), Python has `from module import *` and `from module import A, B`.

## What Changes

### Syntax additions (parser + type checker)

**1. Module-level import (wildcard):**
```tml
use compiler::mir::inst
```
Imports ALL pub symbols from `compiler::mir::inst` into the current scope. Equivalent to writing every `use compiler::mir::inst::Symbol` line.

**2. Grouped import (curly braces):**
```tml
use compiler::mir::inst::{MirInst, InstructionData, BinaryInst, CallInst}
```
Imports only the listed symbols from the module. Reduces 13 lines to 1.

**3. Glob import (star):**
```tml
use compiler::mir::inst::*
```
Same as module-level import. Explicit syntax for "import everything pub".

### Implementation

- **Parser**: extend `parse_use_decl()` to handle `::*`, `::{A, B, C}`, and bare module path (no trailing `::Symbol`)
- **Type checker**: `load_native_module()` already loads all pub symbols from a module; grouped import filters to only the listed names; wildcard imports all
- **Module resolution**: bare `use compiler::mir::inst` triggers module load + import all pub symbols from that module's metadata

### Codebase migration

After implementation, migrate ALL compiler-tml, core, and std source files:
- Replace chains of single-symbol imports with grouped or wildcard imports
- Estimated reduction: ~3,000 lines removed across 135 source files

## Impact
- Affected specs: docs/specs/language-reference.md (import syntax section)
- Affected code: compiler/src/parser/parser.cpp (parse_use_decl), compiler/src/types/env_module_loading.cpp (import resolution), compiler-tml/src/**/*.tml (migration)
- Breaking change: NO (additive syntax, old single-symbol imports still work)
- User benefit: 5-10x fewer import lines, faster coding, less boilerplate
