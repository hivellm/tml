@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64 >nul 2>&1

set OUTDIR=..\results\bin
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set CFLAGS=/O2 /EHsc /std:c++17 /DNDEBUG /I..\..\compiler\include /nologo

echo Building C++ benchmarks (MSVC /O2)...

cl %CFLAGS% /Fe:"%OUTDIR%\math_bench_cpp.exe" math_bench.cpp /link /NOLOGO
cl %CFLAGS% /Fe:"%OUTDIR%\string_bench_cpp.exe" string_bench.cpp /link /NOLOGO
cl %CFLAGS% /Fe:"%OUTDIR%\control_flow_bench_cpp.exe" control_flow_bench.cpp /link /NOLOGO
cl %CFLAGS% /Fe:"%OUTDIR%\closure_bench_cpp.exe" closure_bench.cpp /link /NOLOGO
cl %CFLAGS% /Fe:"%OUTDIR%\function_bench_cpp.exe" function_bench.cpp /link /NOLOGO
cl %CFLAGS% /Fe:"%OUTDIR%\collections_bench_cpp.exe" collections_bench.cpp /link /NOLOGO
cl %CFLAGS% /Fe:"%OUTDIR%\oop_bench_cpp.exe" oop_bench.cpp /link /NOLOGO

echo Done!
dir /b "%OUTDIR%\*_cpp.exe"
