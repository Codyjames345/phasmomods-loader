# Configure and build PlayableGhost (Release, x64) with the installed Visual Studio toolchain, then
# refresh the launchable copy in dist\PhasmoMods (tools/stage.ps1).
param([string]$Config = 'Release')
$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio with the C++ x64 toolset is required.' }
$root = $PSScriptRoot
$build = Join-Path $root 'build'
# The CI build compiles through ccache, which PM_CCACHE names (.github/workflows/build.yml), so a file compiled before
# with the same inputs comes from its cache. Unset, the launcher is set empty, so a local build compiles directly even
# where an earlier configure set one.
$launcher = if ($env:PM_CCACHE) { $env:PM_CCACHE } else { '' }
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>&1 && cmake -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config `"-DCMAKE_C_COMPILER_LAUNCHER=$launcher`" `"-DCMAKE_CXX_COMPILER_LAUNCHER=$launcher`" >nul && cmake --build `"$build`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
& (Join-Path $root 'tools/stage.ps1') -Root $root
