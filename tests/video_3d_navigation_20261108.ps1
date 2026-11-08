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
if ($commit -notmatch 'page->list->Show\(settings::Get\(\).navigation3D') {
    throw 'Loaded foreground does not select standard reset or simplified fade'
}
if ($browser -notmatch 'current->list->Show\(common::transition::Type_3D_SlideToBack1\)') {
    throw 'The immediate parent must retain the native 3D background effect'
}
if ($browser -notmatch 'Type_3D_SlideFromFront, true, false') {
    throw 'Alpha return transition must let ScePaf retire its ListView'
}
'Video 3D navigation regressions passed'
