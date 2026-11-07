$ErrorActionPreference = 'Stop'

$option = Get-Content -Raw "$PSScriptRoot\..\src\option_menu.cpp"
$browser = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$settings = Get-Content -Raw "$PSScriptRoot\..\src\settings_menu.cpp"
$rco = Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml"

if ($option -match 'OptionMenu::OnSizeAdjust') { throw 'legacy async option-menu callback still exists' }
if ($browser -match 'param\.list_item->Show\(mainMenu.*SlideFromBottom') { throw 'recycled main-menu cells are still animated individually' }
if (($browser | Select-String 'list->SetPos\(0\.0f, -272\.0f' -AllMatches).Matches.Count -lt 2) { throw 'page navigation does not restore the canonical list position' }
if ($settings -match 'GetDrawObj\(ui::ImageButton::OBJ_IMAGE\)') { throw 'theme tint still touches the internal image draw object' }
if ($rco -notmatch 'plane_theme_preview') { throw 'safe theme preview plane is missing' }

'ui regression checks passed'
