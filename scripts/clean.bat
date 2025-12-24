@echo off
:: TML Build Cleanup Script
:: Usage: scripts\clean.bat [--all] [--cache]

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%ROOT_DIR%\build"

:: Detect target triple for cache cleaning
set "TARGET=x86_64-pc-windows-msvc"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "TARGET=aarch64-pc-windows-msvc"

if /i "%~1"=="--all" (
    echo Removing entire build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo Done.
) else if /i "%~1"=="--cache" (
    echo Cleaning build cache for %TARGET%...
    if exist "%BUILD_DIR%\cache\%TARGET%" rmdir /s /q "%BUILD_DIR%\cache\%TARGET%"
    echo Done.
) else if /i "%~1"=="--help" (
    echo TML Build Cleanup Script
    echo.
    echo Usage: scripts\clean.bat [options]
    echo.
    echo Options:
    echo   --all    Remove entire build directory
    echo   --cache  Only clean build cache for current target
    echo   --help   Show this help message
    echo.
    echo Without options: cleans outputs and cache for current target
) else (
    echo Cleaning build outputs and cache...
    if exist "%BUILD_DIR%\debug" rmdir /s /q "%BUILD_DIR%\debug"
    if exist "%BUILD_DIR%\release" rmdir /s /q "%BUILD_DIR%\release"
    if exist "%BUILD_DIR%\cache\%TARGET%" rmdir /s /q "%BUILD_DIR%\cache\%TARGET%"
    echo Done.
)
