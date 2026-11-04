<#
.SYNOPSIS
    ChordLaunch のリリース成果物 (ポータブル版 zip / インストーラー / SHA256) を dist\ に作成する。
.DESCRIPTION
    1. MSVC 環境をロードし release-max プリセットでビルド
    2. clp_tests を実行 (失敗時は中断)
    3. ポータブル版 zip を作成
    4. Inno Setup でインストーラーを作成
    5. SHA256SUMS.txt を出力
.PARAMETER Version
    表示用バージョン (例: v26.11-Pre)。ファイル名と Inno Setup に使用する。
#>
param(
    [string]$Version = "v26.11-Pre",
    [string]$FileVersion = "26.11.0.0"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dist = Join-Path $Root "dist"

function Import-MsvcEnvironment {
    if ($env:VSCMD_VER) { return }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe が見つかりません。" }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw "Visual Studio C++ ツールチェーンが見つかりません。" }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    $envLines = cmd.exe /c "`"$vcvars`" >nul && set"
    foreach ($line in $envLines) {
        if ($line -match "^([^=]+)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

function Find-Iscc {
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    throw "Inno Setup 6 (ISCC.exe) が見つかりません。"
}

Push-Location $Root
try {
    Import-MsvcEnvironment

    & cmake --preset release-max
    if ($LASTEXITCODE -ne 0) { throw "configure に失敗しました。" }
    & cmake --build --preset release-max
    if ($LASTEXITCODE -ne 0) { throw "ビルドに失敗しました。" }

    $exe = Join-Path $Root "build\ChordLaunch.exe"
    $tests = Join-Path $Root "build\clp_tests.exe"
    if (-not (Test-Path $exe)) { throw "ChordLaunch.exe が生成されていません。" }
    & $tests
    if ($LASTEXITCODE -ne 0) { throw "単体テストに失敗しました。" }

    if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
    New-Item -ItemType Directory -Force $Dist | Out-Null

    # ポータブル版 zip
    $stage = Join-Path $Dist "ChordLaunch-$Version-portable"
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item $exe $stage
    Copy-Item (Join-Path $Root "README.md") $stage
    $zip = Join-Path $Dist "ChordLaunch-$Version-portable.zip"
    Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal
    Remove-Item -Recurse -Force $stage

    # インストーラー
    $iscc = Find-Iscc
    & $iscc "/DAppVersion=$Version" "/DAppFileVersion=$FileVersion" (Join-Path $Root "installer\ChordLaunch.iss")
    if ($LASTEXITCODE -ne 0) { throw "インストーラーの作成に失敗しました。" }

    # SHA256
    $sums = Get-ChildItem $Dist -File | ForEach-Object {
        $h = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
        "$h  $($_.Name)"
    }
    $sums | Set-Content -Path (Join-Path $Dist "SHA256SUMS.txt") -Encoding ascii

    Write-Host "=== dist ===" -ForegroundColor Cyan
    Get-ChildItem $Dist -File | Select-Object Name, Length | Format-Table -AutoSize
}
finally {
    Pop-Location
}
