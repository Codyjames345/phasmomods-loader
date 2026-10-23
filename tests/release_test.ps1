# Exercise the release script in throwaway git repositories; the real repository is never touched.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'fixture_helpers.ps1')
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pg release ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null

<# Run git in a repository and return its output lines. Git reports progress on stderr, which must not
   count as a failure, so only the exit code decides. #>
function Invoke-Git([string]$Repo, [string[]]$GitArguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & git -C $Repo @GitArguments 2>&1 | ForEach-Object { "$_" }
        if ($LASTEXITCODE -ne 0) { throw "git $GitArguments failed: $output" }
        return $output
    } finally {
        $ErrorActionPreference = $previous
    }
}

<# Create a repository on main holding the release script and a committed VERSION of 0.1.0. The
   identity is set inside the repository only. #>
function New-ReleaseFixture {
    [CmdletBinding(SupportsShouldProcess)]
    param([string]$Name)
    $fixture = Join-Path $testRoot $Name
    if (-not $PSCmdlet.ShouldProcess($fixture, 'Create release fixture')) { return }
    New-Item -ItemType Directory -Path (Join-Path $fixture 'tools') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $project 'tools/release.ps1') -Destination (Join-Path $fixture 'tools')
    Set-Content -LiteralPath (Join-Path $fixture 'VERSION') -Value '0.1.0'
    Invoke-Git $fixture @('init', '--quiet', '--initial-branch', 'main') | Out-Null
    Invoke-Git $fixture @('config', 'user.name', 'Release Test') | Out-Null
    Invoke-Git $fixture @('config', 'user.email', 'release-test@example.invalid') | Out-Null
    Invoke-Git $fixture @('config', 'commit.gpgsign', 'false') | Out-Null
    Invoke-Git $fixture @('config', 'tag.gpgsign', 'false') | Out-Null
    Invoke-Git $fixture @('add', '--all') | Out-Null
    Invoke-Git $fixture @('commit', '--quiet', '--message', 'Start') | Out-Null
    return $fixture
}

<# Require the release script to fail with a message matching $Message and leave the history alone. #>
function Assert-ReleaseFailure([string]$Fixture, [string]$Version, [string]$Message) {
    $head = (Invoke-Git $Fixture @('rev-parse', 'HEAD')) -join ''
    $tags = (Invoke-Git $Fixture @('tag')) -join ','
    try {
        & (Join-Path $Fixture 'tools/release.ps1') -Version $Version | Out-Null
        throw 'The release unexpectedly succeeded.'
    } catch {
        if ($_.Exception.Message -notmatch $Message) { throw }
    }
    if ((Invoke-Git $Fixture @('rev-parse', 'HEAD')) -join '' -ne $head) { throw 'A refused release added a commit.' }
    if ((Invoke-Git $Fixture @('tag')) -join ',' -ne $tags) { throw 'A refused release added a tag.' }
}

