# Restart TML MCP Server
# This script kills any running tml.exe mcp process and restarts it

Write-Host "Restarting TML MCP Server..." -ForegroundColor Cyan

# Find and kill existing tml.exe processes that are running MCP
$tmlProcesses = Get-Process -Name "tml" -ErrorAction SilentlyContinue
if ($tmlProcesses) {
    Write-Host "Killing existing TML processes..." -ForegroundColor Yellow
    $tmlProcesses | Stop-Process -Force
    Start-Sleep -Milliseconds 500
    Write-Host "Killed $($tmlProcesses.Count) process(es)" -ForegroundColor Green
} else {
    Write-Host "No existing TML processes found" -ForegroundColor Gray
}

Write-Host ""
Write-Host "MCP server will be restarted automatically by Claude Code when needed." -ForegroundColor Green
Write-Host "You may need to reload the VS Code window (Ctrl+Shift+P -> 'Developer: Reload Window')" -ForegroundColor Yellow
