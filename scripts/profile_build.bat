@echo off
setlocal enabledelayedexpansion

echo ========================================
echo    TML Build Performance Profiling
echo ========================================
echo.

REM Checar se está rodando como admin
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script requires Administrator privileges
    echo Please run Command Prompt as Administrator and try again
    exit /b 1
)

REM Checar se wpr.exe existe
where wpr.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Windows Performance Toolkit not found
    echo Install from: https://learn.microsoft.com/en-us/windows-hardware/test/wpt/
    exit /b 1
)

REM Criar diretório para output
if not exist "build\profiles" mkdir "build\profiles"

REM Limpar traces anteriores
echo Cleaning previous traces...
wpr.exe -cancel >nul 2>&1

REM Iniciar trace com timeout maior
echo Starting performance trace (CPU profiling)...
wpr.exe -start CPU -filemode -level verbose -buffersize 1024 -minbuffers 100 -maxbuffers 1024

REM Checar se trace iniciou corretamente
if %errorlevel% neq 0 (
    echo ERROR: Failed to start performance trace
    echo Make sure Windows Performance Toolkit is installed and you have admin rights
    exit /b 1
)

REM Rodar build
echo.
echo Building TML compiler...
echo.
call scripts\build.bat

REM Parar trace e salvar
echo.
echo Stopping performance trace...
set TIMESTAMP=%date:~-4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TRACEFILE=build\profiles\tml_build_%TIMESTAMP%.etl

wpr.exe -stop "%TRACEFILE%"

REM Checar se trace foi salvo
if %errorlevel% neq 0 (
    echo ERROR: Failed to stop performance trace and save to %TRACEFILE%
    exit /b 1
)

echo.
echo ========================================
echo Build Performance Profile Saved
echo ========================================
echo.
if exist "%TRACEFILE%" (
    for %%F in ("%TRACEFILE%") do (
        echo File: %%~nxF
        echo Size: %%~zF bytes
        echo Location: %TRACEFILE%
    )
    echo.
    echo To analyze:
    echo   1. Open Windows Performance Analyzer (wpa.exe)
    echo   2. Open the .etl file
    echo   3. Look at:
    echo      - CPU Usage (Sampled) for hotspots
    echo      - Disk I/O for build time breakdown
    echo      - Context Switches for parallelism
    echo      - Process Lifetime for msvsvc.exe, cl.exe, link.exe
) else (
    echo ERROR: Trace file was not created!
    echo Check the error messages above.
)
echo ========================================

endlocal
