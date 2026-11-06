<#
.SYNOPSIS
    ChordLaunch の「ビルド・テスト → コミット → プッシュ → プレリリース作成 → 配置先へコピー」を直列に実行する。
.DESCRIPTION
    既定は DryRun（予定の表示のみで、git / gh / コピーの書込系は実行しない）。
    -Execute を付けたときだけ次を順に実行し、どこかで失敗したら以降へ進まず停止する。
      1. 事前検査（git / gh / ブランチ / 追跡対象の変更）
      2. Build-Release.ps1（ビルド・単体テスト・zip・インストーラー・SHA256SUMS）
      3. git add -u と commit（.old / dist などの無視対象は含めない）
      4. git push origin <ブランチ>
      5. gh release create（連番の新規プレリリースタグ。公開済みタグは付け替えない）
      6. zip とインストーラーを配置先へ上書きコピーし、SHA256 を照合
.PARAMETER Message
    コミットメッセージ（必須）。
.PARAMETER Execute
    指定すると実際に実行する。省略時は DryRun。
.PARAMETER Dest
    成果物のコピー先。既定は E:\ChordLaunch。
.PARAMETER CoAuthor
    コミット末尾に付ける Co-Authored-By 行。空文字で付けない。
.EXAMPLE
    .\scripts\Publish-Release.ps1 -Message "小節跨ぎ配置に対応"            # DryRun
    .\scripts\Publish-Release.ps1 -Message "小節跨ぎ配置に対応" -Execute   # 実行
