@echo off
REM OOP Benchmark Runner - Compares TML, Rust, and C++ performance
REM Run from: benchmarks\scripts\run_oop_bench.bat

setlocal EnableDelayedExpansion

echo ============================================================
echo OOP Performance Benchmark Comparison
echo TML vs Rust vs C++
echo ============================================================
echo.

set "ROOT=%~dp0.."
set "TML_COMPILER=%ROOT%\..\build\release\tml.exe"
set "RESULTS=%ROOT%\results"
set "BIN=%RESULTS%\bin"

if not exist "%RESULTS%" mkdir "%RESULTS%"
if not exist "%BIN%" mkdir "%BIN%"

echo [1/3] Running TML Benchmark...
echo ------------------------------------------------------------

if not exist "%TML_COMPILER%" (
    echo ERROR: TML compiler not found at %TML_COMPILER%
    echo Please build with: scripts\build.bat release
    goto :cpp_bench
)

pushd "%ROOT%\tml"
echo Compiling oop_bench.tml with -O3...
"%TML_COMPILER%" build oop_bench.tml --release -O3 --no-cache -o "%BIN%\oop_bench_tml.exe"
if exist "%BIN%\oop_bench_tml.exe" (
    echo.
    echo --- TML Results ---
    "%BIN%\oop_bench_tml.exe"
) else (
    echo TML build failed
)
popd

:cpp_bench
echo.
echo [2/3] Running C++ Benchmark...
echo ------------------------------------------------------------

REM Try MSVC first
where cl >nul 2>&1
if not errorlevel 1 (
    pushd "%ROOT%\cpp"
    echo Compiling oop_bench.cpp with MSVC /O2...
    cl /O2 /EHsc /nologo oop_bench.cpp /Fe:"%BIN%\oop_bench_cpp.exe" >nul 2>&1
    del oop_bench.obj 2>nul
    if exist "%BIN%\oop_bench_cpp.exe" (
        echo.
        echo --- C++ Results (MSVC) ---
        "%BIN%\oop_bench_cpp.exe"
    ) else (
        echo C++ MSVC build failed
    )
    popd
    goto :rust_bench
)

REM Try g++
where g++ >nul 2>&1
if not errorlevel 1 (
    pushd "%ROOT%\cpp"
    echo Compiling oop_bench.cpp with g++ -O3...
    g++ -O3 -std=c++17 -o "%BIN%\oop_bench_cpp.exe" oop_bench.cpp
    if exist "%BIN%\oop_bench_cpp.exe" (
        echo.
        echo --- C++ Results (g++) ---
        "%BIN%\oop_bench_cpp.exe"
    ) else (
        echo C++ g++ build failed
    )
    popd
    goto :rust_bench
)

echo WARNING: No C++ compiler found (cl or g++), skipping C++ benchmark

:rust_bench
echo.
echo [3/3] Running Rust Benchmark...
echo ------------------------------------------------------------

where rustc >nul 2>&1
if errorlevel 1 (
    echo WARNING: rustc not found, skipping Rust benchmark
    goto :done
)

pushd "%ROOT%\rust"
echo Compiling oop_bench.rs with rustc -O...
rustc -O -o "%BIN%\oop_bench_rust.exe" oop_bench.rs
if exist "%BIN%\oop_bench_rust.exe" (
    echo.
    echo --- Rust Results ---
    "%BIN%\oop_bench_rust.exe"
) else (
    echo Rust build failed
)
popd

:done
echo.
echo ============================================================
echo Benchmark Complete - Binaries saved to: %BIN%
echo ============================================================

endlocal
