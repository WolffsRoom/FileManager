$ErrorActionPreference = 'Stop'
$cpp = Get-Content 'src/browser_menu.cpp' -Raw
$header = Get-Content 'src/browser_menu.h' -Raw
if ($header -notmatch 'ApplyRowHighlight\(BrowserPage \*page, int32_t index, paf::ui::Widget \*button\)') { throw 'Shared row highlight helper missing' }
$start = $cpp.IndexOf('void BrowserMenu::ApplyRowHighlight(')
$end = $cpp.IndexOf('void BrowserMenu::FillFileItem(', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Cannot inspect persistent row highlight implementation' }
$body = $cpp.Substring($start, $end - $start)
if ($body -notmatch 'entry->marked' -or $body -notmatch 'plane_list_item_mark') { throw 'Marked rows do not use the persistent highlight layer' }
if ($body -notmatch 'marked \? 0\.42f : 0\.0f') { throw 'Marked layer does not reset recycled unmarked rows to transparent' }
if ($body -match 'button->SetColor') { throw 'Persistent marking is still overwritten by button focus animation' }
if ($body -notmatch 'SetFocusedState\(index == page->focused\)') { throw 'Cursor focus is not independent from persistent marking' }
if (($cpp | Select-String -Pattern 'ApplyRowHighlight\(' -AllMatches).Matches.Count -lt 4) { throw 'Create/toggle/select-all/clear paths do not share the highlight helper' }
$optionStart = $cpp.IndexOf('void BrowserMenu::HighlightOptionTarget(')
$optionEnd = $cpp.IndexOf('void BrowserMenu::OpenSortMenu(', $optionStart)
$optionBody = $cpp.Substring($optionStart, $optionEnd - $optionStart)
if ($optionBody -notmatch 'if \(!highlighted\)[\s\S]*?ApplyRowHighlight') { throw 'Closing Options does not restore persistent marked-row tint' }
'v0.6 persistent mark highlight passed'
