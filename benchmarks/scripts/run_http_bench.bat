@echo off
REM HTTP Server Simulation Benchmark Runner
REM Compares TML, Rust, and C++ performance for HTTP object handling

setlocal EnableDelayedExpansion

echo ============================================================
echo HTTP Server Simulation Benchmark
echo ============================================================
echo.

set "ROOT=%~dp0.."
set "TML_COMPILER=%ROOT%\..\build\release\tml.exe"
set "RESULTS=%ROOT%\results"

if not exist "%RESULTS%" mkdir "%RESULTS%"

echo [1/3] Running TML Benchmark...
echo ------------------------------------------------------------

if not exist "%TML_COMPILER%" (
    echo ERROR: TML compiler not found at %TML_COMPILER%
    echo Please build with: scripts\build.bat release
    goto :cpp_bench
)

pushd "%ROOT%\tml"
"%TML_COMPILER%" build http_server_bench.tml --release -O3 --no-cache
if exist "http_server_bench.exe" (
    echo.
    http_server_bench.exe
    del http_server_bench.exe 2>nul
) else (
    echo TML build failed
)
popd

:cpp_bench
echo.
echo [2/3] Running C++ Benchmark...
echo ------------------------------------------------------------

where g++ >nul 2>&1
if errorlevel 1 (
    echo WARNING: g++ not found, skipping C++ benchmark
    goto :rust_bench
)

pushd "%ROOT%\cpp"
g++ -O3 -std=c++17 -o http_server_bench.exe http_server_bench.cpp
if exist "http_server_bench.exe" (
    echo.
    http_server_bench.exe
    del http_server_bench.exe 2>nul
) else (
    echo C++ build failed
)
popd

:rust_bench
echo.
echo [3/3] Running Rust Benchmark...
echo ------------------------------------------------------------

where cargo >nul 2>&1
if errorlevel 1 (
    echo WARNING: cargo not found, skipping Rust benchmark
    goto :done
)

pushd "%ROOT%\rust"
cargo bench --bench http_server_bench -- --noplot 2>nul
popd

:done
echo.
echo ============================================================
echo Benchmark Complete
echo ============================================================

endlocal
