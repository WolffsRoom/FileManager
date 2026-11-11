$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
$browser = Get-Content (Join-Path $root 'src/browser_menu.cpp') -Raw
$preview = Get-Content (Join-Path $root 'src/preview_menu.cpp') -Raw
$gif = Get-Content (Join-Path $root 'src/core/gif.c') -Raw
$archive = Get-Content (Join-Path $root 'src/core/archive_ops.c') -Raw
$media = Get-Content (Join-Path $root 'src/core/media_export.c') -Raw
$install = Get-Content (Join-Path $root 'src/core/install.c') -Raw

if ($cmake -notmatch 'VITA_VERSION\s+"00\.80"') { throw 'v0.8 metadata is missing' }
if ($cmake -notmatch 'SceVideoExport_stub') { throw 'video export stub is not linked' }
if ($gif -notmatch 'stbi_load_gif_from_memory' -or $preview -notmatch 'OnGifFrame') { throw 'animated GIF preview is incomplete' }
if ($archive -notmatch 'archive_read_support_format_all' -or $archive -notmatch 'archive_write_set_format_zip') { throw 'archive read/write support is incomplete' }
if ($archive -notmatch 'zip:compression-level=1') { throw 'fast ZIP compression profile is missing' }
if ($media -notmatch 'sceMusicExportFromFile' -or $media -notmatch 'sceVideoExportFromFile') { throw 'native media export is incomplete' }
if ($install -notmatch 'installAppFolder' -or $install -notmatch 'refreshLiveAreaApp') { throw 'folder install/LiveArea refresh is incomplete' }
if ($browser -notmatch 'Action_Extract' -or $browser -notmatch 'Action_CompressZip' -or $browser -notmatch 'Action_ExportMedia') { throw 'v0.8 browser actions are not exposed' }
if ($browser -notmatch 'backgroundInstall') { throw 'folder installation is not dispatched in background mode' }

Write-Host 'v0.8 feature wiring passed'
