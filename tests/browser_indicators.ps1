$ErrorActionPreference = 'Stop'

$rco = [xml](Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml")
$source = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"

$ftp = $rco.resource.pagetable.page[0].plane | Where-Object id -eq 'plane_ftp_indicator_page_browser'
$music = $rco.resource.pagetable.page[0].button | Where-Object id -eq 'button_music_indicator_page_browser'
$alpha = $rco.resource.pagetable.page[0].button | Where-Object id -eq 'button_alpha_page_browser'

if (-not $ftp -or $ftp.layout_hint.pos -ne '-429, 208') { throw 'FTP indicator not at upper left' }
if (-not $music -or $music.layout_hint.pos -ne '-375, 208') { throw 'music indicator not at upper left' }
if (-not $alpha.plane -or $alpha.plane.texture -ne 'tex_new_search') { throw 'index toggle does not render search.png' }
if ($source -match 'SetString\(ToWide\(paf::string\("Superior"\)\)\)') { throw 'legacy Sup/Superior label remains' }
if ($source -notmatch 'ftp::IsRunning\(\)' -or $source -notmatch 'm_ftpShown') { throw 'FTP indicator lacks state cache' }
if ($source -notmatch 'MusicMenu::HasMinimized\(\)' -or $source -notmatch 'm_musicShown') { throw 'Music indicator lacks state cache' }

'browser indicator checks passed'
