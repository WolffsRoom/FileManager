$ErrorActionPreference = 'Stop'

$browserCpp = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$browserHeader = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.h"

# The supplied Music-folder dump maps PC 0x81001ae6 to OptionMenu::OptionMenu;
# R0 was null while the constructor stored a member, consistent with exhausted
# heap after transitions accumulated on recycled rows.
if ($browserHeader -notmatch 'Stop\(StopParam') {
    throw 'recycled list rows do not reset their Popup1 transition'
}
if ($browserCpp -notmatch 'RestoreListGeometry\(current\)') {
    throw 'back navigation does not restore canonical list geometry'
}
if ($browserCpp -notmatch 'new \(std::nothrow\) OptionMenu') {
    throw 'OptionMenu allocation is not guarded against heap exhaustion'
}

'navigation stability checks passed'
