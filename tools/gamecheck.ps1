# The offline game check: copies the four files Cpp2IL needs out of the game folder, dumps them with
# Cpp2IL, and runs build\gamecheck.exe on the dump and the copied GameAssembly.dll. It writes the dump
# to re\dump, the map's addresses to re\map-addresses.tsv and the RVA name table to re\names.tsv, all
# gitignored. It never runs game code and never writes in the game folder.
# With -Mods <dir> it also loads each mod DLL under <dir> and reports the mods the host would not start on
# this build. -IfConfigured exits quietly when the game or Cpp2IL is not set.
# usage: tools\gamecheck.ps1 [-GameDir <dir>] [-Cpp2Il <exe>] [-Mods <dir>] [-IfConfigured]
param([string]$GameDir = $env:PG_GAME_DIR, [string]$Cpp2Il = $env:PG_CPP2IL, [string]$Mods, [switch]$IfConfigured)
$ErrorActionPreference = 'Stop'

if (-not $GameDir -or -not $Cpp2Il) {
  Write-Host 'Offline game check: not configured (set PG_GAME_DIR and PG_CPP2IL).'
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
# The last build's addresses go first, so no failure below leaves them for the RE scripts.
Remove-Item -LiteralPath $addresses -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $addresses) { throw "Cannot remove $addresses." }
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
$checkArgs = @('--dump', $diffable, '--image', (Join-Path $game 'GameAssembly.dll'), '--addresses', $addresses)
if ($Mods) { $checkArgs += @('--mods', (Resolve-Path -LiteralPath $Mods).Path) }
& $check @checkArgs
$code = $LASTEXITCODE
& node (Join-Path $root 're\names.mjs') $diffable
if ($LASTEXITCODE -ne 0) { throw 'node re\names.mjs failed.' }
exit $code
