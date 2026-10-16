# Milestone 7: builds a packaged Windows Development build of Our Last Echo that can be copied to another PC or
# sent to a friend, and zips it. The editor must be closed (cooking reads and writes the same files).
# Usage:  powershell -File Scripts/package_game.ps1        (about 15-40 minutes the first time, faster after)
# Output: Packaged/Windows/        the game folder (run "Play Our Last Echo.exe", or OurLastEcho.exe)
#         Packaged/OurLastEcho-Win64.zip   the same, zipped for sending
# The EOS IDs come from Config/Windows/WindowsEngine.ini (git-ignored): a build made without it can't sign in.
param(
    [string]$EosSdk = "D:\EOS_SDK"   # for the EOS Bootstrapper (makes sure Epic Online Services, which the overlay needs, is installed)
)

$Project = Split-Path -Parent $PSScriptRoot
$Uproject = Join-Path $Project "OurLastEcho.uproject"
$Out = Join-Path $Project "Packaged"
$RunUAT = "D:\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { Write-Output "Close the editor first."; exit 1 }
if (-not (Test-Path (Join-Path $Project "Config\Windows\WindowsEngine.ini"))) { Write-Output "Config/Windows/WindowsEngine.ini (the EOS IDs) is missing: see Config/EOSCredentials.example.ini"; exit 1 }

& $RunUAT BuildCookRun "-project=$Uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore `
    -prereqs -archive "-archivedirectory=$Out" -utf8output -unattended
if ($LASTEXITCODE -ne 0) { Write-Output "PACKAGE FAILED (exit $LASTEXITCODE)"; exit $LASTEXITCODE }

$Game = Join-Path $Out "Windows"
# A launcher that installs/starts Epic Online Services first (the Epic overlay, for friends and invites, needs it)
$Tool = Join-Path $EosSdk "SDK\Tools\EOSBootstrapperTool.exe"
if (Test-Path $Tool) {
    & $Tool -o (Join-Path $Game "Play Our Last Echo.exe") -a "OurLastEcho.exe" -r true | Out-Null
}
# Instructions for whoever receives the build
@"
Our Last Echo (test build)

1. Unzip this whole folder anywhere (don't run it from inside the zip).
2. Double-click "Play Our Last Echo.exe" (or OurLastEcho.exe).
   - If Windows says it protected your PC: More info > Run anyway (the test build isn't signed).
   - If Smart App Control blocks it, the game can't run on this PC until Smart App Control is turned off
     (Windows Security > App & browser control).
   - The first start may install Microsoft runtime files and Epic Online Services.
3. Sign in with your Epic Games account when asked.
4. To join your friend: they host (Play > Host Game) and invite you, or read you the join code:
   choose Play > Join Game, type the code, and press Join with Code.
Problems? Send your friend the newest file in OurLastEcho\Saved\Logs\OnlineSessions.
"@ | Set-Content -Encoding utf8 (Join-Path $Game "READ ME FIRST.txt")

$Zip = Join-Path $Out "OurLastEcho-Win64.zip"
Remove-Item $Zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $Game "*") -DestinationPath $Zip -CompressionLevel Optimal
$size = [math]::Round((Get-Item $Zip).Length / 1MB)
Write-Output "PACKAGED: $Game"
Write-Output "ZIP: $Zip ($size MB)"
