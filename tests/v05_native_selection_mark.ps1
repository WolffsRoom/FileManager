$ErrorActionPreference = 'Stop'
$cpp = Get-Content -LiteralPath 'src/browser_menu.cpp' -Raw

if ($cpp -match 'tex_checkbox_on|tex_checkbox_off') {
    throw 'Browser selection still renders checkbox textures.'
}
if (($cpp | Select-String -Pattern 'SetFocusedState\(' -AllMatches).Matches.Count -lt 5) {
    throw 'Marked, select-all, clear and clipboard paths do not preserve native row highlight.'
}
$fillStart = $cpp.IndexOf('void BrowserMenu::FillFileItem(')
$fillEnd = $cpp.IndexOf('ui::Widget *BrowserMenu::RowCheckbox', $fillStart)
$fill = $cpp.Substring($fillStart, $fillEnd - $fillStart)
if ($fill -notmatch 'check->Hide\(common::transition::Type_Reset\)' -or
    $fill -notmatch 'entry->marked[\s\S]*?page->focused') {
    throw 'Recycled file rows do not hide checkboxes and restore marked/focused state.'
}

Write-Host 'v0.5 native selection mark regression passed'
