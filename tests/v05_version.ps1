$ErrorActionPreference = 'Stop'
$cmake = Get-Content -LiteralPath 'CMakeLists.txt' -Raw
$download = Get-Content -LiteralPath 'src/core/download.c' -Raw
$usb = Get-Content -LiteralPath 'src/usb.cpp' -Raw
if ($cmake -notmatch 'set\(VITA_VERSION\s+"00\.50"\)') { throw 'VPK version is not 0.50' }
if ($download -notmatch 'FileManager/0\.5-test') { throw 'Download user agent is not v0.5-test' }
if ($usb -notmatch '"0\.50"') { throw 'USB identity is not v0.50' }
foreach ($locale in Get-ChildItem 'resource/locale' -Filter '*.xml') {
    if ((Get-Content $locale.FullName -Raw) -notmatch 'File Manager 0\.5 Test') {
        throw "About version missing from $($locale.Name)"
    }
}
Write-Host 'v0.5 version metadata passed'
