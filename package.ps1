# Validate and publish a portable native release without modifying the game installation.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& (Join-Path $root 'validate.ps1')

$payload = [ordered]@{}
foreach ($file in (Import-PowerShellDataFile -LiteralPath (Join-Path $root 'tools/payload.psd1')).Payload) {
    $payload[$file.Name] = $file.Source
}
foreach ($source in $payload.Values) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $source) -PathType Leaf)) {
        throw "The release is missing $source. Build and validate the project before packaging."
    }
}
$buildChecks = Get-Content -LiteralPath (Join-Path $root 'host/build_check.cpp') -Raw
$supportedHashes = [ordered]@{}
foreach ($module in @('GameAssembly', 'UnityPlayer')) {
    $hashes = [regex]::Matches($buildChecks, ('constexpr\s+char\s+k' + $module + 'Sha256\[\]\s*=\s*"([a-f0-9]{64})"\s*;'))
    if ($hashes.Count -ne 1) { throw "Cannot determine the supported $module binary hash from the native build checks." }
    $supportedHashes["$module.dll"] = $hashes[0].Groups[1].Value
}
$dist = Join-Path $root 'dist'

<# Write the release ZIP with forward-slash entry names, as the ZIP format specifies (Windows
   PowerShell's Compress-Archive stores nested paths with backslashes). #>
function New-ReleaseArchive {
    [CmdletBinding(SupportsShouldProcess)]
    param([string]$Folder, [string]$Archive)
    if (-not $PSCmdlet.ShouldProcess($Archive, 'Write release archive')) { return }
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $base = [IO.Path]::GetFullPath($Folder).TrimEnd('\') + '\'
    $zip = [IO.Compression.ZipFile]::Open($Archive, 'Create')
    try {
        foreach ($file in Get-ChildItem -LiteralPath $base -Recurse -File) {
            $entry = $file.FullName.Substring($base.Length).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $entry) | Out-Null
        }
    } finally {
        $zip.Dispose()
    }
}
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$stage = Join-Path $dist ('.package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$content = Join-Path $stage 'content'
New-Item -ItemType Directory -Path $content | Out-Null
try {
    foreach ($name in $payload.Keys) {
        $destination = Join-Path $content $name
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $payload[$name]) -Destination $destination
    }
    $artifacts = foreach ($name in $payload.Keys) {
        $file = Get-Item -LiteralPath (Join-Path $content $name)
        [ordered]@{ name = $name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() }
    }
    [ordered]@{
        formatVersion = 2
        runtime = 'Windows x64 native launcher'
        supportedBinarySha256 = $supportedHashes
        gameplayVerified = $false
        verificationLimits = 'A complete private multiplayer contract has not been verified.'
        artifacts = @($artifacts)
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $content 'manifest.json') -Encoding UTF8
    $archive = Join-Path $stage 'PhasmoMods.zip'
    New-ReleaseArchive $content $archive
    $release = Join-Path $dist 'PhasmoMods.zip'
    if (Test-Path -LiteralPath $release) {
        [IO.File]::Replace($archive, $release, (Join-Path $stage 'previous.zip'))
    } else {
        [IO.File]::Move($archive, $release)
    }
    Get-Item -LiteralPath $release | Select-Object Name, Length
} finally {
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    $distPrefix = [IO.Path]::GetFullPath($dist).TrimEnd('\') + '\'
    if (-not $resolvedStage.StartsWith($distPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to remove a staging directory outside the distribution directory.'
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
