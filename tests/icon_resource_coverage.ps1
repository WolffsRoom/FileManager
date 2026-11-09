$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$resource = Join-Path $root 'resource'
$rco = [xml](Get-Content (Join-Path $resource 'file_manager_plugin.xml') -Raw)
$actual = @{}
foreach ($texture in $rco.resource.texturetable.texture) {
    $actual[$texture.id] = $texture.src
    $path = Join-Path $resource ($texture.src -replace '/', '\')
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing RCO image $($texture.id): $path" }
}

$expected = @{
    tex_new_search = 'ui/search.png'
    tex_new_theme = 'ui/theme_color.png'
    tex_preview_arrow_left = 'ui/preview_arrow_left.png'
    tex_preview_arrow_right = 'ui/preview_arrow_right.png'
    tex_ui_favorite_folder = 'ui/favorite_folder.png'
    tex_ui_favorite_file = 'ui/favorite_file.png'
    tex_ui_game_iso = 'ui/game_iso.png'
    tex_ui_language = 'ui/language.png'
    tex_ui_font_size = 'ui/font_size.png'
    tex_ui_usb = 'ui/usb.png'
    tex_ui_qr = 'ui/qr.png'
    tex_ui_musical = 'ui/musical.png'
    tex_ui_psp = 'ui/psp.png'
    tex_ui_unlock = 'ui/unlock.png'
    tex_ui_locked = 'ui/locked.png'
    tex_ui_suprx = 'ui/suprx.png'
}
foreach ($id in $expected.Keys) {
    if ($actual[$id] -ne $expected[$id]) { throw "Wrong image for $($id): $($actual[$id])" }
}
if ((Get-ChildItem (Join-Path $resource 'ui/New') -Filter '*.png').Count -gt 0) {
    throw 'Resource/ui/New still contains PNG files'
}
$used = @($actual.Values)
$promoted = Get-ChildItem (Join-Path $resource 'ui') -Filter '*.png'
foreach ($file in $promoted) {
    if ($file.Name -in @('favorite_file.png','favorite_folder.png','game_iso.png','language.png','locked.png','musical.png','psp.png','qr.png','suprx.png','font_size.png','unlock.png','usb.png','theme_color.png') -and
        ('ui/' + $file.Name) -notin $used) { throw "Promoted but not referenced: $($file.Name)" }
}
$fs = Get-Content (Join-Path $root 'src/core/fs.cpp') -Raw
$match = [regex]::Match($fs, '(?s)k_gameExtensions\[\]\s*=\s*\{(.*?)\};')
if (-not $match.Success) { throw 'Game extension table missing' }
$extensions = @([regex]::Matches($match.Groups[1].Value, '"(\.[^"]+)"') | ForEach-Object { $_.Groups[1].Value })
if ($extensions.Count -ne 229) { throw "Expected 229 extensions, got $($extensions.Count)" }
if (($extensions | Select-Object -Unique).Count -ne $extensions.Count) { throw 'Duplicate game formats' }
foreach ($ext in @('.iso','.chd','.gba','.3ds','.xci','.suprx','.vpk','.png','.001','.appxbundle')) {
    if ($ext -notin $extensions) { throw "Game extension absent: $ext" }
}
if ('.md' -in $extensions) { throw 'Markdown is incorrectly classified as a game image' }
if ($fs -notmatch '\{ "\.md", fs::Kind_Text \}') { throw 'Markdown is not classified as text' }
$browser = Get-Content (Join-Path $root 'src/browser_menu.cpp') -Raw
$settings = Get-Content (Join-Path $root 'src/settings_menu.cpp') -Raw
if ($browser -notmatch 'tex_ui_suprx' -or $browser -notmatch 'tex_ui_favorite_file' -or $browser -notmatch 'fs::IsGameFormat') { throw 'Browser icon mapping incomplete' }
if ($settings -notmatch 'tex_ui_font_size' -or $settings -notmatch 'tex_ui_language' -or $settings -notmatch 'tex_ui_unlock' -or $settings -notmatch 'tex_ui_locked') { throw 'Settings icon mapping incomplete' }
if ($settings -notmatch 'm_emptyShortcuts->Show') { throw 'Empty shortcut label is still a selectable row' }
if ($browser -notmatch 'UsbMenu::Open') { throw 'USB mode has no safe blocking status page' }
if (-not (Test-Path (Join-Path $root 'src/usb_menu.cpp'))) { throw 'USB menu implementation missing' }
"Icon/resource audit passed: $($extensions.Count) game formats, $($actual.Count) RCO textures"
