$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $root 'src/video_player.cpp') -Raw
$cmake = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
$module = Join-Path $root 'module/libSceAvPlayerPSVitaRGBA8888.suprx'
$stub = Join-Path $root 'third_party/netstream/lib/libSceAvPlayerWebMAF_stub.a'

if (-not (Test-Path -LiteralPath $module)) {
    throw 'RGBA AvPlayer module is not packaged in the project'
}
if (-not (Test-Path -LiteralPath $stub)) {
    throw 'WebMAF import library is missing'
}
if ($cmake -notmatch 'libSceAvPlayerWebMAF_stub\.a' -or
    $cmake -notmatch 'libSceAvPlayerPSVitaRGBA8888\.suprx') {
    throw 'RGBA module or import stub is not part of the build/package'
}
if ($source -notmatch 'autoStart\s*=\s*SCE_FALSE' -or
    $source -notmatch 'eventReplacement\.eventCallback\s*=\s*PlayerEvent') {
    throw 'AvPlayer is not initialized through the READY event lifecycle'
}
if ($source -notmatch 'SCE_AVPLAYER_STATE_READY' -or
    $source -notmatch 'sceAvPlayerStart\(') {
    throw 'READY does not explicitly start playback'
}
if ($source -notmatch 'sceAvPlayerGetVideoDataEx' -or
    $source -notmatch 'ImageMode_RGBA8888' -or
    $source -notmatch 'details\.video\.pitch\s*\*\s*4') {
    throw 'Decoded RGBA frames are not wrapped with their real pitch'
}
if ($source -match 'ImageMode_YUV420P2') {
    throw 'Black-frame PAF YUV wrapper is still active'
}

Write-Output 'v0.9 RGBA video lifecycle contract passed'