try {
    # refuses to release from another branch or a detached head
    $fixture = New-ReleaseFixture 'other branch'
    Invoke-Git $fixture @('switch', '--quiet', '--create', 'topic') | Out-Null
    Assert-ReleaseFailure $fixture '0.2.0' 'Releases are made from main'
    $fixture = New-ReleaseFixture 'detached head'
    Invoke-Git $fixture @('checkout', '--quiet', '--detach') | Out-Null
    Assert-ReleaseFailure $fixture '0.2.0' 'Releases are made from main'

    # refuses a dirty tree
    $fixture = New-ReleaseFixture 'untracked file'
    Set-Content -LiteralPath (Join-Path $fixture 'stray.txt') -Value 'stray'
    Assert-ReleaseFailure $fixture '0.2.0' 'The working tree has uncommitted changes'
    $fixture = New-ReleaseFixture 'modified file'
    Set-Content -LiteralPath (Join-Path $fixture 'VERSION') -Value '0.1.1'
    Assert-ReleaseFailure $fixture '0.2.0' 'The working tree has uncommitted changes'

    # refuses a malformed version
    $fixture = New-ReleaseFixture 'malformed version'
    foreach ($bad in @('1.2', 'v1.2.3', '1.2.3.4', 'one')) {
        Assert-ReleaseFailure $fixture $bad 'MAJOR\.MINOR\.PATCH'
    }

    # refuses a version that is not newer
    $fixture = New-ReleaseFixture 'not newer'
    Assert-ReleaseFailure $fixture '0.1.0' 'newer'
    Set-Content -LiteralPath (Join-Path $fixture 'VERSION') -Value '0.10.0'
    Invoke-Git $fixture @('commit', '--quiet', '--all', '--message', 'Raise') | Out-Null
    Assert-ReleaseFailure $fixture '0.9.0' 'newer'

    # refuses an existing tag
    $fixture = New-ReleaseFixture 'existing tag'
    Invoke-Git $fixture @('tag', 'v0.2.0') | Out-Null
    Assert-ReleaseFailure $fixture '0.2.0' 'already exists'

    # bumps and tags, and pushes nothing without -Push
    $fixture = New-ReleaseFixture 'bump and tag'
    $origin = Join-Path $testRoot 'bump origin.git'
    Invoke-Git $testRoot @('init', '--quiet', '--bare', $origin) | Out-Null
    Invoke-Git $fixture @('remote', 'add', 'origin', $origin) | Out-Null
    $previousHead = (Invoke-Git $fixture @('rev-parse', 'HEAD')) -join ''
    & (Join-Path $fixture 'tools/release.ps1') -Version '0.2.0' | Out-Null
    if ((Get-Content -LiteralPath (Join-Path $fixture 'VERSION') -Raw).Trim() -ne '0.2.0') { throw 'VERSION was not bumped to 0.2.0.' }
    if ((Invoke-Git $fixture @('log', '-1', '--format=%s')) -join '' -ne 'Release 0.2.0') { throw 'The release commit has the wrong subject.' }
    if ((Invoke-Git $fixture @('rev-parse', 'HEAD~1')) -join '' -ne $previousHead) { throw 'The release is not a single commit on the previous head.' }
    if (@(Invoke-Git $fixture @('show', '--name-only', '--format=', 'HEAD')) -join ',' -ne 'VERSION') { throw 'The release commit changed more than VERSION.' }
    if ((Invoke-Git $fixture @('cat-file', '-t', 'v0.2.0')) -join '' -ne 'tag') { throw 'The release tag is not annotated.' }
    $tagged = (Invoke-Git $fixture @('rev-parse', 'v0.2.0^{commit}')) -join ''
    $head = (Invoke-Git $fixture @('rev-parse', 'HEAD')) -join ''
    if ($tagged -ne $head) { throw 'The tag is not on the release commit.' }
    if (@(Invoke-Git $origin @('for-each-ref')).Count) { throw 'The release pushed without -Push.' }

    # -Push sends main and the tag to origin
    $fixture = New-ReleaseFixture 'push'
    $origin = Join-Path $testRoot 'push origin.git'
    Invoke-Git $testRoot @('init', '--quiet', '--bare', $origin) | Out-Null
    Invoke-Git $fixture @('remote', 'add', 'origin', $origin) | Out-Null
    & (Join-Path $fixture 'tools/release.ps1') -Version '0.2.0' -Push | Out-Null
    $pushed = (Invoke-Git $origin @('for-each-ref', '--format=%(refname)')) -join ','
    if ($pushed -ne 'refs/heads/main,refs/tags/v0.2.0') { throw "The release pushed the wrong refs: $pushed" }

    # a rejected push sends nothing and says how to undo the local release or retry
    $fixture = New-ReleaseFixture 'rejected push'
    $origin = Join-Path $testRoot 'rejected origin.git'
    Invoke-Git $testRoot @('init', '--quiet', '--bare', $origin) | Out-Null
    Invoke-Git $fixture @('remote', 'add', 'origin', $origin) | Out-Null
    Invoke-Git $fixture @('push', '--quiet', 'origin', 'main') | Out-Null
    $other = Join-Path $testRoot 'rejected other'
    Invoke-Git $testRoot @('clone', '--quiet', $origin, $other) | Out-Null
    Invoke-Git $other @('config', 'user.name', 'Release Test') | Out-Null
    Invoke-Git $other @('config', 'user.email', 'release-test@example.invalid') | Out-Null
    Invoke-Git $other @('config', 'commit.gpgsign', 'false') | Out-Null
    Invoke-Git $other @('commit', '--quiet', '--allow-empty', '--message', 'Ahead') | Out-Null
    Invoke-Git $other @('push', '--quiet', 'origin', 'main') | Out-Null
    $originRefs = (Invoke-Git $origin @('for-each-ref', '--format=%(refname) %(objectname)')) -join ','
    $message = ''
    try {
        & (Join-Path $fixture 'tools/release.ps1') -Version '0.2.0' -Push | Out-Null
        throw 'The release unexpectedly pushed to an origin that is ahead.'
    } catch {
        $message = $_.Exception.Message
    }
    foreach ($step in @('git tag --delete v0.2.0', 'git reset --hard HEAD~1', 'git push --atomic origin main v0.2.0')) {
        if (-not $message.Contains($step)) { throw "The rejected push did not name '$step': $message" }
    }
    if ((Invoke-Git $origin @('for-each-ref', '--format=%(refname) %(objectname)')) -join ',' -ne $originRefs) { throw 'A rejected push changed origin.' }
    Write-Output 'Release tests passed: the script refuses unsafe releases and bumps, tags and pushes only on request.'
} finally {
    Remove-TestRoot $testRoot
}
