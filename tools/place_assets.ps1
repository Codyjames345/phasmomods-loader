# Place each pinned file of the release (a tools/payload.psd1 entry with a Sha256) from a checkout of the private assets
# repository, as CI does before it builds: the build the pin names, from pinned/<SHA-256>/<file name>, checked against
# its pin. A pin whose build was never published fails here, naming tools/publish_assets.ps1, rather than a release
# shipping another build.
param([Parameter(Mandatory)][string]$Assets, [string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
. (Join-Path $Root 'tools/payload_files.ps1')
$pinned = @((Import-PowerShellDataFile -LiteralPath (Join-Path $Root 'tools/payload.psd1')).Payload |
    Where-Object { $_.ContainsKey('Sha256') })
foreach ($file in $pinned) {
    $from = Join-Path $Assets "pinned/$($file.Sha256)/$(Split-Path $file.Source -Leaf)"
    if (-not (Test-Path -LiteralPath $from -PathType Leaf)) {
        throw ("The private assets repository has no build of $($file.Source) with the pinned SHA-256 $($file.Sha256): " +
            'publish it with tools/publish_assets.ps1.')
    }
    $to = Join-Path $Root $file.Source
    New-Item -ItemType Directory -Force -Path (Split-Path $to -Parent) | Out-Null
    Copy-Item -LiteralPath $from -Destination $to -Force
}
Assert-PayloadSources $Root $pinned
Write-Output "Placed $($pinned.Count) pinned file(s)."
