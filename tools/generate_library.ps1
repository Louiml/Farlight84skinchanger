$ErrorActionPreference = 'Stop'
$api = 'https://farlight-84.fandom.com/api.php'

$roster = @{
    heroes = @(
        @{ name = 'Beau'; group = 'Attack' }, @{ name = 'Ducksyde'; group = 'Attack' },
        @{ name = 'Lucinda'; group = 'Attack' }, @{ name = 'Maggie'; group = 'Attack' },
        @{ name = 'MKR'; group = 'Attack' }, @{ name = 'Syfer'; group = 'Attack' },
        @{ name = 'Captain'; group = 'Defense' }, @{ name = 'Ember'; group = 'Defense' },
        @{ name = 'Watchman'; group = 'Defense' }, @{ name = 'Maychelle'; group = 'Scout' },
        @{ name = 'Phantom'; group = 'Scout' }, @{ name = 'Yong'; group = 'Scout' },
        @{ name = 'Ceanna'; group = 'Support' }, @{ name = 'Momoi'; group = 'Support' },
        @{ name = 'Sunil'; group = 'Support' }
    )
    weapons = @(
        @{ name = 'Invader'; group = 'Assault Rifle' }, @{ name = 'Generator'; group = 'Assault Rifle' },
        @{ name = 'M4'; group = 'Assault Rifle' }, @{ name = 'AK77'; group = 'Assault Rifle' },
        @{ name = 'VSS'; group = 'Assault Rifle' }, @{ name = 'Jupiter-6'; group = 'Assault Rifle' },
        @{ name = 'Porcupine'; group = 'Assault Rifle' }, @{ name = 'Mad Rat'; group = 'Assault Rifle' },
        @{ name = 'MF18'; group = 'SMG' }, @{ name = 'UMP99'; group = 'SMG' },
        @{ name = 'UZI'; group = 'SMG' }, @{ name = 'White Dwarf'; group = 'SMG' },
        @{ name = 'Mad Rabbit'; group = 'SMG' }, @{ name = 'Fanatic'; group = 'Shotgun' },
        @{ name = 'Hound'; group = 'Shotgun' }, @{ name = 'Ancient Star'; group = 'Shotgun' },
        @{ name = 'Defender'; group = 'Sniper Rifle' }, @{ name = 'Bar-95'; group = 'Sniper Rifle' },
        @{ name = 'Stellar Wind'; group = 'Sniper Rifle' }, @{ name = 'Rhino'; group = 'Other' },
        @{ name = 'MG-7'; group = 'Other' }
    )
    vehicles = @(
        @{ name = 'Flamethrower'; group = 'Wheeled' }, @{ name = 'Mobile Turret'; group = 'Wheeled' },
        @{ name = 'Blazing Infantry'; group = 'Wheeled' }, @{ name = 'Single-Pilot Mecha'; group = 'Bionic' },
        @{ name = 'War Spider'; group = 'Bionic' }, @{ name = '4-Legged-Lizard'; group = 'Bionic' },
        @{ name = 'Rocket Tarantula'; group = 'Bionic' }, @{ name = 'Hoverbike'; group = 'Hover' },
        @{ name = 'Wasteland Hovercraft'; group = 'Hover' }, @{ name = 'Air Beast'; group = 'Hover' },
        @{ name = 'Hovercar'; group = 'Hover' }, @{ name = 'Gunboat'; group = 'Hover' },
        @{ name = 'War Falcon'; group = 'Hover' }
    )
}

$rosterLookup = @{}
foreach ($cat in 'heroes', 'weapons', 'vehicles') {
    foreach ($item in $roster[$cat]) {
        $norm = ($item.name.ToLower() -replace '[\.\-\s]', '')
        $rosterLookup[$norm] = $cat
    }
}

$skins = @{ heroes = @{}; weapons = @{}; vehicles = @{} }
$unmatched = New-Object System.Collections.Generic.HashSet[string]

function Add-Skin($cat, $itemName, $skinName) {
    $itemName = $itemName.Trim().TrimEnd('.').Trim()
    $skinName = ($skinName -replace '[\r\n\*#]+', '').Trim().TrimEnd('.', '!', ';', ',').Trim()
    if ($itemName.Length -lt 1 -or $skinName.Length -lt 1) { return }
    $key = ($itemName.ToLower() -replace '[\.\-\s]', '')
    if (-not $rosterLookup.ContainsKey($key)) { [void]$unmatched.Add($itemName); return }
    $realCat = $rosterLookup[$key]
    $map = $skins[$realCat]
    $realName = $null
    foreach ($candidate in $roster[$realCat]) {
        if ((($candidate.name.ToLower()) -replace '[\.\-\s]', '') -eq $key) { $realName = $candidate.name; break }
    }
    if (-not $realName) { [void]$unmatched.Add($itemName); return }
    if (-not $map.ContainsKey($realName)) { $map[$realName] = New-Object System.Collections.Generic.HashSet[string] }
    [void]$map[$realName].Add($skinName)
}

