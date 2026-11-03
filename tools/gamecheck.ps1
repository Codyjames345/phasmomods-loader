# The offline game check: copies the four files Cpp2IL needs out of the game folder, dumps them with
# Cpp2IL, and runs build\gamecheck.exe on the dump and the copied GameAssembly.dll, and on a copy of
# UnityPlayer.dll for the native sites. It writes the dump
# to re\dump, the map's addresses to re\map-addresses.tsv and the RVA name table to re\names.tsv, all
# gitignored. Then it checks the scene facts (tools/assetcheck.py) in copies of the scene and asset
# files they need, with UnityPy in a venv it makes in re\dump\venv on first run, and prints that
# check's lines after gamecheck's. It never runs game code and never writes in the game folder.
# With -Mods <dir> it also loads each mod DLL under <dir> and reports the mods the host would not start on
# this build. -IfConfigured exits quietly when the game or Cpp2IL is not set; nothing here needs Python
# until the check is configured.
# When every check passes it writes this build's checked-build record into sdk\builds, for the release to
# ship (core/build_record.h): the two library hashes and the map's digest, and nothing else. Commit it. A build on an
# engine or metadata version the host's layouts were not written for fails the check and gets no record
# (core/engine_version.h).
# With -Expect it also checks every re\dump\expect\*.txt (lines of "<key> <hex>"): the values the SDK
# migration replaced, which must still resolve to the same numbers (gitignored, never committed).
# usage: tools\gamecheck.ps1 [-GameDir <dir>] [-Cpp2Il <exe>] [-Mods <dir>] [-Expect] [-IfConfigured]
param([string]$GameDir = $env:PM_GAME_DIR, [string]$Cpp2Il = $env:PM_CPP2IL, [string]$Mods, [switch]$Expect, [switch]$IfConfigured)
$ErrorActionPreference = 'Stop'

if (-not $GameDir -or -not $Cpp2Il) {
  Write-Host 'Offline game check: not configured (set PM_GAME_DIR and PM_CPP2IL).'
  if ($IfConfigured) { exit 0 }
  exit 2
}

$root = Split-Path -Parent $PSScriptRoot
$dump = Join-Path $root 're\dump'
$game = Join-Path $dump 'game'
$cs = Join-Path $dump 'cs'
$record = Join-Path $dump 'build.txt'
$check = Join-Path $root 'build\gamecheck.exe'
$addresses = Join-Path $root 're\map-addresses.tsv'
$sceneEntries = Join-Path $dump 'scene-entries.tsv'
# The last build's addresses go first, so no failure below leaves them for the RE scripts.
Remove-Item -LiteralPath $addresses -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $addresses) { throw "Cannot remove $addresses." }
Remove-Item -LiteralPath $sceneEntries -Force -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $check)) { throw "Build first: $check is missing." }

$files = @('GameAssembly.dll', 'Phasmophobia.exe', 'Phasmophobia_Data\globalgamemanagers',
  'Phasmophobia_Data\il2cpp_data\Metadata\global-metadata.dat')
foreach ($file in $files) {
  $from = Join-Path $GameDir $file
  if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { throw "The game folder has no $file." }
  $to = Join-Path $game $file
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $to) | Out-Null
  Copy-Item -LiteralPath $from -Destination $to -Force
}

# UnityPlayer.dll, for the native sites. It is large, so a copy whose size and time match the game's
# is kept.
$unityFrom = Get-Item -LiteralPath (Join-Path $GameDir 'UnityPlayer.dll') -ErrorAction SilentlyContinue
if (-not $unityFrom) { throw 'The game folder has no UnityPlayer.dll.' }
$unityPlayer = Join-Path $game 'UnityPlayer.dll'
$unityTo = Get-Item -LiteralPath $unityPlayer -ErrorAction SilentlyContinue
if (-not ($unityTo -and $unityTo.Length -eq $unityFrom.Length -and $unityTo.LastWriteTimeUtc -eq $unityFrom.LastWriteTimeUtc)) {
  Copy-Item -LiteralPath $unityFrom.FullName -Destination $unityPlayer -Force
}

# The dump depends only on the code and the metadata, so their two hashes say whether it is current.
$hashes = @('GameAssembly.dll', 'Phasmophobia_Data\il2cpp_data\Metadata\global-metadata.dat') |
  ForEach-Object { '{0} {1}' -f (Get-FileHash -LiteralPath (Join-Path $game $_) -Algorithm SHA256).Hash, $_ }
$current = (Test-Path -LiteralPath $record) -and
  ((Get-Content -LiteralPath $record) -join "`n") -eq ($hashes -join "`n")
