$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$cpp = Get-Content -Raw "$root\src\preview_menu.cpp"
$header = Get-Content -Raw "$root\src\preview_menu.h"
$rco = [xml](Get-Content -Raw "$root\resource\file_manager_plugin.xml")
$preview = $rco.resource.pagetable.page | Where-Object id -eq 'page_preview'

if ($cpp -match 'ConvertHalf') { throw 'MP4 preview still performs lossy half-resolution CPU conversion' }
if ($header -match 'm_rgba') { throw 'MP4 preview still owns an intermediate RGBA conversion buffer' }
if ($cpp -notmatch 'ImageMode_YUV420P3') { throw 'MP4 frames are not exposed to PAF as native YUV surfaces' }
if ($cpp -notmatch 'frame\.pData' -or $header -notmatch 'VideoSurfaceEntry') {
    throw 'MP4 preview does not retain zero-copy surfaces for AvPlayer frame buffers'
}
if ($cpp -notmatch 'sceAvPlayerEnableStream') { throw 'MP4 preview does not explicitly enable audio/video streams' }

$slider = $preview.SelectSingleNode(".//progressbar_touch[@id='progressbar_video_page_preview']")
if (-not $slider) { throw 'MP4 preview lacks a native touch seek control' }
if ($cpp -notmatch 'CB_PROGRESSBAR_CHANGED' -or $cpp -notmatch 'OnVideoSeek') {
    throw 'MP4 preview does not commit native slider seeks'
}

'v0.6 MP4 preview checks passed'
