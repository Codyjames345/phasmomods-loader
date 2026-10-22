# Configure and build PlayableGhost (Release, x64) with the installed Visual Studio toolchain, then
# refresh the launchable copy in dist\PhasmoMods (tools/stage.ps1).
param([string]$Config = 'Release')
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio with the C++ x64 toolset is required.' }
$root = $PSScriptRoot
$build = Join-Path $root 'build'
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>&1 && cmake -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config >nul && cmake --build `"$build`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
& (Join-Path $root 'tools/stage.ps1') -Root $root
