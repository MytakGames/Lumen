# Regenerates assets/app.ico: a glowing isometric "brick" (a block with studs,
# a nod to blocky building games) on a deep violet rounded square.
# The art is drawn once at 1024 px and downscaled for each icon size.
# Usage: powershell -ExecutionPolicy Bypass -File tools/make_icon.ps1 [-Preview path.png]
param([string]$Preview)
Add-Type -AssemblyName System.Drawing
$out = Join-Path $PSScriptRoot "..\assets\app.ico"
$S = 1024

function C($a, $r, $g, $b) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function P($x, $y) { New-Object System.Drawing.PointF ([single]$x), ([single]$y) }
function RoundRect($x, $y, $w, $h, $r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddArc($x, $y, $r * 2, $r * 2, 180, 90)
    $p.AddArc($x + $w - $r * 2, $y, $r * 2, $r * 2, 270, 90)
    $p.AddArc($x + $w - $r * 2, $y + $h - $r * 2, $r * 2, $r * 2, 0, 90)
    $p.AddArc($x, $y + $h - $r * 2, $r * 2, $r * 2, 90, 90)
    $p.CloseFigure()
    $p
}
function Lerp($a, $b, $t) { P ($a.X + ($b.X - $a.X) * $t) ($a.Y + ($b.Y - $a.Y) * $t) }

$master = New-Object System.Drawing.Bitmap $S, $S
$g = [System.Drawing.Graphics]::FromImage($master)
$g.SmoothingMode = 'AntiAlias'
$g.Clear([System.Drawing.Color]::Transparent)

# --- background: rounded square, violet -> indigo, with a soft inner border
$bg = RoundRect 16 16 ($S - 32) ($S - 32) 230
$bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush (P 0 0), (P $S $S), (C 255 124 58 237), (C 255 30 27 75)
$g.FillPath($bgBrush, $bg)
$g.SetClip($bg)
# light bloom behind the brick ("Lumen")
$glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$glowPath.AddEllipse(112, 80, 800, 800)
$glow = New-Object System.Drawing.Drawing2D.PathGradientBrush $glowPath
$glow.CenterColor = C 210 120 220 255
$glow.SurroundColors = [System.Drawing.Color[]]@((C 0 120 220 255))
$g.FillPath($glow, $glowPath)
# glossy top sheen
$sheen = New-Object System.Drawing.Drawing2D.LinearGradientBrush (P 0 16), (P 0 520), (C 60 255 255 255), (C 0 255 255 255)
$g.FillRectangle($sheen, 16, 16, $S - 32, 480)
$g.ResetClip()
$g.DrawPath((New-Object System.Drawing.Pen (C 90 255 255 255), 10), $bg)

# --- isometric brick
$cx = 512; $cy = 540; $a = 300; $k = 0.866 * $a; $h = 230   # h = brick height
$T  = P $cx ($cy - $a)
$L  = P ($cx - $k) ($cy - $a / 2)
$R  = P ($cx + $k) ($cy - $a / 2)
$Cn = P $cx $cy
$Lb = P ($cx - $k) ($cy - $a / 2 + $h)
$Rb = P ($cx + $k) ($cy - $a / 2 + $h)
$Cb = P $cx ($cy + $h)

# shadow
$sh = New-Object System.Drawing.Drawing2D.GraphicsPath
$sh.AddEllipse([single]($cx - $k - 20), [single]($cy + $h - 70), [single](2 * $k + 40), [single]150)
$shB = New-Object System.Drawing.Drawing2D.PathGradientBrush $sh
$shB.CenterColor = C 140 10 6 40
$shB.SurroundColors = [System.Drawing.Color[]]@((C 0 10 6 40))
$g.FillPath($shB, $sh)

$left  = [System.Drawing.PointF[]]@($L, $Cn, $Cb, $Lb)
$right = [System.Drawing.PointF[]]@($Cn, $R, $Rb, $Cb)
$top   = [System.Drawing.PointF[]]@($L, $T, $R, $Cn)
$g.FillPolygon((New-Object System.Drawing.Drawing2D.LinearGradientBrush $L, $Cb, (C 255 56 189 248), (C 255 14 116 220)), $left)
$g.FillPolygon((New-Object System.Drawing.Drawing2D.LinearGradientBrush $Cn, $Rb, (C 255 29 102 214), (C 255 30 58 160)), $right)
$g.FillPolygon((New-Object System.Drawing.Drawing2D.LinearGradientBrush $T, $Cn, (C 255 186 245 255), (C 255 103 222 255)), $top)

