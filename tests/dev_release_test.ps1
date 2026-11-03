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
# A development push builds and tests once: the job's own package.ps1 validates what ships, so the validate job would only repeat it.
Assert-That ($workflow -match "  validate:\s+name: Validate\s+(#[^\n]*\s+)?if: github\.ref != 'refs/heads/development'") 'the validate job leaves a development push to the job that publishes it'
$developmentJob = [regex]::Match($workflow, "(?s)  publish-development:.*?(?=\n  publish:)").Value
Assert-That ($developmentJob -and $developmentJob -notmatch 'needs:') 'the development job doesn''t wait on a job a development push skips'
Assert-That ($developmentJob -match 'package\.ps1') 'the development job packs through package.ps1'
Assert-That ((Get-Content -LiteralPath (Join-Path $project 'package.ps1') -Raw) -match "validate\.ps1") 'package.ps1 validates what it packs, so the build that ships is tested'
Assert-That ($workflow -match "(?s)\n  publish:.*?needs: validate") 'a release still needs the validate job'
Write-Host 'The workflow keeps development builds apart from releases.'

Write-Host 'Development build tests passed.'
