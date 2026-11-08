$ErrorActionPreference = 'Stop'
$cpp = Get-Content -LiteralPath 'src/browser_menu.cpp' -Raw
$header = Get-Content -LiteralPath 'src/browser_menu.h' -Raw

if ($header -notmatch 'void CloseSearchForNavigation\(\);') {
    throw 'Navigation has no shared search-close operation.'
}
$start = $cpp.IndexOf('void BrowserMenu::CloseSearchForNavigation()')
$end = $cpp.IndexOf('void BrowserMenu::SetRowIcon', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Search-close implementation is missing.' }
$close = $cpp.Substring($start, $end - $start)
if ($close -notmatch 'm_search->SetString\(empty\)' -or
    $close -notmatch 'FilterSearch\(true\)' -or
    $close -notmatch 'SetSearchVisible\(false\)') {
    throw 'Navigation search-close does not clear the filter and hide the bar.'
}
if (($cpp | Select-String -Pattern 'CloseSearchForNavigation\(\);' -AllMatches).Matches.Count -lt 2) {
    throw 'Forward and back navigation do not both close search.'
}
if ($header -match 'AnimateRows' -or
    $header -notmatch 'param\.list_item->Show\(paf::common::transition::Type_Popup1\)' -or
    $header -notmatch 'param\.list_item->Hide\(paf::common::transition::Type_Reset\)') {
    throw 'Every folder row must use Popup1 and reset when recycled.'
}

Write-Host 'v0.5 navigation search/scroll regression passed'
