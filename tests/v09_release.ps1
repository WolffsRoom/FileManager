$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$cmake = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
$download = Get-Content -LiteralPath (Join-Path $root 'src/core/download.c') -Raw

if ($cmake -notmatch 'set\(VITA_VERSION\s+"00\.90"\)') {
    throw 'VPK metadata is not version 00.90'
}
if ($download -notmatch 'FileManager/0\.9 libhttp/1\.1') {
    throw 'Download user agent is not version 0.9'
}

foreach ($locale in @('pt-br.xml', 'en.xml', 'es.xml', 'fr.xml', 'de.xml')) {
    $text = Get-Content -LiteralPath (Join-Path $root "resource/locale/$locale") -Raw
    if ($text -notmatch 'File Manager 0\.9') {
        throw "$locale does not identify File Manager 0.9"
    }
}

$checks = @(
    'v09_qr_download.ps1',
    'v09_vpk_assets.ps1',
    'v09_archive_modal.ps1',
    'v09_video_player.ps1',
    'icon_resource_coverage.ps1',
    'navigation_stability.ps1',
    'selection_toolbar_native_20261108.ps1',
    'video_3d_navigation_20261108.ps1'
)
foreach ($check in $checks) {
    & (Join-Path $PSScriptRoot $check)
}

Write-Host 'v0.9 integrated release checks passed'
