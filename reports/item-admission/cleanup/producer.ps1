$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath 'D:\GameDev\3DS\mother-encore-3ds').Path
$taskBuild = Join-Path $taskRoot 'build\full-port'
$taskFinal = Get-Content -LiteralPath (Join-Path $taskBuild 'item-admission-host-4\completion.json') -Raw | ConvertFrom-Json
if ($taskFinal.returncode -ne 0 -or -not $taskFinal.inputs_unchanged) { throw 'Current checked test proof required' }
$taskKeep = Get-Content -LiteralPath (Join-Path $taskBuild 'item-admission-host-3\reuse.json') -Raw | ConvertFrom-Json
foreach ($taskEntry in $taskKeep.reused_exact_files.PSObject.Properties) {
    $taskKeptFile = Join-Path $taskRoot $taskEntry.Name
    if ((Get-FileHash -LiteralPath $taskKeptFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskEntry.Value) { throw 'A required current input changed' }
}
$taskNames = @('item_consumption','field_menus','catacombs_item_session','native_session','item_details','catacombs_item_details','catacombs_storage','storage_runtime')
$taskRelative = @($taskNames | ForEach-Object { 'item-admission-host-1\' + $_ }) + @('item-admission-host-2\field_menus','item-admission-host-2\native_session')
$taskPlan = @()
foreach ($taskName in $taskRelative) {
    $taskPath = (Resolve-Path -LiteralPath (Join-Path $taskBuild $taskName)).Path
    if (-not $taskPath.StartsWith($taskBuild + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Target escapes checked build root' }
    $taskFile = Get-Item -LiteralPath $taskPath
    if ($taskFile.PSIsContainer -or ($taskFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Only explicit ordinary generated files may be retired' }
    $taskStream = [IO.File]::OpenRead($taskPath)
    try { $taskMagic = New-Object byte[] 4; if ($taskStream.Read($taskMagic,0,4) -ne 4 -or [BitConverter]::ToString($taskMagic) -ne '7F-45-4C-46') { throw 'Expected a generated ELF executable' } }
    finally { $taskStream.Dispose() }
    $taskPlan += [pscustomobject]@{path=$taskPath;bytes=$taskFile.Length;sha256=(Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$taskRecord = Join-Path $taskBuild 'item-admission-obsolete-retirement.json'
if (Test-Path -LiteralPath $taskRecord) { throw 'Retirement proof already exists' }
@{scope='Ten superseded test ELFs only; all current objects, six reused executables, raw logs and source proofs retained';files=$taskPlan;retired=$false} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskRecord -Encoding utf8
foreach ($taskEntry in $taskPlan) { Remove-Item -LiteralPath $taskEntry.path }
$taskBytes = ($taskPlan | Measure-Object -Property bytes -Sum).Sum
@{scope='Ten superseded test ELFs only; all current objects, six reused executables, raw logs and source proofs retained';files=$taskPlan;retired=$true;bytes_freed=$taskBytes} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskRecord -Encoding utf8
Write-Output ('Retired ' + $taskPlan.Count + ' obsolete test ELFs; freed ' + $taskBytes + ' bytes')
