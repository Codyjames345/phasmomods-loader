# Exercise tools/publish_assets.ps1 and tools/place_assets.ps1 (pinned release files kept in the private assets
# repository) against a throwaway git repository standing in for it, in isolated project fixtures.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'fixture_helpers.ps1')
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pm pinned ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
# The fixtures' git ignores this machine's configuration (its templates, hooks, signing) and gives its commits an author.
$gitConfig = Join-Path $testRoot 'gitconfig'
Set-Content -LiteralPath $gitConfig -Value ''
$saved = @{}
foreach ($name in 'GIT_CONFIG_GLOBAL', 'GIT_CONFIG_NOSYSTEM', 'GIT_AUTHOR_NAME', 'GIT_COMMITTER_NAME', 'GIT_AUTHOR_EMAIL',
    'GIT_COMMITTER_EMAIL') { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$env:GIT_CONFIG_GLOBAL = $gitConfig
$env:GIT_CONFIG_NOSYSTEM = '1'
$env:GIT_AUTHOR_NAME = $env:GIT_COMMITTER_NAME = 'Fixture'
$env:GIT_AUTHOR_EMAIL = $env:GIT_COMMITTER_EMAIL = 'fixture@example.invalid'

<# Run git, throwing when it fails. #>
function Invoke-Git { & git @args; if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed." } }

<# A project whose payload pins one file (to $Pin) beside one it does not, with the scripts the pinned files need. #>
function New-PinFixture([string]$Name, [string]$Pin, [string]$Content) {
    $fixture = Join-Path $testRoot $Name
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'tools'), (Join-Path $fixture 'assets') | Out-Null
    foreach ($script in @('tools/publish_assets.ps1', 'tools/place_assets.ps1', 'tools/payload_files.ps1')) {
        Copy-Item -LiteralPath (Join-Path $project $script) -Destination (Join-Path $fixture $script)
    }
    Set-Content -LiteralPath (Join-Path $fixture 'tools/payload.psd1') -Value @"
@{
    Payload = @(
        @{ Name = 'mods/a/a.bundle'; Source = 'assets/a.bundle'; Sha256 = '$Pin' }
        @{ Name = 'mods/a/a.txt'; Source = 'assets/a.txt' }
    )
}
"@
    if ($Content) { Set-Content -LiteralPath (Join-Path $fixture 'assets/a.bundle') -Value $Content -NoNewline }
    Set-Content -LiteralPath (Join-Path $fixture 'assets/a.txt') -Value 'not pinned'
    return $fixture
}

function Get-Pin([string]$Fixture) {
    ((Import-PowerShellDataFile -LiteralPath (Join-Path $Fixture 'tools/payload.psd1')).Payload | Where-Object { $_.Sha256 }).Sha256
}

function Assert-Throws([scriptblock]$Action, [string]$Message) {
    try { & $Action | Out-Null; throw 'It unexpectedly succeeded.' } catch { if ($_.Exception.Message -notmatch $Message) { throw } }
}

try {
    # The private repository, and a clone of it as the user keeps one.
    $remote = Join-Path $testRoot 'private.git'
    Invoke-Git init --quiet --bare --initial-branch main $remote
    $clone = Join-Path $testRoot 'private'
    Invoke-Git clone --quiet $remote $clone 2>$null
    Set-Content -LiteralPath (Join-Path $clone 'README.md') -Value 'private assets'
    Invoke-Git -C $clone add README.md
    Invoke-Git -C $clone commit --quiet -m 'Start'
    Invoke-Git -C $clone push --quiet -u origin main 2>$null

    # A rebuilt file is pushed under its own hash, and only then pinned; the other entry is left alone.
    $old = 'A' * 64
    $fixture = New-PinFixture 'publish' $old 'build one'
    $one = (Get-FileHash -LiteralPath (Join-Path $fixture 'assets/a.bundle') -Algorithm SHA256).Hash
    & (Join-Path $fixture 'tools/publish_assets.ps1') -Private $clone -Root $fixture | Out-Null
    if ((Get-Pin $fixture) -ne $one) { throw 'The pin did not move to the published build.' }
    if (@(& git -C $remote ls-tree -r --name-only main) -notcontains "pinned/$one/a.bundle") { throw 'The build was not pushed.' }
    if ((Get-Content -LiteralPath (Join-Path $fixture 'tools/payload.psd1') -Raw) -notmatch "Source = 'assets/a.txt' }") {
        throw 'An unpinned entry changed.'
    }

    # A second build is kept beside the first, never in its place, so a branch pinned to the first still finds it.
    Set-Content -LiteralPath (Join-Path $fixture 'assets/a.bundle') -Value 'build two' -NoNewline
    $two = (Get-FileHash -LiteralPath (Join-Path $fixture 'assets/a.bundle') -Algorithm SHA256).Hash
    & (Join-Path $fixture 'tools/publish_assets.ps1') -Private $clone -Root $fixture | Out-Null
    $tree = @(& git -C $remote ls-tree -r --name-only main)
    if ($tree -notcontains "pinned/$one/a.bundle" -or $tree -notcontains "pinned/$two/a.bundle") { throw 'A build was replaced.' }

    # Publishing what is already pinned changes nothing.
    $commits = @(& git -C $remote rev-list main).Count
    & (Join-Path $fixture 'tools/publish_assets.ps1') -Private $clone -Root $fixture | Out-Null
    if (@(& git -C $remote rev-list main).Count -ne $commits) { throw 'An unchanged build was committed again.' }

    # A clone with changes of its own is refused before anything moves.
    Set-Content -LiteralPath (Join-Path $clone 'stray.txt') -Value 'stray'
    Assert-Throws { & (Join-Path $fixture 'tools/publish_assets.ps1') -Private $clone -Root $fixture } 'uncommitted changes'
    Remove-Item -LiteralPath (Join-Path $clone 'stray.txt')
    Assert-Throws { & (Join-Path $fixture 'tools/publish_assets.ps1') -Private (Join-Path $testRoot 'nowhere') -Root $fixture } 'PM_PRIVATE_ASSETS'

    # CI places each branch's own build from a checkout: the first build for a branch pinned to it, the second for one
    # pinned to that.
    $checkout = Join-Path $testRoot 'checkout'
    Invoke-Git clone --quiet $remote $checkout 2>$null
    foreach ($pin in @($one, $two)) {
        $branch = New-PinFixture "place $pin" $pin $null
        & (Join-Path $branch 'tools/place_assets.ps1') -Assets $checkout -Root $branch | Out-Null
        if ((Get-FileHash -LiteralPath (Join-Path $branch 'assets/a.bundle') -Algorithm SHA256).Hash -ne $pin) {
            throw 'The placed build is not the pinned one.'
        }
    }

    # A pin whose build was never published fails CI by name.
    $unpublished = New-PinFixture 'unpublished' ('B' * 64) $null
    Assert-Throws { & (Join-Path $unpublished 'tools/place_assets.ps1') -Assets $checkout -Root $unpublished } 'publish it with tools/publish_assets.ps1'

    # A stage or package refuses a pinned file that is not its pinned build.
    . (Join-Path $project 'tools/payload_files.ps1')
    $payload = (Import-PowerShellDataFile -LiteralPath (Join-Path $fixture 'tools/payload.psd1')).Payload
    Set-Content -LiteralPath (Join-Path $fixture 'assets/a.bundle') -Value 'build three' -NoNewline
    Assert-Throws { Assert-PayloadSources $fixture $payload } 'is not the build tools/payload.psd1 pins'

    # Every CI job that fetches the private assets places them by their pins, never by copying a file straight out.
    $workflow = Get-Content -LiteralPath (Join-Path $project '.github/workflows/build.yml') -Raw
    $fetches = [regex]::Matches($workflow, 'name: Fetch the private assets').Count
    $places = [regex]::Matches($workflow, 'tools/place_assets\.ps1 -Assets private-assets').Count
    if (-not $fetches -or $fetches -ne $places) { throw "The workflow fetches the private assets $fetches time(s) but places them by pin $places." }
    if ($workflow -match 'private-assets/[^ ]*\.bundle') { throw 'The workflow copies a bundle without its pin.' }

    Write-Output 'Pinned asset tests passed: each build is published under its hash, never replaced, and CI places only the pinned one.'
} finally {
    # Back as they were, a variable that was not set removed again, for the tests that run after this one.
    foreach ($name in $saved.Keys) {
        if ($null -eq $saved[$name]) { Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue }
        else { Set-Item -LiteralPath "Env:$name" -Value $saved[$name] }
    }
    Remove-TestRoot $testRoot
}
