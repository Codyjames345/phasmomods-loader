# Build and test every native target and packaging.
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'build.ps1')
if ($LASTEXITCODE -ne 0) { throw 'The native build failed.' }
& ctest --test-dir (Join-Path $PSScriptRoot 'build') --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'The native tests failed.' }
& dotnet run --project (Join-Path $PSScriptRoot 'tests/emote_tools') -c Release
if ($LASTEXITCODE -ne 0) { throw 'The emote tools tests failed.' }
& (Join-Path $PSScriptRoot 'tests/assetcheck_test.ps1')
if ($LASTEXITCODE -ne 0) { throw 'The scene check tests failed.' }
& (Join-Path $PSScriptRoot 'tools/gamecheck.ps1') -IfConfigured -Mods (Join-Path $PSScriptRoot 'dist/PhasmoMods/mods')
if ($LASTEXITCODE -ne 0) { throw 'The offline game check failed.' }
& (Join-Path $PSScriptRoot 'tests/docs_test.ps1')
& (Join-Path $PSScriptRoot 'tests/dev_release_test.ps1')
& (Join-Path $PSScriptRoot 'tools/docs.ps1')
if ($LASTEXITCODE -ne 0) { throw 'The API reference failed.' }
& (Join-Path $PSScriptRoot 'tests/package_test.ps1')
& (Join-Path $PSScriptRoot 'tests/stage_test.ps1')
& (Join-Path $PSScriptRoot 'tests/pinned_assets_test.ps1')
& (Join-Path $PSScriptRoot 'tests/changelog_section_test.ps1')
& (Join-Path $PSScriptRoot 'tools/style_check.ps1')
if ($LASTEXITCODE -ne 0) { throw 'The style check found whitespace to fix.' }
& (Join-Path $PSScriptRoot 'tests/style_check_test.ps1')
& (Join-Path $PSScriptRoot 'tests/release_test.ps1')
