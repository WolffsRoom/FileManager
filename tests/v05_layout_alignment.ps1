$ErrorActionPreference = 'Stop'
[xml]$rco = Get-Content -LiteralPath 'resource/file_manager_plugin.xml' -Raw

$settings = @($rco.resource.pagetable.page | Where-Object id -eq 'page_settings')[0]
$power = @($settings.plane | Where-Object id -eq 'plane_power_actions_page_settings')[0]
if ($power.layout_hint.pos -ne '0, 130') {
    throw 'Settings actions still overlap the Ajustes header.'
}
$powerStyle = @($rco.resource.styletable.style_button | Where-Object id -eq 'style_button_power_native')[0]
$powerBg = @($powerStyle.planeobj | Where-Object id -eq 'plane_obj1')[0]
if ($powerBg.texture0 -ne '_common_texture_option_menu_button_single' -or
    $powerBg.color -ne '1, 1, 1, 0.55') {
    throw 'Settings action buttons lack semitransparent native boundaries.'
}

$browser = @($rco.resource.pagetable.page | Where-Object id -eq 'page_browser')[0]
$bar = @($browser.plane | Where-Object id -eq 'plane_selection_bar_page_browser')[0]
$buttons = @($bar.button)
$expectedCenters = @('-267, 0', '-89, 0', '89, 0', '267, 0')
for ($i = 0; $i -lt 4; $i++) {
    if ($buttons[$i].layout_hint.size -ne '168, 48' -or $buttons[$i].layout_hint.pos -ne $expectedCenters[$i]) {
        throw "Selection action $i is not centered symmetrically."
    }
}
$selectionStyle = @($rco.resource.styletable.style_button | Where-Object id -eq 'style_button_selection_action')[0]
if ($selectionStyle.textobj.align_x -ne '1' -or $selectionStyle.textobj.align_y -ne '1') {
    throw 'Selection labels are not centered inside their buttons.'
}

Write-Host 'v0.5 layout alignment regression passed'
