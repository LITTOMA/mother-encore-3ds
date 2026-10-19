$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath 'D:\GameDev\3DS\mother-encore-3ds').Path
$binaryRoot = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'build/full-port')).Path
$names = @('story-object-delete-tests','field-objects-data-tests','field-objects-lifecycle-tests','story-key-grant-tests','story-object-production-tutorial-tests','story-object-production-session-tests','story-object-production-restore-tests','story-object-production-field-story-tests')
$records = @()
foreach ($name in $names) {
    $resolved = (Resolve-Path -LiteralPath (Join-Path $binaryRoot $name)).Path
    if (-not $resolved.StartsWith($binaryRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Target outside owned build folder' }
    $entry = Get-Item -LiteralPath $resolved
    if ($entry.PSIsContainer) { throw 'Expected a single owned test binary' }
    $records += [pscustomobject]@{ path=$resolved; bytes=$entry.Length; sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $resolved).Hash.ToLowerInvariant() }
}
$proof = [pscustomobject]@{ scope='Completed owned host test ELFs only; successful source/test logs retained; actual accepted f029 game, Record save, tool, core cache and protected preexisting dirs kept'; files=$records; retired_bytes=($records | Measure-Object -Property bytes -Sum).Sum }
$proof | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $binaryRoot 'story-object-deferred-cleanup.json') -Encoding utf8
foreach ($record in $records) {
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $record.path).Hash.ToLowerInvariant() -ne $record.sha256) { throw 'Test binary changed before retirement' }
    Remove-Item -LiteralPath $record.path
    if (Test-Path -LiteralPath $record.path) { throw 'Owned file still present' }
}
Write-Output ('Retired {0} completed owned test bytes; 8 exact ELF paths removed' -f $proof.retired_bytes)
