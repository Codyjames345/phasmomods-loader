# Exercise tools/assetcheck.py's check on a recorded snapshot (tests/fixtures/assetcheck), which needs
# Python but not UnityPy: a path present, one missing and one Transform.Find would have to choose for,
# the layer table, Animator parameters, and what makes it exit 0, 1 or 2. Skipped, and green, when no
# Python is installed: only the configured offline check needs it.
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$fixture = Join-Path $PSScriptRoot 'fixtures/assetcheck'
$check = Join-Path $project 'tools/assetcheck.py'

$python = if (Get-Command py -ErrorAction SilentlyContinue) { @('py', '-3') }
  elseif (Get-Command python -ErrorAction SilentlyContinue) { @('python') }
if (-not $python) {
  Write-Host 'assetcheck test: skipped, no Python is installed.'
  exit 0
}

<# Run the check on the snapshot with these entries; its output lines and exit code. #>
function Invoke-AssetCheck([string]$Entries, [string]$Snapshot = (Join-Path $fixture 'snapshot.json')) {
  $exe = $python[0]
  $rest = @($python | Select-Object -Skip 1)
  # Windows PowerShell turns a native command's error output into a terminating error under Stop.
  $ErrorActionPreference = 'Continue'
  $lines = @(& $exe @rest -I $check --snapshot $Snapshot --entries $Entries 2>$null)
  return @{ Lines = $lines; Code = $LASTEXITCODE }
}

$root = Join-Path ([IO.Path]::GetTempPath()) ('pg assetcheck ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
  # Every kind of answer, in the entries' order; a required entry is missing, so it exits 1.
  $all = Join-Path $fixture 'entries.tsv'
  $run = Invoke-AssetCheck $all
  $expected = @(Get-Content -LiteralPath (Join-Path $fixture 'expected.txt'))
  if ($run.Code -ne 1) { throw "assetcheck exited $($run.Code) on the fixture, expected 1." }
  if (($run.Lines -join "`n") -ne ($expected -join "`n")) {
    throw "assetcheck printed:`n$($run.Lines -join "`n")`nexpected:`n$($expected -join "`n")"
  }
  Write-Host 'assetcheck reports a present path, a missing one and the layer table.'

  # Without the required entry that is missing, what is left missing is optional: it exits 0.
  $optional = Join-Path $root 'optional.tsv'
  $kept = @(Get-Content -LiteralPath $all | Where-Object { -not $_.StartsWith("Scene.missing`t") })
  [IO.File]::WriteAllText($optional, (($kept -join "`n") + "`n"))
  $run = Invoke-AssetCheck $optional
  if ($run.Code -ne 0 -or $run.Lines.Count -ne $kept.Count) {
    throw "assetcheck exited $($run.Code) with only optional entries missing, expected 0."
  }
  Write-Host 'assetcheck passes when only optional entries are missing.'

  # Input it cannot read exits 2: a malformed entry line, and a snapshot that is not JSON.
  $bad = Join-Path $root 'bad.tsv'
  [IO.File]::WriteAllText($bad, "Scene.contents`tprefab`t1`tResources/Player`tPages`tBookController`t`n")
  $run = Invoke-AssetCheck $bad
  if ($run.Code -ne 2) { throw "assetcheck exited $($run.Code) on a malformed entry, expected 2." }
  $notJson = Join-Path $root 'snapshot.json'
  [IO.File]::WriteAllText($notJson, 'not json')
  $run = Invoke-AssetCheck $all $notJson
  if ($run.Code -ne 2) { throw "assetcheck exited $($run.Code) on a snapshot that is not JSON, expected 2." }
  $malformed = Join-Path $root 'malformed.json'
  [IO.File]::WriteAllText($malformed, '{"layers": [], "places": {"Resources/Player": {"objects": [{"name": "A"}]}}}')
  $run = Invoke-AssetCheck $all $malformed
  if ($run.Code -ne 2) { throw "assetcheck exited $($run.Code) on a malformed snapshot, expected 2." }
  Write-Host 'assetcheck exits 2 on input it cannot read.'

  # A hierarchy whose parents loop (A under B under A) still gets an answer, naming the loop's path.
  $cyclic = Join-Path $root 'cyclic.json'
  [IO.File]::WriteAllText($cyclic, '{"layers": [], "places": {"Loop": {"objects": [' +
    '{"name": "A", "parent": 1, "components": ["Root"]}, {"name": "B", "parent": 0, "components": []}],' +
    ' "controllers": {}}}}')
  $loopEntries = Join-Path $root 'loop.tsv'
  [IO.File]::WriteAllText($loopEntries, "Scene.loop`tscene`t1`tLoop`tB/C`tRoot`t`n")
  $run = Invoke-AssetCheck $loopEntries $cyclic
  if ($run.Code -ne 1 -or ($run.Lines -join "`n") -ne 'MISSING Scene.loop nothing at B/C under B/A in Loop') {
    throw "assetcheck on a looping hierarchy exited $($run.Code) with:`n$($run.Lines -join "`n")"
  }
  Write-Host 'assetcheck ends on a hierarchy that loops.'
} finally {
  Remove-Item -LiteralPath $root -Recurse -Force
}
exit 0
