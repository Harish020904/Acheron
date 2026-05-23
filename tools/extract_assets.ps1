param()

$ErrorActionPreference = "Stop"

$downloadsDir = "C:\Users\Harish\Downloads"
$assetsDir = "d:\projects\Acheron\assets"
$tempDir = "d:\projects\Acheron\temp_extract"

# Create directories
$dirs = @("fonts", "ui", "icons", "tiles", "buildings", "roads", "vehicles", "overlays", "particles", "shaders", "generated/svg")
foreach ($d in $dirs) {
    $path = Join-Path $assetsDir $d
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
}

if (-not (Test-Path $tempDir)) {
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Extract-Zip($zipName, $destFolder) {
    $zipPath = Join-Path $downloadsDir $zipName
    if (Test-Path $zipPath) {
        Write-Host "Extracting $zipName..."
        Expand-Archive -Path $zipPath -DestinationPath $destFolder -Force
    } else {
        Write-Host "Warning: $zipName not found."
    }
}

# 1. Orbitron (Fonts)
$orbitronTemp = Join-Path $tempDir "orbitron"
Extract-Zip "Orbitron.zip" $orbitronTemp
if (Test-Path $orbitronTemp) {
    Get-ChildItem -Path $orbitronTemp -Recurse -Filter "Orbitron-*.ttf" | ForEach-Object {
        $weight = $_.Name.Replace("Orbitron-", "").Replace(".ttf", "").ToLower()
        $newName = "orbitron_$weight.ttf"
        Copy-Item -Path $_.FullName -Destination (Join-Path $assetsDir "fonts\$newName") -Force
    }
}

# 2. Kenney UI Pack (Grey Double)
$uiTemp = Join-Path $tempDir "ui"
Extract-Zip "kenney_ui-pack.zip" $uiTemp
if (Test-Path $uiTemp) {
    $greyDoublePath = Join-Path $uiTemp "PNG\Grey\Double"
    if (Test-Path $greyDoublePath) {
        Get-ChildItem -Path $greyDoublePath -Filter "*.png" | ForEach-Object {
            $newName = "ui_grey_$($_.Name)"
            Copy-Item -Path $_.FullName -Destination (Join-Path $assetsDir "ui\$newName") -Force
        }
    }
}

# 3. Kenney Tiny Town (Tiles, Roads, Vehicles)
$tinyTownTemp = Join-Path $tempDir "tiny_town"
Extract-Zip "kenney_tiny-town.zip" $tinyTownTemp
if (Test-Path $tinyTownTemp) {
    # Tileset
    $tilesetPacked = Join-Path $tinyTownTemp "Tilemap\tilemap_packed.png"
    if (Test-Path $tilesetPacked) {
        Copy-Item -Path $tilesetPacked -Destination (Join-Path $assetsDir "tiles\tileset_tiny_town.png") -Force
    }
    # Vehicles (Tiles 0049 to 0055 approx, just grabbing them)
    $tilesPath = Join-Path $tinyTownTemp "Tiles"
    if (Test-Path $tilesPath) {
        Get-ChildItem -Path $tilesPath -Filter "tile_005[0-5].png" | ForEach-Object {
            $num = $_.Name.Replace("tile_00", "").Replace(".png", "")
            $newName = "vehicle_$num.png"
            Copy-Item -Path $_.FullName -Destination (Join-Path $assetsDir "vehicles\$newName") -Force
        }
    }
}

# 4. Platformer Art Buildings (Awnings, Vents)
$bldgTemp = Join-Path $tempDir "buildings"
Extract-Zip "kenney_platformer-art-buildings.zip" $bldgTemp
if (Test-Path $bldgTemp) {
    $tilesPath = Join-Path $bldgTemp "Tiles"
    if (Test-Path $tilesPath) {
        Get-ChildItem -Path $tilesPath -Include "awning*.png", "chimney*.png", "door*.png", "window*.png" -Recurse | ForEach-Object {
            $newName = "building_prop_$($_.Name.ToLower())"
            Copy-Item -Path $_.FullName -Destination (Join-Path $assetsDir "buildings\$newName") -Force
        }
    }
}

# 5. Cyberpunk City 2 (Neon tileset, Background)
$cyberTemp = Join-Path $tempDir "cyberpunk"
Extract-Zip "cyberpunk_city_2_files.zip" $cyberTemp
if (Test-Path $cyberTemp) {
    $tileset = Join-Path $cyberTemp "cyberpunk city 2 files\Environment\tileset.png"
    if (Test-Path $tileset) {
        Copy-Item -Path $tileset -Destination (Join-Path $assetsDir "tiles\tileset_cyberpunk.png") -Force
    }
    
    $lights = Join-Path $cyberTemp "cyberpunk city 2 files\Environment\props\lights"
    if (Test-Path $lights) {
        Get-ChildItem -Path $lights -Filter "*.png" | ForEach-Object {
            $newName = "building_glow_$($_.Name)"
            Copy-Item -Path $_.FullName -Destination (Join-Path $assetsDir "buildings\$newName") -Force
        }
    }
}

# 6. Input Prompts (1-bit pixel keys)
$inputTemp = Join-Path $tempDir "inputs"
Extract-Zip "kenney_input-prompts-pixel-1-bit.zip" $inputTemp
if (Test-Path $inputTemp) {
    $tilesPath = Join-Path $inputTemp "Tiles (Black)"
    if (Test-Path $tilesPath) {
        # Grab all just in case, or map specific ones.
        # We will grab all 0000-0080 which covers keyboard.
        Get-ChildItem -Path $tilesPath -Filter "*.png" | ForEach-Object {
            $num = $_.Name.Replace("tile_", "").Replace(".png", "")
            if ([int]$num -lt 100) {
                $newName = "icon_key_$num.png"
                Copy-Item -Path $_.FullName -Destination (Join-Path $assetsDir "icons\$newName") -Force
            }
        }
    }
}

# Cleanup Temp
Remove-Item -Path $tempDir -Recurse -Force | Out-Null

Write-Host "Asset extraction complete."
