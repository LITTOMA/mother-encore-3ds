$ErrorActionPreference = 'Stop'
$taskWorkspace = 'D:\GameDev\3DS\mother-encore-3ds'
$taskBase = [IO.Path]::GetFullPath((Join-Path $taskWorkspace 'build\full-port'))
$auditFile = Join-Path $taskBase 'shadow-cleanup-audit.json'
$audit = Get-Content -LiteralPath $auditFile -Raw | ConvertFrom-Json
if ($audit.owned_build -ne $taskBase) { throw 'Owned build root mismatch' }
$live = @(Get-CimInstance Win32_Process | Where-Object { $_.Name -like '*godot*' -or $_.Name -eq 'azahar.exe' })
if ($live.Count) { throw 'Native engine or emulator still active' }
if ((Get-FileHash -LiteralPath $audit.kept_archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $audit.kept_archive_sha256) { throw 'Current tested archive changed' }
function Check-Ancestry([string]$target) {
  $current = [IO.Path]::GetFullPath($target)
  if (-not $current.StartsWith($taskBase + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Target escaped owned build root' }
  while ($current.Length -ge $taskWorkspace.Length) {
    $item = Get-Item -LiteralPath $current -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse point in target ancestry' }
    if ($current -eq $taskWorkspace) { break }
    $current = [IO.Path]::GetDirectoryName($current)
  }
}
$total = [long]0
foreach ($target in $audit.targets) {
  $path = [IO.Path]::GetFullPath($target.path)
  Check-Ancestry $path
  if ($target.kind -eq 'native-project') {
    if ($path -notmatch '\\shadow-clock-native-(1|2|3|4|8|9|10|11|12|13|14)\\temporary$') { throw 'Unexpected native directory' }
    if ($target.marker -ne '.owned-shadow-clock-reference' -or $target.marker_value -ne 'shadow-clock-reference-v1') { throw 'Unexpected native owner specification' }
    $marker = Join-Path $path $target.marker
    if ([Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($marker)) -ne $target.marker_value) { throw 'Native owner mismatch' }
    $children = @(Get-ChildItem -LiteralPath $path -Recurse -Force)
    if (@($children | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Reparse point in native tree' }
    $files = @($children | Where-Object { -not $_.PSIsContainer })
    if ($files.Count -ne @($target.files).Count) { throw 'Native inventory count changed' }
    $measured = [long]0
    foreach ($record in $target.files) {
      $file = [IO.Path]::GetFullPath((Join-Path $path $record.relative))
      if (-not $file.StartsWith($path + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Native file escaped target' }
      $item = Get-Item -LiteralPath $file -Force
      if ($item.PSIsContainer -or $item.Length -ne $record.bytes -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $record.sha256) { throw 'Native inventory changed' }
      $measured += $item.Length
    }
    if ($measured -ne $target.bytes) { throw 'Native measured bytes changed' }
  } elseif ($target.kind -eq 'compiler-file') {
    $parent = [IO.Path]::GetDirectoryName($path)
    if ($parent -ne (Join-Path $taskBase 'shadow-core-host-1') -and $parent -ne (Join-Path $taskBase 'shadow-core-host-2')) { throw 'Compiler directory mismatch' }
    $name = [IO.Path]::GetFileName($path)
    if ($name -notmatch '^runtime_[a-z0-9_]+\.cpp\.o$' -and -not ($name -eq 'libencore-shadow.a' -and $parent -eq (Join-Path $taskBase 'shadow-core-host-1'))) { throw 'Compiler target name mismatch' }
    if ($target.marker -ne '.owned-shadow-core-host' -or $target.marker_value -ne 'shadow-core-host-v1' -or [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes((Join-Path $parent $target.marker))) -ne $target.marker_value) { throw 'Compiler owner mismatch' }
    $item = Get-Item -LiteralPath $path -Force
    if ($item.PSIsContainer -or $item.Length -ne $target.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $target.sha256) { throw 'Compiler member proof changed' }
  } else { throw 'Unknown cleanup kind' }
  $total += [long]$target.bytes
}
if ($total -ne $audit.total_bytes) { throw 'Cleanup total mismatch' }
foreach ($target in $audit.targets) {
  if ($target.kind -eq 'native-project') { Remove-Item -LiteralPath $target.path -Recurse -Force } else { Remove-Item -LiteralPath $target.path -Force }
  if (Test-Path -LiteralPath $target.path) { throw 'Removal incomplete' }
}
if ((Get-FileHash -LiteralPath $audit.kept_archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $audit.kept_archive_sha256) { throw 'Preserved archive changed' }
$completion = @{ removed_targets = @($audit.targets).Count; removed_bytes = $total; current_archive_preserved = $true; all_target_paths_verified = $true; all234_objects_equal_archive_members = $true; native_source_and_adapters_and_logs_retained = $true }
$completion | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskBase 'shadow-cleanup-completion.json') -Encoding utf8NoBOM
Copy-Item -LiteralPath (Join-Path $taskBase 'shadow-cleanup-completion.json') -Destination (Join-Path $taskWorkspace 'reports\catacombs-shadow-graphics\cleanup-completion.json')
$completion | ConvertTo-Json -Compress
