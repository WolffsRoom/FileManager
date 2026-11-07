$ErrorActionPreference = 'Stop'
$preview = Get-Content -Raw "$PSScriptRoot\..\src\preview_menu.cpp"
$music = Get-Content -Raw "$PSScriptRoot\..\src\music_menu.cpp"
$dialog = Get-Content -Raw "$PSScriptRoot\..\src\dialog.cpp"
$rco = Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml"

foreach ($id in 'plane_thumb_left_page_preview','plane_thumb_current_page_preview','plane_thumb_right_page_preview','button_up_page_preview','button_down_page_preview','button_arrow_left_page_preview','button_arrow_right_page_preview') {
  if ($rco -notmatch $id) { throw "missing preview control: $id" }
}
if ($preview -notmatch 'SetViewMode') { throw 'preview view-mode state machine missing' }
if ($rco -notmatch 'button_music_indicator_page_browser') { throw 'persistent minimized-music indicator missing' }
if ($music -notmatch 'RestoreMinimized') { throw 'minimized player cannot be restored' }
foreach ($id in 'glyph_prev','glyph_play','glyph_pause','glyph_next') {
  if ($rco -notmatch $id) { throw "missing graphical transport glyph: $id" }
}
if ($dialog -notmatch 'REPEAT_ROUND') { throw 'VPK bubble does not have a looping animation' }

'media UI feature checks passed'
