# Build and test every native target and packaging.
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE -ne 0) { throw 'The native build failed.' }
& ctest --test-dir (Join-Path $PSScriptRoot 'build') --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'The native tests failed.' }
& (Join-Path $PSScriptRoot 'tests/package_test.ps1')
& (Join-Path $PSScriptRoot 'tests/stage_test.ps1')
& (Join-Path $PSScriptRoot 'tests/release_test.ps1')
