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

REM Checar se xperf.exe existe
where xperf.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Windows Performance Toolkit not found
    echo Install from: https://learn.microsoft.com/en-us/windows-hardware/test/wpt/
    exit /b 1
)

REM Criar diretório para output
if not exist "build\profiles" mkdir "build\profiles"

REM Parar qualquer trace anterior
echo Stopping any previous traces...
xperf.exe -stop >nul 2>&1

REM Gerar timestamp para arquivo
set TIMESTAMP=%date:~-4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TRACEFILE=build\profiles\tml_build_%TIMESTAMP%.etl

REM Iniciar trace com xperf (mais direto que wpr)
echo Starting performance trace...
xperf.exe -on PROC_THREAD+LOADER+DISK_IO+HARD_FAULTS -f "%TRACEFILE%"

REM Rodar build
echo.
echo Building TML compiler...
echo.
call scripts\build.bat

REM Parar trace e salvar
echo.
echo Stopping performance trace...
xperf.exe -stop

REM Checar se trace foi salvo
if exist "%TRACEFILE%" (
    echo Trace file created successfully
) else (
    echo ERROR: Trace file was not created!
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
