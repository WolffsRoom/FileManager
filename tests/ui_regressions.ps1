$ErrorActionPreference = 'Stop'

$option = Get-Content -Raw "$PSScriptRoot\..\src\option_menu.cpp"
$browser = (Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp") +
    (Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.h")
$settings = Get-Content -Raw "$PSScriptRoot\..\src\settings_menu.cpp"
$rco = Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml"

if ($option -match 'OptionMenu::OnSizeAdjust') { throw 'legacy async option-menu callback still exists' }
if ($browser -match 'param\.list_item->Show\(mainMenu.*SlideFromBottom') { throw 'recycled main-menu cells are still animated individually' }
if ($browser -match 'AnimateRows\(param.list_view\)' -or
    $browser -notmatch 'param\.list_item->Show\(paf::common::transition::Type_Popup1\)' -or
    $browser -notmatch 'paf::common::transition::Type_Reset') {
    throw 'all browser rows should use v0.2 Popup1 and reset safely when recycled'
}
if ($browser -match 'StartListItem\(') { throw 'post-v0.2 custom cell animation path is still active' }
if ($settings -match 'GetDrawObj\(ui::ImageButton::OBJ_IMAGE\)') { throw 'theme tint still touches the internal image draw object' }
if ($rco -notmatch 'plane_theme_preview') { throw 'safe theme preview plane is missing' }

'ui regression checks passed'
