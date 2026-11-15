$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'src/core/vpk_assets.h'
$sourcePath = Join-Path $root 'src/core/vpk_assets.c'

if (-not (Test-Path $headerPath) -or -not (Test-Path $sourcePath)) {
    throw 'VPK asset validation module is missing'
}

$header = Get-Content -Raw $headerPath
$source = Get-Content -Raw $sourcePath
$install = Get-Content -Raw (Join-Path $root 'src/core/install.c')
$info = Get-Content -Raw (Join-Path $root 'src/info_menu.cpp')

if ($header -notmatch 'VpkAssetReport' -or $header -notmatch 'inspectVpkAssets' -or
    $header -notmatch 'normalizeExtractedVpkAssets') {
    throw 'VPK asset validation interface is incomplete'
}
if ($source -notmatch 'PNG_COLOR_TYPE_PALETTE' -or $source -notmatch 'png_set_PLTE' -or
    $source -notmatch 'png_set_tRNS') {
    throw 'LiveArea PNG conversion does not emit indexed PNG with transparency'
}
if ($source -notmatch 'sce_sys/icon0\.png' -or $source -notmatch 'sce_sys/livearea/contents/') {
    throw 'LiveArea asset enumeration is incomplete'
}
if ($source -notmatch '\.fm9tmp' -or $source -notmatch 'sceIoRename') {
    throw 'Corrected PNG assets are not written atomically'
}
if ($install -notmatch 'normalizeExtractedVpkAssets\(FM_PACKAGE_DIR') {
    throw 'VPK installation promotes assets before normalization'
}
if ($info -notmatch 'assets\.fixable' -or $info -notmatch 'assets\.invalid') {
    throw 'VPK install modal does not warn about fixable or invalid assets'
}

Write-Host 'v0.9 VPK LiveArea asset validation passed'
