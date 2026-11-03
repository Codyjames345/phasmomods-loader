# Builds the SDK's API reference with Doxygen into build\docs\html, from sdk\Doxyfile. A warning fails it, so an undocumented public
# declaration, a missing @param or @return, or a broken @see stops the build. Doxygen is used for this alone and is not part of
# what ships. It comes from PM_DOXYGEN (the executable's path) or the PATH, and must be the pinned version.
# -IfConfigured runs only the Doxygen PM_DOXYGEN names and exits quietly without one, as tools/gamecheck.ps1 does for a game that is
# not set up: a build machine's own Doxygen on the PATH, of whatever version, doesn't ask for the reference.
# usage: tools\docs.ps1 [-IfConfigured]
param([switch]$IfConfigured)
$ErrorActionPreference = 'Stop'
$pinned = '1.9.8'

$doxygen = $env:PM_DOXYGEN
if (-not $doxygen -and -not $IfConfigured) {
  $found = Get-Command doxygen -ErrorAction SilentlyContinue
  if ($found) { $doxygen = $found.Source }
}
if (-not $doxygen -or -not (Test-Path -LiteralPath $doxygen -PathType Leaf)) {
  Write-Host "API reference: no Doxygen found (install $pinned, or set PM_DOXYGEN to doxygen.exe)."
  if ($IfConfigured) { exit 0 }
  exit 2
}
$version = (& $doxygen --version | Select-Object -First 1).Trim()
if (-not $version.StartsWith($pinned)) { throw "The API reference needs Doxygen $pinned; $doxygen is $version." }

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
  New-Item -ItemType Directory -Force -Path (Join-Path $root 'build\docs') | Out-Null
  & $doxygen 'sdk\Doxyfile'
  $code = $LASTEXITCODE
} finally {
  Pop-Location
}
if ($code -ne 0) {
  $log = Join-Path $root 'build\docs\warnings.txt'
  if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | Select-Object -First 40 | Write-Host }
  throw "The API reference has warnings ($code); the full list is in build\docs\warnings.txt."
}
Write-Host 'API reference: no warnings. Open build\docs\html\index.html.'
