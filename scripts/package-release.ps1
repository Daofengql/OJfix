param(
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64",
    [string]$Configuration = "Release",
    [string]$UpxPath = "D:\OtherTools\upx-5.1.0-win64\upx.exe"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $scriptRoot
$solution = Join-Path $root "OJfix.sln"
$dist = Join-Path $root "dist"
$sourceExe = Join-Path $root "$Platform\$Configuration\OJfix.exe"
$distExe = Join-Path $dist "OJfix.exe"

function Find-MSBuild {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "MSBuild.exe was not found."
}

if (-not (Test-Path -LiteralPath $UpxPath)) {
    throw "UPX was not found at: $UpxPath"
}

$msbuild = Find-MSBuild

Write-Host "Building $Configuration|$Platform..."
& $msbuild $solution /p:Configuration=$Configuration /p:Platform=$Platform /m /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path -LiteralPath $sourceExe)) {
    throw "Build output was not found: $sourceExe"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath $sourceExe -Destination $distExe -Force

$before = (Get-Item -LiteralPath $distExe).Length
Write-Host "Compressing with UPX..."
& $UpxPath --best --lzma $distExe
if ($LASTEXITCODE -ne 0) {
    throw "UPX failed with exit code $LASTEXITCODE."
}

$after = (Get-Item -LiteralPath $distExe).Length
Write-Host "Packaged: $distExe"
Write-Host ("Size: {0:N0} bytes -> {1:N0} bytes" -f $before, $after)
