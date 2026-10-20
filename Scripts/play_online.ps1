# Starts Our Last Echo as a real game from the project, without packaging: a standalone game process (not Play In
# Editor), so it signs in with Epic and plays online over Epic Online Services like the packaged build. Content
# loads straight from the project, so new commits can be played as soon as they're pulled.
# Run it through "Play Online.bat" in the project folder. The editor doesn't need to be open.
#   -Engine "<engine folder>"   use this Unreal Engine install (otherwise found from the project's engine association)
#   -WhatIf                     only print what it would run
param(
    [string]$Engine = "",
    [int]$ResX = 1280,
    [int]$ResY = 720,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$Project = Split-Path -Parent $PSScriptRoot
$Uproject = Join-Path $Project "OurLastEcho.uproject"

function Stop-WithMessage($Message) {
    Write-Host ""
    Write-Host $Message -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

# ---- Find the engine
function Find-Engine {
    $Association = (Get-Content $Uproject -Raw | ConvertFrom-Json).EngineAssociation
    $Candidates = @()
    if ($Association) {
        # A source / custom build registered on this PC (a GUID)
        $Builds = Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
        if ($Builds -and $Builds.PSObject.Properties[$Association]) { $Candidates += $Builds.$Association }
        # An Epic Games Launcher install ("5.8")
        $Launcher = Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association" -ErrorAction SilentlyContinue
        if ($Launcher) { $Candidates += $Launcher.InstalledDirectory }
    }
    # The Epic Games Launcher's list of installed engines (UE_5.8 first: the project was made with Unreal Engine 5.8)
    $Installed = Join-Path $env:ProgramData "Epic\UnrealEngineLauncher\LauncherInstalled.dat"
    if (Test-Path $Installed) {
        $Engines = (Get-Content $Installed -Raw | ConvertFrom-Json).InstallationList | Where-Object { $_.AppName -like "UE_5.*" }
        $Candidates += ($Engines | Where-Object { $_.AppName -eq "UE_5.8" }).InstallLocation
        $Candidates += ($Engines | Sort-Object AppName -Descending).InstallLocation
    }
    # Then any launcher install of 5.8 and any registered build
    $Launcher = Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8" -ErrorAction SilentlyContinue
    if ($Launcher) { $Candidates += $Launcher.InstalledDirectory }
    $Builds = Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
    if ($Builds) { $Builds.PSObject.Properties | Where-Object { $_.Name -notlike "PS*" } | ForEach-Object { $Candidates += $_.Value } }
    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path (Join-Path $Candidate "Engine\Binaries\Win64\UnrealEditor.exe"))) { return $Candidate }
    }
    return $null
}

if (-not $Engine) { $Engine = Find-Engine }
if (-not $Engine -or -not (Test-Path (Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"))) {
    Stop-WithMessage ("Couldn't find Unreal Engine 5.8 on this PC.`n" +
        "Install it from the Epic Games Launcher, or right-click OurLastEcho.uproject > Switch Unreal Engine version,`n" +
        "or run:  powershell -File Scripts\play_online.ps1 -Engine `"<your engine folder, e.g. C:\Program Files\Epic Games\UE_5.8>`"")
}
$Editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"
Write-Host "Unreal Engine: $Engine"

# ---- Checks
if (-not (Test-Path (Join-Path $Project "Config\Windows\WindowsEngine.ini"))) {
    Stop-WithMessage ("Config\Windows\WindowsEngine.ini is missing: it holds the game's Epic Online Services IDs, and without it`n" +
        "you can't sign in. It isn't in git (the repo is public): get it privately from whoever runs the project.")
}
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like "OurLastEcho*Unreal Editor*" }) {
    Write-Host "Note: the editor is open. That's fine for playing, but close it first if the game code needs building." -ForegroundColor Yellow
}

# ---- Build the game code if this PC hasn't yet (Binaries/ isn't in git)
$Dll = Join-Path $Project "Binaries\Win64\UnrealEditor-OurLastEcho.dll"
if (-not (Test-Path $Dll)) {
    Write-Host "The game code isn't built on this PC yet: building it now (needs Visual Studio 2022 with the C++ game workload; a few minutes)..."
    $Build = Join-Path $Engine "Engine\Build\BatchFiles\Build.bat"
    $Command = "`"$Build`" OurLastEchoEditor Win64 Development `"-Project=$Uproject`" -WaitMutex"
    if ($WhatIf) { Write-Host "Would run: $Command" }
    else {
        & $Build OurLastEchoEditor Win64 Development "-Project=$Uproject" -WaitMutex
        if ($LASTEXITCODE -ne 0) { Stop-WithMessage "The build failed (see above). Is Visual Studio 2022 installed with 'Game development with C++'?" }
    }
}

# ---- Start the game
$Arguments = @("`"$Uproject`"", "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY")
if ($WhatIf) { Write-Host "Would run: `"$Editor`" $($Arguments -join ' ')"; exit 0 }
Write-Host "Starting Our Last Echo (the first start takes a while: it prepares shaders and content)..."
Start-Process $Editor -ArgumentList $Arguments
