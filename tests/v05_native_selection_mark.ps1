$ErrorActionPreference = 'Stop'
$cpp = Get-Content -LiteralPath 'src/browser_menu.cpp' -Raw

if ($cpp -notmatch 'tex_checkbox_on' -or $cpp -notmatch 'check->SetActivate\(false\)') {
    throw 'Browser selection does not render a non-interactive native checkbox.'
}
if (($cpp | Select-String -Pattern 'ApplyRowHighlight\(' -AllMatches).Matches.Count -lt 4) {
    throw 'Marked, select-all and clear paths do not preserve persistent row highlight.'
}
$fillStart = $cpp.IndexOf('void BrowserMenu::FillFileItem(')
$fillEnd = $cpp.IndexOf('ui::Widget *BrowserMenu::RowCheckbox', $fillStart)
$fill = $cpp.Substring($fillStart, $fillEnd - $fillStart)
if ($fill -notmatch 'ApplyRowHighlight') {
    throw 'Recycled file rows do not restore marked/focused state.'
}

Write-Host 'v0.5 native selection mark regression passed'
