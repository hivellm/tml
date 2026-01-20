@echo off
REM JSON Benchmark Runner - Compares all implementations
REM Run from: benchmarks\scripts\

setlocal enabledelayedexpansion

echo ============================================================
echo JSON Benchmark Suite - Multi-Language Comparison
echo ============================================================
echo.

set RESULTS_DIR=..\results\json
if not exist %RESULTS_DIR% mkdir %RESULTS_DIR%

set TIMESTAMP=%date:~-4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%
set TIMESTAMP=%TIMESTAMP: =0%

echo Results will be saved to: %RESULTS_DIR%
echo.

REM ============================================================
REM Build TML JSON Benchmark (C++)
REM ============================================================
echo.
echo [1/5] Building TML C++ JSON Benchmark...
echo.

pushd ..\cpp
if not exist json_bench.exe (
    clang++ -O3 -std=c++20 -I..\..\compiler\include -I..\..\compiler\src ^
        json_bench.cpp ^
        ..\..\compiler\src\json\json_value.cpp ^
        ..\..\compiler\src\json\json_parser.cpp ^
        ..\..\compiler\src\json\json_serializer.cpp ^
        ..\..\compiler\src\json\json_builder.cpp ^
        -o json_bench.exe
    if errorlevel 1 (
        echo ERROR: Failed to build TML JSON benchmark
        popd
        goto :error
    )
)
echo TML C++ benchmark built successfully.
popd

REM ============================================================
REM Run TML C++ Benchmark
REM ============================================================
echo.
echo [2/5] Running TML C++ JSON Benchmark...
echo.

pushd ..\cpp
json_bench.exe > %RESULTS_DIR%\tml_cpp_%TIMESTAMP%.txt 2>&1
if errorlevel 1 (
    echo WARNING: TML benchmark returned non-zero exit code
)
type %RESULTS_DIR%\tml_cpp_%TIMESTAMP%.txt
popd

REM ============================================================
REM Run Python Benchmark
REM ============================================================
echo.
echo [3/5] Running Python JSON Benchmark...
echo.

where python >nul 2>&1
if errorlevel 1 (
    echo WARNING: Python not found, skipping Python benchmark
) else (
    pushd ..\python
    python json_bench.py > %RESULTS_DIR%\python_%TIMESTAMP%.txt 2>&1
    if errorlevel 1 (
        echo WARNING: Python benchmark returned non-zero exit code
    )
    type %RESULTS_DIR%\python_%TIMESTAMP%.txt
    popd
)

REM ============================================================
REM Run Node.js Benchmark
REM ============================================================
echo.
echo [4/5] Running Node.js JSON Benchmark...
echo.

where node >nul 2>&1
if errorlevel 1 (
    echo WARNING: Node.js not found, skipping Node.js benchmark
) else (
    pushd ..\node
    node json_bench.js > %RESULTS_DIR%\nodejs_%TIMESTAMP%.txt 2>&1
    if errorlevel 1 (
        echo WARNING: Node.js benchmark returned non-zero exit code
    )
    type %RESULTS_DIR%\nodejs_%TIMESTAMP%.txt
    popd
)

REM ============================================================
REM Run Rust Benchmark
REM ============================================================
echo.
echo [5/5] Running Rust JSON Benchmark...
echo.

where cargo >nul 2>&1
if errorlevel 1 (
    echo WARNING: Cargo not found, skipping Rust benchmark
) else (
    pushd ..\rust\json_bench
    cargo build --release >nul 2>&1
    if errorlevel 1 (
        echo WARNING: Failed to build Rust benchmark
    ) else (
        target\release\json_bench.exe > ..\..\..\results\json\rust_%TIMESTAMP%.txt 2>&1
        if errorlevel 1 (
            echo WARNING: Rust benchmark returned non-zero exit code
        )
        type ..\..\..\results\json\rust_%TIMESTAMP%.txt
    )
    popd
)

REM ============================================================
REM Summary
REM ============================================================
echo.
echo ============================================================
echo Benchmark Complete!
echo ============================================================
echo.
echo Results saved to:
echo   - %RESULTS_DIR%\tml_cpp_%TIMESTAMP%.txt
echo   - %RESULTS_DIR%\python_%TIMESTAMP%.txt
echo   - %RESULTS_DIR%\nodejs_%TIMESTAMP%.txt
echo   - %RESULTS_DIR%\rust_%TIMESTAMP%.txt
echo.

goto :end

:error
echo.
echo Benchmark failed with errors.
exit /b 1

:end
endlocal
