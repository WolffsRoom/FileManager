$ErrorActionPreference = 'Stop'
$rco = [xml](Get-Content 'resource/file_manager_plugin.xml' -Raw)
$cpp = Get-Content 'src/settings_menu.cpp' -Raw
$header = Get-Content 'src/settings_menu.h' -Raw
$textures = @($rco.resource.texturetable.texture)
$offOn = @($textures | Where-Object id -like 'tex_switch_off_on_*')
$onOff = @($textures | Where-Object id -like 'tex_switch_on_off_*')
if ($offOn.Count -ne 17 -or $onOff.Count -ne 17) { throw "Expected 17 frames per direction; got $($offOn.Count)/$($onOff.Count)" }
foreach ($texture in $offOn + $onOff) {
    $path = Join-Path 'resource' ($texture.src -replace '/', '\')
    if (-not (Test-Path $path)) { throw "Missing switch frame $path" }
}
if ($header -notmatch 'StartSwitchAnimation\(int32_t oldRow, int32_t newRow\)' -or $header -notmatch 'AdvanceSwitchAnimation\(\)') { throw 'Two-row switch animation API missing' }
if ($cpp -notmatch '25000ULL') { throw 'Switch animation does not step every 25 ms' }
if ($cpp -notmatch 'if \(m_switchAnimating\) return;') { throw 'Repeated switch input is not guarded' }
if ($cpp -notmatch 'm_switchFrame >= 16') { throw 'Switch animation does not stop on its terminal frame' }
if ($cpp -notmatch 'k_switchOnOff\[m_switchFrame\]' -or $cpp -notmatch 'k_switchOffOn\[m_switchFrame\]') { throw 'Both on-to-off and off-to-on directions are not rendered' }
if ($cpp -notmatch 'if \(IsSelected\(row\) && !customToggle\) return;' -or $cpp.IndexOf('if (m_switchAnimating) return;') -gt $cpp.IndexOf('settings::Save();')) { throw 'Switch guard does not precede settings mutation' }
if ($cpp -notmatch 'm_switchAnimating = false;[\s\S]{0,100}m_switchOldRow = -1') { throw 'Page rebuild does not cancel stale switch animation rows' }
$template = $rco.resource.templatetable.template | Where-Object id -eq 'template_list_item_file'
$check = $template.list_item.plane | Where-Object id -eq 'plane_list_item_check'
if ($check.layout_hint.size -ne '72, 72') { throw 'Switch is not enlarged to 72x72' }
'v0.6 animated switch passed'
