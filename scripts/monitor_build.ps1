# PowerShell script para monitorar build em tempo real
# Mostra CPU, Memory, e tempo de compilação por arquivo

param(
    [string]$BuildScript = "scripts\build.bat"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "    TML Build Performance Monitor" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Função para formatar bytes
function Format-Bytes {
    param([long]$bytes)
    if ($bytes -gt 1GB) { return "{0:N2} GB" -f ($bytes / 1GB) }
    if ($bytes -gt 1MB) { return "{0:N2} MB" -f ($bytes / 1MB) }
    return "{0:N2} KB" -f ($bytes / 1KB)
}

# Iniciar build
$startTime = Get-Date
Write-Host "Starting build at $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Yellow
Write-Host ""

# Rodar build e capturar output
$buildProcess = Start-Process -FilePath "cmd.exe" `
    -ArgumentList "/c `"$BuildScript`"" `
    -PassThru `
    -NoNewWindow `
    -RedirectStandardOutput ".sandbox\build_output.log" `
    -RedirectStandardError ".sandbox\build_errors.log"

# Monitorar durante build
$maxCPU = 0
$maxMemory = 0
$updateInterval = 2 # segundos

while (-not $buildProcess.HasExited) {
    # Pegar info de todos os msbuild.exe e cl.exe processes
    $buildProcs = Get-Process -Name "msbuild", "cl", "link" -ErrorAction SilentlyContinue |
        Measure-Object -Property CPU, WorkingSet -Sum

    $totalCPU = $buildProcs | Select-Object -ExpandProperty Sum | Select-Object -First 1
    $totalMemory = $buildProcs | Select-Object -ExpandProperty Sum | Select-Object -Last 1

    if ($totalCPU -gt $maxCPU) { $maxCPU = $totalCPU }
    if ($totalMemory -gt $maxMemory) { $maxMemory = $totalMemory }

    $elapsed = (Get-Date) - $startTime
    $processCount = @(Get-Process -Name "msbuild", "cl", "link" -ErrorAction SilentlyContinue).Count

    Write-Host -NoNewline "`r[$('{0:hh\:mm\:ss}' -f $elapsed)] " -ForegroundColor Yellow
    Write-Host -NoNewline "CPU: $([Math]::Round($totalCPU, 1))s | "
    Write-Host -NoNewline "Mem: $(Format-Bytes $totalMemory) | "
    Write-Host -NoNewline "Procs: $processCount" -ForegroundColor Green

    Start-Sleep -Seconds $updateInterval
}

# Aguardar término
$buildProcess.WaitForExit()
$exitCode = $buildProcess.ExitCode

$endTime = Get-Date
$totalTime = $endTime - $startTime

Write-Host ""
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Complete" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Total Time:    $('{0:hh\:mm\:ss}' -f $totalTime)" -ForegroundColor Cyan
Write-Host "Max CPU Time:  $([Math]::Round($maxCPU, 1))s" -ForegroundColor Cyan
Write-Host "Max Memory:    $(Format-Bytes $maxMemory)" -ForegroundColor Cyan
Write-Host "Exit Code:     $exitCode" -ForegroundColor $(if ($exitCode -eq 0) { 'Green' } else { 'Red' })
Write-Host ""
Write-Host "Build output logged to: .sandbox\build_output.log" -ForegroundColor Yellow
Write-Host "Build errors logged to: .sandbox\build_errors.log" -ForegroundColor Yellow

# Análise do log para encontrar arquivos mais lentos
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Analyzing build log for slow files..." -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan

$logContent = Get-Content ".sandbox\build_output.log" -Raw
$lines = $logContent -split "`n"

# Procurar por padrões de compilação
$compilingFiles = @()
foreach ($line in $lines) {
    if ($line -match "^\s*(\w+\.cpp)") {
        $compilingFiles += $matches[1]
    }
}

if ($compilingFiles.Count -gt 0) {
    Write-Host "Total files compiled: $($compilingFiles.Count)" -ForegroundColor Green
    Write-Host ""
    Write-Host "Sample files compiled:" -ForegroundColor Yellow
    $compilingFiles | Select-Object -First 10 | ForEach-Object { Write-Host "  - $_" }
}

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "✓ Build succeeded!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "✗ Build failed with exit code $exitCode" -ForegroundColor Red
}
