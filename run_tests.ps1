$tests = Get-ChildItem "F:\Node\hivellm\tml\packages\core\tests\*.test.tml"
$passed = 0
$failed = 0

foreach ($t in $tests) {
    $output = & "F:\Node\hivellm\tml\build\debug\tml.exe" run $t.FullName 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[PASS] $($t.Name)" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "[FAIL] $($t.Name)" -ForegroundColor Red
        Write-Host $output
        $failed++
    }
}

Write-Host ""
Write-Host "Total: $passed passed, $failed failed"
