# Tests the development builds' version and clean-up (tools/dev_releases.ps1), and that the workflow keeps them apart from releases.
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
. (Join-Path $project 'tools/dev_releases.ps1')

function Assert-That([bool]$Condition, [string]$What) {
    if (-not $Condition) { throw "dev_release_test: $What" }
}

# The next minor's pre-release: above the release it follows, below the release it leads to, and rising with each build.
Assert-That ((Get-DevVersion "0.2.0`r`n" 12) -eq '0.3.0-dev.12') 'a development build is the next minor''s pre-release'
Assert-That ((Get-DevVersion '1.9.3' 7) -eq '1.10.0-dev.7') 'the minor carries past nine and the patch resets'
$threw = $false
try { Get-DevVersion '0.3.0-dev.4' 5 | Out-Null } catch { $threw = $true }
Assert-That $threw 'a version already a development build is refused: only a release version is committed'
Write-Host 'A development build is numbered after the release it follows.'

# Velopack's GitHub source reads only the newest ten releases, then filters them. Ten development builds would hide the latest
# release from every player, so few are kept, and a release is never touched.
function New-Release([string]$Tag, [bool]$Pre, [string]$When) { [pscustomobject]@{ tagName = $Tag; isPrerelease = $Pre; createdAt = $When } }
$listed = @(
    (New-Release 'dev-0.3.0-dev.1' $true '2026-11-01T00:00:00Z'),
    (New-Release 'dev-0.3.0-dev.5' $true '2026-11-05T00:00:00Z'),
    (New-Release 'v0.2.0' $false '2026-11-02T00:00:00Z'),
    (New-Release 'dev-0.3.0-dev.3' $true '2026-11-03T00:00:00Z'),
    (New-Release 'v0.3.0-rc' $true '2026-10-01T00:00:00Z'),
    (New-Release 'dev-0.3.0-dev.4' $true '2026-11-04T00:00:00Z'),
    (New-Release 'dev-not-pre' $false '2026-10-02T00:00:00Z')
)
$old = @(Select-OldDevReleases $listed 3)
Assert-That (($old -join ',') -eq 'dev-0.3.0-dev.1') "the oldest development builds beyond three go, and nothing else: $($old -join ',')"
Assert-That (@(Select-OldDevReleases $listed 10).Count -eq 0) 'within the limit nothing goes'
Assert-That ($script:DevReleasesKept -le 5) 'the limit leaves the newest release well within the ten Velopack reads'
Write-Host 'Old development builds are removed, and a release never is.'

# The workflow: development builds publish only from the branch, as pre-releases on their own channel, under a tag no release
# pattern matches, and a release still comes only from a tag.
$workflow = Get-Content -LiteralPath (Join-Path $project '.github/workflows/build.yml') -Raw
Assert-That ($workflow -match "branches: \[main, development\]") 'pushes to development run the workflow'
Assert-That ($workflow -match "if: github\.ref == 'refs/heads/development'") 'the development job runs from that branch alone'
Assert-That ($workflow -match "if: startsWith\(github\.ref, 'refs/tags/v'\)") 'a release comes from a tag alone'
Assert-That ($workflow -match '--pre\b') 'a development build is a pre-release, so the latest-release link and players stay on the release'
Assert-That ($workflow -match '--channel dev\b') 'a development build is on its own channel'
Assert-That ($workflow -match '--tag "dev-') 'a development tag doesn''t match the release pattern'
Assert-That ($workflow -match 'Remove-OldDevReleases') 'the job clears old development builds'
Assert-That ($workflow -match 'changelog_section\.ps1' -and $workflow -match 'gh release edit') 'every release and development build keeps its notes from the changelog'
# A development push and a release's tag each build and test once: the publishing job's own package.ps1 validates what ships, so the
# validate job would only repeat it. A push to main validates, having nothing else to do it.
Assert-That ($workflow -match "  validate:\s+name: Validate\s+(#[^\n]*\s+)*if: github\.ref != 'refs/heads/development' && !startsWith\(github\.ref, 'refs/tags/'\)") 'the validate job leaves a development push and a tag to the jobs that publish them'
$developmentJob = [regex]::Match($workflow, "(?s)  publish-development:.*?(?=\n  publish:)").Value
Assert-That ($developmentJob -and $developmentJob -notmatch 'needs:') 'the development job doesn''t wait on a job a development push skips'
Assert-That ($developmentJob -match 'package\.ps1') 'the development job packs through package.ps1'
Assert-That ((Get-Content -LiteralPath (Join-Path $project 'package.ps1') -Raw) -match "validate\.ps1") 'package.ps1 validates what it packs, so the build that ships is tested'
$releaseJob = [regex]::Match($workflow, "(?s)\n  publish:.*").Value
Assert-That ($releaseJob -and $releaseJob -notmatch 'needs:') 'the release job doesn''t wait on a job a tag skips'
Assert-That ($releaseJob -match 'package\.ps1') 'the release job packs through package.ps1, which validates it'
Assert-That ($releaseJob.IndexOf('package.ps1') -lt $releaseJob.IndexOf('vpk upload')) 'a release is validated and packed before anything is uploaded'
Write-Host 'The workflow keeps development builds apart from releases.'

# The compile cache: the validate and development jobs compile through ccache, kept between runs, so an unchanged file is
# not compiled again. The release job compiles everything from source, so what every player installs owes nothing to an
# earlier run. Each ccache download is the pinned release, checked against its hash before it runs, and build.ps1 hands
# it to CMake only when PM_CCACHE names it, so a local build compiles directly.
$validateJob = [regex]::Match($workflow, "(?s)  validate:.*?(?=\n  publish-development:)").Value
foreach ($job in @{ 'validate' = $validateJob; 'development' = $developmentJob }.GetEnumerator()) {
    Assert-That ($job.Value -match 'uses: actions/cache@[0-9a-f]{40} ') "the $($job.Key) job keeps the compile cache, by a pinned action"
    Assert-That ($job.Value -match 'PM_CCACHE=') "the $($job.Key) job compiles through ccache"
    $buildAt = [Math]::Max($job.Value.IndexOf('run: .\validate.ps1'), $job.Value.IndexOf('run: .\package.ps1'))
    Assert-That ($buildAt -gt $job.Value.IndexOf('PM_CCACHE=')) "the $($job.Key) job has ccache before it builds"
}
Assert-That ($releaseJob -notmatch 'actions/cache@' -and $releaseJob -notmatch 'PM_CCACHE') 'the release job compiles from source alone'
$downloads = [regex]::Matches($workflow, 'ccache-4\.14\.2-windows-x86_64\.zip').Count
$checks = [regex]::Matches($workflow, "6993CE645E53192CEC592EE23186200B574AC4C59097ECDE8D9CFD681B47AE84").Count
Assert-That ($downloads -eq 2 -and $checks -eq 2) 'each ccache download is checked against the pinned hash'
$build = Get-Content -LiteralPath (Join-Path $project 'build.ps1') -Raw
Assert-That ($build -match 'CMAKE_CXX_COMPILER_LAUNCHER=\$launcher' -and $build -match '\$env:PM_CCACHE') 'build.ps1 compiles through ccache only when PM_CCACHE names it'
Write-Host 'CI compiles through a kept cache, and a release from source alone.'

Write-Host 'Development build tests passed.'
