$ErrorActionPreference = 'Stop'

$cpp = Get-Content -Raw "$PSScriptRoot\..\src\music_menu.cpp"
$rco = [xml](Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml")
$music = $rco.resource.pagetable.page | Where-Object id -eq 'page_music'
$prev = $music.button | Where-Object id -eq 'button_prev_page_music'
$next = $music.button | Where-Object id -eq 'button_next_page_music'
$bar = $music.progressbar_touch | Where-Object id -eq 'progressbar_seek_page_music'

if ($prev.plane.texture -ne 'tex_preview_arrow_left') { throw 'previous control does not use the left arrow asset' }
if ($next.plane.texture -ne 'tex_preview_arrow_right') { throw 'next control does not use the right arrow asset' }
if (-not $bar) { throw 'music native seek control is missing' }
if ($cpp -notmatch 'GetTexture\(tex_ui_music\)') { throw 'lg_music fallback cover is missing' }
if ($cpp -notmatch 'UpdateSeekPreview' -or $cpp -notmatch 'musicSeek\(percent \* m_seconds\)') { throw 'drag-to-seek does not provide live feedback and commit' }

'music interaction checks passed'
