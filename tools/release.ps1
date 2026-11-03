# Bump the suite version, commit it and tag it. Usage: tools\release.ps1 -Version <x.y.z> [-Push] [-NoCheckedBuild]
# -Push also sends main and the new tag to origin together, which starts the release workflow.
# It builds first, so the map it checks is the map being released.
# -NoCheckedBuild releases a map no checked-build record holds, for a fix that must ship while the live game build still has
# an unresolved entry (tools/check_build_record.ps1): the release commit also holds the file that lets CI through, and
# every player's notice says the game updated. Without it the release refuses.
param(
    [Parameter(Mandatory)][string]$Version,
    [switch]$Push,
    [switch]$NoCheckedBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

<# Run git in the project and return its output. Git reports progress on stderr, which must not count
   as a failure, so only the exit code decides. #>
function Invoke-Git([string[]]$GitArguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & git -C $root @GitArguments 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { throw "git $GitArguments failed: $output" }
        return $output
    } finally {
        $ErrorActionPreference = $previous
    }
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "The version must be MAJOR.MINOR.PATCH, not '$Version'." }
if ((Invoke-Git @('branch', '--show-current')) -join '' -ne 'main') { throw 'Releases are made from main. Switch to main first.' }
if (@(Invoke-Git @('status', '--porcelain')).Count) { throw 'The working tree has uncommitted changes. Commit or stash them first.' }
& (Join-Path $root 'build.ps1')
if (-not $NoCheckedBuild) { & (Join-Path $PSScriptRoot 'check_build_record.ps1') -Root $root }

$versionFile = Join-Path $root 'VERSION'
$current = (Get-Content -LiteralPath $versionFile -Raw).Trim()
if ($current -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION must be MAJOR.MINOR.PATCH, not '$current'." }
if ([version]$Version -le [version]$current) { throw "The version $Version must be newer than the current $current." }

$tag = "v$Version"
if (@(Invoke-Git @('tag', '--list', $tag)).Count) { throw "The tag $tag already exists." }

if ($NoCheckedBuild) {
    $digestFile = Join-Path $root 'build/map-digest.txt'
    if (-not (Test-Path -LiteralPath $digestFile -PathType Leaf)) { throw 'build\map-digest.txt is missing. The build should have written it.' }
    Set-Content -LiteralPath (Join-Path $root 'sdk/builds/unchecked-release.allow') -Value "map=$((Get-Content -LiteralPath $digestFile -Raw).Trim())"
    & (Join-Path $PSScriptRoot 'check_build_record.ps1') -Root $root
    Invoke-Git @('add', '--', 'sdk/builds/unchecked-release.allow') | Out-Null
}
Set-Content -LiteralPath $versionFile -Value $Version
Invoke-Git @('add', '--', 'VERSION') | Out-Null
Invoke-Git @('commit', '--quiet', '--message', "Release $Version",
    '--message', "Raise the suite version from $current to $Version. Pushing the $tag tag builds and publishes the release.") | Out-Null
# From here on a failure leaves the release commit (and perhaps the tag) behind, so it says how to
# undo them or retry.
$tagged = $false
try {
    Invoke-Git @('tag', '--annotate', $tag, '--message', "Release $Version") | Out-Null
    $tagged = $true
    if ($Push) {
        # Atomic: origin gets main and the tag together or neither.
        Invoke-Git @('push', '--atomic', 'origin', 'main', $tag) | Out-Null
    }
} catch {
    $undo = if ($tagged) { "git tag --delete $tag, then git reset --hard HEAD~1" } else { 'git reset --hard HEAD~1' }
    $retry = if ($tagged) { "Or fix the cause and retry with: git push --atomic origin main $tag" } else { "Or fix the cause and tag it with: git tag --annotate $tag --message `"Release $Version`"" }
    throw "$($_.Exception.Message)`nThe release $Version was committed but not pushed; nothing reached origin. To undo it (VERSION goes back to $current), run: $undo. $retry"
}

if ($Push) {
    Write-Output "Released $Version and pushed main and $tag to origin."
} else {
    Write-Output "Released $Version locally. Push it with: git push --atomic origin main $tag"
}
