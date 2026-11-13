# Bump the suite version, commit it and tag it. Usage: tools\release.ps1 -Version <x.y.z> [-Push] [-NoCheckedBuild]
# -Push also sends main and the new tag to origin together, which starts the release workflow.
# It builds first, so the map it checks is the map being released.
# It refuses until CHANGELOG.md's Unreleased section holds entries, then turns that section into the dated
# notes for this version and starts an empty Unreleased, all in the same commit as the bump (tools/changelog_section.ps1
# reads that section back when the workflow publishes the release).
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

# The players' release notes are written by a person in CHANGELOG.md (AI_POLICY.md) before the release runs:
# the Unreleased section must hold at least one entry. Its words here are for the maintainer, not the player.
$changelogFile = Join-Path $root 'CHANGELOG.md'
if (-not (Test-Path -LiteralPath $changelogFile -PathType Leaf)) { throw "CHANGELOG.md is missing. Write the release notes for $Version there first." }
$unreleased = @(& (Join-Path $PSScriptRoot 'changelog_section.ps1') -Version 'Unreleased' -Path $changelogFile)
if ($LASTEXITCODE -ne 0) { throw "CHANGELOG.md has no ## [Unreleased] section. Write the release notes for $Version there first." }
if (@($unreleased | Where-Object { $_.Trim() -ne '' }).Count -eq 0) { throw "The Unreleased section of CHANGELOG.md has no entries. Write the release notes for $Version there first." }

if ($NoCheckedBuild) {
    $digestFile = Join-Path $root 'build/map-digest.txt'
    if (-not (Test-Path -LiteralPath $digestFile -PathType Leaf)) { throw 'build\map-digest.txt is missing. The build should have written it.' }
    Set-Content -LiteralPath (Join-Path $root 'sdk/builds/unchecked-release.allow') -Value "map=$((Get-Content -LiteralPath $digestFile -Raw).Trim())"
    & (Join-Path $PSScriptRoot 'check_build_record.ps1') -Root $root
    Invoke-Git @('add', '--', 'sdk/builds/unchecked-release.allow') | Out-Null
}
# Turn Unreleased into the version's own dated section and open a fresh empty one above it, last of the checks
# so nothing can fail after the edit; the release commit carries this file with the VERSION bump, and the
# workflow reads that section as the release's notes.
$changelogLines = @(Get-Content -LiteralPath $changelogFile)
$datedHeading = "## [$Version] - $(Get-Date -Format 'yyyy-MM-dd')"
for ($i = 0; $i -lt $changelogLines.Count; $i++) {
    if ($changelogLines[$i] -match '^## \[Unreleased\]\s*$') {
        $before = if ($i -gt 0) { $changelogLines[0..($i - 1)] } else { @() }
        $after = if ($i + 1 -lt $changelogLines.Count) { $changelogLines[($i + 1)..($changelogLines.Count - 1)] } else { @() }
        $changelogLines = @($before) + @('## [Unreleased]', '', $datedHeading) + @($after)
        break
    }
}
Set-Content -LiteralPath $changelogFile -Value $changelogLines
Set-Content -LiteralPath $versionFile -Value $Version
Invoke-Git @('add', '--', 'CHANGELOG.md', 'VERSION') | Out-Null
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
