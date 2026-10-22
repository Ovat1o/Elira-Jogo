$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $PSScriptRoot
$DepsDir = Join-Path $ProjectDir ".deps"
$ToolchainDir = Join-Path $DepsDir "w64devkit"
$BinDir = Join-Path $ToolchainDir "bin"
$MakeExe = Join-Path $BinDir "make.exe"
$GccExe = Join-Path $BinDir "gcc.exe"

$W64DevkitVersion = "2.10.0"
$ArchiveName = "w64devkit-x64-$W64DevkitVersion.7z.exe"
$ArchiveUrl = "https://github.com/skeeto/w64devkit/releases/download/v$W64DevkitVersion/$ArchiveName"
$ExpectedSha256 = "18d0a4c71a166f8401ab6305781bec5882b40b5e06ba9807c61cb5f3b3c6325e"
$ArchivePath = Join-Path $DepsDir $ArchiveName

Write-Host "==> Preparando o ELIRA no Windows"

if (-not (Test-Path $GccExe) -or -not (Test-Path $MakeExe)) {
    New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null

    Write-Host "==> Baixando o w64devkit $W64DevkitVersion"
    Invoke-WebRequest -Uri $ArchiveUrl -OutFile $ArchivePath

    $ActualSha256 = (Get-FileHash -Algorithm SHA256 $ArchivePath).Hash.ToLowerInvariant()
    if ($ActualSha256 -ne $ExpectedSha256) {
        throw "O arquivo baixado nao passou na verificacao de integridade."
    }

    Write-Host "==> Extraindo o compilador"
    & $ArchivePath -y "-o$DepsDir" | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao extrair o w64devkit."
    }
}

if (-not (Test-Path $GccExe) -or -not (Test-Path $MakeExe)) {
    throw "O GCC ou o Make nao foi encontrado em $BinDir."
}

$env:Path = "$BinDir;$env:Path"

Write-Host "==> Compilando o jogo"
Push-Location $ProjectDir
try {
    & $MakeExe clean
    if ($LASTEXITCODE -ne 0) { throw "Falha ao limpar a compilacao anterior." }

    & $MakeExe
    if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar o jogo." }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Pronto. Execute o jogo com:"
Write-Host "  .\ELIRA.exe"
