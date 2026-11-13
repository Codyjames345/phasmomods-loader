# Print the body of one CHANGELOG.md section: the lines between "## [<Version>]" and the next "## [" heading,
# with blank ends removed. "Unreleased" is a section like any other. A section that is empty prints nothing and
# succeeds; a changelog or a section that is not there fails with exit code 1, so a caller can tell the two apart.
# The workflow uses this for release notes: a release takes its own section, a development build takes Unreleased.
# Usage: tools\changelog_section.ps1 -Version <x.y.z or Unreleased> [-Path <file>]
param(
    [Parameter(Mandatory)][string]$Version,
    [string]$Path
)
$ErrorActionPreference = 'Stop'
if (-not $Path) { $Path = Join-Path (Split-Path $PSScriptRoot -Parent) 'CHANGELOG.md' }
if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    [Console]::Error.WriteLine("There is no changelog at $Path.")
    exit 1
}
$lines = @(Get-Content -LiteralPath $Path)
# A section heading is "## [<Version>]", optionally with " - YYYY-MM-DD" after it, as Keep a Changelog writes it.
$pattern = '^## \[' + [regex]::Escape($Version) + '\]( - \d{4}-\d{2}-\d{2})?\s*$'
$start = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match $pattern) { $start = $i; break }
}
if ($start -lt 0) {
    [Console]::Error.WriteLine("The changelog at $Path has no section `"## [$Version]`".")
    exit 1
}
$body = @()
for ($i = $start + 1; $i -lt $lines.Count; $i++) {
    if ($lines[$i].StartsWith('## [')) { break }
    $body += $lines[$i]
}
# Trim blank ends by index: a slice like $body[1..0] runs backwards and would never empty the array.
$first = 0
$last = $body.Count - 1
while ($first -le $last -and $body[$first].Trim() -eq '') { $first++ }
while ($last -ge $first -and $body[$last].Trim() -eq '') { $last-- }
for ($i = $first; $i -le $last; $i++) { Write-Output $body[$i] }
exit 0
