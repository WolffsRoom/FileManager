$ErrorActionPreference = 'Stop'

$rco = [xml](Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml")
$source = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"

$ftp = $rco.resource.pagetable.page[0].plane | Where-Object id -eq 'plane_ftp_indicator_page_browser'
$music = $rco.resource.pagetable.page[0].button | Where-Object id -eq 'button_music_indicator_page_browser'
$alpha = $rco.resource.pagetable.page[0].button | Where-Object id -eq 'button_alpha_page_browser'

if (-not $ftp -or $ftp.layout_hint.pos -ne '356, -215') { throw 'FTP indicator is not immediately left of Options' }
if (-not $music -or $music.layout_hint.pos -ne '356, -165') { throw 'music indicator is not above the FTP indicator' }
if (-not $alpha.plane -or $alpha.plane.texture -ne 'tex_new_search') { throw 'index toggle does not render search.png' }
if ($source -match 'SetString\(ToWide\(paf::string\("Superior"\)\)\)') { throw 'legacy Sup/Superior label remains' }
if ($source -notmatch 'ftp::IsRunning\(\).*m_ftpIndicator') { throw 'FTP visibility is not bound to runtime state' }
if ($source -notmatch 'MusicMenu::HasMinimized\(\).*m_musicIndicator') { throw 'music visibility is not bound to minimized playback' }

'browser indicator checks passed'
