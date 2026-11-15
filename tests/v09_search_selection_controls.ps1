$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$cpp=Get-Content (Join-Path $root 'src/browser_menu.cpp') -Raw
$h=Get-Content (Join-Path $root 'src/browser_menu.h') -Raw
$rco=Get-Content (Join-Path $root 'resource/file_manager_plugin.xml') -Raw
$pt=Get-Content (Join-Path $root 'resource/locale/pt-br.xml') -Raw
if($h -notmatch 'bool\s+CloseSearch\s*\(' -or $cpp -notmatch 'if\s*\(CloseSearch\(\)\)\s*return'){throw 'Circle does not consume search before navigation'}
if($cpp -notmatch 'm_optionsButton->SetKeycode\(inputdevice::pad::Data::PAD_MENU\)' -or $cpp -notmatch 'startButton->SetKeycode\(inputdevice::pad::Data::PAD_START\)'){throw 'Start/Triangle mapping is wrong'}
if($rco -notmatch 'style_plane_selection_bar[\s\S]+size="960, 68"'){throw 'selection toolbar is not 960 px'}
if($pt -notmatch 'src="Selec\. Tudo"' -or $pt -notmatch 'src="Desel\. Tudo"'){throw 'compact select-all labels are missing'}
if($h -notmatch 'AllSelected' -or $h -notmatch 'UpdateSelectionToolbar' -or $cpp -notmatch 'const bool select = !AllSelected'){throw 'select-all is not a true toggle'}
if($cpp -notmatch 'selection->SetColor\(0\.20f, 0\.58f, 1\.0f, marked \? 0\.78f : 0\.0f\)'){throw 'selection repaint is not immediate'}
Write-Output 'v0.9 search/selection controls contract passed'
