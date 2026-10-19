param([ValidateSet('loader-test','combined-prototype','combined-polished','combined-expanded','combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')][string]$Mode='loader-test')
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
# Python 3.11+ with Pillow and lz4 (ISAAC_PYTHON overrides), and a WSL distro with
# devkitPro devkitA64 at /opt/devkitpro (ISAAC_WSL_DISTRO overrides; see docs/build-environment.md).
$taskPython=if($env:ISAAC_PYTHON){$env:ISAAC_PYTHON}else{(Get-Command python -All -ErrorAction SilentlyContinue | Where-Object {$_.Source -notlike '*WindowsApps*'} | Select-Object -First 1).Source}
if(-not $taskPython){throw 'Python not found: install Python 3.11+ or set ISAAC_PYTHON'}
$taskDistro=if($env:ISAAC_WSL_DISTRO){$env:ISAAC_WSL_DISTRO}else{'IsaacDevkit'}
& $taskPython (Join-Path $PSScriptRoot 'generate_build_profile.py')
if($LASTEXITCODE -ne 0){throw 'Build profile generation failed'}
& $taskPython (Join-Path $PSScriptRoot 'generate_boss_names.py')
if($LASTEXITCODE -ne 0){throw 'Boss label generation failed'}
& $taskPython (Join-Path $PSScriptRoot 'timemachine\generate_guards.py')
if($LASTEXITCODE -ne 0){throw 'Time Machine guard generation failed'}
if($Mode -in @('combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')){
    & $taskPython (Join-Path $PSScriptRoot 'debug_kit\generate_guards.py')
    if($LASTEXITCODE -ne 0){throw 'Debug kit guard generation failed'}
}
if($Mode -in @('combined-refined','combined-plus','combined-balance','combined-menu')){
    & $taskPython (Join-Path $PSScriptRoot 'item_menu\generate_catalogue.py')
    if($LASTEXITCODE -ne 0){throw 'Item menu catalogue generation failed'}
}
if($Mode -in @('combined-plus','combined-balance','combined-menu')){
    & $taskPython (Join-Path $PSScriptRoot 'iteration5\generate_guards.py')
    if($LASTEXITCODE -ne 0){throw 'Iteration 5 guard generation failed'}
}
if($Mode -eq 'combined-menu'){
    foreach($taskGenerator in @('generate_guards.py','generate_catalogue.py','generate_assets.py')){
        & $taskPython (Join-Path $PSScriptRoot ('menu_ui\'+$taskGenerator))
        if($LASTEXITCODE -ne 0){throw "Mod menu generator failed: $taskGenerator"}
    }
}
$taskFeatures=Join-Path $taskProject 'native\source\program\generated\features.hpp'
$taskDiagnostic=if($Mode -eq 'loader-test'){'true'}else{'false'}
$taskBanner=if($Mode -in @('combined-polished','combined-expanded','combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')){'false'}else{'true'}
$taskExtended=if($Mode -in @('combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')){'true'}else{'false'}
[IO.File]::WriteAllText($taskFeatures,"#pragma once`nnamespace isaac { inline constexpr bool DiagnosticOnly = $taskDiagnostic; inline constexpr bool ShowStatusBanner = $taskBanner; inline constexpr bool ExtendedFeatures = $taskExtended; }`n",[Text.UTF8Encoding]::new($false))
# Imported Git shell scripts may have Windows line endings.
Get-ChildItem -LiteralPath (Join-Path $taskProject 'native\misc\scripts') -Filter '*.sh' | ForEach-Object {
    [IO.File]::WriteAllText($_.FullName,[IO.File]::ReadAllText($_.FullName).Replace("`r`n","`n"),[Text.UTF8Encoding]::new($false))
}
& $taskPython (Join-Path $PSScriptRoot 'prepare_npdm.py') (Join-Path (Split-Path -Parent $taskProject) 'Isaac-dump\exefs\main.npdm') (Join-Path $taskProject 'native\deploy\main.npdm') --report (Join-Path $taskProject 'evidence\npdm-changes.json')
if($LASTEXITCODE -ne 0){throw 'Descriptor verification failed'}
# The project folder as seen from WSL is passed as an argument ($1), never as shell code.
$taskWslNative=(wsl -d $taskDistro --exec wslpath -a ($taskProject+'\native')).Trim()
if($taskWslNative -notmatch '^/[A-Za-z0-9_./ +-]+$'){throw "Unexpected WSL project path: $taskWslNative"}
wsl -d $taskDistro --exec /bin/bash -lc 'export DEVKITPRO=/opt/devkitpro; export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH; cd "$1" && make -j4' build-native $taskWslNative
if($LASTEXITCODE -ne 0){throw 'Switch compilation failed'}
& $taskPython (Join-Path $PSScriptRoot 'check_build.py') --mode $Mode
if($LASTEXITCODE -ne 0){throw 'Compiled file verification failed'}
$taskPackage=Join-Path $taskProject "dist\$Mode"
$taskExefs=Join-Path $taskPackage 'atmosphere\contents\010021C000B6A000\exefs'
New-Item -ItemType Directory -Force -Path $taskExefs | Out-Null
foreach($name in @('subsdk9','main.npdm')){
    Copy-Item -LiteralPath (Join-Path $taskProject "native\deploy\$name") -Destination (Join-Path $taskExefs $name)
}
Copy-Item -LiteralPath (Join-Path $taskProject "evidence\check-$Mode.json") -Destination (Join-Path $taskPackage 'build-check.json')
if($Mode -in @('combined-polished','combined-expanded','combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')){
    & $taskPython (Join-Path $PSScriptRoot 'package_ui.py') --mode $Mode
    if($LASTEXITCODE -ne 0){throw 'UI resource packaging failed'}
}
$taskReadme=if($Mode -eq 'combined-menu'){'docs\iteration-7-testing.md'}elseif($Mode -eq 'combined-balance'){'docs\iteration-6-testing.md'}elseif($Mode -eq 'combined-plus'){'docs\iteration-5-testing.md'}elseif($Mode -eq 'combined-refined'){'docs\iteration-4-testing.md'}elseif($Mode -eq 'combined-extended'){'docs\iteration-3-testing.md'}elseif($Mode -eq 'combined-expanded'){'docs\iteration-2-testing.md'}else{'docs\console-testing.md'}
Copy-Item -LiteralPath (Join-Path $taskProject $taskReadme) -Destination (Join-Path $taskPackage 'READ BEFORE TESTING.md')
if($Mode -in @('combined-expanded','combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')){
    $taskPackageDocs=Join-Path $taskPackage 'docs'
    New-Item -ItemType Directory -Force -Path $taskPackageDocs | Out-Null
    foreach($taskName in @('iteration-2.md','timemachine-port.md','planetarium-port.md')){
        Copy-Item -LiteralPath (Join-Path $taskProject ('docs\'+$taskName)) -Destination (Join-Path $taskPackageDocs $taskName)
    }
    Get-ChildItem -LiteralPath (Join-Path $taskProject 'docs') -Filter 'eid*.md' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $taskPackageDocs $_.Name)
    }
    if($Mode -in @('combined-extended','combined-refined','combined-plus','combined-balance','combined-menu')){
        foreach($taskName in @('iteration-3.md','debug-kit.md','specialist-port.md','item-visuals-port.md','instant-break-port.md')){
            Copy-Item -LiteralPath (Join-Path $taskProject ('docs\'+$taskName)) -Destination (Join-Path $taskPackageDocs $taskName)
        }
    }
}
if($Mode -in @('combined-refined','combined-plus','combined-balance','combined-menu')){
    foreach($taskName in @('iteration-4.md','iteration-4-testing.md','clean-map-port.md','item-menu.md','refined-installer.md')){
        Copy-Item -LiteralPath (Join-Path $taskProject ('docs\'+$taskName)) -Destination (Join-Path $taskPackageDocs $taskName)
    }
}
if($Mode -in @('combined-plus','combined-balance','combined-menu')){
    foreach($taskName in @('iteration-5.md','iteration-5-testing.md')){
        Copy-Item -LiteralPath (Join-Path $taskProject ('docs\'+$taskName)) -Destination (Join-Path $taskPackageDocs $taskName)
    }
}
if($Mode -in @('combined-balance','combined-menu')){
    foreach($taskName in @('iteration-6.md','iteration-6-testing.md','balance-changes.md')){
        Copy-Item -LiteralPath (Join-Path $taskProject ('docs\'+$taskName)) -Destination (Join-Path $taskPackageDocs $taskName)
    }
}
if($Mode -eq 'combined-menu'){
    foreach($taskName in @('iteration-7.md','iteration-7-testing.md','mod-menu.md')){
        Copy-Item -LiteralPath (Join-Path $taskProject ('docs\'+$taskName)) -Destination (Join-Path $taskPackageDocs $taskName)
    }
}
Write-Output "Verified local package: $taskPackage. Console testing still required."