# studs (2x2) on the top face
$studW = 150; $studH = 86; $studUp = 48
$studs = foreach ($u in 0.3, 0.7) {
    foreach ($v in 0.3, 0.7) {
        # top face point: Cn + u*(L-Cn) + v*(R-Cn)
        [pscustomobject]@{ X = $Cn.X + $u * ($L.X - $Cn.X) + $v * ($R.X - $Cn.X)
                           Y = $Cn.Y + $u * ($L.Y - $Cn.Y) + $v * ($R.Y - $Cn.Y) }
    }
}
# back to front, so nearer studs overlap farther ones
foreach ($st in ($studs | Sort-Object Y)) {
    & {
        $px = $st.X; $py = $st.Y
        $x0 = $px - $studW / 2
        # cylinder side
        $side = New-Object System.Drawing.Drawing2D.GraphicsPath
        $side.AddArc([single]$x0, [single]($py - $studH / 2), [single]$studW, [single]$studH, 0, 180)
        $side.AddLine([single]$x0, [single]$py, [single]$x0, [single]($py - $studUp))
        $side.AddArc([single]$x0, [single]($py - $studUp - $studH / 2), [single]$studW, [single]$studH, 180, -180)
        $side.CloseFigure()
        $g.FillPath((New-Object System.Drawing.Drawing2D.LinearGradientBrush (P $x0 0), (P ($x0 + $studW) 0), (C 255 79 200 245), (C 255 37 120 220)), $side)
        # cap
        $g.FillEllipse((New-Object System.Drawing.Drawing2D.LinearGradientBrush (P $x0 ($py - $studUp - $studH / 2)), (P ($x0 + $studW) ($py - $studUp + $studH / 2)), (C 255 225 252 255), (C 255 130 228 255)), [single]$x0, [single]($py - $studUp - $studH / 2), [single]$studW, [single]$studH)
    }
}

# crisp light edges
$edge = New-Object System.Drawing.Pen (C 200 255 255 255), 12
$edge.LineJoin = 'Round'
$g.DrawLine($edge, $L, $T); $g.DrawLine($edge, $T, $R)
$g.DrawLine((New-Object System.Drawing.Pen (C 150 255 255 255), 10), $L, $Cn)
$g.DrawLine((New-Object System.Drawing.Pen (C 150 255 255 255), 10), $Cn, $R)
$g.DrawLine((New-Object System.Drawing.Pen (C 110 255 255 255), 10), $Cn, $Cb)
# sparkle (top right)
$sp = New-Object System.Drawing.Drawing2D.GraphicsPath
$sx = 800; $sy = 225; $sr = 78; $si = 16
$sp.AddPolygon([System.Drawing.PointF[]]@((P $sx ($sy - $sr)), (P ($sx + $si) ($sy - $si)), (P ($sx + $sr) $sy), (P ($sx + $si) ($sy + $si)), (P $sx ($sy + $sr)), (P ($sx - $si) ($sy + $si)), (P ($sx - $sr) $sy), (P ($sx - $si) ($sy - $si))))
$g.FillPath((New-Object System.Drawing.SolidBrush (C 245 255 255 255)), $sp)
$g.Dispose()

if ($Preview) { $master.Save($Preview, [System.Drawing.Imaging.ImageFormat]::Png) }

# --- downscale to each icon size and pack as PNG-compressed ICO
$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $gg = [System.Drawing.Graphics]::FromImage($bmp)
    $gg.InterpolationMode = 'HighQualityBicubic'
    $gg.PixelOffsetMode = 'HighQuality'
    $gg.CompositingQuality = 'HighQuality'
    $gg.DrawImage($master, 0, 0, $s, $s)
    $gg.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $pngs += , $ms.ToArray()
}
# 128 px PNG for the in-app sidebar logo (embedded as RCDATA by assets/app.rc)
$logo = New-Object System.Drawing.Bitmap 128, 128
$gl = [System.Drawing.Graphics]::FromImage($logo)
$gl.InterpolationMode = 'HighQualityBicubic'; $gl.PixelOffsetMode = 'HighQuality'; $gl.CompositingQuality = 'HighQuality'
$gl.DrawImage($master, 0, 0, 128, 128)
$gl.Dispose()
$logo.Save((Join-Path $PSScriptRoot "..\assets\logo.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$logo.Dispose()
$master.Dispose()
$fs = [System.IO.File]::Create($out)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $d = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([byte]$d); $bw.Write([byte]$d); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32)
    $bw.Write([UInt32]$pngs[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Close()
Write-Output "Wrote $out"
