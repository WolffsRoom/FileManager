$ErrorActionPreference = 'Stop'

$cpp = Get-Content -Raw "$PSScriptRoot\..\src\preview_menu.cpp"
$header = Get-Content -Raw "$PSScriptRoot\..\src\preview_menu.h"
$rco = [xml](Get-Content -Raw "$PSScriptRoot\..\resource\file_manager_plugin.xml")
$preview = $rco.resource.pagetable.page | Where-Object id -eq 'page_preview'
$title = $preview.text | Where-Object id -eq 'text_title_page_preview'

if ($title.layout_hint.pos -ne '0, 18') { throw 'image filename is not near the lower screen edge' }
if ($cpp -notmatch 'CB_BTN_PRESS_OUT' -or $cpp -notmatch 'SetArrowPressed') { throw 'preview arrows lack press/release opacity feedback' }
if ($cpp -notmatch 'msg_opt_rename' -or $cpp -notmatch 'msg_opt_delete') { throw 'preview Options lacks Rename/Delete' }
if ($header -notmatch 'RemoveCurrentPath' -or $cpp -notmatch 'sceIoRemove') { throw 'preview deletion does not safely update the path list' }
if ($cpp -notmatch 'sceIoRename' -or $cpp -notmatch 'OnRenameEntered') { throw 'preview rename is not implemented' }

'preview action checks passed'
