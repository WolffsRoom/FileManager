$ErrorActionPreference = 'Stop'
$cpp = Get-Content -LiteralPath 'src/browser_menu.cpp' -Raw
$header = Get-Content -LiteralPath 'src/browser_menu.h' -Raw

if ($cpp -notmatch 'void BrowserMenu::PollSearch\(\)') {
    throw 'Search does not poll the IME text while the user types.'
}
if ($cpp -notmatch 'menu->PollSearch\(\)') {
    throw 'Incremental filtering is not registered on the PAF main-thread tick.'
}
if ($cpp -notmatch 'FilterSearch\(false\)') {
    throw 'Typing must filter without closing the IME.'
}
if ($cpp -notmatch 'if \(finishEditing\)[\s\S]*?m_search->EndEdit\(\)') {
    throw 'Only a completed search may close the IME.'
}
if ($cpp -notmatch 'SetFocus\(0, 0, ui::ListView::FOCUS_ALIGN_TYPE_HEAD') {
    throw 'Search results must always return to the first visible row.'
}
if ($header -notmatch 'paf::string m_lastSearchQuery') {
    throw 'Incremental search change detection state is missing.'
}

Write-Host 'v0.5 incremental search regression passed'
