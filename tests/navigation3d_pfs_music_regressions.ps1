$ErrorActionPreference = 'Stop'
$root=Split-Path -Parent $PSScriptRoot
$cfg=Get-Content -Raw "$root/src/settings.cpp"
$header=Get-Content -Raw "$root/src/settings.h"
$menu=Get-Content -Raw "$root/src/settings_menu.cpp"
$browser=Get-Content -Raw "$root/src/browser_menu.cpp"
$bheader=Get-Content -Raw "$root/src/browser_menu.h"
$pfs=Get-Content -Raw "$root/src/core/pfs.c"
$legacy=Get-Content -Raw "$root/legacy/pfs.c"
if ($cfg -notmatch 'navigation_3d=%d' -or $cfg -notmatch 'navigation3D = v' -or
    $cfg -notmatch 'settings::Config s_config = \{ 0, settings::View_List, 0, 0, -1, 1, 1, 0, 1, 70, 1, 1, 0 \};') {
    throw '3D navigation must persist and default enabled'
}
if ($menu -notmatch 'Level_Navigation' -or $menu -notmatch 'Main_Navigation' -or
    $menu -notmatch 'c.navigation3D = row') {
    throw 'Settings must expose both navigation modes'
}
if ($browser -notmatch 'current->list->Show\(common::transition::Type_3D_SlideToBack1\)' -or
    $browser -notmatch 'Type_3D_SlideFromFront, true, false' -or
    $browser -match 'm_transitionFrames|retireFrames|RestoreListGeometry') {
    throw 'Original alpha ScePaf transition pair is not restored'
}
if ($browser -notmatch 'Type_FadeinFast') {
    throw 'Simple navigation setting must retain lightweight fade'
}
if ($browser -notmatch 'm_optionsButton->SetKeycode\(inputdevice::pad::Data::PAD_START\)' -or
    $browser -notmatch 'startButton->SetKeycode\(inputdevice::pad::Data::PAD_MENU\)') {
    throw 'Expected Start = options and Triangle = search'
}
if ($menu -notmatch 'msg_btn_triangle, msg_cmd_settings' -or
    $menu -notmatch 'msg_btn_start, msg_cmd_options') {
    throw 'Settings command help does not match the new mapping'
}
$startLoad=$browser.IndexOf('void BrowserMenu::LoadPage(')
$commitLoad=$browser.IndexOf('void BrowserMenu::CommitLoad(')
$endLoad=$browser.IndexOf('intrusive_ptr<graph::Surface> BrowserMenu::Icon(')
if ($startLoad -lt 0 -or $commitLoad -lt $startLoad -or $endLoad -lt $commitLoad) {
    throw 'Browser load/commit split missing'
}
$worker=$browser.Substring($startLoad,$commitLoad-$startLoad)
$commit=$browser.Substring($commitLoad,$endLoad-$commitLoad)
if ($worker -notmatch 'DeferCall\(' -or $worker -match '\bInsertCell\(' -or
    $worker -match '\bDeleteCell\(' -or $worker -match 'TemplateOpen\(') {
    throw 'Music folder ListView still mutated on worker thread'
}
if ($commit -notmatch 'InsertCell\(' -or $commit -notmatch 'page->loaded = true') {
    throw 'PAF UI thread does not own the ListView refresh'
}
if ($browser -notmatch 'void BrowserMenu::RequestItemDecide\(' -or
    $browser -notmatch 'void BrowserMenu::ApplyItemDecide\(' -or
    $browser -notmatch 'void BrowserMenu::ApplySelectButton\(') {
    throw 'File open and Select still mutate PAF during button callbacks'
}
if ($pfs -notmatch 'static const int knownIds\[\] = \{ 0x6E, 0x12E, 0x12F, 0x3ED \}' -or
    $pfs -notmatch 'sceAppMgrGameDataMount' -or $pfs -notmatch 'pfsVerifyMount' -or
    $pfs -notmatch 'pfs.log' -or $pfs -notmatch 'sceAppMgrUmount') {
    throw 'VitaShell PFS IDs, verified fallback and diagnostic missing'
}
if ($browser -notmatch 'if \(forceDecrypt && !supportedPfsRoot\)' -or
    $browser -notmatch 'mountResult < 0 \? mountResult' -or
    $browser -notmatch 'pfsIsProtectedContainer\(page->logicalPath.c_str\(\)\)') {
    throw 'Open decrypted still silently falls back to undecrypted folder'
}
foreach($name in @('en','pt-br','es','fr','de')) {
    $xml=Get-Content -Raw "$root/resource/locale/$name.xml"
    foreach($key in @('msg_set_navigation','msg_navigation_native','msg_navigation_simple')){
        if (-not $xml.Contains('id="'+$key+'"')){throw "Missing $key in $name"}
    }
}
'3D, Select/Start, PFS, and Music main-thread regression checks passed'
