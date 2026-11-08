$ErrorActionPreference = 'Stop'
$root=Split-Path -Parent $PSScriptRoot
[xml]$rco=Get-Content -Raw (Join-Path $root 'resource/file_manager_plugin.xml')
$browser=Get-Content -Raw (Join-Path $root 'src/browser_menu.cpp')
$page=@($rco.resource.pagetable.page | Where-Object id -eq 'page_browser')[0]
$bar=@($page.plane | Where-Object id -eq 'plane_selection_bar_page_browser')[0]
if (!$bar) { throw 'Selection bar not in browser page' }
$expected=@('button_all_selection_page_browser','button_copy_selection_page_browser','button_move_selection_page_browser','button_delete_selection_page_browser')
$actual=@($bar.button | ForEach-Object { $_.id })
if (($actual -join ';') -ne ($expected -join ';')) { throw "Expected exactly four buttons in order, got $($actual -join ',')" }
if ($bar.layout_hint.size -ne '840, 68' -or $bar.layout_hint.pos -ne '-60, -238') {throw 'Toolbar must leave the native options corner visible'}
$selectionStyle=@($rco.resource.styletable.style_button | Where-Object id -eq 'style_button_selection_action')[0]
if (!$selectionStyle -or $selectionStyle.planeobj[0].texture0 -ne '_common_texture_option_menu_button_single') { throw 'Selection buttons do not use a neutral native texture' }
if ($selectionStyle.planeobj[1].texture0 -ne '_common_texture_option_menu_button_single_glow') {throw 'Missing button touch/focus glow'}
$barStyle=@($rco.resource.styletable.style_plane | Where-Object id -eq 'style_plane_selection_bar')[0]
if (!$barStyle -or $barStyle.color -ne '0.08, 0.08, 0.10, 0.92') {throw 'Purple selection bar still active'}
$item=@($rco.resource.templatetable.template | Where-Object id -eq 'template_list_item_file')[0]
if (!$item) {throw 'File/folder row template not found'}
$listItem=$item.list_item
$button=$listItem.image_button
if (@($button.plane | Where-Object id -eq 'plane_list_item_check').Count -ne 0) {throw 'Checkbox still clipped inside file button'}
if (@($listItem.plane | Where-Object id -eq 'plane_list_item_check').Count -ne 1) {throw 'Checkbox not a single overlay beside file button'}
$box=@($listItem.plane | Where-Object id -eq 'plane_list_item_check')[0]
if ($box.texture -ne 'tex_checkbox_off' -or $box.layout_hint.pos -ne '411, 0') {throw 'Checkbox overlay needs correct off state and right edge'}
foreach($text in @('msg_select_all','msg_opt_copy','msg_opt_move','msg_opt_delete')) {
 if (-not $browser.Contains('g_appPlugin->GetString('+$text+')')) {throw ('Missing button label '+$text)}
}
foreach($id in $expected) {if (!$browser.Contains('m_root->FindChild('+$id+')')) {throw ('Missing callback target '+$id)}}
if ($browser.Contains('button_cancel_selection_page_browser')) {throw 'Old Cancel button should not appear in selection bar'}
if (!$browser.Contains('void BrowserMenu::DoActionPublic(Action action)') -or !$browser.Contains('m_selectionActionPending = true;')) {throw 'Selection actions must defer their PAF event'}
$start=$browser.IndexOf('void BrowserMenu::FillFileItem(')
$end=$browser.IndexOf('void BrowserMenu::FillRootItem(', $start)
$fileCode=$browser.Substring($start,$end-$start)
if (!$fileCode.Contains('RowCheckbox(button->GetParent())') -or
 !$fileCode.Contains('check->Show(common::transition::Type_Reset);')) {throw 'Checkbox does not display when row is marked/selection active'}
if ($fileCode.Contains('0.75f, entry->marked')) {throw 'Purple tint still applied to marked files'}
$start=$browser.IndexOf('void BrowserMenu::ApplyMarkToggle(')
$end=$browser.IndexOf('void BrowserMenu::SelectAll()', $start)
$mark=$browser.Substring($start,$end-$start)
if (!$mark.Contains('check->SetTexture(') -or !$mark.Contains('check->Show(common::transition::Type_Reset)') -or !$mark.Contains('if (!check || !button) continue;')) {throw 'Marking fails to update visible checkboxes safely'}
$start=$browser.IndexOf('void BrowserMenu::ApplySelectAll(')
$end=$browser.IndexOf('void BrowserMenu::OnOptions()', $start)
$all=$browser.Substring($start,$end-$start)
if (!$all.Contains('check->Show(common::transition::Type_Reset)') -or !$all.Contains('if (!check || !button) continue;')) {throw 'Select All does not show checkboxes safely'}
if (!$browser.Contains('DoAction(Action_ClearSelection);')) {throw 'Circle should cancel selection without backing out of the folder'}
'Selection toolbar / file-folder checkboxes / native buttons: PASS'
