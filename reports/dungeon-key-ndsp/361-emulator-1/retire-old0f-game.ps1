$ErrorActionPreference='Stop'
$workspace=(Resolve-Path -LiteralPath 'D:\GameDev\3DS\mother-encore-3ds').Path
$old=(Resolve-Path -LiteralPath (Join-Path $workspace 'build\full-port\wall0f-current\verified-artifact\3ds\encore-native\encore-native.3dsx')).Path
$fresh=(Resolve-Path -LiteralPath (Join-Path $workspace 'build\full-port\key361-current\verified-artifact\3ds\encore-native\encore-native.3dsx')).Path
foreach($target in $old,$fresh) {
 if(-not $target.StartsWith(($workspace+'\build\full-port\'),[StringComparison]::OrdinalIgnoreCase)) { throw 'Game target escaped task build directory' }
 $entry=Get-Item -LiteralPath $target -Force
 while($entry.FullName -ne $workspace) { if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Game target traverses a reparse point' }; if($entry -is [IO.FileInfo]) { $entry=$entry.Directory } else { $entry=$entry.Parent } }
}
$proof=Get-Content -LiteralPath (Join-Path $workspace 'reports\dungeon-key-ndsp\361-emulator-1\verification.json') -Raw | ConvertFrom-Json
if(-not $proof.semantic_equal -or -not $proof.temporary_profiles_removed -or $proof.repetitions -ne 2 -or $proof.checkpoints_each -ne 90) { throw 'Replacement emulator evidence incomplete' }
$oldReceipt=Get-Content -LiteralPath (Join-Path $workspace 'build\full-port\wall0f-current\verified-artifact\build-receipt.json') -Raw | ConvertFrom-Json
if($oldReceipt.source_commit -ne '0f203b24694b127e4643c4e27e719bd65601ea22') { throw 'Old game ownership/source mismatch' }
$newHash=(Get-FileHash -LiteralPath $fresh -Algorithm SHA256).Hash.ToLowerInvariant()
if($newHash -ne 'd78b4dbe78c9a424eb1b96ea829bdf382573c46b1945f88a40e7b4388d2fb83b' -or (Get-Item -LiteralPath $fresh).Length -ne 264975208) { throw 'Replacement game differs' }
# Exclusive read refuses an old game still in use; never enumerate/log CDB tokens.
$stream=[IO.File]::Open($old,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try { $bytes=$stream.Length; $algorithm=[Security.Cryptography.SHA256]::Create(); try { $oldHash=[Convert]::ToHexString($algorithm.ComputeHash($stream)).ToLowerInvariant() } finally { $algorithm.Dispose() } } finally { $stream.Dispose() }
if($bytes -ne 264975024 -or $oldHash -ne '723537dcc1d69289ff7e59e8459847f62a20cb16e260e999b5a98eecbdf3c508' -or $oldReceipt.artifacts.'encore-native.3dsx'.sha256 -ne $oldHash) { throw 'Old game bytes differ from retained source receipt' }
$out=Join-Path $workspace 'reports\dungeon-key-ndsp\361-emulator-1'
Copy-Item -LiteralPath (Join-Path $workspace 'build\full-port\wall0f-current\verified-artifact\build-receipt.json') -Destination (Join-Path $out 'retired-source0f-receipt.json')
Remove-Item -LiteralPath $old
if(Test-Path -LiteralPath $old) { throw 'Old game retirement failed' }
@{scope='Only superseded task-owned0f game file;old notices/receipt/raw test logs,61ea/efda baselines,SDK,DSP and protected assets retained';old=@{path=$old;bytes=$bytes;sha256=$oldHash;exclusive_read_verified=$true};replacement=@{path=$fresh;sha256=$newHash;repetitions=2;checkpoints_each=90};removed_bytes=$bytes;utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'superseded-game-retirement.json') -Encoding utf8
Write-Output ('Retired superseded game bytes: '+$bytes)