$list = Invoke-WebRequest -Uri "${api}?action=query&list=allpages&apprefix=Patch%20Notes%2F&aplimit=500&format=json" -UseBasicParsing -TimeoutSec 30
$titles = [regex]::Matches($list.Content, '"title":"(Patch Notes/[^"]+)"') | ForEach-Object { $_.Groups[1].Value }
"patch note pages: $($titles.Count)"

$pairCount = 0
foreach ($title in $titles) {
    $encoded = [uri]::EscapeDataString($title)
    try {
        $resp = Invoke-WebRequest -Uri "${api}?action=parse&page=$encoded&format=json&prop=wikitext" -UseBasicParsing -TimeoutSec 30
        $j = $resp.Content | ConvertFrom-Json
        $wt = $j.parse.wikitext.'*'
    } catch {
        "FETCH FAILED: $title"
        continue
    }
    $rawLines = $wt -split "`n"
    foreach ($raw in $rawLines) {
        $line = ($raw -replace "`r", '').Trim().TrimStart('*', '#', ':').Trim()
        if ($line -match '^(?:SSR|SR|S|R) (Hero|Weapon|Vehicle) Skin (.+?) - (.+)$') {
            Add-Skin $matches[1] $matches[2] $matches[3]
            $pairCount++
        }
        elseif ($line -match '^(?:SSR|SR|S|R) Vehicle ([^-]+) Skin - (.+)$') {
            Add-Skin 'Vehicle' $matches[1] $matches[2]
            $pairCount++
        }
    }
    Start-Sleep -Milliseconds 150
}

"skin pairs extracted: $pairCount"

$manualPath = Join-Path $PSScriptRoot 'manual_skins.json'
$manualCount = 0
if (Test-Path $manualPath) {
    $manual = Get-Content $manualPath -Raw | ConvertFrom-Json
    foreach ($itemProp in $manual.PSObject.Properties) {
        $itemName = $itemProp.Name
        $key = ($itemName.ToLower() -replace '[\.\-\s]', '')
        if (-not $rosterLookup.ContainsKey($key)) { "manual item not in roster (skipped): $itemName"; continue }
        $realCat = $rosterLookup[$key]
        $realName = $null
        foreach ($candidate in $roster[$realCat]) {
            if ((($candidate.name.ToLower()) -replace '[\.\-\s]', '') -eq $key) { $realName = $candidate.name; break }
        }
        if (-not $realName) { continue }
        $map = $skins[$realCat]
        if (-not $map.ContainsKey($realName)) { $map[$realName] = New-Object System.Collections.Generic.HashSet[string] }
        foreach ($skinName in @($itemProp.Value)) {
            $s = "$skinName".Trim()
            if ($s -and $map[$realName].Add($s)) { $manualCount++ }
        }
    }
    "manual skins merged: $manualCount"
}

$perCategory = @()
foreach ($cat in 'heroes', 'weapons', 'vehicles') {
    $entries = @()
    $total = 0
    foreach ($item in $roster[$cat]) {
        $itemSkins = @()
        if ($skins[$cat].ContainsKey($item.name)) {
            $itemSkins = @($skins[$cat][$item.name] | Sort-Object)
            $total += $itemSkins.Count
        }
        $entry = [ordered]@{ name = $item.name; group = $item.group; skins = $itemSkins }
        $entries += $entry
    }
    "$cat : $total skins across $(@($skins[$cat].Keys).Count) items"
    $perCategory += @{ category = $cat; total = $total }
    Set-Variable -Name "built_$cat" -Value $entries
}

if ($unmatched.Count -gt 0) {
    "unmatched items (skipped): $($unmatched.Count)"
    $unmatched | Sort-Object | Select-Object -First 15
}

$library = @{
    heroes = (Get-Variable -Name 'built_heroes').Value
    weapons = (Get-Variable -Name 'built_weapons').Value
    vehicles = (Get-Variable -Name 'built_vehicles').Value
}
$json = $library | ConvertTo-Json -Depth 6
$outPath = Join-Path $PSScriptRoot '..\gui\library_data.h'
$header = "#pragma once`r`n`r`nstatic const char* kLibraryJson = R`"json($json)json`";`r`n"
[System.IO.File]::WriteAllText($outPath, $header, (New-Object System.Text.UTF8Encoding($false)))
"wrote: $outPath ($([math]::Round((Get-Item $outPath).Length/1KB,1)) KB)"
