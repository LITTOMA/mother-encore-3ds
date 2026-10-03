param(
    [string]$Image = 'devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5',
    [switch]$BootstrapTools,
    [switch]$MapAssets
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $projectRoot
try {
    # Inspect locally; never pull or upgrade the verified image implicitly.
    $imageInfo = & docker image inspect $Image --format '{{json .}}'
    if ($LASTEXITCODE -ne 0) { throw 'Local Docker image unavailable.' }
    $imageInfo | Set-Content -Encoding utf8 reports/local-docker-image.json
    if ($BootstrapTools) {
        & docker run --rm --pull never -v "${projectRoot}:/work" -w /work --entrypoint bash $Image -lc 'set -e; python3 tools/bootstrap_cia_tools.py --from-source --bannertool-sha256 e4259c08fe8944ebadd5f4b96f9a8603e5427338074cfc46323fbbc3410d51ed'
        if ($LASTEXITCODE -ne 0) { throw 'Official tool bootstrap failed.' }
    }
    if (-not (Test-Path tools/bin/source-build-manifest.json)) {
        throw 'Run again with -BootstrapTools to build the fixed official packaging tools.'
    }
    if ($MapAssets) {
        & python tools/map_asset.py prepare
        if ($LASTEXITCODE -ne 0) { throw 'Original map texture preparation failed; upstream pin and Pillow are required.' }
        & docker run --rm --pull never --network none -v "${projectRoot}:/work" -w /work --entrypoint bash $Image -lc 'set -e; make map-assets'
        if ($LASTEXITCODE -ne 0) { throw 'Original map texture compilation failed.' }
    }
    & docker run --rm --pull never --network none -v "${projectRoot}:/work" -w /work --entrypoint bash $Image tools/verify_m0.sh
    if ($LASTEXITCODE -ne 0) { throw 'M0 verification failed; inspect the saved reports.' }
} finally {
    Pop-Location
}
