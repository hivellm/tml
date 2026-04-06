@echo off
:: Zig C++ wrapper for CMake
:: -nostdlib++: use MSVC C++ stdlib, not Zig's bundled libc++
:: -Wno-unused-command-line-argument: suppress -nostdinc++ warning from -nostdlib++
:: Strips unsupported MSVC linker args
setlocal enabledelayedexpansion
set "CMD=%*"
set "CMD=!CMD:-Xlinker /MANIFEST:EMBED=!"
set "CMD=!CMD:-Xlinker /version:0.0=!"
zig c++ -target x86_64-windows-msvc -nostdlib++ -Wno-unused-command-line-argument !CMD!
