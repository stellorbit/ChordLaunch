<#
.SYNOPSIS
    ChordLaunch アプリアイコン (resources\ChordLaunch.ico) を生成する。
.DESCRIPTION
    Windows PowerShell 5.1 (System.Drawing) で描画する。
    デザイン: 青の角丸正方形の上段にコード（重なる3音）を表すピアノロール風バー、
    下段にピアノ鍵盤を配置する。小サイズ (32px 以下) は鍵盤の細部を省いて判読性を保つ。
    出力 ICO は 16/24/32/48/64/128/256px の PNG 圧縮エントリを含む。
.PARAMETER OutIco
    出力する ICO のパス。省略時は ..\resources\ChordLaunch.ico。
.PARAMETER PreviewPng
    指定時は 256px のプレビュー PNG も保存する。
.PARAMETER ExportDir
    指定時は、Web サイトのサムネイル等に使う大サイズ PNG (透過) を ChordLaunch-icon-<size>.png として保存する。
.PARAMETER ExportSizes
    ExportDir 指定時に出力する一辺のピクセル数。既定は 1024 と 512。
#>
param(
    [string]$OutIco = (Join-Path $PSScriptRoot "..\resources\ChordLaunch.ico"),
    [string]$PreviewPng = "",
    [string]$ExportDir = "",
    [int[]]$ExportSizes = @(1024, 512)
)

Add-Type -AssemblyName System.Drawing

function New-RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [Math]::Min($r * 2, [Math]::Min($w, $h))
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-IconBitmap([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $small = ($s -le 32)

    # 背景: 青の縦グラデーション角丸正方形
    $m = $s * 0.03
    $bgPath = New-RoundedPath $m $m ($s - 2 * $m) ($s - 2 * $m) ($s * 0.22)
    $rect = New-Object System.Drawing.RectangleF(0, 0, $s, $s)
    $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect,
        [System.Drawing.ColorTranslator]::FromHtml("#3B82F6"),
        [System.Drawing.ColorTranslator]::FromHtml("#1D4ED8"), 90.0)
    $g.FillPath($bgBrush, $bgPath)

    # 上段: 重なる3音を表すピアノロール風バー
    $white = [System.Drawing.Brushes]::White
    $lightBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#BFDBFE"))
    if ($small) { $bh = $s * 0.14; $gap = $s * 0.055; $y0 = $s * 0.14 }
    else        { $bh = $s * 0.105; $gap = $s * 0.035; $y0 = $s * 0.17 }
    $bars = @(
        @{ x = 0.20; w = 0.46; b = $white },
        @{ x = 0.30; w = 0.50; b = $lightBrush },
        @{ x = 0.20; w = 0.60; b = $white }
    )
    for ($i = 0; $i -lt 3; $i++) {
        $bx = $s * $bars[$i].x
        $bw = $s * $bars[$i].w
        $by = $y0 + $i * ($bh + $gap)
        $bp = New-RoundedPath $bx $by $bw $bh ($bh / 2)
        $g.FillPath($bars[$i].b, $bp)
        $bp.Dispose()
    }

    # 下段: ピアノ鍵盤
    $kx = $s * 0.14; $kw = $s * 0.72
    if ($small) { $ky = $s * 0.62; $kh = $s * 0.26 } else { $ky = $s * 0.60; $kh = $s * 0.28 }
    $kp = New-RoundedPath $kx $ky $kw $kh ($s * 0.045)
    $g.FillPath($white, $kp)
    $navy = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml("#1E3A8A"))
    if ($small) {
        # 黒鍵2本のみ（細部省略）
        $bkw = $s * 0.10; $bkh = $kh * 0.62
        foreach ($frac in @(0.34, 0.66)) {
            $g.FillRectangle($navy, [float]($kx + $kw * $frac - $bkw / 2), [float]$ky, [float]$bkw, [float]$bkh)
        }
    } else {
        $keys = 5
        $kwEach = $kw / $keys
        $sep = New-Object System.Drawing.Pen([System.Drawing.ColorTranslator]::FromHtml("#93C5FD"), [float]([Math]::Max(1, $s * 0.008)))
        for ($i = 1; $i -lt $keys; $i++) {
            $g.DrawLine($sep, [float]($kx + $i * $kwEach), [float]$ky, [float]($kx + $i * $kwEach), [float]($ky + $kh))
        }
        $bkw = $kwEach * 0.56; $bkh = $kh * 0.60
        foreach ($i in @(1, 2, 4)) {
            $g.FillRectangle($navy, [float]($kx + $i * $kwEach - $bkw / 2), [float]$ky, [float]$bkw, [float]$bkh)
        }
        $sep.Dispose()
    }

    $kp.Dispose(); $bgPath.Dispose(); $bgBrush.Dispose(); $lightBrush.Dispose(); $navy.Dispose(); $g.Dispose()
    return $bmp
}

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngs = @()
foreach ($s in $sizes) {
    $bmp = New-IconBitmap $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,($ms.ToArray())
    if ($PreviewPng -and $s -eq 256) { $bmp.Save($PreviewPng, [System.Drawing.Imaging.ImageFormat]::Png) }
    $ms.Dispose(); $bmp.Dispose()
}

$outDir = Split-Path -Parent $OutIco
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }

$fs = [System.IO.File]::Create($OutIco)
$bw = New-Object System.IO.BinaryWriter($fs)
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32)
    $bw.Write([UInt32]$pngs[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Dispose(); $fs.Dispose()
Write-Host "Generated: $OutIco"

if ($ExportDir) {
    if (-not (Test-Path $ExportDir)) { New-Item -ItemType Directory -Force $ExportDir | Out-Null }
    foreach ($s in $ExportSizes) {
        $bmp = New-IconBitmap $s
        $path = Join-Path $ExportDir "ChordLaunch-icon-$s.png"
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "Exported: $path"
    }
}
