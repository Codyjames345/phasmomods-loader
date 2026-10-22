# Replace dist\PhasmoMods with exactly the release payload (tools/payload.psd1) from the current
# build, so the launcher there always runs what was built last. The old copy is moved aside before
# the new one takes its place: while the launcher holds a file in it, nothing changes and the build
# reports why.
param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$payload = (Import-PowerShellDataFile -LiteralPath (Join-Path $Root 'tools/payload.psd1')).Payload
$dist = Join-Path $Root 'dist'
$target = Join-Path $dist 'PhasmoMods'
$id = [guid]::NewGuid().ToString('N')
$stage = Join-Path $dist ".stage-$id"
$old = Join-Path $dist ".old-$id"

<# Remove a temporary folder of this script's, never anything outside dist. #>
function Remove-Temporary([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($dist).TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to remove a folder outside the distribution directory.'
    }
    if (Test-Path -LiteralPath $full) { Remove-Item -LiteralPath $full -Recurse -Force }
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    foreach ($file in $payload) {
        $source = Join-Path $Root $file.Source
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Cannot stage dist\PhasmoMods: $($file.Source) is missing." }
        $destination = Join-Path $stage $file.Name
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination
    }
    if (Test-Path -LiteralPath $target) {
        try {
            [IO.Directory]::Move($target, $old)
        } catch {
            throw "Cannot replace dist\PhasmoMods while a file in it is in use (close the launcher): $($_.Exception.Message)"
        }
    }
    try {
        [IO.Directory]::Move($stage, $target)
    } catch {
        if (Test-Path -LiteralPath $old) { [IO.Directory]::Move($old, $target) }  # put the previous copy back
        throw
    }
} finally {
    Remove-Temporary $stage
    Remove-Temporary $old
}
