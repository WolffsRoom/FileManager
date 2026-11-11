$ErrorActionPreference = 'Stop'
$cmake = Get-Content -LiteralPath 'CMakeLists.txt' -Raw
$download = Get-Content -LiteralPath 'src/core/download.c' -Raw
$usb = Get-Content -LiteralPath 'src/usb.cpp' -Raw
if ($cmake -notmatch 'set\(VITA_VERSION\s+"00\.70"\)') { throw 'VPK version is not 0.70' }
if ($download -notmatch 'FileManager/0\.7') { throw 'Download user agent is not v0.7' }
if ($usb -notmatch '"0\.70"') { throw 'USB identity is not v0.70' }
foreach ($locale in Get-ChildItem 'resource/locale' -Filter '*.xml') {
    if ((Get-Content $locale.FullName -Raw) -notmatch 'File Manager 0\.7') {
        throw "About version missing from $($locale.Name)"
    }
}
Write-Host 'v0.7 version metadata passed'
