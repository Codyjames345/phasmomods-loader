# Validate, then pack the release with Velopack's vpk into dist\releases: Setup.exe and the full package.
# Each run replaces dist\releases whole. Nothing in the game installation changes.
# -Vpk is the command that runs vpk (its first element) and any arguments it starts with.
# -Channel packs for a Velopack channel other than the default, as development builds do (docs/releasing.md).
param([string[]]$Vpk = @('dotnet', 'tool', 'run', 'vpk'), [string]$Channel)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& (Join-Path $root 'validate.ps1')

$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+(-dev\.\d+)?$') { throw "VERSION must be MAJOR.MINOR.PATCH, or a development build's, not '$version'." }
# A map change the offline check has not seen would make every player's notice say the game updated.
& (Join-Path $root 'tools/check_build_record.ps1') -Root $root
. (Join-Path $root 'tools/payload_files.ps1')
$payload = Get-PayloadFiles $root
try { Assert-PayloadSources $root $payload } catch { throw "$($_.Exception.Message) Build and validate the project before packaging." }
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
    $channelArguments = if ($Channel) { @('--channel', $Channel) } else { @() }
    # The title names the Start menu shortcut and the installed-apps entry; the icon is Setup.exe's and that entry's.
    & $Vpk[0] @($Vpk | Select-Object -Skip 1) pack --packId PhasmoModsLoader --packVersion $version `
        --packTitle 'PhasmoMods Loader' --icon (Join-Path $root 'tools/launcher.ico') `
        --packDir $content --mainExe 'PhasmoMods Loader.exe' --runtime win-x64 --outputDir $output @channelArguments
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
