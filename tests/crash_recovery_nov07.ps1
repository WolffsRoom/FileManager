$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Source([string]$name) { Get-Content -Raw (Join-Path $root $name) }
function Check([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}
$browser = Source 'src/browser_menu.cpp'
$fs = Source 'src/core/fs.cpp'
$fsh = Source 'src/core/fs.h'
$preview = Source 'src/preview_menu.cpp'
$settings = Source 'src/settings_menu.cpp'
$optionMenu = Source 'src/option_menu.cpp'
$usb = Source 'src/usb.cpp'
$usbMenu = Source 'src/usb_menu.cpp'
[xml]$rco = Source 'resource/file_manager_plugin.xml'

# Two hardware dumps landed at OptionMenu::OptionMenu with R0=NULL.
Check ($optionMenu -match '::operator new\(sizeof\(OptionMenu\), std::nothrow\)') 'OptionMenu may construct with null this'
Check ($optionMenu -match 'DeferCall\(\[\]\(void \*userdata\)') 'OptionMenu closes during touch callback'
Check ($optionMenu -match 'GetChildrenNum\(\) <= before') 'OptionMenu does not guard template allocation'
Check ($optionMenu -match 'closeParam.fade = false') 'OptionMenu releases its page too late'
Check ($browser -match 'OptionMenu::Create') 'Browser bypasses checked option menu factory'

Check ($browser -match 'page->list->DestroyWidget\(\)') 'Closed folder retains its ListView'
Check ($browser -match 'current->list->Show\(common::transition::Type_Reset\)' -and $browser -match 'm_retiredPages.push_back\(outgoing\)' -and $browser -notmatch 'RecreateList\(current\)') 'Back must reverse the 3D transition and defer outgoing-widget deletion'
Check ($browser -match 'm_backPending = true') 'Back may destroy a ListView inside its own button handler'
Check ($browser -match 'fs::EntryList fresh') 'List refresh is not safely staged'
Check ($browser -match 'while \(owner && owner != page->list\)') 'Stale row callback can target another page'

Check ($fsh -match 'void RenamePath' -and $fsh -match 'void RemovePath') 'Shortcut API incomplete'
Check ($fs -match 'void shortcuts::RenamePath' -and $fs -match 'void shortcuts::RemovePath') 'Shortcut maintenance missing'
Check ($preview -match 'shortcuts::RenamePath' -and $preview -match 'shortcuts::RemovePath') 'Preview does not update favorites'
Check ($preview -match 'm_filesChanged && m_onChanged') 'Preview edits do not refresh browser'
Check ($browser -match 'shortcuts::RenamePath' -and $browser -match 'shortcuts::RemovePath') 'Browser edits do not update favorites'
Check ($settings -match 'QueueLevel\(Level_Shortcuts\)' -and $settings -match 'shortcuts::Remove') 'Unselected favorite remains in list'
Check ($settings -match 'void SettingsMenu::QueueLevel') 'Settings edits ListView during its own button handler'

$ftpIcon = @($rco.resource.pagetable.page[0].plane | Where-Object id -eq 'plane_ftp_indicator_page_browser')[0]
$musicIcon = @($rco.resource.pagetable.page[0].button | Where-Object id -eq 'button_music_indicator_page_browser')[0]
Check ($ftpIcon.layout_hint.pos -eq '-429, 208') 'FTP indicator wrong position'
Check ($musicIcon.layout_hint.pos -eq '-375, 208') 'Music indicator wrong position'
Check ($browser -match 'playing != m_musicShown' -and $browser -match 'ftpActive != m_ftpShown') 'Indicators not state-driven'
Check ($browser -notmatch 'Register\(AlphaIndexTask') 'Music index worker remains active'
Check ($browser -match 'm_alphaButton->Hide\(common::transition::Type_Reset\)') 'Music index still displayed'
Check ($browser -match 'strcasecmp\(ext, "\.bin"\) == 0\) return tex_ui_root') 'BIN icon not root'

$settingsPage = @($rco.resource.pagetable.page | Where-Object id -eq 'page_settings')[0]
foreach ($button in @('button_power_off_page_settings','button_power_restart_page_settings','button_power_sleep_page_settings')) {
    Check (@($settingsPage.plane.button | Where-Object id -eq $button).Count -ge 1) "Power button missing $button"
}
Check ($settings -match 'dialog::OpenYesNo' -and $settings -match 'scePowerRequestStandby\(' -and $settings -match 'scePowerRequestColdReset\(' -and $settings -match 'scePowerRequestSuspend\(') 'Power APIs or confirmations absent'
$empty = @($rco.resource.styletable.style_text | Where-Object id -eq 'style_text_shortcuts_empty')[0]
Check ($null -ne $empty -and $empty.textobj.word_wrap -eq '1') 'Empty message not wrapped'
foreach ($lang in @('pt-br','en','es','fr','de')) {
    $content = Source ('resource/locale/' + $lang + '.xml')
    foreach ($key in @('msg_power_off','msg_power_restart','msg_power_sleep','msg_power_confirm_off','msg_power_confirm_restart','msg_power_confirm_sleep','msg_usb_failed','msg_usb_ftp_conflict')) {
        Check ($content.Contains('id="' + $key + '"')) ('Missing ' + $key + ' in ' + $lang)
    }
    Check ($content -match '&#10;') ('Missing newline in ' + $lang + ' empty shortcut text')
}
foreach ($icon in @('turn-off.png','restart.png','standbye.png')) {
    Check (Test-Path (Join-Path $root ('resource/ui/' + $icon))) ('Missing power icon ' + $icon)
}
Check ($usbMenu -match 'm_startResult = usb::Toggle\(m_device\)' -and $usbMenu -match 'DeferCall\(') 'USB not started after modal creation'
Check ($browser -match 'UsbMenu::Open') 'Select not opening USB modal'
Check ($usb -match 'sceAppMgrDestroyOtherApp\(') 'USB not preparing mounts'
Check ($usb -match 'USB_ACTIVE' -and $usb -match 'FM_DATA_DIR "/usb.log"') 'USB phase logging missing'
Check ($usb -match 's_storageStarted' -and $usb -match 's_mtpStopped') 'USB partial cleanup tracking missing'
'v0.4 2026-11-07 crash recovery checks passed'
