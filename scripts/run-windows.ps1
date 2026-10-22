$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $PSScriptRoot
$MakeExe = Join-Path $ProjectDir ".deps\w64devkit\bin\make.exe"

if (-not (Test-Path $MakeExe)) {
    throw "Ambiente nao preparado. Execute .\scripts\setup-windows.ps1 primeiro."
}

Push-Location $ProjectDir
try {
    & $MakeExe run
    if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar ou executar o jogo." }
}
finally {
    Pop-Location
}
