# Tests tools/style_check.ps1 on throwaway repositories: one good and one bad file of each kind.
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pg style ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null

<# Build a committed repository holding the given files (path -> exact bytes as text) and return its folder. #>
function New-StyleFixture([string]$Name, [hashtable]$Files) {
    $fixture = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Path $fixture | Out-Null
    foreach ($path in $Files.Keys) {
        $file = Join-Path $fixture $path
        New-Item -ItemType Directory -Force -Path (Split-Path $file -Parent) | Out-Null
        if ($Files[$path] -is [byte[]]) { [IO.File]::WriteAllBytes($file, $Files[$path]) }
        else { [IO.File]::WriteAllText($file, $Files[$path]) }
    }
    & git -C $fixture init --quiet --initial-branch main | Out-Null
    & git -C $fixture add --all | Out-Null
    & git -C $fixture -c user.name=Style -c user.email=style@example.invalid commit --quiet --message Start | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'The style fixture could not commit.' }
    return $fixture
}

<# Run the checker on a fixture and return its exit code and output. #>
function Invoke-Check([string]$Fixture) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & (Join-Path $project 'tools/style_check.ps1') -Root $Fixture 2>&1 | ForEach-Object { "$_" }
        return @{code = $LASTEXITCODE; text = ($output -join "`n")}
    } finally {
        $ErrorActionPreference = $previous
    }
}

try {
    # every kind of problem is caught, on the file it belongs to
    $bad = New-StyleFixture 'bad' @{
        'space.cpp'    = "int main() {`n  return 0;  `n}`n"
        'tab.h'        = "#pragma once`n`n`tskip;`n"
        'last.ps1'     = 'Write-Output x'
        'breaks.md'    = "One two`n`nbrk `nnot ending"
        'plain.txt'    = "fine `n"
        'binary.dat'   = "x `n" + [char]0 + "tail "
        'broken.txt'   = [byte[]](0xFF, 0xFE, 0x61, 0x0A)
        'tests/fixtures/data.tsv' = "a`tb`t"
        'third_party/up.c' = "int x;  "
    }
    $run = Invoke-Check $bad
    if ($run.code -eq 0) { throw 'The checker accepted messy files.' }
    foreach ($want in @('space.cpp', 'tab.h', 'last.ps1', 'breaks.md', 'plain.txt', 'broken.txt')) {
        if (-not $run.text.Contains($want)) { throw "The checker did not name $want :`n$($run.text)" }
    }
    foreach ($skip in @('binary.dat', 'tests/fixtures/data.tsv', 'third_party/up.c')) {
        if ($run.text.Contains($skip)) { throw "The checker named $skip , which it must leave alone :`n$($run.text)" }
    }
    # Markdown keeps its two-space line breaks; its missing final newline still fails.
    if (-not $run.text.Contains('breaks.md')) { throw 'The Markdown missing its final newline passed.' }

    # a tidy tree passes, Markdown line breaks and all
    $good = New-StyleFixture 'good' @{
        'ok.cpp'   = "int main() {`n  return 0;`n}`n"
        'ok.md'    = "First line with a break  `nsecond line`n`n## A heading`n"
        'ok.ps1'   = "Write-Output x`n"
        'ok.yml'   = "a: 1`n"
        'ok.bat'   = "echo hi`r`n"
    }
    $run = Invoke-Check $good
    if ($run.code -ne 0) { throw "The checker failed a tidy tree :`n$($run.text)" }

    Write-Output 'Style check tests passed: trailing spaces, tabs in C/C++ and missing final newlines are named, data files and vendored code are not.'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
