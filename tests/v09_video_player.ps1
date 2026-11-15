$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'src/video_player.h'
$sourcePath = Join-Path $root 'src/video_player.cpp'
if (-not (Test-Path $headerPath) -or -not (Test-Path $sourcePath)) {
    throw 'VideoPlayer module is missing'
}
$header = Get-Content $headerPath -Raw
$source = Get-Content $sourcePath -Raw
$preview = Get-Content (Join-Path $root 'src/preview_menu.cpp') -Raw
$cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw

foreach ($api in @('Open', 'Close', 'Update', 'PauseResume', 'Seek', 'SeekTo',
                    'CurrentTime', 'Duration', 'Frame', 'HasFirstFrame')) {
    if ($header -notmatch "\b$api\s*\(") { throw "VideoPlayer::$api is missing" }
}
if ($source -notmatch 'bits\s*!=\s*0.+0xFF000000u.+0x80000000u') {
    throw 'opaque AvPlayer handles are not validated bitwise'
}
if ($source -notmatch 'basePriority\s*=\s*0xA0' -or
    $source -notmatch 'numOutputVideoFrameBuffers\s*=.+5') {
    throw 'proven AvPlayer priority/five-buffer configuration is missing'
}
if ($source -notmatch 'sceAvPlayerGetVideoDataEx' -or
    $source -notmatch 'ImageMode_RGBA8888') {
    throw 'RGBA WebMAF frame path is missing'
}
if ($source -notmatch 'sceAvPlayerGetAudioData' -or
    $source -notmatch 'video_diagnostic\.txt') {
    throw 'audio output or diagnostic log is missing'
}
if ($preview -notmatch 'm_videoPlayer\.Open' -or $preview -notmatch 'm_videoPlayer\.Update') {
    throw 'PreviewMenu is not using VideoPlayer'
}
if ($cmake -notmatch 'src/video_player\.cpp') { throw 'VideoPlayer is not compiled' }

Write-Output 'v0.9 native RGBA MP4 player passed'