#>
param(
    [Parameter(Mandatory = $true)][string]$Message,
    [switch]$Execute,
    [string]$Dest = "E:\ChordLaunch",
    [string]$CoAuthor = "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Branch = "main"

function Step([string]$text) { Write-Host "=== $text ===" -ForegroundColor Cyan }

function Invoke-Native {
    param([string]$Exe, [string[]]$Arguments)
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe $($Arguments -join ' ') が失敗しました (exit $LASTEXITCODE)。" }
}

function Assert-Preconditions {
    foreach ($cmd in @("git", "gh")) {
        if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { throw "$cmd が見つかりません。" }
    }
    & gh auth status *> $null
    if ($LASTEXITCODE -ne 0) { throw "gh が未認証です。gh auth login を実行してください。" }

    $current = (& git branch --show-current).Trim()
    if ($current -ne $Branch) { throw "現在のブランチは '$current' です。'$Branch' で実行してください。" }

    $changes = & git status --porcelain --untracked-files=no
    if (-not $changes) { throw "コミット対象の変更（追跡ファイル）がありません。" }
    return $changes
}

# src\version.h のバージョン表記（例: v26.11-Pre）に連番を付けた次のタグを求める。
function Get-NextPreTag {
    $header = Get-Content (Join-Path $Root "src\version.h") -Raw
    if ($header -notmatch '#define CHORDLAUNCH_VERSION_STR "([^"]+)"') {
        throw "src\version.h から CHORDLAUNCH_VERSION_STR を取得できません。"
    }
    $prefix = $Matches[1]
    $pattern = "^" + [regex]::Escape($prefix) + "(\d*)$"

    $json = & gh release list --limit 200 --json tagName
    if ($LASTEXITCODE -ne 0) { throw "リリース一覧の取得に失敗しました。" }
    $tags = @(($json | ConvertFrom-Json) | ForEach-Object { $_.tagName })

    $max = 0
    $latest = $null
    foreach ($t in $tags) {
        if ($t -match $pattern) {
            $n = if ($Matches[1]) { [int]$Matches[1] } else { 1 }
            if ($n -gt $max) { $max = $n; $latest = $t }
        }
    }
    if (-not $latest) { throw "'$prefix' 系の既存リリースが見つからないため、雛形ノートを取得できません。" }

    $next = "$prefix$($max + 1)"
    if ($tags -contains $next) { throw "タグ $next は既に存在します。" }
    return [pscustomobject]@{ Next = $next; Latest = $latest; Prefix = $prefix }
}

# 直近リリースの本文を雛形にし、1行目のタグ名だけ差し替える。
function New-ReleaseNotesFile([string]$latestTag, [string]$prefix, [string]$nextTag) {
    $body = (& gh release view $latestTag --json body -q ".body")
    if ($LASTEXITCODE -ne 0) { throw "$latestTag のノート取得に失敗しました。" }
    $lines = @($body -split "`r?`n")
    $lines[0] = $lines[0] -replace ([regex]::Escape($prefix) + "\d*"), $nextTag
    $file = Join-Path ([System.IO.Path]::GetTempPath()) "chordlaunch-notes-$nextTag.md"
    [System.IO.File]::WriteAllText($file, ($lines -join "`n"), (New-Object System.Text.UTF8Encoding($false)))
    return $file
}

function Copy-ToDest([string]$distDir, [string]$dest) {
    New-Item -ItemType Directory -Force $dest | Out-Null
    foreach ($f in Get-ChildItem $distDir -File | Where-Object { $_.Name -match '\.(zip|exe)$' }) {
        $target = Join-Path $dest $f.Name
        Copy-Item $f.FullName $target -Force
        $a = (Get-FileHash $f.FullName -Algorithm SHA256).Hash
        $b = (Get-FileHash $target -Algorithm SHA256).Hash
        if ($a -ne $b) { throw "コピー後のハッシュが一致しません: $($f.Name)" }
        Write-Host "  copied: $target"
    }
}

Push-Location $Root
try {
    Step "事前検査"
    $changes = Assert-Preconditions
    $tag = Get-NextPreTag
    $dist = Join-Path $Root "dist"

    Write-Host "ブランチ    : $Branch"
    Write-Host "新規タグ    : $($tag.Next)（雛形: $($tag.Latest)、プレリリース）"
    Write-Host "コピー先    : $Dest"
    Write-Host "メッセージ  : $Message"
    Write-Host "コミット対象:"
    $changes | ForEach-Object { Write-Host "  $_" }

    if (-not $Execute) {
        Write-Host ""
        Write-Host "DryRun です。実行するには -Execute を付けてください。" -ForegroundColor Yellow
        return
    }

    Step "ビルド・テスト・成果物作成"
    Invoke-Native "pwsh" @("-NoProfile", "-File", (Join-Path $PSScriptRoot "Build-Release.ps1"))
    foreach ($name in @("ChordLaunch-*-portable.zip", "ChordLaunch-*-setup.exe", "SHA256SUMS.txt")) {
        if (-not (Get-ChildItem $dist -Filter $name -File)) { throw "dist に $name がありません。" }
    }

    Step "コミット"
    Invoke-Native "git" @("add", "-u")
    $commitArgs = @("commit", "-m", $Message)
    if ($CoAuthor) { $commitArgs += @("-m", $CoAuthor) }
    Invoke-Native "git" $commitArgs

    Step "プッシュ"
    Invoke-Native "git" @("push", "origin", $Branch)

    Step "プレリリース作成 $($tag.Next)"
    $notes = New-ReleaseNotesFile $tag.Latest $tag.Prefix $tag.Next
    $assets = @(Get-ChildItem $dist -File | ForEach-Object { $_.FullName })
    $relArgs = @("release", "create", $tag.Next) + $assets + @(
        "--prerelease", "--target", $Branch,
        "--title", "ChordLaunch $($tag.Next)", "--notes-file", $notes)
    Invoke-Native "gh" $relArgs
    Remove-Item $notes -Force -ErrorAction SilentlyContinue

    Step "配置先へコピー"
    Copy-ToDest $dist $Dest

    Write-Host ""
    Write-Host "完了: $($tag.Next)" -ForegroundColor Green
}
finally {
    Pop-Location
}
