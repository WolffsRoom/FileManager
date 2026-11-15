$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$rco = Get-Content (Join-Path $root 'resource/file_manager_plugin.xml') -Raw
$browser = Get-Content (Join-Path $root 'src/browser_menu.cpp') -Raw
$header = Get-Content (Join-Path $root 'src/info_menu.h') -Raw
$info = Get-Content (Join-Path $root 'src/info_menu.cpp') -Raw
$pt = Get-Content (Join-Path $root 'resource/locale/pt-br.xml') -Raw

if ($rco -notmatch 'ui/vpk_icon\.png.+id="tex_ui_vpk_package"') {
    throw 'vpk_icon.png is not registered with its own RCO texture'
}
if ($browser -notmatch 'strcasecmp\(ext, "\.zip"\).+tex_ui_archive') {
    throw '.zip does not use archive_icon.png'
}
if ($browser -notmatch 'case fs::Kind_Package:\s*return tex_ui_vpk;') {
    throw 'non-ZIP archives do not use lg_vpk.png'
}
if ($browser -notmatch 'case fs::Kind_Vpk:\s*return tex_ui_vpk_package;') {
    throw '.vpk does not use vpk_icon.png'
}
if ($header -notmatch 'Mode_Archive' -or $info -notmatch 'mode == Mode_Archive') {
    throw 'archive information mode is missing'
}
if ($browser -notmatch 'InfoMenu::Mode_Archive' -or $browser -notmatch 'OnArchiveAnswer') {
    throw 'touching an archive does not open an extract-here modal'
}
if ($pt -notmatch 'id="msg_extract_here"') {
    throw 'Extract here localization is missing'
}
if (-not (Test-Path (Join-Path $root 'resource/ui/vpk_icon.png'))) {
    throw 'resource/ui/vpk_icon.png is missing'
}

Write-Output 'v0.9 archive modal and icon mapping passed'
