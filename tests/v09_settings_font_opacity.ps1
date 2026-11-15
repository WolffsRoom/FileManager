$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$cfg=Get-Content (Join-Path $root 'src/settings.cpp') -Raw
$menu=Get-Content (Join-Path $root 'src/settings_menu.cpp') -Raw
$rco=Get-Content (Join-Path $root 'resource/file_manager_plugin.xml') -Raw
$pt=Get-Content (Join-Path $root 'resource/locale/pt-br.xml') -Raw
if($cfg -notmatch 'settings::View_List, 0, 0, -1, 0,' -or $cfg -notmatch 'v\s*==\s*2\s*\?\s*1\s*:\s*v'){throw 'font default/migration is missing'}
if($menu -notmatch 'k_fontSizes\[\]\s*=\s*\{\s*msg_font_small,\s*msg_font_normal\s*\}' -or $menu -notmatch 'Level_FontSize:\s*return 2'){throw 'font menu still has three sizes'}
if($pt -notmatch 'src="Maior" id="msg_font_normal"' -or $pt -match 'src="Grande"'){throw 'Portuguese font labels are wrong'}
if($pt -notmatch 'src="Opacidade dos menus" id="msg_set_preview_opacity"'){throw 'menu opacity label is wrong'}
if($rco -notmatch 'text_font_preview_lead_page_settings' -or $rco -notmatch 'text_font_preview_body_page_settings' -or $pt -notmatch 'Lorem Ipsum'){throw 'complete font preview is missing'}
if($menu -notmatch 'plane_list_item_selection[\s\S]+Hide\(common::transition::Type_Reset\)'){throw 'Settings selection overlay is not neutralized'}
Write-Output 'v0.9 settings/font/opacity contract passed'
