@echo off
:: Build just llvm-ar (needed by the TML runtime archiver)
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

where cl.exe >nul 2>&1
if not !errorlevel!==0 (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    )
)

cd build\llvm
cmake --build . --target llvm-ar --config Release
if errorlevel 1 (
    echo llvm-ar build failed.
    exit /b 1
)
echo.
echo llvm-ar.exe built at: %ROOT_DIR%\build\llvm\bin\llvm-ar.exe

endlocal
