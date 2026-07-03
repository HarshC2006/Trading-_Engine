$ErrorActionPreference = "Stop"

g++ -std=c++17 -O2 -Wall -Wextra -Iinclude `
    src\DataHandler.cpp `
    src\Features.cpp `
    src\ModelIO.cpp `
    src\Models.cpp `
    src\Strategy.cpp `
    src\Execution.cpp `
    src\Portfolio.cpp `
    src\Performance.cpp `
    src\Backtest.cpp `
    tests\test_main.cpp `
    -o quant_tests.exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

.\quant_tests.exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Tests passed"
