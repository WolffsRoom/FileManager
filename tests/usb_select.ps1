$ErrorActionPreference = 'Stop'

$settings = Get-Content -Raw "$PSScriptRoot\..\src\settings.cpp"
$menu = Get-Content -Raw "$PSScriptRoot\..\src\settings_menu.cpp"
$usb = if (Test-Path "$PSScriptRoot\..\src\usb.cpp") { Get-Content -Raw "$PSScriptRoot\..\src\usb.cpp" } else { '' }
$browser = Get-Content -Raw "$PSScriptRoot\..\src\browser_menu.cpp"
$cmake = Get-Content -Raw "$PSScriptRoot\..\CMakeLists.txt"

if ($settings -notmatch 'usb_device=%d' -or $settings -notmatch 'usbDevice') { throw 'USB device preference is not persisted' }
if ($menu -notmatch 'Level_UsbDevice' -or $menu -notmatch 'return 4') { throw 'Settings does not expose four USB devices' }
foreach ($path in @('sdstor0:xmc-lp-ign-userext','sdstor0:gcd-lp-ign-gamero','sdstor0:gcd-lp-ign-entire','sdstor0:uma-pp-act-a')) {
    if ($usb -notmatch [regex]::Escape($path)) { throw "USB path mapping missing: $path" }
}
if ($browser -notmatch 'usb::Toggle') { throw 'Select does not activate the configured USB device' }
if ($cmake -notmatch 'modules/usbdevice/usbdevice.skprx module/usbdevice.skprx') { throw 'usbdevice.skprx is not packaged' }
if ($cmake -notmatch 'resources/umass.skprx module/umass.skprx') { throw 'umass.skprx is not packaged' }

'USB Select checks passed'
