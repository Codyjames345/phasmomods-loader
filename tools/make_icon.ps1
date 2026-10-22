# Draw the launcher's icon, "PM" in marker on a strip of masking tape like the main menu's labels,
# and write tools/launcher.ico (PNG images, 16 to 256 px). The .ico is kept in the repository, so
# the build never runs this; run it again only to change the icon. Needs Windows and its Segoe
# Print and Segoe UI Black fonts: the handwriting is too thin to read below 64 px, so the small
# sizes letter it in the heavier face on a taller strip.
param([string]$Out = (Join-Path $PSScriptRoot 'launcher.ico'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sizes = 16, 20, 24, 32, 40, 48, 64, 256
$tape = [System.Drawing.Color]::FromArgb(255, 232, 220, 184)
$tapeEdge = [System.Drawing.Color]::FromArgb(255, 196, 182, 140)
$ink = [System.Drawing.Color]::FromArgb(255, 34, 30, 28)

<# One size of the icon as PNG bytes. #>
function Draw([int]$size) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.TranslateTransform($size / 2, $size / 2)
        $small = $size -lt 64
        $g.RotateTransform($(if ($small) { -5 } else { -8 }))
        # The strip, with torn ends: a zigzag of $teeth points down each short side. Its rotated
        # corners stay inside the square.
        $w = $size * 0.9; $h = $size * $(if ($small) { 0.66 } else { 0.56 }); $teeth = 4; $bite = $size * 0.05
        $points = [System.Collections.Generic.List[System.Drawing.PointF]]::new()
        for ($i = 0; $i -le $teeth; $i++) {
            $points.Add([System.Drawing.PointF]::new(-$w / 2 + ($i % 2) * $bite, -$h / 2 + $h * $i / $teeth))
        }
        for ($i = 0; $i -le $teeth; $i++) {
            $points.Add([System.Drawing.PointF]::new($w / 2 - ($i % 2) * $bite, $h / 2 - $h * $i / $teeth))
        }
        $fill = [System.Drawing.SolidBrush]::new($tape)
        $edge = [System.Drawing.Pen]::new($tapeEdge, [Math]::Max(1, $size / 48))
        $g.FillPolygon($fill, $points.ToArray())
        $g.DrawPolygon($edge, $points.ToArray())
        $family = if ($small) { 'Segoe UI Black' } else { 'Segoe Print' }
        $style = if ($small) { [System.Drawing.FontStyle]::Regular } else { [System.Drawing.FontStyle]::Bold }
        $format = [System.Drawing.StringFormat]::new([System.Drawing.StringFormatFlags]::NoWrap)
        $format.Alignment = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center
        # The largest lettering that fits inside the torn ends and the strip's height.
        $font = [System.Drawing.Font]::new($family, $h, $style, [System.Drawing.GraphicsUnit]::Pixel)
        $measured = $g.MeasureString('PM', $font, [System.Drawing.PointF]::new(0, 0), $format)
        $scale = [Math]::Min(($w - 2 * $bite) * 0.86 / $measured.Width, $h * 0.95 / $measured.Height)
        $font.Dispose()
        $font = [System.Drawing.Font]::new($family, $h * $scale, $style, [System.Drawing.GraphicsUnit]::Pixel)
        $text = [System.Drawing.SolidBrush]::new($ink)
        $g.DrawString('PM', $font, $text, [System.Drawing.RectangleF]::new(-$w / 2, -$h / 2 - $h * 0.04, $w, $h), $format)
        $fill.Dispose(); $edge.Dispose(); $font.Dispose(); $format.Dispose(); $text.Dispose()
    } finally {
        $g.Dispose()
    }
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    return , $stream.ToArray()
}

# ICONDIR, one ICONDIRENTRY per image, then the images (Windows reads PNG entries since Vista).
$images = foreach ($size in $sizes) { , (Draw $size) }
$file = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($file)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $edgeByte = [byte]($sizes[$i] % 256)  # 0 means 256
    $writer.Write($edgeByte); $writer.Write($edgeByte); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($image in $images) { $writer.Write($image) }
$writer.Flush()
[IO.File]::WriteAllBytes($Out, $file.ToArray())
$writer.Dispose()
