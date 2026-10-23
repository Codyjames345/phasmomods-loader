# Exercise release publication through the package script in isolated project fixtures.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'fixture_helpers.ps1')
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pg package ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$payload = (Import-PowerShellDataFile -LiteralPath (Join-Path $project 'tools/payload.psd1')).Payload

<# Create a project with synthetic artifacts, a previous release to protect, and a fake vpk. The fake
   records its arguments and the files it was asked to pack, writes a Setup.exe and a package into
   its output folder, and exits with $VpkExit. #>
function New-PackageFixture {
    [CmdletBinding(SupportsShouldProcess)]
    param([string]$Name, [int]$VpkExit = 0)
    $fixture = Join-Path $testRoot $Name
    if (-not $PSCmdlet.ShouldProcess($fixture, 'Create package fixture')) { return }
    foreach ($directory in @('tools', 'dist/PhasmoMods', 'dist/releases')) {
        New-Item -ItemType Directory -Path (Join-Path $fixture $directory) -Force | Out-Null
    }
    Copy-Item -LiteralPath (Join-Path $project 'package.ps1') -Destination $fixture
    Copy-Item -LiteralPath (Join-Path $project 'tools/payload.psd1') -Destination (Join-Path $fixture 'tools')
    Set-Content -LiteralPath (Join-Path $fixture 'validate.ps1') -Value '# Fixture validation succeeds.'
    Set-Content -LiteralPath (Join-Path $fixture 'VERSION') -Value '0.1.0'
    Write-PayloadSources $fixture $payload
    Set-Content -LiteralPath (Join-Path $fixture 'dist/releases/previous-release.txt') -Value 'previous release'
    Set-Content -LiteralPath (Join-Path $fixture 'dist/PhasmoMods/Retired.dll') -Value 'stale file'
    $fake = @'
$here = Split-Path $MyInvocation.MyCommand.Path -Parent
$args | Set-Content -LiteralPath (Join-Path $here 'vpk-args.txt')
$packDir = $args[[array]::IndexOf($args, '--packDir') + 1]
$outputDir = $args[[array]::IndexOf($args, '--outputDir') + 1]
$base = (Get-Item -LiteralPath $packDir).FullName.TrimEnd('\') + '\'
Get-ChildItem -LiteralPath $packDir -Recurse -File | ForEach-Object { $_.FullName.Substring($base.Length).Replace('\', '/') } |
    Set-Content -LiteralPath (Join-Path $here 'vpk-content.txt')
Set-Content -LiteralPath (Join-Path $outputDir 'PhasmoModsLoader-win-Setup.exe') -Value 'setup'
Set-Content -LiteralPath (Join-Path $outputDir 'PhasmoModsLoader-0.1.0-full.nupkg') -Value 'package'
exit __EXIT__
'@
    Set-Content -LiteralPath (Join-Path $fixture 'fake-vpk.ps1') -Value $fake.Replace('__EXIT__', "$VpkExit")
    return $fixture
}

<# Run the fixture's package script with its fake vpk. #>
function Invoke-Package([string]$Fixture) {
    & (Join-Path $Fixture 'package.ps1') -Vpk (Join-Path $Fixture 'fake-vpk.ps1') | Out-Null
}

<# List the previous release folder's names and hashes, to prove a failure leaves it alone. #>
function Get-ReleaseState([string]$Fixture) {
    $releases = Join-Path $Fixture 'dist/releases'
    @(Get-ChildItem -LiteralPath $releases -Recurse -File | Sort-Object FullName |
        ForEach-Object { $_.Name + ':' + (Get-FileHash -LiteralPath $_.FullName).Hash }) -join '|'
}

<# Require a specific failure and verify that it cannot replace the previous release. #>
function Assert-PackageFailure([string]$Fixture, [string]$Message) {
    $before = Get-ReleaseState $Fixture
    try {
        Invoke-Package $Fixture
        throw 'Packaging unexpectedly succeeded.'
    } catch {
        if ($_.Exception.Message -notmatch $Message) { throw }
    }
    if ((Get-ReleaseState $Fixture) -ne $before) {
        throw 'A failed package attempt changed the previous release.'
    }
}

try {
    foreach ($failure in @('native build', 'native tests')) {
        $fixture = New-PackageFixture $failure
        $validationScript = Join-Path $fixture 'validate.ps1'
        Copy-Item -LiteralPath (Join-Path $project 'validate.ps1') -Destination $validationScript -Force
        New-Item -ItemType Directory -Path (Join-Path $fixture 'tests') | Out-Null
        Set-Content -LiteralPath (Join-Path $fixture 'tests/package_test.ps1') -Value '# Fixture package checks do not recurse.'
        Set-Content -LiteralPath (Join-Path $fixture 'tests/stage_test.ps1') -Value '# Fixture stage checks run on their own.'
        Set-Content -LiteralPath (Join-Path $fixture 'tests/release_test.ps1') -Value '# Fixture release checks run on their own.'
        Set-Content -LiteralPath (Join-Path $fixture 'tools/gamecheck.ps1') -Value 'exit 0 # The fixture has no game to check.'
        $buildExit = if ($failure -eq 'native build') { 7 } else { 0 }
        $testExit = if ($failure -eq 'native tests') { 7 } else { 0 }
        Set-Content -LiteralPath (Join-Path $fixture 'build.ps1') -Value "exit $buildExit"
        $fakeCtest = Join-Path $fixture 'ctest.cmd'
        Set-Content -LiteralPath $fakeCtest -Encoding ASCII -Value "@exit /b $testExit"
        $originalPath = $env:PATH
        try {
            $env:PATH = $fixture + ';' + $originalPath
            if ((Get-Command ctest).Source -ne $fakeCtest) { throw 'The fixture did not isolate the CTest command.' }
            Assert-PackageFailure $fixture "The $failure failed"

            # Removing either real exit-code guard must let an invalid release through and fail the assertion.
            $validation = Get-Content -LiteralPath $validationScript -Raw
            $guard = 'if ($LASTEXITCODE -ne 0) { throw ''The ' + $failure + ' failed.'' }'
            if ([regex]::Matches($validation, [regex]::Escape($guard)).Count -ne 1) {
                throw 'Cannot locate the validation guard for the mutation check.'
            }
            Set-Content -LiteralPath $validationScript -Value $validation.Replace($guard, '')
            $mutationCaught = $false
            try {
                Assert-PackageFailure $fixture "The $failure failed"
            } catch {
                if ($_.Exception.Message -ne 'Packaging unexpectedly succeeded.') { throw }
                $mutationCaught = $true
            }
            if (-not $mutationCaught) { throw "The package test did not catch the removed $failure guard." }
        } finally {
            $env:PATH = $originalPath
        }
    }
    foreach ($missing in @('PhasmoMods Loader.exe', 'ModHost.dll', 'PlayableGhost.dll')) {
        $fixture = New-PackageFixture $missing
        Remove-Item -LiteralPath (Join-Path $fixture "build/$missing")
        Assert-PackageFailure $fixture 'missing|Cannot find'
    }

    $fixture = New-PackageFixture 'bad version'
    Set-Content -LiteralPath (Join-Path $fixture 'VERSION') -Value 'one'
    Assert-PackageFailure $fixture 'VERSION'

    # pack passes version and id
    $fixture = New-PackageFixture 'successful release'
    Invoke-Package $fixture
    $arguments = @(Get-Content -LiteralPath (Join-Path $fixture 'vpk-args.txt'))
    $expectedPrefix = @('pack', '--packId', 'PhasmoModsLoader', '--packVersion', '0.1.0')
    if (Compare-Object $expectedPrefix @($arguments[0..4]) -SyncWindow 0) { throw "vpk did not receive the pack command, id and version: $arguments" }
    $mainExe = $arguments[[array]::IndexOf($arguments, '--mainExe') + 1]
    if ($mainExe -ne 'PhasmoMods Loader.exe') { throw "vpk received the wrong main executable: $mainExe" }

    # pack stages exactly the payload, and nothing from a stale dist/PhasmoMods
    $staged = @(Get-Content -LiteralPath (Join-Path $fixture 'vpk-content.txt') | Sort-Object)
    $expected = @($payload | ForEach-Object { $_.Name } | Sort-Object)
    if (Compare-Object $expected $staged) { throw "vpk was given a folder that is not exactly the payload: $staged" }
    if (-not (Test-Path -LiteralPath (Join-Path $fixture 'dist/releases/PhasmoModsLoader-win-Setup.exe'))) { throw 'Packaging did not publish Setup.exe to dist/releases.' }
    if (-not (Test-Path -LiteralPath (Join-Path $fixture 'dist/releases/PhasmoModsLoader-0.1.0-full.nupkg'))) { throw 'Packaging did not publish the full package to dist/releases.' }
    if (Test-Path -LiteralPath (Join-Path $fixture 'dist/PhasmoMods.zip')) { throw 'Packaging still writes the old ZIP.' }
    if (@(Get-ChildItem -LiteralPath (Join-Path $fixture 'dist') -Force | Where-Object { $_.Name -like '.*' }).Count) {
        throw 'Packaging left a staging folder in dist.'
    }

    # failed pack leaves dist/releases untouched
    $fixture = New-PackageFixture 'failed pack' -VpkExit 1
    Assert-PackageFailure $fixture 'vpk'
    if (@(Get-ChildItem -LiteralPath (Join-Path $fixture 'dist') -Force | Where-Object { $_.Name -like '.*' }).Count) {
        throw 'A failed pack left a staging folder in dist.'
    }

    $fixture = New-PackageFixture 'first release'
    Remove-Item -LiteralPath (Join-Path $fixture 'dist/releases') -Recurse
    Invoke-Package $fixture
    if (-not (Test-Path -LiteralPath (Join-Path $fixture 'dist/releases/PhasmoModsLoader-win-Setup.exe') -PathType Leaf)) {
        throw 'Packaging did not publish the first release.'
    }
    Write-Output 'Package tests passed: failures preserve the release; vpk packs exactly the verified payload.'
} finally {
    Remove-TestRoot $testRoot
}
