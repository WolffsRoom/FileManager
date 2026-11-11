$ErrorActionPreference = 'Stop'
$xml = [xml](Get-Content 'resource/file_manager_plugin.xml' -Raw)
$cpp = Get-Content 'src/settings_menu.cpp' -Raw
$header = Get-Content 'src/settings_menu.h' -Raw
$page = $xml.resource.pagetable.page | Where-Object id -eq 'page_settings'
$preview = $page.text | Where-Object id -eq 'text_font_preview_page_settings'
if (-not $preview) { throw 'Font preview widget is missing' }
if ($preview.layout_hint.pos -ne '0, -160') { throw 'Font preview is not below the three font rows' }
if ($cpp -notmatch 'm_list->SetPos\(0\.0f, -254\.0f, 0\.0f\)') { throw 'Settings main list gap was not increased' }
if ($cpp -notmatch 'void SettingsMenu::UpdateFontPreview\(\)') { throw 'Font preview update is missing' }
if ($cpp -notmatch 'settings::FileFontSize\(\) / 28\.0f') { throw 'Preview does not use the selected file font size' }
if ($header -notmatch 'm_fontPreview') { throw 'Settings does not retain the preview widget' }
$template = $xml.resource.templatetable.template | Where-Object id -eq 'template_list_item_file'
$theme = $template.list_item.image_button.plane | Where-Object id -eq 'plane_theme_preview'
if ($theme.layout_hint.pos -ne '-364.1, 0' -or $theme.layout_hint.size -ne '91.8, 63') { throw 'Theme color layer is not centered on the icon slot' }
foreach ($locale in Get-ChildItem 'resource/locale' -Filter '*.xml') {
    if ((Get-Content $locale.FullName -Raw) -notmatch 'id="msg_font_preview"') { throw "Font preview copy missing from $($locale.Name)" }
}
'v0.6 settings layout/font preview passed'
