# The development builds' number and clean-up, for the workflow's development job. Dot-source it; it does nothing by itself.
# A development build is a GitHub pre-release on Velopack's "dev" channel (docs/releasing.md), under a dev-* tag.

# How many development builds stay on GitHub. Velopack's GitHub source reads only the newest ten releases and then filters them,
# so ten development builds would hide the latest release from every player and quietly stop their updates.
$script:DevReleasesKept = 3

<# The version of development build number $Build after the release $Version: the next minor's pre-release, above the release it
   follows and below the one it leads to. Only a release version is ever committed to VERSION. #>
function Get-DevVersion([string]$Version, [int]$Build) {
    $current = $Version.Trim()
    if ($current -notmatch '^(\d+)\.(\d+)\.\d+$') { throw "VERSION must be MAJOR.MINOR.PATCH, not '$current'." }
    return '{0}.{1}.0-dev.{2}' -f $Matches[1], ([int]$Matches[2] + 1), $Build
}

<# The tags of the development builds in $Releases (objects with tagName, isPrerelease and createdAt) beyond the newest $Keep. A
   release is never among them: only a pre-release under a dev-* tag is one of these. #>
function Select-OldDevReleases([object[]]$Releases, [int]$Keep) {
    $builds = @($Releases | Where-Object { $_.isPrerelease -and $_.tagName -like 'dev-*' } |
        Sort-Object { [datetimeoffset]$_.createdAt } -Descending)
    return @($builds | Select-Object -Skip $Keep | ForEach-Object { $_.tagName })
}

<# Deletes the old development builds and their tags from $Repository, through the gh command. #>
function Remove-OldDevReleases([string]$Repository) {
    $listed = & gh release list --repo $Repository --limit 100 --json 'tagName,isPrerelease,createdAt' | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw "gh release list failed with exit code $LASTEXITCODE." }
    foreach ($tag in Select-OldDevReleases $listed $script:DevReleasesKept) {
        & gh release delete $tag --repo $Repository --yes --cleanup-tag
        if ($LASTEXITCODE -ne 0) { throw "gh release delete $tag failed with exit code $LASTEXITCODE." }
    }
}
