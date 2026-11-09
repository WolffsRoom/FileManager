$ErrorActionPreference = 'Stop'
$rco = [xml](Get-Content 'resource/file_manager_plugin.xml' -Raw)
$cpp = Get-Content 'src/settings_menu.cpp' -Raw
$required = 'cross','circle','triangle','square','select','l','r','left','right'
foreach ($name in $required) {
    $node = @($rco.resource.texturetable.texture | Where-Object id -eq "tex_control_$name")[0]
    if (-not $node) { throw "Missing normalized control texture: $name" }
    $path = Join-Path 'resource' ($node.src -replace '/', '\')
    if (-not (Test-Path $path)) { throw "Missing normalized control PNG: $path" }
    Add-Type -AssemblyName System.Drawing
    $img = [Drawing.Image]::FromFile((Resolve-Path $path))
    try { if ($img.Width -ne 64 -or $img.Height -ne 64) { throw "$name is not normalized to 64x64" } } finally { $img.Dispose() }
}
if ($cpp -notmatch 'm_level == Level_Commands[\s\S]{0,120}64\.0f[\s\S]{0,40}44\.0f') { throw 'Controls icons are not rendered smaller than Settings icons' }
if ($cpp -match 'Level_Commands[\s\S]{0,200}LAYOUT_TYPE_GRID') { throw 'Controls uses a grid instead of the vertical list' }
foreach ($id in 'tex_control_cross','tex_control_circle','tex_control_triangle','tex_control_square','tex_control_select','tex_control_l','tex_control_r','tex_control_left','tex_control_right') {
    if ($cpp -notmatch $id) { throw "Controls mapping does not use $id" }
}
'v0.6 controls assets passed'
