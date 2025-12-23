@echo off
:: TML Build Cleanup Script
:: Usage: scripts\clean.bat [--all]

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%ROOT_DIR%\build"

:: Detect target triple (Windows MSVC)
set "TARGET=x86_64-pc-windows-msvc"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=aarch64-pc-windows-msvc"

if /i "%~1"=="--all" (
    echo Removing entire build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo Done.
) else if /i "%~1"=="--help" (
    echo TML Build Cleanup Script
    echo.
    echo Usage: scripts\clean.bat [options]
    echo.
    echo Options:
    echo   --all    Remove entire build directory (all targets)
    echo   --help   Show this help message
    echo.
    echo Without options: cleans current host target (%TARGET%)
) else (
    echo Cleaning host target: %TARGET%...
    if exist "%BUILD_DIR%\%TARGET%" rmdir /s /q "%BUILD_DIR%\%TARGET%"
    echo Done.
)
