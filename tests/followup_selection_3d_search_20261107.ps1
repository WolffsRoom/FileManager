$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$browser = Get-Content -Raw (Join-Path $root 'src/browser_menu.cpp')
$settings = Get-Content -Raw (Join-Path $root 'src/settings_menu.cpp')
[xml]$rco = Get-Content -Raw (Join-Path $root 'resource/file_manager_plugin.xml')
function Assert([bool]$ok, [string]$reason) {
    if (-not $ok) { throw $reason }
}
$begin = $browser.IndexOf('void BrowserMenu::ApplyMarkToggle(int32_t index)')
$end = $browser.IndexOf('void BrowserMenu::SelectAll()', $begin)
Assert ($begin -ge 0 -and $end -gt $begin) 'Cannot inspect mark toggle'
$mark = $browser.Substring($begin,$end-$begin)
Assert ($mark.Contains('if (!button) continue;')) 'Off-screen/empty list rows still reach native focused state'
Assert ($mark.Contains('ApplyRowHighlight(')) 'Selected row persistent highlight unexpectedly removed'

$selAllBegin = $browser.IndexOf('void BrowserMenu::ApplySelectAll()')
$selAllEnd = $browser.IndexOf('void BrowserMenu::OnOptions()', $selAllBegin)
Assert ($selAllBegin -ge 0 -and $selAllEnd -gt $selAllBegin) 'Cannot inspect Select All'
$all = $browser.Substring($selAllBegin, $selAllEnd-$selAllBegin)
Assert ($all.Contains('if (!button) continue;')) 'Select All still accesses recycled placeholder cells'
Assert (-not $all.Contains('DeleteCell(')) 'Select All still destroys entire list'
$clearBegin = $browser.IndexOf('case Action_ClearSelection:')
$clearEnd = $browser.IndexOf('case Action_Copy:', $clearBegin)
Assert ($clearBegin -ge 0 -and $clearEnd -gt $clearBegin) 'Cannot inspect Cancel selection'
$clear = $browser.Substring($clearBegin, $clearEnd-$clearBegin)
Assert (-not $clear.Contains('OnMarkToggle(i)')) 'Cancel toggles only first selected file'
Assert ($clear.Contains('page->entries.at(i)->marked = false;')) 'Cancel did not clear every marked item'
Assert ($clear.Contains('if (check)') -and $clear.Contains('if (button)')) 'Cancel still touches nonexistent child buttons'

Assert (-not $mark.Contains('item->FindChild(image_button_list_item)->SetColor(')) 'Unguarded SetColor returned'
$start = $browser.IndexOf('bool BrowserMenu::PopPage()')
$stop = $browser.IndexOf('void BrowserMenu::Refresh()', $start)
Assert ($start -ge 0 -and $stop -gt $start) 'Missing page pop'
$pop = $browser.Substring($start,$stop-$start)
Assert ($pop.Contains('current->list->Hide(common::transition::Type_3D_SlideToBack1)')) 'Alpha parent reverse sequence missing'
Assert ($pop.Contains('current->list->Show(common::transition::Type_Reset)')) 'Alpha parent reset missing'
Assert (-not $browser.Contains('m_transitionFrames')) 'Frame-count transition override remains'
$start = $browser.IndexOf('void BrowserMenu::ToggleSearch()')
$stop = $browser.IndexOf('void BrowserMenu::SetRowIcon(', $start)
$search = $browser.Substring($start,$stop-$start)
Assert ($search.Contains('page->list->SetFocus(0, 0, ui::ListView::FOCUS_ALIGN_TYPE_HEAD')) 'Search does not scroll up before keyboard'
$start = $browser.IndexOf('void BrowserMenu::OnSearch()')
$stop = $browser.IndexOf('void BrowserMenu::OnForward()', $start)
$filter = $browser.Substring($start,$stop-$start)
Assert ($filter.Contains('page->list->SetFocus(0, 0, ui::ListView::FOCUS_ALIGN_TYPE_HEAD')) 'Filtered results do not begin at top'
Assert ($settings.Contains('m_list->SetSize(960.0f, 304.0f, 0.0f)')) 'Settings main list has no extra room'
$settingsPage = @($rco.resource.pagetable.page | Where-Object id -eq 'page_settings')[0]
$bar = @($settingsPage.plane | Where-Object id -eq 'plane_power_actions_page_settings')[0]
Assert ($bar.layout_hint.pos -eq '0, 130') 'Power bar is not below the Ajustes header'
'PASS: Vita 3D root/return, null selection button, settings gap, search-to-head'
