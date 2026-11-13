# Check the whitespace rules the project keeps, across the tracked text files: no line ends in a space or a
# tab, every text file ends with a newline, and C/C++ never indents with a tab. Markdown is exempt from the
# trailing-space rule because two trailing spaces are its line break. Files with a NUL byte are binary and
# left alone, as are third_party/ (vendored as its authors wrote it) and tests/fixtures/ (fixture inputs are
# data, spaces and tabs included; tests/fixtures/assetcheck/entries.tsv needs its trailing tab to keep its
# seventh column). Usage: tools\style_check.ps1 [-Root <folder>]; exit 0 when the tree is clean, 1 with a
# line per bad file. The test for this is tests/style_check_test.ps1; validate.ps1 runs both.
param(
    [string]$Root
)
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path $PSScriptRoot -Parent }
$files = & git -C $Root ls-files
if ($LASTEXITCODE -ne 0) { Write-Error "git ls-files failed in $Root"; exit 2 }
$problems = @()
foreach ($file in $files) {
    if ($file -match '^(third_party|tests/fixtures)/') { continue }
    $path = Join-Path $Root ($file -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -eq 0) { continue }
    if ($bytes -contains 0) { continue }  # binary: no business policing its bytes
    try { $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes) }
    catch { $problems += "${file}: not valid UTF-8"; continue }
    $lines = $text -split "`r?`n"
    $isCc = $file -match '\.(c|cc|cpp|h|hpp)$'
    $isMarkdown = $file -match '\.md$'
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($isCc -and $lines[$i] -match "^\t") { $problems += "${file}:$($i + 1): indented with a tab" }
        if (-not $isMarkdown -and $lines[$i] -match '[ \t]+$') { $problems += "${file}:$($i + 1): trailing whitespace" }
    }
    if ($text[-1] -ne "`n") { $problems += "${file}: no newline at end of file" }
}
foreach ($problem in $problems) { Write-Output "style: $problem" }
if ($problems.Count) {
    Write-Output "style: $($problems.Count) problems in $(($problems | ForEach-Object { ($_ -split ':')[0] } | Sort-Object -Unique).Count) files."
    exit 1
}
exit 0
