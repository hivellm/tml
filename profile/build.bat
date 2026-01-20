@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   TML Profile - Build All Benchmarks
echo ============================================
echo.

cd /d "%~dp0"

:: Create results directory
if not exist "results\bin" mkdir "results\bin"

echo --- Building C++ Benchmarks ---
echo.

:: Build C++ benchmarks
cd cpp

:: Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake not found. Please install CMake.
    exit /b 1
)

:: Create build directory
if not exist build mkdir build
cd build

:: Configure and build
cmake -G "Visual Studio 17 2022" -A x64 .. >nul 2>nul
if %ERRORLEVEL% neq 0 (
    cmake -G "Visual Studio 16 2019" -A x64 .. >nul 2>nul
    if %ERRORLEVEL% neq 0 (
        cmake .. >nul 2>nul
    )
)

cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo ERROR: C++ build failed
    exit /b 1
)

cd ..\..

echo.
echo --- Building TML Benchmarks ---
echo.

:: Build TML benchmarks
cd tml

:: Check for TML compiler
set TML_EXE=..\..\build\release\tml.exe
if not exist "%TML_EXE%" (
    set TML_EXE=..\..\build\debug\tml.exe
)
if not exist "%TML_EXE%" (
    echo ERROR: TML compiler not found. Run scripts\build.bat first.
    exit /b 1
)

:: Build each TML benchmark
for %%f in (*.tml) do (
    echo Building %%f...
    "%TML_EXE%" build "%%f" --release -o "..\results\bin\%%~nf.exe"
    if !ERRORLEVEL! neq 0 (
        echo WARNING: Failed to build %%f
    )
)

cd ..

echo.
echo ============================================
echo   Build Complete
echo ============================================
echo.
echo Binaries in: profile\results\bin\
dir /b results\bin\*.exe 2>nul

endlocal
