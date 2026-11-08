$ErrorActionPreference = 'Stop'
[xml]$rco = Get-Content -LiteralPath 'resource/file_manager_plugin.xml' -Raw
$cpp = Get-Content -LiteralPath 'src/settings_menu.cpp' -Raw
$header = Get-Content -LiteralPath 'src/settings_menu.h' -Raw

$textures = @($rco.resource.texturetable.texture)
foreach ($pair in @(
    @('tex_ui_toggle_on','ui/toggle-on.png'),
    @('tex_ui_toggle_off','ui/toggle-off.png'),
    @('tex_ui_anim_options','ui/AnimOptions.png'))) {
    $node = @($textures | Where-Object id -eq $pair[0])[0]
    if (!$node -or $node.src -ne $pair[1]) { throw "Missing Settings texture $($pair[0])" }
}
if ($cpp -notmatch 'Main_Navigation\) icon = tex_ui_anim_options' -or
    $cpp -notmatch 'case Level_Navigation:[\s\S]*?icon = tex_ui_anim_options') {
    throw 'Navigation Animation does not use AnimOptions.png.'
}
if ($header -notmatch 'bool ShowsToggle\(\) const;') { throw 'Settings choice pages have no toggle policy.' }
if ($cpp -notmatch 'tex_ui_toggle_on.*tex_ui_toggle_off' -or
    $cpp -notmatch 'check->Show\(common::transition::Type_Reset\)') {
    throw 'Settings choices do not show explicit on/off toggles.'
}
if ($cpp -notmatch '91\.8f, 63\.0f' -or $cpp -match '71\.4f, 49\.0f') {
    throw 'Settings list icons are not uniformly 10 percent smaller.'
}
$powerStyle = @($rco.resource.styletable.style_button | Where-Object id -eq 'style_button_power_native')[0]
$bg = @($powerStyle.planeobj | Where-Object id -eq 'plane_obj1')[0]
if ($bg.texture0 -ne '_common_texture_option_menu_button_single' -or $bg.color -ne '1, 1, 1, 0.55') {
    throw 'Power actions lack a semitransparent native button boundary.'
}

Write-Host 'v0.5 Settings visuals regression passed'
