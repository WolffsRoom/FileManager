$ErrorActionPreference = 'Stop'

$source = Get-Content -Raw "$PSScriptRoot\..\src\settings_menu.cpp"
$rco = [xml](Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml")
$template = $rco.resource.templatetable.template | Where-Object id -eq 'template_list_item_file'
$item = $template.list_item
$button = $item.image_button

# Crash Ajustes 2026-11-08: runtime LR 0x810251c2 relocates to
# SettingsMenu::CreateItem immediately after Widget::Hide with R0 == NULL.
# The selection UI moved the checkbox from image_button to list_item.
if (-not ($item.plane | Where-Object id -eq 'plane_list_item_check')) {
    throw 'fixture no longer represents a sibling checkbox'
}
if ($button.plane | Where-Object id -eq 'plane_list_item_check') {
    throw 'fixture unexpectedly contains the checkbox inside image_button'
}

$createStart = $source.IndexOf('ui::ListItem *SettingsMenu::CreateItem')
$createEnd = $source.IndexOf('void SettingsMenu::UpdateChecks', $createStart)
$create = $source.Substring($createStart, $createEnd - $createStart)
if ($create -notmatch 'item->FindChild\(plane_list_item_check\)') {
    throw 'Settings still resolves the sibling checkbox from the image button'
}
if ($create -notmatch 'if \(check\)' -or $create -notmatch 'if \(themePreview\)') {
    throw 'optional Settings row decorations are not null-guarded'
}

$pt = [xml](Get-Content -Raw "$PSScriptRoot\..\resource\locale\pt-br.xml")
$native = $pt.stringset.string | Where-Object id -eq 'msg_navigation_native'
$simple = $pt.stringset.string | Where-Object id -eq 'msg_navigation_simple'
if ($native.src -ne 'Padrão' -or $simple.src -ne 'Simplificado') {
    throw 'Portuguese navigation choices are not Padrão/Simplificado'
}

'v0.5 Settings crash regression passed'
