$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath 'D:\GameDev\3DS\mother-encore-3ds').Path
$buildRoot = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'build')).Path
$proofRoot = Join-Path $taskRoot 'reports\item-admission\ci-followups'
if (-not (Test-Path -LiteralPath (Join-Path $proofRoot 'source.json'))) { throw 'Retained evidence required before cleanup' }
$retired = @()
foreach ($name in @('audio','coin_scene_data','coin_scene','coin_scene_source')) {
    $target = (Resolve-Path -LiteralPath (Join-Path $buildRoot "full-port\item-admission-ci-followups-1\$name")).Path
    if (-not $target.StartsWith($buildRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Executable outside intended build root' }
    $item = Get-Item -LiteralPath $target
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 -or $item.PSIsContainer) { throw 'Only ordinary owned executable files may be retired' }
    $retired += [pscustomobject]@{path=$target;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()}
    Remove-Item -LiteralPath $target
}
foreach ($number in @(1,2)) {
    $target = (Resolve-Path -LiteralPath (Join-Path $buildRoot "full-port\coin-audio-original-$number\project")).Path
    if (-not $target.StartsWith($buildRoot + '\full-port\coin-audio-original-', [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Original temporary project outside intended build root' }
    if ((Get-Content -LiteralPath (Join-Path $target '.owned-coin-scene-reference') -Raw) -cne 'coin-scene-reference-v1') { throw 'Original temporary project ownership mismatch' }
    $items = @(Get-Item -LiteralPath $target) + @(Get-ChildItem -LiteralPath $target -Force -Recurse)
    if (@($items | Where-Object {($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0}).Count -ne 0) { throw 'Do not recursively remove reparse points' }
    $bytes = ($items | Where-Object {-not $_.PSIsContainer} | Measure-Object -Property Length -Sum).Sum
    $retired += [pscustomobject]@{path=$target;bytes=$bytes;scope='Copied original observer project after successful engine exit and frozen input/result verification; receipts, producer, raw observations and logs retained'}
    Remove-Item -LiteralPath $target -Recurse -Force
}
$record = [pscustomobject]@{retired=$retired;total_file_bytes=($retired|Measure-Object -Property bytes -Sum).Sum;baseline_archive_preserved=$true;tested_game_preserved=$true}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $proofRoot 'cleanup.json') -Encoding UTF8
$record | Select-Object total_file_bytes,baseline_archive_preserved,tested_game_preserved | ConvertTo-Json
