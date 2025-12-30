@echo off
REM TML Language Benchmarks - Run All
REM
REM This script runs benchmarks for all languages and collects results

setlocal enabledelayedexpansion

echo ============================================================
echo TML Language Benchmarks
echo ============================================================
echo.

set ROOT=%~dp0..
set TML_EXE=%ROOT%\build\debug\tml.exe

REM Check if TML compiler exists
if not exist "%TML_EXE%" (
    echo [ERROR] TML compiler not found at %TML_EXE%
    echo Please build the compiler first: scripts\build.bat
    exit /b 1
)

echo [TML] Running TML benchmarks...
echo ------------------------------------------------------------

REM Compile and run TML benchmarks
for %%f in ("%~dp0tml\*.tml") do (
    if /i not "%%~nf"=="tml" (
        echo.
        echo Compiling %%~nf.tml...
        "%TML_EXE%" build "%%f"
        if errorlevel 1 (
            echo [ERROR] Failed to compile %%~nf.tml
        ) else (
            echo Running %%~nf...
            "%ROOT%\build\debug\%%~nf.exe"
        )
    )
)

echo.
echo [C++] Running C++ benchmarks...
echo ------------------------------------------------------------

REM Check for MSVC
where cl >nul 2>&1
if %errorlevel%==0 (
    pushd "%~dp0cpp"
    echo Compiling algorithms.cpp with MSVC...
    cl /O2 /EHsc /nologo algorithms.cpp /Fe:algorithms.exe >nul 2>&1
    if exist algorithms.exe (
        algorithms.exe
        del algorithms.exe algorithms.obj 2>nul
    ) else (
        echo [ERROR] Failed to compile with MSVC
    )
    popd
) else (
    REM Check for GCC
    where g++ >nul 2>&1
    if %errorlevel%==0 (
        pushd "%~dp0cpp"
        echo Compiling algorithms.cpp with g++...
        g++ -O3 -o algorithms.exe algorithms.cpp
        if exist algorithms.exe (
            algorithms.exe
            del algorithms.exe 2>nul
        ) else (
            echo [ERROR] Failed to compile with g++
        )
        popd
    ) else (
        echo [SKIP] No C++ compiler found (cl or g++)
    )
)

echo.
echo [Go] Running Go benchmarks...
echo ------------------------------------------------------------

where go >nul 2>&1
if %errorlevel%==0 (
    pushd "%~dp0go"
    echo Running main.go...
    go run main.go algorithms_test.go
    echo.
    echo Running Go benchmarks (short)...
    go test -bench=. -benchtime=100ms
    popd
) else (
    echo [SKIP] Go not found in PATH
)

echo.
echo [Rust] Rust benchmarks require manual run:
echo   cd benchmarks\rust
echo   cargo bench
echo.

echo ============================================================
echo Benchmarks complete!
echo ============================================================
