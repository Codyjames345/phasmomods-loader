# Tests tools/docs.ps1's choice of Doxygen with a stand-in that only says its version. Build machines carry a Doxygen of their own on
# the PATH, so -IfConfigured runs only the one PM_DOXYGEN names; without the switch the script is asked for the reference, and the PATH
# may supply it.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$script = Join-Path $root 'tools/docs.ps1'
$work = Join-Path ([IO.Path]::GetTempPath()) "pm-docs-test-$PID"
New-Item -ItemType Directory -Force -Path $work | Out-Null
$savedPath, $savedDoxygen = $env:PATH, $env:PM_DOXYGEN

<# Runs the script in its own process and returns its exit code and output. #>
function Invoke-Docs([string[]]$Arguments) {
    $shell = (Get-Process -Id $PID).Path
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $shell -NoProfile -ExecutionPolicy Bypass -File $script @Arguments 2>&1 | ForEach-Object { "$_" }
        return @{ Code = $LASTEXITCODE; Output = ($output -join "`n") }
    } finally {
        $ErrorActionPreference = $previous
    }
}

try {
    $standIn = Join-Path $work 'doxygen.cmd'
    Set-Content -LiteralPath $standIn -Value "@echo 1.0.0"
    $env:PATH = "$work;$savedPath"
    $env:PM_DOXYGEN = $null

    $run = Invoke-Docs @('-IfConfigured')
    if ($run.Code -ne 0) { throw "-IfConfigured failed on a Doxygen that only sits on the PATH:`n$($run.Output)" }
    if ($run.Output -match 'needs Doxygen') { throw "-IfConfigured judged a Doxygen that only sits on the PATH:`n$($run.Output)" }
    Write-Host '-IfConfigured leaves a Doxygen on the PATH alone.'

    $run = Invoke-Docs @()
    if ($run.Code -eq 0 -or $run.Output -notmatch 'needs Doxygen') { throw "A wrong version on the PATH passed when the reference was asked for:`n$($run.Output)" }
    Write-Host 'Asked for the reference, a wrong version on the PATH fails.'

    $env:PATH = $savedPath
    $env:PM_DOXYGEN = $standIn
    $run = Invoke-Docs @('-IfConfigured')
    if ($run.Code -eq 0 -or $run.Output -notmatch 'needs Doxygen') { throw "A wrong version named by PM_DOXYGEN passed:`n$($run.Output)" }
    Write-Host 'A wrong version PM_DOXYGEN names fails, -IfConfigured or not.'
} finally {
    $env:PATH = $savedPath
    $env:PM_DOXYGEN = $savedDoxygen
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Docs tests passed.'
