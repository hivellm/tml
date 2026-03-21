# Path Selection Forces Legacy for Any File with Imports

**Category**: architecture
**Tags**: codegen, architecture, mir, legacy, path-selection

## Description

At query_core.cpp:616-620, files with 'use' imports or local generics are routed to the AST/legacy path. Since virtually all real-world TML code has imports, the MIR path only handles simple standalone files. This severely limits MIR adoption and means the legacy path remains the de facto primary codegen for production code, despite MIR being architecturally superior.

## When NOT to Use

This routing should be expanded to send more code through the MIR path. The goal should be MIR handling all code, with legacy as explicit fallback only for unsupported features.
