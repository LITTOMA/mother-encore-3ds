$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path -LiteralPath 'D:/GameDev/3DS/mother-encore-3ds').Path
$privateRoot=Join-Path $taskRoot 'build/full-port'
$proof=Get-Content -Raw -LiteralPath (Join-Path $privateRoot 'key-renderer-host-5.proof.json') | ConvertFrom-Json
if($proof.exit -ne 0 -or !$proof.unchanged){throw 'Current two-mode renderer check did not pass'}
function Check-OwnedPath([string]$path){
 $absolute=[IO.Path]::GetFullPath($path)
 if(!$absolute.StartsWith($privateRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup outside task build directory'}
 $cursor=Get-Item -LiteralPath $absolute
 if(!$cursor.PSIsContainer){if($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Cleanup target is a reparse point'};$cursor=$cursor.Directory}
 while($cursor.FullName -ne $taskRoot){if($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Cleanup crosses reparse point'};$cursor=$cursor.Parent;if(!$cursor){throw 'Cleanup ancestry escaped'}}
 return $absolute
}
$retired=@();$empty=@()
foreach($number in 1..4){
 $path=Join-Path $privateRoot ('key-renderer-host-'+$number+'/check')
 $absolute=Check-OwnedPath $path
 $script=Get-Content -Raw -LiteralPath (Join-Path $privateRoot ('key-renderer-host-'+$number+'.sh'))
 if(!$script.Contains('-o build/full-port/key-renderer-host-'+$number+'/check')){throw 'Historical compiler does not own target'}
 $item=Get-Item -LiteralPath $absolute
 $retired+=@{path=$absolute;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash.ToLower();kind='superseded owned host test executable'}
}
foreach($name in @('key-renderer-fixture-1/scratch','key-renderer-fixture-1/scratch-3','key-renderer-fixture-1/scratch-4')){
 $absolute=Check-OwnedPath (Join-Path $privateRoot $name)
 $items=@(Get-ChildItem -LiteralPath $absolute -Recurse -Force)
 if(@($items | Where-Object {!$_.PSIsContainer}).Count){throw 'Failed scratch still contains files;preserve for diagnosis'}
 foreach($item in $items){$null=Check-OwnedPath $item.FullName}
 $empty+=@{path=$absolute;directories=@($items | Sort-Object {$_.FullName.Length} -Descending | ForEach-Object {$_.FullName})}
}
# Only individually named executables and verified empty directories; no
# recursive deletion and no traversal through the historical upstream junction.
foreach($file in $retired){Remove-Item -LiteralPath $file.path}
foreach($entry in $empty){foreach($directory in $entry.directories){Remove-Item -LiteralPath $directory};Remove-Item -LiteralPath $entry.path}
$receipt=@{verified_source=5;removed_executables=$retired;removed_bytes=($retired | Measure-Object bytes -Sum).Sum;removed_empty_scratch=$empty;latest_host_executable_retained=(Test-Path -LiteralPath (Join-Path $privateRoot 'key-renderer-host-5/check'));raw_failure_logs_retained=$true}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $privateRoot 'key-renderer-cleanup-1.json') -Encoding utf8
$receipt | Select-Object removed_bytes,latest_host_executable_retained,raw_failure_logs_retained | ConvertTo-Json
