# Validate, then pack the release with Velopack's vpk into dist\releases: Setup.exe and the full package.
# Each run replaces dist\releases whole. Nothing in the game installation changes.
# -Vpk is the command that runs vpk (its first element) and any arguments it starts with.
param([string[]]$Vpk = @('dotnet', 'tool', 'run', 'vpk'))
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& (Join-Path $root 'validate.ps1')

$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION must be MAJOR.MINOR.PATCH, not '$version'." }
$payload = (Import-PowerShellDataFile -LiteralPath (Join-Path $root 'tools/payload.psd1')).Payload
foreach ($file in $payload) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $file.Source) -PathType Leaf)) {
        throw "The release is missing $($file.Source). Build and validate the project before packaging."
    }
}
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$stage = Join-Path $dist ('.pack-' + [guid]::NewGuid().ToString('N'))
$content = Join-Path $stage 'content'
$output = Join-Path $stage 'releases'
$releases = Join-Path $dist 'releases'
New-Item -ItemType Directory -Path $content, $output | Out-Null
# dotnet finds .config/dotnet-tools.json by searching up from the current folder, so work from the project root.
Push-Location $root
try {
    if (-not $PSBoundParameters.ContainsKey('Vpk')) {
        & dotnet tool restore | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Cannot restore the pinned vpk tool.' }
    }
    foreach ($file in $payload) {
        $destination = Join-Path $content $file.Name
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $file.Source) -Destination $destination
    }
    & $Vpk[0] @($Vpk | Select-Object -Skip 1) pack --packId PhasmoModsLoader --packVersion $version `
        --packDir $content --mainExe 'PhasmoMods Loader.exe' --runtime win-x64 --outputDir $output
    if ($LASTEXITCODE -ne 0) { throw "vpk failed with exit code $LASTEXITCODE." }

    # Publish only a finished pack: the previous releases stay in place until the new ones replace them whole.
    $previous = Join-Path $stage 'previous'
    if (Test-Path -LiteralPath $releases) { [IO.Directory]::Move($releases, $previous) }
    [IO.Directory]::Move($output, $releases)
    Get-ChildItem -LiteralPath $releases -File | Select-Object Name, Length
} finally {
    Pop-Location
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    $distPrefix = [IO.Path]::GetFullPath($dist).TrimEnd('\') + '\'
    if (-not $resolvedStage.StartsWith($distPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to remove a staging directory outside the distribution directory.'
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
