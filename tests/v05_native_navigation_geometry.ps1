$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath 'src/browser_menu.cpp' -Raw

$setStart = $source.IndexOf('void BrowserMenu::SetSearchVisible(bool visible)')
$setEnd = $source.IndexOf('void BrowserMenu::ToggleSearch()', $setStart)
$setSearch = $source.Substring($setStart, $setEnd - $setStart)
if ($setSearch -notmatch 'if \(target == m_searchVisible\) return;') {
    throw 'Repeated hidden-search updates still mutate active list geometry.'
}

$pushStart = $source.IndexOf('void BrowserMenu::PushPage(')
$pushEnd = $source.IndexOf('bool BrowserMenu::PopPage()', $pushStart)
$push = $source.Substring($pushStart, $pushEnd - $pushStart)
if ($push -notmatch 'if \(m_searchVisible\)\s+SetSearchVisible\(false\);') {
    throw 'An open search is not normalized before the parent starts its native transition.'
}

$commitStart = $source.IndexOf('void BrowserMenu::CommitLoad(')
$commitEnd = $source.IndexOf('intrusive_ptr<graph::Surface> BrowserMenu::Icon', $commitStart)
$commit = $source.Substring($commitStart, $commitEnd - $commitStart)
if ($commit -match 'navigation3D[\s\S]{0,100}Type_Reset') {
    throw 'Native foreground list still receives the redundant reset absent from the initial release.'
}

Write-Host 'v0.5 native navigation geometry regression passed'
