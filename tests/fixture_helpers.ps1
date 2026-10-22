# Helpers shared by the packaging tests (package_test.ps1, stage_test.ps1). Dot-source this file.

<# Give every payload source in tools/payload.psd1 a synthetic file naming itself, under $Fixture. #>
function Write-PayloadSources([string]$Fixture, $Payload) {
    foreach ($file in $Payload) {
        $source = Join-Path $Fixture $file.Source
        New-Item -ItemType Directory -Force -Path (Split-Path $source -Parent) | Out-Null
        Set-Content -LiteralPath $source -Value "fixture $($file.Source)"
    }
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
