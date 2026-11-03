# Helpers shared by the packaging tests (package_test.ps1, stage_test.ps1). Dot-source this file.

<# Give every payload source in tools/payload.psd1 a synthetic file naming itself, under $Fixture. #>
function Write-PayloadSources([string]$Fixture, $Payload) {
    foreach ($file in $Payload) {
        $source = Join-Path $Fixture $file.Source
        New-Item -ItemType Directory -Force -Path (Split-Path $source -Parent) | Out-Null
        Set-Content -LiteralPath $source -Value "fixture $($file.Source)"
    }
}

<# Give a fixture the files every release script now needs beside the payload: the helper and the check, a
   checked-build record, and the digest the build writes, which match unless $Digest says otherwise. #>
function Write-RecordFixture([string]$Fixture, [string]$Project, [string]$Digest = '0123456789abcdef') {
    New-Item -ItemType Directory -Force -Path (Join-Path $Fixture 'tools'), (Join-Path $Fixture 'sdk/builds'), (Join-Path $Fixture 'build') | Out-Null
    foreach ($script in @('tools/payload_files.ps1', 'tools/check_build_record.ps1')) {
        Copy-Item -LiteralPath (Join-Path $Project $script) -Destination (Join-Path $Fixture $script)
    }
    Set-Content -LiteralPath (Join-Path $Fixture 'sdk/builds/fixture-build.txt') -Value "gameassembly=$('a' * 64)`nunityplayer=$('b' * 64)`nmap=0123456789abcdef"
    Set-Content -LiteralPath (Join-Path $Fixture 'build/map-digest.txt') -Value $Digest
}

<# Remove a test's fixture root, refusing anything outside the temporary directory. #>
function Remove-TestRoot([string]$TestRoot) {
    $resolvedRoot = [IO.Path]::GetFullPath($TestRoot)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to remove a test fixture outside the temporary directory.'
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
