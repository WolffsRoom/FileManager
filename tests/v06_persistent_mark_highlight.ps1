$ErrorActionPreference = 'Stop'
$cpp = Get-Content 'src/browser_menu.cpp' -Raw
$header = Get-Content 'src/browser_menu.h' -Raw
if ($header -notmatch 'ApplyRowHighlight\(BrowserPage \*page, int32_t index, paf::ui::Widget \*button\)') { throw 'Shared row highlight helper missing' }
$start = $cpp.IndexOf('void BrowserMenu::ApplyRowHighlight(')
$end = $cpp.IndexOf('void BrowserMenu::FillFileItem(', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Cannot inspect persistent row highlight implementation' }
$body = $cpp.Substring($start, $end - $start)
if ($body -notmatch 'entry->marked' -or $body -notmatch 'SetColor\(') { throw 'Marked rows do not receive a persistent color' }
if ($body -notmatch 'marked \? 0\.62f : 1\.0f' -or $body -notmatch 'marked \? 0\.88f : 1\.0f') { throw 'Marked color does not reset recycled unmarked rows to neutral' }
if ($body -notmatch 'SetFocusedState\(index == page->focused\)') { throw 'Cursor focus is not independent from persistent marking' }
if (($cpp | Select-String -Pattern 'ApplyRowHighlight\(' -AllMatches).Matches.Count -lt 4) { throw 'Create/toggle/select-all/clear paths do not share the highlight helper' }
'v0.6 persistent mark highlight passed'
