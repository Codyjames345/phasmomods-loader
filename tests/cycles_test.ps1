# Exercise tools/cycles.py, the map-header cycle check (sdk/internals/mapping.md): it must exit 1 on
# a cycle whose edge sits at the top of a file (tests/fixtures/cycles/unsafe), exit 0 on the
# after-the-entries layout it exists to allow (tests/fixtures/cycles/allowed), and exit 0 on the
# real sdk/map. A fixture supplies the failing case so the real headers are never broken to prove
# the step works. Skipped, and green, when no Python is installed, as tests/assetcheck_test.ps1 is:
# cycles.py needs Python but nothing else. validate.ps1 runs this as its own guarded step.
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$fixture = Join-Path $PSScriptRoot 'fixtures/cycles'
$check = Join-Path $project 'tools/cycles.py'

<# Whether this command runs Python: the Windows Store's python.exe alias is found but exits 9009. #>
function Test-Python([string[]]$Command) {
  if (-not (Get-Command $Command[0] -ErrorAction SilentlyContinue)) { return $false }
  $exe = $Command[0]
  $rest = @($Command | Select-Object -Skip 1)
  $ErrorActionPreference = 'Continue'
  & $exe @rest -I -c 'import sys' 2>$null | Out-Null
  return $LASTEXITCODE -eq 0
}

<# uv's Python 3.14, the version tools/gamecheck.ps1 makes its venv with, or nothing. #>
function Find-UvPython {
  if (-not (Get-Command uv -ErrorAction SilentlyContinue)) { return $null }
  $ErrorActionPreference = 'Continue'
  $found = & uv python find 3.14 2>$null
  if ($LASTEXITCODE -ne 0) { return $null }
  return $found
}

$candidates = @(, @('py', '-3')) + @(, @('python'))
$uvPython = Find-UvPython
if ($uvPython) { $candidates = @(, @($uvPython)) + $candidates }
$python = $null
foreach ($candidate in $candidates) {
  if (Test-Python $candidate) { $python = $candidate; break }
}
if (-not $python) {
  Write-Host 'cycles test: skipped, no Python is installed.'
  exit 0
}

<# Run the check over these headers; its output lines and exit code. #>
function Invoke-Cycles([string]$MapDir) {
  $exe = $python[0]
  $rest = @($python | Select-Object -Skip 1)
  # Windows PowerShell turns a native command's error output into a terminating error under Stop.
  $ErrorActionPreference = 'Continue'
  $lines = @(& $exe @rest -I $check $MapDir 2>$null)
  return @{ Lines = $lines; Code = $LASTEXITCODE }
}

# A cycle whose edges are both top-of-file includes: cycles.py reports it and exits 1.
$run = Invoke-Cycles (Join-Path $fixture 'unsafe')
if ($run.Code -ne 1) { throw "cycles.py exited $($run.Code) on the unsafe fixture, expected 1: $(($run.Lines -join ' / '))" }
if (-not (@($run.Lines | Where-Object { $_ -like 'cycle:*' }))) {
  throw "cycles.py did not name the cycle on the unsafe fixture: $(($run.Lines -join ' / '))"
}
Write-Host 'cycles.py flags a top-of-file include cycle.'

# The same cycle built only from after-the-entries includes: the layout, so it is allowed and exits 0.
$run = Invoke-Cycles (Join-Path $fixture 'allowed')
if ($run.Code -ne 0) { throw "cycles.py exited $($run.Code) on the allowed fixture, expected 0: $(($run.Lines -join ' / '))" }
if (-not (@($run.Lines | Where-Object { $_ -like 'allowed*' }))) {
  throw "cycles.py did not allow the after-the-entries cycle: $(($run.Lines -join ' / '))"
}
Write-Host 'cycles.py allows the after-the-entries layout.'

# The real map headers: every cycle between them uses the layout, so the tree passes.
$run = Invoke-Cycles (Join-Path $project 'sdk/map')
if ($run.Code -ne 0) {
  Write-Host ($run.Lines -join "`n")
  throw 'cycles.py found an unsafe include cycle in sdk/map; see sdk/internals/mapping.md.'
}
Write-Host 'cycles.py passes on sdk/map.'
exit 0
