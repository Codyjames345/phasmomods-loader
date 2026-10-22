# Exercise tools/stage.ps1 (the launchable copy in dist\PhasmoMods) in isolated project fixtures.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'fixture_helpers.ps1')
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pg stage ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$payload = (Import-PowerShellDataFile -LiteralPath (Join-Path $project 'tools/payload.psd1')).Payload

<# Create a project with a synthetic file for every payload source and a stale launchable copy. #>
function New-StageFixture {
    [CmdletBinding(SupportsShouldProcess)]
    param([string]$Name)
    $fixture = Join-Path $testRoot $Name
    if (-not $PSCmdlet.ShouldProcess($fixture, 'Create stage fixture')) { return }
    New-Item -ItemType Directory -Path (Join-Path $fixture 'tools') -Force | Out-Null
    foreach ($script in @('tools/stage.ps1', 'tools/payload.psd1')) {
        Copy-Item -LiteralPath (Join-Path $project $script) -Destination (Join-Path $fixture $script)
    }
    Write-PayloadSources $fixture $payload
    New-Item -ItemType Directory -Path (Join-Path $fixture 'dist/PhasmoMods/mods/retired') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $fixture 'dist/PhasmoMods/ModHost.dll') -Value 'previous build'
    Set-Content -LiteralPath (Join-Path $fixture 'dist/PhasmoMods/mods/retired/Retired.dll') -Value 'stale mod'
    return $fixture
}

<# Relative paths and sizes only: a file the launcher holds cannot be read. #>
function Get-Listing([string]$Fixture) {
    $folder = Join-Path $Fixture 'dist/PhasmoMods'
    @(Get-ChildItem -LiteralPath $folder -File -Recurse |
        ForEach-Object { "$($_.FullName.Substring($folder.Length + 1).Replace('\', '/'))=$($_.Length)" } | Sort-Object)
}

function Assert-NoLeftovers([string]$Fixture) {
    if (@(Get-ChildItem -LiteralPath (Join-Path $Fixture 'dist') -Directory -Filter '.*').Count) {
        throw 'Staging left a temporary folder in dist.'
    }
}

<# Require a specific failure and verify that the previous launchable copy is untouched. #>
function Assert-StageFailure([string]$Fixture, [string]$Message) {
    $before = Get-Listing $Fixture
    try {
        & (Join-Path $Fixture 'tools/stage.ps1') -Root $Fixture | Out-Null
        throw 'Staging unexpectedly succeeded.'
    } catch {
        if ($_.Exception.Message -notmatch $Message) { throw }
    }
    if (Compare-Object $before (Get-Listing $Fixture)) { throw 'A failed staging changed the previous launchable copy.' }
    Assert-NoLeftovers $Fixture
}

try {
    $fixture = New-StageFixture 'success'
    & (Join-Path $fixture 'tools/stage.ps1') -Root $fixture | Out-Null
    $expected = @($payload | ForEach-Object { "$($_.Name)=fixture $($_.Source)" } | Sort-Object)
    $folder = Join-Path $fixture 'dist/PhasmoMods'
    $staged = @(Get-ChildItem -LiteralPath $folder -File -Recurse |
        ForEach-Object { "$($_.FullName.Substring($folder.Length + 1).Replace('\', '/'))=$((Get-Content -LiteralPath $_.FullName -Raw).Trim())" } | Sort-Object)
    if (Compare-Object $expected $staged) {
        throw 'dist\PhasmoMods does not hold exactly the current payload (stale files must go).'
    }
    Assert-NoLeftovers $fixture

    $fixture = New-StageFixture 'missing build output'
    Remove-Item -LiteralPath (Join-Path $fixture 'build/ModHost.dll')
    Assert-StageFailure $fixture 'ModHost.dll is missing'

    $fixture = New-StageFixture 'launcher holds a file'
    foreach ($name in @('a-first.txt', 'z-last.txt')) {  # a deletion that stops at the held file would take one of these
        Set-Content -LiteralPath (Join-Path $fixture "dist/PhasmoMods/$name") -Value 'previous'
    }
    $held = [IO.File]::Open((Join-Path $fixture 'dist/PhasmoMods/ModHost.dll'), 'Open', 'Read', 'None')
    try {
        Assert-StageFailure $fixture 'in use'
    } finally {
        $held.Dispose()
    }
    Write-Output 'Stage tests passed: dist\PhasmoMods holds exactly the current build, and a failed refresh keeps the previous copy.'
} finally {
    Remove-TestRoot $testRoot
}
