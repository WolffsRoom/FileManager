$ErrorActionPreference = 'Stop'

$browserCpp = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$browserHeader = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.h"

# The supplied Music-folder dump maps PC 0x81001ae6 to OptionMenu::OptionMenu;
# R0 was null while the constructor stored a member, consistent with exhausted
# heap after transitions accumulated on recycled rows.
if ($browserHeader -notmatch 'Stop\(StopParam') {
    throw 'recycled list rows do not reset their Popup1 transition'
}
if ($browserCpp -notmatch 'RestoreListGeometry\(current\)' -or $browserCpp -notmatch 'current->list->Show\(common::transition::Type_FadeinFast\)' -or $browserCpp -notmatch 'page->list->DestroyWidget\(\)') {
    throw 'back navigation does not reuse and restore the parent list safely'
}
if ($browserCpp -match 'RecreateList\(current\)' -or $browserCpp -match 'Type_3D_SlideToBack1') {
    throw 'old list recreation or overlapping 3D transition is still used'
}
if ($browserCpp -notmatch 'OptionMenu::Create') {
    throw 'OptionMenu creation is not guarded'
}

'navigation stability checks passed'
