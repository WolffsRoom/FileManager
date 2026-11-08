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
if ($commit -notmatch 'if \(!settings::Get\(\).navigation3D\)[\s\S]*?Type_FadeinFast' -or
    $commit -match 'navigation3D[\s\S]{0,100}Type_Reset') {
    throw 'Loaded foreground must keep native reset state and only fade in simplified mode'
}
if ($browser -notmatch 'current->list->Show\(common::transition::Type_3D_SlideToBack1\)') {
    throw 'The immediate parent must retain the native 3D background effect'
}
if ($browser -notmatch 'Type_3D_SlideFromFront, true, false') {
    throw 'Alpha return transition must let ScePaf retire its ListView'
}
'Video 3D navigation regressions passed'
