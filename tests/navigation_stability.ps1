$ErrorActionPreference = 'Stop'

$browserCpp = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$browserHeader = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.h"

# The supplied Music-folder dump maps PC 0x81001ae6 to OptionMenu::OptionMenu;
# R0 was null while the constructor stored a member, consistent with exhausted
# heap after transitions accumulated on recycled rows.
if ($browserHeader -notmatch 'Stop\(StopParam') {
    throw 'recycled list rows do not reset their Popup1 transition'
}
if ($browserCpp -notmatch 'RestoreListGeometry\(current\)' -or
    $browserCpp -notmatch 'm_retiredPages.push_back\(outgoing\)' -or
    $browserCpp -notmatch 'outgoing->retireFrames = 24' -or
    $browserCpp -notmatch 'void BrowserMenu::FreePage') {
    throw '3D back animation releases a live page too early or does not reset geometry'
}
if ($browserCpp -match 'RecreateList\(current\)') {
    throw 'old duplicate-list recreation is still active'
}
if ($browserCpp -notmatch 'OptionMenu::Create') {
    throw 'OptionMenu creation is not guarded'
}

'navigation stability checks passed'
