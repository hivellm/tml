@echo off
:: TML Build Cleanup Script
:: Usage: scripts\clean.bat [--all]

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%ROOT_DIR%\build"

if /i "%~1"=="--all" (
    echo Removing entire build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo Done.
) else (
    echo Cleaning build artifacts...
    if exist "%BUILD_DIR%\debug" rmdir /s /q "%BUILD_DIR%\debug"
    if exist "%BUILD_DIR%\release" rmdir /s /q "%BUILD_DIR%\release"
    echo Done. (Cache preserved. Use --all to remove everything)
)
