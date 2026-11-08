$ErrorActionPreference = 'Stop'
$xml = Get-Content -LiteralPath 'resource/file_manager_plugin.xml' -Raw
$cpp = Get-Content -LiteralPath 'src/browser_menu.cpp' -Raw
$header = Get-Content -LiteralPath 'src/browser_menu.h' -Raw

if ($xml -notmatch 'plane_selection_bar_page_browser[\s\S]*?size="840, 68" pos="-60, -238"') {
    throw 'Selection bar must leave the native options corner visible.'
}
if (($xml | Select-String -Pattern 'size="185, 48"' -AllMatches).Matches.Count -lt 4) {
    throw 'Selection action buttons were not narrowed consistently.'
}
if ($xml.IndexOf('plane_selection_bar_page_browser') -gt $xml.IndexOf('button_search_page_browser')) {
    throw 'Options corner must be declared after the selection bar so it stays in front.'
}
if ($header -notmatch 'RowCheckbox\(paf::ui::Widget \*rowRoot\)') {
    throw 'Shared row-checkbox lookup is missing.'
}
if (($cpp | Select-String -Pattern 'FindChild\(plane_list_item_check\)' -AllMatches).Matches.Count -ne 1) {
    throw 'All checkbox lookups must go through RowCheckbox.'
}
if ($cpp -notmatch 'RowCheckbox\(item\)') {
    throw 'Visible and recycled rows are not using the shared checkbox lookup.'
}

Write-Host 'v0.5 selection layout regression passed'
