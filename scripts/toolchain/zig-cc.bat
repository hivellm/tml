@echo off
:: Zig CC wrapper for CMake
:: Strips unsupported MSVC linker args from -Xlinker flags
setlocal enabledelayedexpansion
set "CMD=%*"
set "CMD=!CMD:-Xlinker /MANIFEST:EMBED=!"
set "CMD=!CMD:-Xlinker /version:0.0=!"
zig cc -target x86_64-windows-msvc !CMD!
