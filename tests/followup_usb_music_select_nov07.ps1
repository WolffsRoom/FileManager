$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw "$root/src/main.cpp"
$option = Get-Content -Raw "$root/src/option_menu.cpp"
$browser = Get-Content -Raw "$root/src/browser_menu.cpp"
$usb = Get-Content -Raw "$root/src/usb_menu.cpp"
[xml]$rco = Get-Content -Raw "$root/resource/file_manager_plugin.xml"
if ($main -notmatch 'MainThreadCallList::Register\(DrainDeferredUi, NULL\)' -or
    $main -notmatch 'sceKernelCreateMutex\("FM::UiDeferred"' -or
    $main -notmatch 'ready\[i\].fn\(ready\[i\].userdata\)') {
    throw 'Deferred UI jobs must run on the actual PAF main thread'
}
if ($main -match 'class DeferJob') { throw 'Legacy worker-thread deferred UI remains' }
if (([regex]::Matches($option, 'if \(!userdata \|\| !self\) return')).Count -ne 2) {
    throw 'OptionMenu callbacks still dereference null PAF user data'
}
if ($browser -match 'Type_3D_SlideToBack1' -or $browser -match 'RecreateList\(current\)') {
    throw 'Stale navigation path can overlay previous and current folders'
}
if ($browser -notmatch 'for \(uint32_t i = 0; i < m_pages.size\(\); \+\+i\)' -or
    $browser -notmatch 'm_pages.at\(i\)->list->Hide\(common::transition::Type_Reset\)') {
    throw 'PushPage does not hide every inactive folder list'
}
if ($browser -notmatch 'void BrowserMenu::ApplyMarkToggle\(int32_t index\)' -or
    $browser -notmatch 'void BrowserMenu::ApplySelectAll\(\)' -or
    $browser -notmatch 'm_markPending = true' -or
    $browser -notmatch 'm_selectAllPending = true') {
    throw 'Selecting via Square/long press still mutates live PAF widgets inside the event'
}
if ($usb -notmatch 'void UsbMenu::StartConnection\(\)' -or
    $usb -notmatch 'DeferCall\(\[\]\(void \*userdata\)' -or
    $usb -notmatch 'const int32_t result = usb::Stop\(\)') {
    throw 'USB should start/stop after its PAF button callback returns'
}
$styles = @($rco.resource.styletable.style_button | Where-Object id -eq 'style_button_power_native')
if ($styles.Count -ne 1 -or
    $styles[0].planeobj[0].texture0 -ne '_common_texture_option_menu_button_single' -or
    $styles[0].planeobj[1].texture0 -ne '_common_texture_option_menu_button_single_glow') {
    throw 'Power buttons do not use neutral Sony-native texture and interaction glow'
}
foreach ($buttonId in @('button_power_off_page_settings','button_power_restart_page_settings','button_power_sleep_page_settings')) {
    $node = @($rco.resource.pagetable.page | Where-Object id -eq 'page_settings')[0]
    $item = @($node.plane.button | Where-Object id -eq $buttonId)
    if ($item.Count -ne 1 -or $item[0].style -ne 'style_button_power_native') {
        throw "Power button $buttonId has wrong native style"
    }
}
'2026-11-07 second hardware follow-up checks passed'
