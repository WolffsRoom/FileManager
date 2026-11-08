$ErrorActionPreference = 'Stop'
$root=Split-Path -Parent $PSScriptRoot
$browser=Get-Content -Raw "$root/src/browser_menu.cpp"
$commitStart=$browser.IndexOf('void BrowserMenu::CommitLoad(')
$commitEnd=$browser.IndexOf('intrusive_ptr<graph::Surface> BrowserMenu::Icon(', $commitStart)
if ($commitStart -lt 0 -or $commitEnd -le $commitStart) {throw 'Missing load commit'}
$commit=$browser.Substring($commitStart,$commitEnd-$commitStart)
if ($commit -match 'Show\(native3D' -or $commit -match 'Show\(common::transition::Type_3D_SlideFromFront') {
    throw 'Delayed folder load can run an unbounded half-screen 3D foreground transition'
}
if ($commit -notmatch 'RestoreListGeometry\(page\)' -or
    $commit -notmatch 'page->list->SetTransitionComplete\(true\)' -or
    $commit -notmatch 'page->list->Show\(common::transition::Type_FadeinFast\)') {
    throw 'The incoming page must always be full screen with a safe foreground fade'
}
if ($browser -notmatch 'previous->list->Show\(common::transition::Type_3D_SlideToBack1\)') {
    throw 'The immediate parent must retain the native 3D background effect'
}
if ($browser -match 'Type_3D_SlideFromFront, true, false') {
    throw 'DoReverse must not auto-delete widgets still owned by the retained-pages stack'
}
if ($browser -notmatch 'Type_3D_SlideFromFront, false, false') {
    throw '3D return animation missing with non-owning cleanup'
}
if ($browser -notmatch 'hasFolderBackground = native3D' -or
    $browser -notmatch '!previous->path.empty\(\)') {
    throw 'Root/device list cannot appear as a second layer behind first folder'
}
'Video 3D navigation regressions passed'
