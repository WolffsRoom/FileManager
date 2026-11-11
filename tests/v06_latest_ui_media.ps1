$ErrorActionPreference = 'Stop'
$rcoRaw = Get-Content 'resource/file_manager_plugin.xml' -Raw
$settings = Get-Content 'src/settings.cpp' -Raw
$menu = Get-Content 'src/settings_menu.cpp' -Raw
$browser = Get-Content 'src/browser_menu.cpp' -Raw
$video = Get-Content 'src/preview_menu.cpp' -Raw
$music = Get-Content 'src/music_menu.cpp' -Raw

if ($settings -notmatch 's_config = \{ 2,' -or $settings -notmatch 'msg_theme_sony_blue') { throw 'Graphite default or Sony Blue theme missing' }
if ($settings -notmatch 'return k_themeCount - 2') { throw 'Custom theme compatibility index changed' }
if ($rcoRaw -notmatch 'style_button_option_single[\s\S]{0,400}align_x="1"') { throw 'Install/cancel labels are not centered' }
if (($rcoRaw | Select-String '_common_texture_dialog_frame' -AllMatches).Matches.Count -lt 3) { throw 'Custom dialogs do not use rounded native frames' }
if ($video -notmatch 'haveVideo' -or $video -notmatch 'sceAvPlayerDisableStream') { throw 'MP4 player does not select a single audio/video pair' }
if ($menu -notmatch 'Main_ShowVpkIcons' -or $browser -notmatch 'showVpkIcons' -or $browser -notmatch 'makeBubblePng') { throw 'Configurable VPK bubble icons missing' }
if ($browser -notmatch '122\.4f, 84\.0f') { throw 'VPK icons are not 20 percent larger' }
if ($music -notmatch 'MinimizedCover' -or $music -notmatch 'SetPos\(-375\.0f, 208\.0f') { throw 'Animated minimized album cover missing' }

[xml]$xml = $rcoRaw
$template = $xml.resource.templatetable.template | Where-Object id -eq 'template_list_item_file'
$children = @($template.list_item.ChildNodes | Where-Object NodeType -eq 'Element')
$buttonIndex = -1
$markIndex = -1
for ($i = 0; $i -lt $children.Count; $i++) {
    if ($children[$i].id -eq 'image_button_list_item') { $buttonIndex = $i }
    if ($children[$i].id -eq 'plane_list_item_mark') { $markIndex = $i }
}
if ($markIndex -le $buttonIndex) { throw 'Persistent selection mark is still behind the row background' }

'v0.6 latest UI/media refinements passed'
