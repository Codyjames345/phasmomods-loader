# Exercise release publication through the package script in isolated project fixtures.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'fixture_helpers.ps1')
$project = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('pg package ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$payload = (Import-PowerShellDataFile -LiteralPath (Join-Path $project 'tools/payload.psd1')).Payload

<# Create a project with synthetic artifacts and a previous release to protect. #>
function New-PackageFixture {
    [CmdletBinding(SupportsShouldProcess)]
    param([string]$Name)
    $fixture = Join-Path $testRoot $Name
    if (-not $PSCmdlet.ShouldProcess($fixture, 'Create package fixture')) { return }
    foreach ($directory in @('host', 'tools', 'dist/PhasmoMods')) {
        New-Item -ItemType Directory -Path (Join-Path $fixture $directory) -Force | Out-Null
    }
    Copy-Item -LiteralPath (Join-Path $project 'package.ps1') -Destination $fixture
    Copy-Item -LiteralPath (Join-Path $project 'tools/payload.psd1') -Destination (Join-Path $fixture 'tools')
    Set-Content -LiteralPath (Join-Path $fixture 'validate.ps1') -Value '# Fixture validation succeeds.'
    Write-PayloadSources $fixture $payload
    $source = 'constexpr char kGameAssemblySha256[] = "' + ('a' * 64) + '";' + [Environment]::NewLine +
              'constexpr char kUnityPlayerSha256[] = "' + ('b' * 64) + '";'
    Set-Content -LiteralPath (Join-Path $fixture 'host/build_check.cpp') -Value $source
    Set-Content -LiteralPath (Join-Path $fixture 'dist/PhasmoMods.zip') -Value 'previous release'
    Set-Content -LiteralPath (Join-Path $fixture 'dist/PhasmoMods/Retired.dll') -Value 'stale file'
    return $fixture
}

<# Require a specific failure and verify that it cannot replace the previous release. #>
function Assert-PackageFailure([string]$Fixture, [string]$Message) {
    $release = Join-Path $Fixture 'dist/PhasmoMods.zip'
    $before = (Get-FileHash -LiteralPath $release).Hash
    try {
        & (Join-Path $Fixture 'package.ps1') | Out-Null
        throw 'Packaging unexpectedly succeeded.'
    } catch {
        if ($_.Exception.Message -notmatch $Message) { throw }
    }
    if ((Get-FileHash -LiteralPath $release).Hash -ne $before) {
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
    $fixture = New-PackageFixture 'archive failure'
    # Inject a failure at the archive command in the isolated script, without overriding a system cmdlet.
    $fixtureScript = Join-Path $fixture 'package.ps1'
    $contents = Get-Content -LiteralPath $fixtureScript -Raw
    $archiveCall = '(?m)^\s*New-ReleaseArchive \$content[^\r\n]*'
    if ([regex]::Matches($contents, $archiveCall).Count -ne 1) { throw 'Cannot locate the archive boundary for failure injection.' }
    Set-Content -LiteralPath $fixtureScript -Value ([regex]::Replace($contents, $archiveCall, "    throw 'expected archive failure'"))
    Assert-PackageFailure $fixture 'expected archive failure'
    $fixture = New-PackageFixture 'bad supported hashes'
    Set-Content -LiteralPath (Join-Path $fixture 'host/build_check.cpp') -Value 'no supported hashes'
    Assert-PackageFailure $fixture 'supported.*hash|hash.*supported'

    $fixture = New-PackageFixture 'successful release'
    & (Join-Path $fixture 'package.ps1') | Out-Null
    $extracted = Join-Path $fixture 'extracted'
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $fixture 'dist/PhasmoMods.zip'))
    try {
        $names = @($zip.Entries | ForEach-Object { $_.FullName })
    } finally {
        $zip.Dispose()
    }
    if (@($names | Where-Object { $_.Contains('\') }).Count) { throw 'The release ZIP stores entry names with backslashes.' }
    if (-not ($names -contains 'PhasmoMods Loader.exe') -or ($names -contains 'ModLauncher.exe')) { throw 'The release ZIP must hold PhasmoMods Loader.exe and no ModLauncher.exe.' }
    if (-not ($names -contains 'mods/playable-ghost/PlayableGhost.dll')) { throw 'The release ZIP does not keep the mods folder layout.' }
    Expand-Archive -LiteralPath (Join-Path $fixture 'dist/PhasmoMods.zip') -DestinationPath $extracted
    $expected = @(@($payload | ForEach-Object { $_.Name }) + 'manifest.json' | Sort-Object)
    $actual = @(Get-ChildItem -LiteralPath $extracted -Recurse -File | ForEach-Object { $_.FullName.Substring($extracted.Length + 1).Replace('\', '/') } | Sort-Object)
    if (Compare-Object $expected $actual) { throw 'The release archive does not contain exactly the payload, in its folders, and the manifest.' }
    foreach ($file in $payload) {
        if ((Get-Content -LiteralPath (Join-Path $extracted $file.Name) -Raw).Trim() -ne "fixture $($file.Source)") {
            throw "The release holds the wrong file at $($file.Name)."
        }
    }
    $manifest = Get-Content -LiteralPath (Join-Path $extracted 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.gameplayVerified -ne $false) { throw 'Packaging claims unverified gameplay.' }
    if ($manifest.supportedBinarySha256.'GameAssembly.dll' -ne ('a' * 64) -or
        $manifest.supportedBinarySha256.'UnityPlayer.dll' -ne ('b' * 64)) {
        throw 'Supported hashes were not derived from the native build checks.'
    }
    if (@($manifest.artifacts).Count -ne @($payload).Count) { throw 'The manifest must describe every payload file.' }
    foreach ($artifact in $manifest.artifacts) {
        $file = Get-Item -LiteralPath (Join-Path $extracted $artifact.name)
        if ($file.Length -ne $artifact.bytes -or (Get-FileHash -LiteralPath $file.FullName).Hash -ne $artifact.sha256) {
            throw "The manifest does not describe $($artifact.name)."
        }
    }
    $fixture = New-PackageFixture 'first release'
    Remove-Item -LiteralPath (Join-Path $fixture 'dist/PhasmoMods.zip')
    & (Join-Path $fixture 'package.ps1') | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $fixture 'dist/PhasmoMods.zip') -PathType Leaf)) {
        throw 'Packaging did not publish the first release.'
    }
    Write-Output 'Package tests passed: failures preserve the release; the ZIP contains only verified payload files.'
} finally {
    Remove-TestRoot $testRoot
}
