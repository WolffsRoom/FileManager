$ErrorActionPreference = 'Stop'
$cpp = Get-Content 'src/text_view.cpp' -Raw
$header = Get-Content 'src/text_view.h' -Raw
$rco = [xml](Get-Content 'resource/file_manager_plugin.xml' -Raw)
$page = $rco.resource.pagetable.page | Where-Object id -eq 'page_text_view'
if (-not ($page.text_box | Where-Object id -eq 'text_box_search_page_text_view')) { throw 'Text preview search box missing' }
if (-not ($page.button | Where-Object id -eq 'button_search_page_text_view')) { throw 'Text preview Triangle search binding missing' }
if ($cpp -notmatch 'button_search_page_text_view[\s\S]{0,240}PAD_MENU') { throw 'Triangle does not open text search' }
if ($cpp -notmatch 'button_options_page_text_view[\s\S]{0,120}PAD_START') { throw 'Start does not open text options' }
foreach ($api in 'SetSearchVisible','ApplySearch','CloseSearch','OriginalRow') {
    if ($header -notmatch $api -or $cpp -notmatch "TextView::$api") { throw "Text search API missing: $api" }
}
if ($header -notmatch 'm_visibleLines' -or $cpp -notmatch 'm_visibleLines\.at') { throw 'Filtered rows do not map to original text lines' }
if ($cpp -notmatch 'm_search->GetString' -or $cpp -notmatch 'ContainsNoCase') { throw 'Incremental search filtering missing' }
if ($cpp -notmatch 'm_visibleLines\.empty\(\)') { throw 'No-match search is not guarded' }
if ($header -notmatch 'm_editRow' -or $cpp -notmatch 'if \(row < 0 && index != Option_Save\) return;') { throw 'Line actions can target hidden row zero after a no-match search' }
if ($cpp -notmatch 'm_editRow = row' -or $cpp -notmatch 'int32_t row = m_editRow') { throw 'Editor callback does not retain the validated original row' }
if ($cpp -notmatch 'CloseSearch\(\)[\s\S]*?m_search->SetString\(empty\)') { throw 'Closing search does not restore the complete line set' }
'v0.6 text search passed'
