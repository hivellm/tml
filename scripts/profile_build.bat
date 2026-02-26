@echo off
setlocal enabledelayedexpansion

echo ========================================
echo    TML Build Performance Profiling
echo ========================================
echo.

REM Checar se wpr.exe existe
where wpr.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Windows Performance Toolkit not found
    echo Install from: https://learn.microsoft.com/en-us/windows-hardware/test/wpt/
    exit /b 1
)

REM Criar diretório para output
if not exist "build\profiles" mkdir "build\profiles"

REM Iniciar trace
echo Starting performance trace...
wpr.exe -start CPU -filemode

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

echo.
echo ========================================
echo Trace saved to: %TRACEFILE%
echo.
echo To analyze:
echo   1. Open Windows Performance Analyzer (wpa.exe)
echo   2. Open the .etl file
echo   3. Look at CPU Usage and Compiler processes
echo ========================================

endlocal
