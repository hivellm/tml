@echo off
setlocal enabledelayedexpansion

:: TML Compiler Build Script for Windows
:: Usage: scripts\build.bat [debug|release] [--clean] [--tests]

:: Get the root directory (parent of scripts\)
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"
set "ROOT_DIR=%CD%"

:: Detect target triple (Windows MSVC)
set "TARGET=x86_64-pc-windows-msvc"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=aarch64-pc-windows-msvc"

:: Default values
set "BUILD_TYPE=debug"
set "CLEAN_BUILD=0"
set "BUILD_TESTS=ON"

:: Parse arguments
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="debug" set "BUILD_TYPE=debug" & shift & goto :parse_args
if /i "%~1"=="release" set "BUILD_TYPE=release" & shift & goto :parse_args
if /i "%~1"=="--clean" set "CLEAN_BUILD=1" & shift & goto :parse_args
if /i "%~1"=="--no-tests" set "BUILD_TESTS=OFF" & shift & goto :parse_args
if /i "%~1"=="--tests" set "BUILD_TESTS=ON" & shift & goto :parse_args
if /i "%~1"=="--help" goto :show_help
if /i "%~1"=="-h" goto :show_help
echo Unknown argument: %~1
exit /b 1

:show_help
echo TML Compiler Build Script
echo.
echo Usage: scripts\build.bat [debug^|release] [options]
echo.
echo Build types:
echo   debug     Build with debug symbols (default)
echo   release   Build with optimizations
echo.
echo Options:
echo   --clean     Clean build directory before building
echo   --tests     Build tests (default)
echo   --no-tests  Don't build tests
echo   --help      Show this help message
echo.
echo Host target: %TARGET%
echo.
echo Output structure (like Rust's target/):
echo   build\^<target^>\debug\
echo   build\^<target^>\release\
exit /b 0

:args_done

:: Set build directory (target-specific, like Rust's target/<triple>/)
set "BUILD_DIR=%ROOT_DIR%\build\%TARGET%\%BUILD_TYPE%"

echo.
echo ========================================
echo        TML Compiler Build System
echo ========================================
echo.
echo Target:      %TARGET%
echo Build type:  %BUILD_TYPE%
echo Build dir:   %BUILD_DIR%
echo Tests:       %BUILD_TESTS%
echo.

:: Clean if requested
if "%CLEAN_BUILD%"=="1" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

:: Create directories
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: Set CMake build type
set "CMAKE_BUILD_TYPE=Debug"
if /i "%BUILD_TYPE%"=="release" set "CMAKE_BUILD_TYPE=Release"

:: Configure CMake
echo Configuring CMake...
cd /d "%BUILD_DIR%"

cmake "%ROOT_DIR%\packages\compiler" ^
    -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
    -DTML_BUILD_TESTS=%BUILD_TESTS% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

:: Build
echo.
echo Building TML compiler...

cmake --build . --config %CMAKE_BUILD_TYPE%

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

:: Copy compile_commands.json to root for IDE support
if exist "compile_commands.json" (
    copy /y "compile_commands.json" "%ROOT_DIR%\" >nul 2>&1
)

:: Print result
echo.
echo ========================================
echo          Build Complete!
echo ========================================
echo.
echo Compiler:    %BUILD_DIR%\%CMAKE_BUILD_TYPE%\tml.exe
if "%BUILD_TESTS%"=="ON" (
    echo Tests:       %BUILD_DIR%\%CMAKE_BUILD_TYPE%\tml_tests.exe
)
echo.

endlocal
