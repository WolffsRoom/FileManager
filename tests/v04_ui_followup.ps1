$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$browser = Get-Content "$root/src/browser_menu.cpp" -Raw
$music = Get-Content "$root/src/music_menu.cpp" -Raw
$settings = Get-Content "$root/src/settings_menu.cpp" -Raw
$rco = Get-Content "$root/resource/file_manager_plugin.xml" -Raw

if ($rco -match '<plane texture="tex_new_theme" id="plane_theme_preview">') {
    throw 'theme preview texture still leaks through the shared file-row template'
}
if ($settings -notmatch 'k_themeIcons' -or $settings -notmatch 'icon = k_themeIcons\[row\]') {
    throw 'theme icons do not use aligned per-theme button textures'
}
if ($browser -notmatch 'Type_3D_SlideToBack1' -or
    $browser -notmatch 'Type_3D_SlideFromFront' -or
    $browser -notmatch 'DoReverse\(-100.0f, outgoing->list' -or
    $browser -notmatch 'Type_3D_SlideFromFront, true, false' -or
    $browser -notmatch 'Type_FadeinFast') {
    throw 'navigating forward/back must support native Vita 3D and simple fade'
}
if ($browser -match 'Type_SlideFromRight2' -or $browser -match 'Type_SlideFromLeft2') {
    throw 'generic lateral transitions still replace the native folder navigation'
}
if ($browser -match 'openingAnimation') {
    throw 'post-v0.2 list-level opening animation is still present'
}
if ($music -notmatch 's_minimizedPlayer != NULL && musicIsPlaying\(\)') {
    throw 'background indicator is not restricted to active playback'
}
if ($rco -notmatch 'button_music_indicator_page_browser[\s\S]{0,160}pos="-375, 208"') {
    throw 'music indicator is not positioned at the upper left'
}
if ($browser -notmatch 'msg_installing[\s\S]{0,900}m_opSources\.at\(0\)') {
    throw 'installation progress does not include the VPK source name'
}
if ($rco -notmatch 'text_title_page_preview[\s\S]{0,120}pos="0, 18"') {
    throw 'image title is not centered near the bottom border'
}

Write-Output 'v0.4 follow-up UI checks passed'
