# Build a fresh portable Windows folder. Write only its path to stdout so callers
# can pass it directly to finalize_friends_package.py.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$binaryDirectory = Join-Path $projectRoot 'out/build/src/Release'
$releaseDirectory = Join-Path $projectRoot 'out/releases'

function Copy-RuntimeDependencies {
    param(
        [string]$PackageDirectory,
        [string[]]$SearchDirectories,
        [string]$Dumpbin
    )

    $availableLibraries = @{}
    foreach ($directory in $SearchDirectories) {
        foreach ($file in Get-ChildItem -LiteralPath $directory -Filter '*.dll') {
            $availableLibraries[$file.Name] = $file.FullName
        }
    }

    $pending = New-Object 'System.Collections.Generic.Queue[string]'
    foreach ($executable in Get-ChildItem -LiteralPath $PackageDirectory -Filter '*.exe') {
        $pending.Enqueue($executable.FullName)
    }
    $included = @{}
    while ($pending.Count -gt 0) {
        $binary = $pending.Dequeue()
        $dependencies = & $Dumpbin /dependents $binary
        if ($LASTEXITCODE -ne 0) {
            throw "Could not inspect runtime dependencies: $binary"
        }
        foreach ($line in $dependencies) {
            if ($line -notmatch '^\s+([\w.-]+\.dll)\s*$') {
                continue
            }
            $name = $Matches[1]
            # Windows system libraries are resolved by the OS, not redistributed.
            if ($availableLibraries.ContainsKey($name) -and !$included.ContainsKey($name)) {
                $included[$name] = $true
                Copy-Item -LiteralPath $availableLibraries[$name] -Destination $PackageDirectory
                $pending.Enqueue($availableLibraries[$name])
            }
        }
    }
}

foreach ($name in @('blobby.exe', 'blobby-server.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $binaryDirectory $name))) {
        throw "Missing $name. Build the Release configuration first."
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio) {
    throw 'Visual Studio C++ tools were not found.'
}
$dumpbin = Get-ChildItem "$visualStudio/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" |
    Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
$runtimeDirectory = Get-ChildItem "$visualStudio/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT" -Directory |
    Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
if (!$dumpbin -or !$runtimeDirectory) {
    throw 'The x64 C++ runtime or dumpbin was not found.'
}

$stageName = 'stage-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$package = Join-Path $releaseDirectory "$stageName/BlobbyVolley2HD"
$dataDirectory = Join-Path $package 'data'
$licenseDirectory = Join-Path $package 'licenses'
New-Item -ItemType Directory -Force $dataDirectory, $licenseDirectory | Out-Null

foreach ($name in @('blobby.exe', 'blobby-server.exe')) {
    Copy-Item -LiteralPath (Join-Path $binaryDirectory $name) -Destination $package
}
foreach ($name in @('gfx.zip', 'backgrounds.zip', 'sounds.zip', 'scripts.zip', 'rules.zip')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "out/build/data/$name") -Destination $dataDirectory
}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'data') -File) {
    if ($file.Extension -in @('.xml', '.lua')) {
        Copy-Item -LiteralPath $file.FullName -Destination $dataDirectory
    }
}
# A distributed copy starts with project defaults, never the developer's settings.
Copy-Item -LiteralPath (Join-Path $projectRoot 'data/default-config.xml') -Destination (Join-Path $dataDirectory 'config.xml') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'data/default-inputconfig.xml') -Destination (Join-Path $dataDirectory 'inputconfig.xml') -Force
Copy-RuntimeDependencies -PackageDirectory $package -SearchDirectories @($binaryDirectory, $runtimeDirectory) -Dumpbin $dumpbin

foreach ($name in @('COPYING', 'AUTHORS')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $package
}
foreach ($name in @('sdl2', 'sdl2-image', 'physfs', 'libpng', 'zlib', 'libwebp')) {
    $notice = Join-Path $projectRoot "out/build/vcpkg_installed/x64-windows/share/$name/copyright"
    Copy-Item -LiteralPath $notice -Destination (Join-Path $licenseDirectory "$name.txt")
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'data/gfx/hd/menu-font-license.txt') -Destination (Join-Path $licenseDirectory 'Nunito-OFL.txt')

@('@echo off', 'cd /d "%~dp0"', 'start "" "%~dp0blobby.exe"') |
    Set-Content -LiteralPath (Join-Path $package 'Start-Blobby.cmd') -Encoding ascii
@(
    'Blobby Volley 2 HD - Windows 10/11, 64-bit, portable',
    '',
    'DE: ZIP komplett entpacken und Start-Blobby.cmd starten. Kein Installer erforderlich.',
    'Alle DLL-Dateien und den data-Ordner neben blobby.exe lassen.',
    '',
    'EN: Extract the entire ZIP and run Start-Blobby.cmd. No installer needed.',
    'Keep all DLLs and the data folder beside blobby.exe.',
    '',
    'Languages: German, English, Czech, Spanish, French, and Italian.',
    'Default background: strand1. Settings start with clean project defaults.',
    'Graphics archives contain HD artwork and the runtime Nunito TTF only.',
    '',
    'The game license is in COPYING. Third-party notices are in licenses/.',
    'When distributing a binary, make its corresponding GPL source available.'
) | Set-Content -LiteralPath (Join-Path $package 'README.txt') -Encoding utf8

Write-Output $package
