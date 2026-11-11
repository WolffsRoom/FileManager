$ErrorActionPreference = 'Stop'
$xml = [xml](Get-Content 'resource/file_manager_plugin.xml' -Raw)
$settings = Get-Content 'src/settings_menu.cpp' -Raw
$browser = Get-Content 'src/browser_menu.cpp' -Raw

$template = $xml.resource.templatetable.template | Where-Object id -eq 'template_list_item_file'
$button = $template.list_item.image_button
$check = $template.list_item.plane | Where-Object id -eq 'plane_list_item_check'
$theme = $button.plane | Where-Object id -eq 'plane_theme_preview'

if ($check.texture -ne '_common_texture_transparent') { throw 'Recycled rows can flash the selection square' }
if ($template.list_item.plane | Where-Object id -eq 'plane_list_item_mark') { throw 'Selection overlay can block list touch input' }
if ($theme.layout_hint.size -ne '91.8, 63' -or $theme.layout_hint.pos -ne '-364.1, 0') { throw 'Theme color layer is not centered over the icon object' }
if ($settings -notmatch 'button->SetSize\(920\.0f, 80\.0f, 0\.0f\)') { throw 'Settings switch is outside the touchable row area' }

$highlightStart = $browser.IndexOf('void BrowserMenu::ApplyRowHighlight(')
$highlightEnd = $browser.IndexOf('void BrowserMenu::FillFileItem(', $highlightStart)
$highlight = $browser.Substring($highlightStart, $highlightEnd - $highlightStart)
if ($highlight -notmatch 'text_list_item_name' -or $highlight -notmatch 'text_list_item_info') { throw 'Marked rows do not tint their visible text' }
if ($highlight -match 'GetDrawObj\(ui::ImageButton::OBJ_IMAGE\)') { throw 'Selection accesses an unsafe recycled ImageButton draw object' }
if ($highlight -match 'plane_list_item_mark') { throw 'Selection overlay can intercept touch input' }
if ($highlight -match 'button->SetColor') { throw 'Focus animation can still overwrite the selected tint' }

$searchStart = $browser.IndexOf('void BrowserMenu::SetSearchVisible(')
$searchEnd = $browser.IndexOf('void BrowserMenu::ToggleSearch(', $searchStart)
$search = $browser.Substring($searchStart, $searchEnd - $searchStart)
$resize = $search.IndexOf('page->list->SetSize(960.0f, 368.0f')
$head = $search.IndexOf('FOCUS_ALIGN_TYPE_HEAD')
if ($resize -lt 0 -or $head -le $resize) { throw 'Search does not align the first row after resizing the list' }

'v0.6 touch, selection and search regression passed'
