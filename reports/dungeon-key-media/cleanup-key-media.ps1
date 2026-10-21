$ErrorActionPreference='Stop'
$workspace=(Resolve-Path -LiteralPath 'D:\GameDev\3DS\mother-encore-3ds').Path
$evidence=Join-Path $workspace 'reports\dungeon-key-media'
$manifest=Get-Content -LiteralPath (Join-Path $evidence 'native-project-manifests.json') -Raw | ConvertFrom-Json -AsHashtable
if (Get-Process -Name 'Godot*' -ErrorAction SilentlyContinue) { throw 'Original engine is still active' }
$checked=@()
foreach($entry in $manifest.GetEnumerator()) {
 $target=(Resolve-Path -LiteralPath (Join-Path $workspace $entry.Key)).Path
 if(-not $target.StartsWith(($workspace+'\build\full-port\key-media-native-'),[StringComparison]::OrdinalIgnoreCase) -or -not $target.EndsWith('\project')) { throw 'Unexpected native project target' }
 $root=Get-Item -LiteralPath $target -Force
 if($root.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Project root is a reparse point' }
 if((Get-Content -LiteralPath (Join-Path $target '.owned-key-media-reference') -Raw) -ne 'source-key-media-reference-v1' -or $entry.Value.marker -ne 'source-key-media-reference-v1') { throw 'Ownership marker differs' }
 $children=@(Get-ChildItem -LiteralPath $target -Recurse -Force)
 if(@($children | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Project contains a reparse point' }
 $files=@($children | Where-Object { -not $_.PSIsContainer })
 if($files.Count -ne $entry.Value.files.Count) { throw 'Project file set differs' }
 $bytes=0L
 foreach($file in $files) {
  $key=$file.FullName.Substring($target.Length+1).Replace('\','/')
  $expected=$entry.Value.files[$key]
  if(-not $expected -or $file.Length -ne $expected.bytes -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected.sha256) { throw ('Project file changed: '+$key) }
  $bytes+=$file.Length
 }
 $checked+=@{path=$target;files=$files.Count;bytes=$bytes;marker_verified=$true;sha256_verified=$true;reparse_free=$true}
}
foreach($row in $checked) { Remove-Item -LiteralPath $row.path -Recurse -Force; if(Test-Path -LiteralPath $row.path) { throw 'Project retirement failed' } }
$proof=@{scope='Only seven owned completed source-reference project directories; evidence/SDK/upstream/DSP/games/protected assets retained';projects=$checked;bytes=($checked | Measure-Object -Property bytes -Sum).Sum;utc=[DateTime]::UtcNow.ToString('o')}
$proof | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $evidence 'native-project-retirement.json') -Encoding utf8
$proof | ConvertTo-Json -Depth 3
