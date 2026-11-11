$ErrorActionPreference = 'Stop'
$settingsH = Get-Content 'src/settings.h' -Raw
$settingsCpp = Get-Content 'src/settings.cpp' -Raw
$settingsMenu = Get-Content 'src/settings_menu.cpp' -Raw
$browser = Get-Content 'src/browser_menu.cpp' -Raw
$preview = Get-Content 'src/preview_menu.cpp' -Raw
$music = Get-Content 'src/music_menu.cpp' -Raw
$rco = [xml](Get-Content 'resource/file_manager_plugin.xml' -Raw)

if ($settingsH -notmatch 'int32_t previewOpacity' -or $settingsH -notmatch 'int32_t showDetails' -or $settingsH -notmatch 'float PreviewOpacity\(\)') { throw 'Preview/detail settings contract missing' }
if ($settingsCpp -notmatch 'preview_opacity=' -or $settingsCpp -notmatch 'show_details=' -or $settingsCpp -notmatch 'v < 25 \|\| v > 90') { throw 'Preview/detail settings are not persisted with safe limits' }
if ($settingsCpp -notmatch '0\.70f' -or $settingsCpp -notmatch 'showDetails = v') { throw 'Preview opacity/default detail behavior missing' }

$page = $rco.resource.pagetable.page | Where-Object id -eq 'page_settings'
$slider = $page.progressbar_touch | Where-Object id -eq 'progressbar_preview_opacity_page_settings'
if (-not $slider) { throw 'Settings does not use a native touch progress bar for opacity' }
if (-not ($page.text | Where-Object id -eq 'text_preview_opacity_page_settings')) { throw 'Opacity percentage label missing' }
if ($settingsMenu -notmatch 'CB_PROGRESSBAR_CHANGED' -or $settingsMenu -notmatch 'msg_set_show_details') { throw 'Opacity/details Settings controls are not wired' }

if ($preview -notmatch 'settings::PreviewOpacity\(\)') { throw 'Image preview does not use configured opacity' }
if ($music -notmatch '0\.92f') { throw 'Music player background is not consistently opaque' }

if ($browser -notmatch 'FileDetails\(const fs::Entry \*entry\)' -or $browser -notmatch 'settings::Get\(\)\.showDetails') { throw 'Optional file detail formatter missing' }
if ($browser -notmatch '%s - %02u/%02u/%04u - %02u:%02u') { throw 'File detail format is not TYPE - date - modification time' }
if ($browser -notmatch 'if \(entry->isDir \|\| !settings::Get\(\)\.showDetails\) return paf::string\(\);') { throw 'Folders or disabled details still receive metadata' }

'v0.6 preview transparency and file details passed'
