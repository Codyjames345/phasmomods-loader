# The release's files, for tools/stage.ps1, package.ps1 and their tests. Dot-source this file.

<# Every file of the release under the project at $Root: tools/payload.psd1's, then each checked-build
   record the offline check has written, each as @{ Name; Source }. #>
function Get-PayloadFiles([string]$Root) {
    $data = Import-PowerShellDataFile -LiteralPath (Join-Path $Root 'tools/payload.psd1')
    $records = @(Get-ChildItem -LiteralPath (Join-Path $Root $data.Records.Source) -Filter '*.txt' -File -ErrorAction SilentlyContinue | Sort-Object Name)
    @($data.Payload) + @($records | ForEach-Object { @{ Name = "$($data.Records.Name)/$($_.Name)"; Source = "$($data.Records.Source)/$($_.Name)" } })
}
