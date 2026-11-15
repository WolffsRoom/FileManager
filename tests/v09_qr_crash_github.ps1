$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$qh = Get-Content (Join-Path $root 'src/qr_menu.h') -Raw
$qc = Get-Content (Join-Path $root 'src/qr_menu.cpp') -Raw
$dc = Get-Content (Join-Path $root 'src/core/download.c') -Raw
$cm = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
$browser = Get-Content (Join-Path $root 'src/browser_menu.cpp') -Raw

if ($qh -notmatch 'quirc_code\s*\*m_code' -or $qh -notmatch 'quirc_data\s*\*m_data' -or
    $qc -notmatch 'calloc\(.+quirc_code' -or $qc -notmatch 'calloc\(.+quirc_data') {
    throw 'large quirc work buffers are still on the callback stack'
}
if ($qc -notmatch 'QUIRC_MAX_GRID_SIZE' -or $qc -notmatch 'QUIRC_MAX_PAYLOAD' -or
    $qc -notmatch 'sceCameraStop[\s\S]+m_cb') {
    throw 'QR bounds or camera-before-callback lifecycle is missing'
}
foreach ($token in @('CURLOPT_FOLLOWLOCATION','CURLOPT_MAXREDIRS','CURLINFO_RESPONSE_CODE','CURLOPT_HEADERFUNCTION','CURLOPT_XFERINFOFUNCTION')) {
    if ($dc -notmatch $token) { throw "curl download contract missing $token" }
}
if ($browser -notmatch '\.part' -or $dc -notmatch 'sceIoRemove') { throw 'partial download cleanup is missing' }
if ($cm -notmatch '\bcurl\b' -or $cm -notmatch '\bssl\b' -or $cm -notmatch '\bpthread\b') {
    throw 'curl/OpenSSL link dependencies are missing'
}
Write-Output 'v0.9 QR crash/GitHub download contract passed'
