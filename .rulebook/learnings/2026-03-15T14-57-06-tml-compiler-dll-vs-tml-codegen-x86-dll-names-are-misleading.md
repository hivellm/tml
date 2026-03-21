# tml_compiler.dll vs tml_codegen_x86.dll — names are misleading
**Source**: manual
**Date**: 2026-03-15
**Tags**: build, plugins, dll, gotcha
The naming is counterintuitive: tml_codegen_x86.dll (~78MB) sounds like it does TML→IR codegen, but it only does IR→object (LLVMBackend) + LLD linker. tml_compiler.dll (~104MB) contains ALL compiler code including TML→IR (LLVMIRGen, gen_call_generic_struct_method, etc.). When fixing LEGACY codegen bugs, rebuild tml_compiler_plugin NOT tml_codegen_x86_plugin. Command: cmd //c "scripts\\build.bat --target tml_compiler_plugin". If DLL locked by running process, use os.rename to free the filename.