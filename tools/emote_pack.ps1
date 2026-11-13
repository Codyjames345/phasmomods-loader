# Runs the PhasmoMods emote tools (sdk/unity/com.phasmomods.emote-tools) inside a Unity project.
# -FindLoops cuts each emote's song and clip to their measured loops and rewrites the pack folder's
# emotes.json; -Build measures the loudness files and builds the pack's asset bundle into the pack
# folder. Each step runs only when asked, FindLoops first, each in its own batch-mode Unity run.
# The project must have the emote tools package installed; the pack folder holds emotes.json and
# the loudness files and receives the bundle.
# usage: powershell -File tools\emote_pack.ps1 -Unity <Unity.exe> -Project <Unity project folder> `
#          -Pack <emote pack folder> [-FindLoops] [-Build]
param(
    [Parameter(Mandatory = $true)][string]$Unity,
    [Parameter(Mandatory = $true)][string]$Project,
    [Parameter(Mandatory = $true)][string]$Pack,
    [switch]$FindLoops,
    [switch]$Build,
    [int]$TimeoutMinutes = 30
)

$ErrorActionPreference = 'Stop'

if (-not $FindLoops -and -not $Build) {
    throw 'give at least one of -FindLoops and -Build'
}

$Pack = (Resolve-Path -LiteralPath $Pack).ProviderPath

function Invoke-EmoteStep {
    param([string]$Name)
    $log = Join-Path ([IO.Path]::GetTempPath()) ("emote_pack_" + $Name + "_" +
        [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss') + ".log")
    # Waits for Unity itself only: Start-Process -Wait also waits for the helpers Unity leaves running (its licensing
    # client), so it never returned.
    $process = Start-Process -FilePath $Unity -NoNewWindow -PassThru -ArgumentList @(
        '-batchmode', '-nographics',
        '-projectPath', "`"$Project`"",
        '-logFile', "`"$log`"",
        '-executeMethod', ('PhasmoMods.EmoteTools.EmotePackTool.' + $Name + 'Batch'),
        '-pack', "`"$Pack`"")
    $null = $process.Handle  # held now, or ExitCode reads empty once Unity has gone
    if (-not $process.WaitForExit($TimeoutMinutes * 60 * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "Unity step $Name ran past $TimeoutMinutes minutes and was stopped; the log is at $log"
    }
    $lines = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log } else { @() }
    $lines | Where-Object { $_ -match '^(EMLOOP|EM |EMFAIL)' } | ForEach-Object { Write-Host $_ }
    if ($process.ExitCode -ne 0) {
        # A script that does not compile never reaches the tool, so show the compiler's errors too.
        $lines | Where-Object { $_ -match 'error CS\d+' } | Select-Object -Unique | ForEach-Object { Write-Host $_ }
        throw "Unity step $Name exited with code $($process.ExitCode); the log is at $log"
    }
    Remove-Item -LiteralPath $log -ErrorAction SilentlyContinue
}

if ($FindLoops) { Invoke-EmoteStep 'FindLoops' }
if ($Build) { Invoke-EmoteStep 'Build' }
