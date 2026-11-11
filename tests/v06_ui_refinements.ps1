$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$xml = [xml](Get-Content -Raw "$root\resource\file_manager_plugin.xml")
$browser = Get-Content -Raw "$root\src\browser_menu.cpp"
$settings = Get-Content -Raw "$root\src\settings_menu.cpp"
$preview = Get-Content -Raw "$root\src\preview_menu.cpp"

$template = $xml.resource.templatetable.template | Where-Object id -eq 'template_list_item_file'
$button = $template.list_item.image_button
$name = $button.text | Where-Object id -eq 'text_list_item_name'
$details = $button.text | Where-Object id -eq 'text_list_item_info'
if (-not $name) { throw 'file name does not have an independent top line' }
if (-not $details -or [int](($details.layout_hint.pos -split ',')[1].Trim()) -ge 0) {
    throw 'file details are not positioned below the file name'
}
if ($browser -notmatch 'CB_GESTURE_LONG_PRESS') { throw 'touch-and-hold does not mark a browser row' }

foreach ($id in 'button_thumb_left_page_preview','button_thumb_current_page_preview','button_thumb_right_page_preview') {
    if (-not $xml.SelectSingleNode("//*[@id='$id']")) { throw "missing clickable preview thumbnail: $id" }
}
if ($preview -notmatch 'OnThumbnail' -or $preview -notmatch 'Timer\(0\.[0-9]+f, Timer::FUNC_SINE') {
    throw 'preview mode and thumbnail changes are not animated'
}

$style = $xml.resource.styletable.style_progressbar_touch | Where-Object id -eq 'style_progressbar_touch_native'
$textures = @($style.planeobj | ForEach-Object texture0)
if ($textures -notcontains '_common_texture_progressbar_base' -or
    $textures -notcontains '_common_texture_progressbar_slider') {
    throw 'opacity slider does not use native ScePaf progressbar assets'
}
if ($settings -notmatch 'colored child plane is the theme icon[\s\S]{0,180}button->SetTexture\(tex\)') {
    throw 'Themes still renders a neutral icon below the colored icon'
}

'v0.6 UI refinement checks passed'
