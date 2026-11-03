# Refuse to publish a map the offline check has not seen. Stops with an error unless a checked-build record
# in sdk\builds holds the digest of the map last built (build\map-digest.txt, which the build writes). A
# record made with another map is no record to the host, so every player's notice would say the game updated.
# The offline check needs the game's files and cannot run in CI, so it is a local step before tagging.
# tools/release.ps1 runs this before it bumps, and package.ps1, which CI's release runs, after it validates.
# Records of older game builds may be stale: nobody plays those builds, and their files stay until removed.
# A fix can have to ship while the live game build still has an entry the map cannot resolve, so the check cannot pass and no
# record can be written. Then sdk\builds\unchecked-release.allow, holding "map=<this digest>", lets this one map through with
# a warning: tools/release.ps1 -NoCheckedBuild commits it, and CI reads it, since it is in the tag. Players on that build see
# the notice's update paragraph, which is true of it. It names one digest, so it says nothing about the next map change.
param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'

$digestFile = Join-Path $Root 'build/map-digest.txt'
if (-not (Test-Path -LiteralPath $digestFile -PathType Leaf)) { throw 'build\map-digest.txt is missing. Build the project first.' }
$digest = (Get-Content -LiteralPath $digestFile -Raw).Trim()
$records = @(Get-ChildItem -LiteralPath (Join-Path $Root 'sdk/builds') -Filter '*.txt' -File -ErrorAction SilentlyContinue)
foreach ($record in $records) {
    # All three lines, as the host's reader asks (core/build_record.h): anything less is no record to it.
    $lines = @(Get-Content -LiteralPath $record.FullName)
    if (($lines -contains "map=$digest") -and ($lines -cmatch '^gameassembly=[0-9a-f]{64}$') -and ($lines -cmatch '^unityplayer=[0-9a-f]{64}$')) { return }
}
$allow = Join-Path $Root 'sdk/builds/unchecked-release.allow'
if ((Test-Path -LiteralPath $allow -PathType Leaf) -and (@(Get-Content -LiteralPath $allow) -contains "map=$digest")) {
    Write-Warning "This map ($digest) has no checked-build record, and sdk\builds\unchecked-release.allow lets it through: every player's notice will say the game updated."
    return
}
throw "No checked-build record in sdk\builds was made with this map ($digest). Run tools\gamecheck.ps1 on the game build you support, and commit what it writes."
