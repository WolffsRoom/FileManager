$ErrorActionPreference = 'Stop'
$cmake = Get-Content -LiteralPath 'CMakeLists.txt' -Raw
$download = Get-Content -LiteralPath 'src/core/download.c' -Raw
$usb = Get-Content -LiteralPath 'src/usb.cpp' -Raw
if ($cmake -notmatch 'set\(VITA_VERSION\s+"00\.60"\)') { throw 'VPK version is not 0.60' }
if ($download -notmatch 'FileManager/0\.6-beta') { throw 'Download user agent is not v0.6-beta' }
if ($usb -notmatch '"0\.60"') { throw 'USB identity is not v0.60' }
foreach ($locale in Get-ChildItem 'resource/locale' -Filter '*.xml') {
    if ((Get-Content $locale.FullName -Raw) -notmatch 'File Manager 0\.6 Beta') {
        throw "About version missing from $($locale.Name)"
    }
}
Write-Host 'v0.6-beta version metadata passed'
