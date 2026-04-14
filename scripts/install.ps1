#Requires -Version 5.1
<#
.SYNOPSIS
    Installs TML compiler to Program Files and registers it in PATH.

.DESCRIPTION
    Copies the build output (tml.exe, DLLs, plugins) to
    C:\Program Files\TML\ and adds it to the system PATH (if running
    as Administrator) or the user PATH (fallback).

.PARAMETER BuildType
    debug (default) or release

.PARAMETER InstallDir
    Override the default install directory.
    Default: C:\Program Files\TML

.EXAMPLE
    # From PowerShell (as admin):
    .\scripts\install.ps1 -BuildType release

    # From build.bat (automatic after build):
    powershell -ExecutionPolicy Bypass -File scripts\install.ps1 -BuildType debug
#>

param(
    [string]$BuildType  = "debug",
    [string]$InstallDir = "C:\Program Files\TML"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Resolve repo root (script lives in <root>\scripts\)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$BinDir    = Join-Path $RepoRoot "build\$BuildType\bin"

# Auto-elevate to Administrator when installing to a system directory
$IsAdmin = ([Security.Principal.WindowsPrincipal] `
            [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
                [Security.Principal.WindowsBuiltInRole]::Administrator)

$NeedsAdmin = $InstallDir -like "*Program Files*" -or $InstallDir -like "*Windows*"

if ($NeedsAdmin -and -not $IsAdmin) {
    Write-Host "Requesting Administrator privileges for install to '$InstallDir'..."
    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$($MyInvocation.MyCommand.Path)`"",
        "-BuildType", $BuildType,
        "-InstallDir", "`"$InstallDir`""
    )
    $proc = Start-Process powershell -ArgumentList $args -Verb RunAs -Wait -PassThru
    exit $proc.ExitCode
}

# Validate source
if (-not (Test-Path $BinDir)) {
    Write-Host ""
    Write-Host "ERROR: Build output not found at: $BinDir" -ForegroundColor Red
    Write-Host "       Run  scripts\build.bat $BuildType  first." -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path (Join-Path $BinDir "tml.exe"))) {
    Write-Host ""
    Write-Host "ERROR: tml.exe not found in $BinDir" -ForegroundColor Red
    exit 1
}

$PathScope = if ($IsAdmin) { "Machine" } else { "User" }

Write-Host ""
Write-Host "========================================"
Write-Host "        TML Compiler Installer"
Write-Host "========================================"
Write-Host ""
Write-Host "Source  : $BinDir"
Write-Host "Target  : $InstallDir"
Write-Host "PATH    : $PathScope scope"
if (-not $IsAdmin) {
    Write-Host ""
    Write-Host "NOTE: Not running as Administrator." -ForegroundColor Yellow
    Write-Host "      Installing to '$InstallDir' (may fail if dir requires admin)." -ForegroundColor Yellow
    Write-Host "      PATH will be set in User scope." -ForegroundColor Yellow
}
Write-Host ""

# Create install directory
try {
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
        Write-Host "Created: $InstallDir"
    }
} catch {
    Write-Host ""
    Write-Host "ERROR: Cannot create '$InstallDir': $_" -ForegroundColor Red
    Write-Host "       Run PowerShell as Administrator, or pass a different -InstallDir." -ForegroundColor Yellow
    exit 1
}

# Copy binaries and DLLs
Write-Host "Copying files..."

$Copied = 0

# Top-level files (exe, dll, pdb)
Get-ChildItem -Path $BinDir -File | ForEach-Object {
    $Dest = Join-Path $InstallDir $_.Name
    Copy-Item -Path $_.FullName -Destination $Dest -Force
    Write-Host "  OK  $($_.Name)"
    $Copied++
}

# plugins\ subdirectory (keep structure)
$PluginsSource = Join-Path $BinDir "plugins"
if (Test-Path $PluginsSource) {
    $PluginsDest = Join-Path $InstallDir "plugins"
    if (-not (Test-Path $PluginsDest)) {
        New-Item -ItemType Directory -Path $PluginsDest -Force | Out-Null
    }
    Get-ChildItem -Path $PluginsSource -File | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination (Join-Path $PluginsDest $_.Name) -Force
        Write-Host "  OK  plugins\$($_.Name)"
        $Copied++
    }
}

# lib\native\win-x64 (vcpkg native libs used by TML packages)
$NativeSource = Join-Path $RepoRoot "build\$BuildType\lib\native\win-x64"
if (Test-Path $NativeSource) {
    $NativeDest = Join-Path $InstallDir "lib\native\win-x64"
    if (-not (Test-Path $NativeDest)) {
        New-Item -ItemType Directory -Path $NativeDest -Force | Out-Null
    }
    Get-ChildItem -Path $NativeSource -File | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination (Join-Path $NativeDest $_.Name) -Force
        $Copied++
    }
    Write-Host "  OK  lib\native\win-x64\ ($( (Get-ChildItem $NativeSource -File).Count ) files)"
}

# docs\docs.json (API documentation index used by tml doc and MCP)
$DocsJson = Join-Path $RepoRoot "docs\docs.json"
if (Test-Path $DocsJson) {
    $DocsDest = Join-Path $InstallDir "docs"
    if (-not (Test-Path $DocsDest)) {
        New-Item -ItemType Directory -Path $DocsDest -Force | Out-Null
    }
    Copy-Item -Path $DocsJson -Destination (Join-Path $DocsDest "docs.json") -Force
    Write-Host "  OK  docs\docs.json"
    $Copied++
} else {
    Write-Host "  SKIP docs\docs.json (not found - run build first)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Copied $Copied file(s)."

# Register in PATH
Write-Host ""
Write-Host "Checking PATH..."

$CurrentPath = [Environment]::GetEnvironmentVariable("Path", $PathScope)
$Entries = $CurrentPath -split ";" | Where-Object { $_ -ne "" }

if ($Entries -contains $InstallDir) {
    Write-Host "  Already in $PathScope PATH - nothing to do."
} else {
    $NewPath = ($Entries + $InstallDir) -join ";"
    [Environment]::SetEnvironmentVariable("Path", $NewPath, $PathScope)
    Write-Host "  Added to $PathScope PATH: $InstallDir" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Restart your PowerShell terminal for the PATH change to take effect." -ForegroundColor Cyan
}

# Register TML MCP server in Claude Code global config (~/.claude/mcp.json)
# This makes the tml MCP available in every project without a local .mcp.json.
Write-Host ""
Write-Host "Registering TML MCP server in Claude Code global config..."
$ClaudeDir  = Join-Path $env:USERPROFILE ".claude"
$McpConfig  = Join-Path $ClaudeDir "mcp.json"
$DaemonPath = (Join-Path $InstallDir "tml_daemon.exe") -replace "\\", "\\\\"

if (-not (Test-Path $ClaudeDir)) {
    New-Item -ItemType Directory -Path $ClaudeDir -Force | Out-Null
}

$McpEntry = "{`n  `"mcpServers`": {`n    `"tml`": {`n      `"command`": `"$DaemonPath`",`n      `"args`": []`n    }`n  }`n}`n"

if (Test-Path $McpConfig) {
    try {
        $existing = Get-Content $McpConfig -Raw | ConvertFrom-Json
        $existing.mcpServers | Add-Member -NotePropertyName "tml" -NotePropertyValue ([pscustomobject]@{
            command = (Join-Path $InstallDir "tml_daemon.exe")
            args    = @()
        }) -Force
        $existing | ConvertTo-Json -Depth 10 | Set-Content $McpConfig -Encoding UTF8
        Write-Host "  Updated: $McpConfig" -ForegroundColor Green
    } catch {
        Set-Content -Path $McpConfig -Value $McpEntry -Encoding UTF8
        Write-Host "  Written: $McpConfig" -ForegroundColor Green
    }
} else {
    Set-Content -Path $McpConfig -Value $McpEntry -Encoding UTF8
    Write-Host "  Written: $McpConfig" -ForegroundColor Green
}

# Verify
Write-Host ""
$ExePath = Join-Path $InstallDir "tml.exe"
if (Test-Path $ExePath) {
    try {
        $VerOutput = & $ExePath --version 2>&1
        Write-Host "Installed: $VerOutput" -ForegroundColor Green
    } catch {
        Write-Host "Installed: $ExePath (could not run --version)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "        Install Complete!"
Write-Host "========================================"
Write-Host ""
