# Publish each pinned file of the release (a tools/payload.psd1 entry with a Sha256: one the public repository cannot
# hold, such as the Squatter's bundle) to the private assets repository, then pin it to this build.
#
# The private repository keeps every published build of a pinned file, by its content, at pinned/<SHA-256>/<file name>,
# and never replaces one, so each branch's pin names exactly the build it was made with and CI fetches that one
# (tools/place_assets.ps1). The file is pushed before the pin changes, so a pin never names a build that is not there.
#
# -Private is a local clone of the private assets repository (default: the PM_PRIVATE_ASSETS environment variable).
# Commit the changed tools/payload.psd1 with the code that needs the new build.
param([string]$Private = $env:PM_PRIVATE_ASSETS, [string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
if (-not $Private -or -not (Test-Path -LiteralPath (Join-Path $Private '.git'))) {
    throw 'Give -Private, or set PM_PRIVATE_ASSETS, to a local clone of the private assets repository.'
}
$payloadFile = Join-Path $Root 'tools/payload.psd1'
$pinned = @((Import-PowerShellDataFile -LiteralPath $payloadFile).Payload | Where-Object { $_.ContainsKey('Sha256') })
if (-not $pinned.Count) { throw 'tools/payload.psd1 pins no file.' }

<# Run git in the private clone, throwing when it fails. A native command's success is its exit code: git writes
   warnings to stderr even when it succeeds (Windows line endings on a plain add), and Windows PowerShell 5.1 turns a
   stderr line under Stop into a terminating error, so run git under Continue and judge only the exit code. #>
function Invoke-Private([string[]]$Arguments) {
    $ErrorActionPreference = 'Continue'
    & git -C $Private @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in the private assets repository." }
}

if (@(& git -C $Private status --porcelain).Count) { throw 'The private assets clone has uncommitted changes; commit or discard them first.' }
Invoke-Private @('pull', '--ff-only', '--quiet')

$changes = @()
foreach ($file in $pinned) {
    $source = Join-Path $Root $file.Source
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "$($file.Source) is missing: build it first." }
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $relative = "pinned/$hash/$(Split-Path $file.Source -Leaf)"
    $target = Join-Path $Private $relative
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) {
        New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
        Copy-Item -LiteralPath $source -Destination $target
        Invoke-Private @('add', '--', $relative)
    }
    if ($hash -ne $file.Sha256) { $changes += @{ Source = $file.Source; Hash = $hash } }
}
if (@(& git -C $Private diff --cached --name-only).Count) {
    Invoke-Private @('commit', '--quiet', '-m', "Publish $(($pinned | ForEach-Object { Split-Path $_.Source -Leaf }) -join ', ') for the public build")
    Invoke-Private @('push', '--quiet')
}

# Only now, with the build pushed, does the pin move to it.
$text = [IO.File]::ReadAllText($payloadFile)
foreach ($change in $changes) {
    $pattern = "(Source = '$([regex]::Escape($change.Source))'; Sha256 = ')[0-9A-Fa-f]*(')"
    if (-not [regex]::IsMatch($text, $pattern)) { throw "Cannot find the pin of $($change.Source) in tools/payload.psd1." }
    $text = [regex]::Replace($text, $pattern, "`${1}$($change.Hash)`${2}")
    Write-Output "Pinned $($change.Source) to $($change.Hash)."
}
if ($changes.Count) { [IO.File]::WriteAllText($payloadFile, $text) } else { Write-Output 'Every pinned file is already published and pinned.' }
