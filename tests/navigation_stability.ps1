$ErrorActionPreference = 'Stop'

$browserCpp = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$browserHeader = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.h"

# The supplied Music-folder dump maps PC 0x81001ae6 to OptionMenu::OptionMenu;
# R0 was null while the constructor stored a member, consistent with exhausted
# heap after transitions accumulated on recycled rows.
if ($browserHeader -notmatch 'Stop\(StopParam') {
    throw 'recycled list rows do not reset their Popup1 transition'
}
if ($browserCpp -notmatch 'Type_3D_SlideFromFront, true, false' -or
    $browserCpp -match 'm_transitionFrames|retireFrames|RestoreListGeometry') {
    throw '3D navigation does not use alpha ScePaf ownership'
}
if ($browserCpp -match 'RecreateList\(current\)') {
    throw 'old duplicate-list recreation is still active'
}
if ($browserCpp -notmatch 'OptionMenu::Create') {
    throw 'OptionMenu creation is not guarded'
}

'navigation stability checks passed'
