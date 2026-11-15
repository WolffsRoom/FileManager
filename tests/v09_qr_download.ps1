$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$downloadH = Get-Content -Raw (Join-Path $root 'src/core/download.h')
$downloadC = Get-Content -Raw (Join-Path $root 'src/core/download.c')
$browser = Get-Content -Raw (Join-Path $root 'src/browser_menu.cpp')

if ($downloadH -notmatch 'typedef struct DownloadInfo' -or
    $downloadH -notmatch 'inspectDownloadUrl') {
    throw 'HTTP download metadata interface is missing'
}
if ($downloadC -notmatch 'sceHttpGetAllResponseHeaders' -or
    $downloadC -notmatch 'Content-Disposition' -or
    $downloadC -notmatch 'Content-Type') {
    throw 'QR download does not classify server response metadata'
}
if ($downloadC -notmatch 'sceHttpSetAutoRedirect\(tmpl, SCE_TRUE\)') {
    throw 'HTTP redirects are not enabled'
}
if ($browser -notmatch 'ux0:Downloads' -or $browser -notmatch '\.part') {
    throw 'QR downloads are not staged atomically in ux0:Downloads'
}
if ($browser -notmatch 'sceIoRename\(' -or $browser -notmatch 'OnProgress') {
    throw 'QR download lacks final rename or progress reporting'
}
if ($browser -match 'sceHttpsDisableOption') {
    throw 'QR path still disables TLS server verification'
}

Write-Host 'v0.9 QR download flow passed'
