$ErrorActionPreference = 'Stop'

$browser = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$header = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.h"

$pushStart = $browser.IndexOf('void BrowserMenu::PushPage')
$pushEnd = $browser.IndexOf('void BrowserMenu::FreePage', $pushStart)
$push = $browser.Substring($pushStart, $pushEnd - $pushStart)
$popStart = $browser.IndexOf('bool BrowserMenu::PopPage')
$popEnd = $browser.IndexOf('void BrowserMenu::Refresh', $popStart)
$pop = $browser.Substring($popStart, $popEnd - $popStart)

# FileManager-alpha.vpk (SHA-256 46272F...C419511) uses this native pair:
# parent Show(Type_3D_SlideToBack1), foreground reverse SlideFromFront.
if ($push -notmatch 'current->list->Show\(common::transition::Type_3D_SlideToBack1\)') {
    throw 'forward navigation does not use the alpha native parent transition'
}
if ($pop -notmatch 'DoReverse\(-100\.0f, outgoing->list,\s*common::transition::Type_3D_SlideFromFront, true, false\)') {
    throw 'back navigation does not use alpha native reverse auto-retirement'
}
if ($pop -notmatch '(?s)current->list->Hide\(common::transition::Type_3D_SlideToBack1\).*current->list->Show\(common::transition::Type_Reset\)') {
    throw 'back navigation does not restore the parent with the alpha sequence'
}
if ($browser -match 'm_transitionFrames|retireFrames|RestoreListGeometry') {
    throw 'custom frame-count/geometry transition controller still overrides ScePaf'
}
if ($header -match 'm_transitionFrames|retireFrames|m_retiredPages') {
    throw 'custom transition ownership remains in BrowserMenu state'
}
if ($pop -notmatch 'Type_FadeinFast') { throw 'Simplified navigation mode was removed' }

'v0.5 alpha navigation regression passed'
