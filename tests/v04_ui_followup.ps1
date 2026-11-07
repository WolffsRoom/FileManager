$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$browser = Get-Content "$root/src/browser_menu.cpp" -Raw
$music = Get-Content "$root/src/music_menu.cpp" -Raw
$settings = Get-Content "$root/src/settings_menu.cpp" -Raw
$rco = Get-Content "$root/resource/file_manager_plugin.xml" -Raw

if ($rco -match '<plane texture="tex_new_theme" id="plane_theme_preview">') {
    throw 'theme preview texture still leaks through the shared file-row template'
}
if ($settings -notmatch 'themePreview->SetTexture\(g_appPlugin->GetTexture\(tex_new_theme\)') {
    throw 'theme texture is not scoped to the Themes screen'
}
if ($browser -notmatch 'Type_3D_SlideToBack1' -or $browser -notmatch 'Type_3D_SlideFromFront') {
    throw 'native Vita 3D folder navigation was not restored'
}
if ($browser -match 'Type_SlideFromRight2' -or $browser -match 'Type_SlideFromLeft2') {
    throw 'generic lateral transitions still replace the native folder navigation'
}
if ($browser -notmatch 'openingAnimation = path\.empty\(\)') {
    throw 'folder pages still replay the main-menu opening animation'
}
if ($music -notmatch 's_minimizedPlayer != NULL && musicIsPlaying\(\)') {
    throw 'background indicator is not restricted to active playback'
}
if ($rco -notmatch 'button_music_indicator_page_browser[\s\S]{0,160}pos="430, -160"') {
    throw 'music indicator is not positioned above the options corner'
}
if ($browser -notmatch 'msg_installing[\s\S]{0,900}m_opSources\.at\(0\)') {
    throw 'installation progress does not include the VPK source name'
}
if ($rco -notmatch 'text_title_page_preview[\s\S]{0,120}pos="0, 88"') {
    throw 'image title was not raised above the lower edge'
}

Write-Output 'v0.4 follow-up UI checks passed'
