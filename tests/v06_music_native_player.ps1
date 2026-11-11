$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$rco = [xml](Get-Content -Raw "$root\resource\file_manager_plugin.xml")
$music = $rco.resource.pagetable.page | Where-Object id -eq 'page_music'
$cpp = Get-Content -Raw "$root\src\music_menu.cpp"
$core = Get-Content -Raw "$root\src\core\music.c"

$slider = $music.progressbar_touch | Where-Object id -eq 'progressbar_seek_page_music'
if (-not $slider) { throw 'music seek must use a native touch progress bar' }
if (-not ($music.corner_button | Where-Object id -eq 'button_options_page_music')) {
    throw 'music actions must be available from the bottom-right options button'
}
if ($music.button | Where-Object id -in @('button_export_page_music', 'button_background_page_music', 'button_native_music_page_music')) {
    throw 'music actions must not remain as separate body buttons'
}
if ($cpp -match 'sceTouchPeek') { throw 'music seek must not poll raw touch coordinates' }
if ($cpp -notmatch 'CB_PROGRESSBAR_CHANGED') { throw 'native seek changes are not handled' }
if ($cpp -notmatch 'OptionMenu::Create' -or $cpp -notmatch 'msg_music_export' -or
    $cpp -notmatch 'msg_music_background' -or $cpp -notmatch 'msg_music_open_native') {
    throw 'music options menu does not expose all three actions'
}

# Guard the actual unit conversion that fixes MP3/OGG seeking: seconds are
# clamped to the duration and converted to the decoder's raw byte offset.
if ($core -notmatch 'seconds\s*>\s*s_duration') { throw 'seek time is not clamped to duration' }
if ($core -notmatch 's_fileSize\s*\*\s*seconds\s*/\s*s_duration') {
    throw 'seek time is not converted to a raw file offset'
}

'v0.6 native music player checks passed'
