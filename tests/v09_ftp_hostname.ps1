$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$cpp=Get-Content (Join-Path $root 'src/ftp.cpp') -Raw
$menu=Get-Content (Join-Path $root 'src/ftp_menu.cpp') -Raw
foreach($api in @('sceNetResolverCreate','sceNetResolverStartAton','sceNetResolverDestroy')){if($cpp -notmatch $api){throw "missing reverse DNS API $api"}}
if($cpp -match 's_status\.client\[n\]\s*=\s*ip\[n\]'){throw 'FTP client IP is copied to visible status'}
if($cpp -notmatch 'sceKernelCreateThread' -or $cpp -notmatch '2000000' -or $cpp -notmatch 'generation'){throw 'bounded asynchronous hostname lookup is missing'}
if($menu -notmatch 'msg_ftp_client_private'){throw 'private generic client fallback is missing'}
Write-Output 'v0.9 FTP hostname privacy contract passed'
