$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$musicH = Get-Content (Join-Path $root 'src/core/music.h') -Raw
$musicC = Get-Content (Join-Path $root 'src/core/music.c') -Raw
$videoH = Get-Content (Join-Path $root 'src/video_player.h') -Raw
$videoC = Get-Content (Join-Path $root 'src/video_player.cpp') -Raw
$preview = Get-Content (Join-Path $root 'src/preview_menu.cpp') -Raw

foreach ($api in @('musicIsOpen','musicIsPaused','musicPause','musicResume')) {
    if ($musicH -notmatch "\b$api\s*\(") { throw "explicit music API missing: $api" }
    if ($musicC -notmatch "\b$api\s*\(") { throw "explicit music implementation missing: $api" }
}
if ($preview -notmatch 'musicIsPlaying\(\)[\s\S]+musicPause\(\)' -or
    $preview -notmatch 'm_musicWasPlaying[\s\S]+musicResume\(\)') {
    throw 'video preview does not own and conditionally restore background music'
}
if ($videoH -notmatch 'SetSpeed\s*\(' -or $videoH -notmatch 'Speed\s*\(' -or
    $videoC -notmatch 'sceAvPlayerSetTrickSpeed' -or
    $preview -notmatch '100.+150.+200') {
    throw 'video speed 1x/1.5x/2x cycle is missing'
}
if ($preview -notmatch 'exportMediaFile\(.+1') {
    throw 'video export action is missing'
}
Write-Output 'v0.9 video audio ownership/options contract passed'
