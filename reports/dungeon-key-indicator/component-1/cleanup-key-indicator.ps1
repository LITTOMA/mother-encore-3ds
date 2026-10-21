$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path -LiteralPath 'D:/GameDev/3DS/mother-encore-3ds').Path
$privateRoot=Join-Path $taskRoot 'build/full-port'
$reports=Join-Path $taskRoot 'reports/dungeon-key-indicator'
$proof=Get-Content -Raw -LiteralPath (Join-Path $privateRoot 'key-indicator-host-3.proof.json') | ConvertFrom-Json
if($proof.exit -ne 0 -or !$proof.unchanged){throw 'Current HUD checks did not pass'}
if(@(Get-Process -Name 'Godot*' -ErrorAction SilentlyContinue).Count){throw 'Godot still running;do not retire projects'}
function Check-Path([string]$path){
 $absolute=[IO.Path]::GetFullPath($path)
 if(!$absolute.StartsWith($privateRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Target outside task build directory'}
 $cursor=Get-Item -LiteralPath $absolute
 if(!$cursor.PSIsContainer){if($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Target file reparse'};$cursor=$cursor.Directory}
 while($cursor.FullName -ne $taskRoot){if($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Target ancestry reparse'};$cursor=$cursor.Parent;if(!$cursor){throw 'Target ancestry escaped'}}
 return $absolute
}
$projects=@();$executables=@()
foreach($number in 2..15){
 $parent=Join-Path $privateRoot ('key-indicator-native-'+$number)
 $target=Check-Path (Join-Path $parent 'project')
 if((Split-Path (Split-Path $target -Parent) -Leaf) -ne ('key-indicator-native-'+$number)){throw 'Project identity differs'}
 if((Get-Content -Raw -LiteralPath (Join-Path $target '.owned-key-indicator-reference')) -ne 'source-key-indicator-reference-v1'){throw 'Project ownership marker differs'}
 $manifest=Get-Content -Raw -LiteralPath (Join-Path $reports ('native-'+$number+'/owned-project-files.json')) | ConvertFrom-Json
 $files=@(Get-ChildItem -LiteralPath $target -File -Recurse -Force);$bytes=0
 if($files.Count -ne @($manifest.PSObject.Properties).Count){throw 'Project file set changed'}
 foreach($item in Get-ChildItem -LiteralPath $target -Recurse -Force){$null=Check-Path $item.FullName}
 foreach($file in $files){$relative=$file.FullName.Substring($target.Length+1).Replace('\','/');$expected=$manifest.PSObject.Properties[$relative];if(!$expected -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower() -ne $expected.Value){throw 'Project file identity changed'};$bytes+=$file.Length}
 $projects+=@{path=$target;files=$files.Count;bytes=$bytes;manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $reports ('native-'+$number+'/owned-project-files.json')) -Algorithm SHA256).Hash.ToLower()}
}
foreach($number in 1..2){
 $path=Check-Path (Join-Path $privateRoot ('key-indicator-host-'+$number+'/check'))
 $script=Get-Content -Raw -LiteralPath (Join-Path $privateRoot ('key-indicator-host-'+$number+'.sh'))
 if(!$script.Contains('-o build/full-port/key-indicator-host-'+$number+'/check')){throw 'Compiler does not own old executable'}
 $item=Get-Item -LiteralPath $path;$executables+=@{path=$path;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower()}
}
# Every absolute target, ownership marker, file digest and non-reparse ancestry
# has passed before any deletion. Readonly upstream is never enumerated here.
foreach($project in $projects){Remove-Item -LiteralPath $project.path -Recurse}
foreach($file in $executables){Remove-Item -LiteralPath $file.path}
$receipt=@{projects=$projects;superseded_executables=$executables;removed_bytes=(($projects+$executables)|Measure-Object bytes -Sum).Sum;latest_executable_retained=(Test-Path -LiteralPath (Join-Path $privateRoot 'key-indicator-host-3/check'));scope='Only SHA/marker-verified owned reference projects and two old host binaries;all outer logs and receipts retained'}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $privateRoot 'key-indicator-cleanup-1.json') -Encoding utf8
$receipt | Select-Object removed_bytes,latest_executable_retained | ConvertTo-Json