if (-not $current) {
  Remove-Item -LiteralPath $record -Force -ErrorAction SilentlyContinue
  if (Test-Path -LiteralPath $cs) { Remove-Item -LiteralPath $cs -Recurse -Force }
  Write-Host 'Dumping the game with Cpp2IL...'
  & $Cpp2Il --game-path $game --exe-name Phasmophobia --use-processor attributeinjector `
    --output-as diffable-cs --output-to $cs | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "Cpp2IL failed ($LASTEXITCODE)." }
  Set-Content -LiteralPath $record -Value $hashes -Encoding ascii
}

$diffable = Join-Path $cs 'DiffableCs'
$checkArgs = @('--dump', $diffable, '--image', (Join-Path $game 'GameAssembly.dll'), '--unity-player', $unityPlayer,
  '--addresses', $addresses, '--scene-entries', $sceneEntries, '--game-data', (Join-Path $game 'Phasmophobia_Data'))
if ($Mods) { $checkArgs += @('--mods', (Resolve-Path -LiteralPath $Mods).Path) }
if ($Expect) {
  $expectFiles = @(Get-ChildItem -LiteralPath (Join-Path $dump 'expect') -Filter '*.txt' -File -ErrorAction SilentlyContinue | Sort-Object Name)
  if ($expectFiles.Count -eq 0) {
    Write-Host 'No expectation files: re\dump\expect has no *.txt.'
    exit 2
  }
  foreach ($file in $expectFiles) { $checkArgs += @('--expect', $file.FullName) }
}
& $check @checkArgs
$code = $LASTEXITCODE
& node (Join-Path $root 're\names.mjs') $diffable
if ($LASTEXITCODE -ne 0) { throw 'node re\names.mjs failed.' }
if ($code -eq 2) { exit 2 }  # gamecheck could not read its input, so it wrote no scene entries

# The scene facts. The venv is made once, and made afresh when the pinned requirements change, so
# nothing a previous set installed stays in it. Every package is pinned with its hashes and installed
# from wheels only (tools/assetcheck-requirements.txt).
$venv = Join-Path $dump 'venv'
$python = Join-Path $venv 'Scripts\python.exe'
$requirements = Join-Path $PSScriptRoot 'assetcheck-requirements.txt'
$installed = Join-Path $venv 'assetcheck-requirements.txt'
$fresh = (Test-Path -LiteralPath $python) -and (Test-Path -LiteralPath $installed) -and
  ((Get-FileHash -LiteralPath $installed).Hash -eq (Get-FileHash -LiteralPath $requirements).Hash)
if (-not $fresh) {
  if (Test-Path -LiteralPath $venv) { Remove-Item -LiteralPath $venv -Recurse -Force }
  Write-Host 'Making the Python venv for the scene check...'
  # uv's Python 3.14 first, the version the requirements are compiled for: the py launcher's -3
  # finds only python.org (PythonCore) installs, not the ones uv registers.
  $uvPython = $null
  if (Get-Command uv -ErrorAction SilentlyContinue) {
    $ErrorActionPreference = 'Continue'
    $uvPython = & uv python find 3.14 2>$null
    if ($LASTEXITCODE -ne 0) { $uvPython = $null }
    $ErrorActionPreference = 'Stop'
  }
  if ($uvPython) { & $uvPython -I -m venv $venv }
  elseif (Get-Command py -ErrorAction SilentlyContinue) { & py -3 -I -m venv $venv }
  elseif (Get-Command python -ErrorAction SilentlyContinue) { & python -I -m venv $venv }
  else { throw 'The scene check (tools/assetcheck.py) needs Python 3; none is installed.' }
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $python)) { throw "Cannot make the venv $venv." }
  & $python -I -m pip install --quiet --disable-pip-version-check --require-hashes --only-binary=:all: -r $requirements
  if ($LASTEXITCODE -ne 0) { throw 'Cannot install the scene check''s requirements.' }
  Copy-Item -LiteralPath $requirements -Destination $installed -Force
}

# The scene and asset files it reads, copied until it names no file that is not there yet: a file's
# references are known only once it is copied. A copy whose size and time match the game's is kept,
# since some of these files are large.
$assetcheck = Join-Path $PSScriptRoot 'assetcheck.py'
$data = Join-Path $game 'Phasmophobia_Data'
for ($round = 0; ; ++$round) {
  $needed = @(& $python -I $assetcheck --data $data --entries $sceneEntries --needs)
  if ($LASTEXITCODE -ne 0) { throw "assetcheck could not list the files it reads ($LASTEXITCODE)." }
  $copied = 0
  foreach ($name in $needed) {
    if ($name -notmatch '^[A-Za-z0-9_.]+$' -or $name.StartsWith('.')) { throw "assetcheck named an unexpected file: $name" }
    $from = Get-Item -LiteralPath (Join-Path $GameDir "Phasmophobia_Data\$name") -ErrorAction SilentlyContinue
    if (-not $from) { throw "The game folder has no Phasmophobia_Data\$name." }
    $to = Get-Item -LiteralPath (Join-Path $data $name) -ErrorAction SilentlyContinue
    if ($to -and $to.Length -eq $from.Length -and $to.LastWriteTimeUtc -eq $from.LastWriteTimeUtc) { continue }
    Copy-Item -LiteralPath $from.FullName -Destination (Join-Path $data $name) -Force
    ++$copied
  }
  if ($copied -eq 0) { break }
  if ($round -ge 5) { throw 'assetcheck kept naming new files to copy.' }
}
& $python -I $assetcheck --data $data --entries $sceneEntries
$sceneCode = $LASTEXITCODE
if ($sceneCode -eq 2) { exit 2 }
$result = [Math]::Max($code, $sceneCode)
if ($result -eq 0) {
  & $check --write-record (Join-Path $root 'sdk\builds') --image (Join-Path $game 'GameAssembly.dll') --unity-player $unityPlayer --game-data $data
  if ($LASTEXITCODE -ne 0) { throw 'gamecheck could not write the checked-build record.' }
}
exit $result
