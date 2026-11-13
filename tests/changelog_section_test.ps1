# Tests the changelog section extractor (tools/changelog_section.ps1) on a throwaway file; nothing is published.
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pg changelog ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null

<# Write a changelog fixture and return its path. #>
function New-Changelog([string]$Name, [string]$Text) {
    $file = Join-Path $testRoot ($Name + '.md')
    Set-Content -LiteralPath $file -Value $Text
    return $file
}

<# Run the extractor and return its output lines and exit code. #>
function Invoke-Section([string]$File, [string]$Version) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& (Join-Path $project 'tools/changelog_section.ps1') -Version $Version -Path $File 2>$null)
        return @{lines = $output; code = $LASTEXITCODE}
    } finally {
        $ErrorActionPreference = $previous
    }
}

try {
    $full = New-Changelog 'full' @'
# Changelog

Prose before the first section.

## [Unreleased]

### Added

- First note.

## [0.2.0] - 2026-01-05

### Fixed

- Second note.

## [0.1.0] - 2025-12-01

### Added

- Third note.
'@

    # a version's own section, and only it
    $result = Invoke-Section $full '0.2.0'
    if ($result.code -ne 0) { throw 'Extracting 0.2.0 failed.' }
    if (($result.lines -join '|') -ne '### Fixed||- Second note.') { throw "0.2.0 gave the wrong body: $($result.lines -join '|')" }

    # the Unreleased section by the same name
    $result = Invoke-Section $full 'Unreleased'
    if ($result.code -ne 0) { throw 'Extracting Unreleased failed.' }
    if (($result.lines -join '|') -ne '### Added||- First note.') { throw "Unreleased gave the wrong body: $($result.lines -join '|')" }

    # an empty Unreleased section prints nothing and succeeds, so a development build simply has no body
    $empty = New-Changelog 'empty' "# Changelog

## [Unreleased]

## [0.2.0] - 2026-01-05

- Second note.
"
    $result = Invoke-Section $empty 'Unreleased'
    if ($result.code -ne 0) { throw 'Extracting an empty Unreleased failed.' }
    if (@($result.lines | Where-Object { $_.Trim() -ne '' }).Count) { throw 'An empty Unreleased printed something.' }

    # a version with no section is a failure, so a release never publishes empty notes by accident
    $result = Invoke-Section $full '0.3.0'
    if ($result.code -eq 0) { throw 'A missing section succeeded.' }
    if ($result.lines.Count) { throw 'A missing section printed a body.' }

    # a changelog that is not there fails too
    $result = Invoke-Section (Join-Path $testRoot 'gone.md') '0.2.0'
    if ($result.code -eq 0) { throw 'A missing CHANGELOG.md succeeded.' }

    Write-Output 'Changelog section tests passed: each section comes out whole, empty prints nothing, and what is missing fails.'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
