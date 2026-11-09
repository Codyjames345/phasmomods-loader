# The release's files, for tools/stage.ps1, package.ps1 and their tests. Dot-source this file.

<# Every file of the release under the project at $Root: tools/payload.psd1's, then each checked-build
   record the offline check has written, each as @{ Name; Source }. #>
function Get-PayloadFiles([string]$Root) {
    $data = Import-PowerShellDataFile -LiteralPath (Join-Path $Root 'tools/payload.psd1')
    $records = @(Get-ChildItem -LiteralPath (Join-Path $Root $data.Records.Source) -Filter '*.txt' -File -ErrorAction SilentlyContinue | Sort-Object Name)
    @($data.Payload) + @($records | ForEach-Object { @{ Name = "$($data.Records.Name)/$($_.Name)"; Source = "$($data.Records.Source)/$($_.Name)" } })
}

<# Throw unless every file of $Payload is under $Root and each one with a Sha256 (a pinned file: one the public
   repository cannot hold, such as the Squatter's bundle) has exactly that content. A mismatch is a rebuilt file whose pin
   was not updated, or a fetched one that is not the pinned build. #>
function Assert-PayloadSources([string]$Root, $Payload) {
    foreach ($file in $Payload) {
        $source = Join-Path $Root $file.Source
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "$($file.Source) is missing." }
        if (-not $file.Sha256) { continue }
        $actual = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        if ($actual -ne $file.Sha256) {
            throw ("$($file.Source) is not the build tools/payload.psd1 pins (SHA-256 $($file.Sha256), this one $actual). " +
                'A rebuilt file is published to the private assets repository and pinned with tools/publish_assets.ps1.')
        }
    }
}
