param(
    [string]$BuildDir = "C:\chromium\src\out\BetaRelease",
    [string]$AppVersion = "Beta 1",
    [string]$OutName = "Whister-Beta1-Setup.exe"
)
$ErrorActionPreference = "Stop"
$Repo = $PSScriptRoot
$Stage = Join-Path $Repo "dist\stage"
$Icon = Join-Path $Repo "installer\whister.ico"

$Makensis = (Get-Command makensis -ErrorAction SilentlyContinue).Source
if (-not $Makensis) {
    foreach ($p in "C:\Program Files (x86)\NSIS\makensis.exe", "C:\Program Files\NSIS\makensis.exe") {
        if (Test-Path $p) { $Makensis = $p; break }
    }
}
if (-not $Makensis) { throw "NSIS not found. Install it with: winget install NSIS.NSIS" }
if (-not (Test-Path "$BuildDir\chrome.exe")) { throw "chrome.exe not found in $BuildDir. Build first." }

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null

# Runtime files only; build tools, debug runtimes and logs are left out.
$Files = @(
    "chrome.exe", "chrome.dll", "chrome_elf.dll", "chrome_proxy.exe", "chrome_pwa_launcher.exe",
    "chrome_wer.dll", "notification_helper.exe", "elevation_service.exe", "eventlog_provider.dll",
    "d3dcompiler_47.dll", "dxcompiler.dll", "libEGL.dll", "libGLESv2.dll",
    "vk_swiftshader.dll", "vk_swiftshader_icd.json", "vulkan-1.dll",
    "icudtl.dat", "v8_context_snapshot.bin", "snapshot_blob.bin",
    "chrome_100_percent.pak", "chrome_200_percent.pak", "resources.pak",
    "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_atomic_wait.dll", "vccorlib140.dll"
)
foreach ($f in $Files) {
    $src = Join-Path $BuildDir $f
    if (Test-Path $src) { Copy-Item $src $Stage -Force } else { Write-Host "skip (missing): $f" -ForegroundColor Yellow }
}
Get-ChildItem $BuildDir -Filter "*.manifest" -File | Copy-Item -Destination $Stage -Force
foreach ($d in "locales", "MEIPreload", "default_apps", "extensions", "IwaKeyDistribution", "resources", "hyphen-data", "angledata") {
    $src = Join-Path $BuildDir $d
    if (Test-Path $src) { Copy-Item $src (Join-Path $Stage $d) -Recurse -Force }
}

$Out = Join-Path $Repo "dist\$OutName"
& $Makensis /V2 "/DSTAGE=$Stage" "/DICON=$Icon" "/DOUTFILE=$Out" "/DAPPVERSION=$AppVersion" (Join-Path $Repo "installer\whister.nsi")
if ($LASTEXITCODE -ne 0) { throw "makensis failed ($LASTEXITCODE)" }

$Hash = (Get-FileHash $Out -Algorithm SHA256).Hash
"$Hash  $OutName" | Set-Content (Join-Path $Repo "dist\$OutName.sha256")
Write-Host ">>> Built $Out" -ForegroundColor Green
Write-Host ">>> SHA256 $Hash"
