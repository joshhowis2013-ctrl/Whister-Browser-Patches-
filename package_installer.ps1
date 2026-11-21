param(
    [string]$BuildDir = "C:\chromium\src\out\BetaRelease",
    [string]$AppVersion,
    [string]$OutName,
    [string]$UpdaterConfigPath = (Join-Path $PSScriptRoot "updater\WhisterUpdater\updater-config.json")
)
$ErrorActionPreference = "Stop"
$Repo = $PSScriptRoot
$Stage = Join-Path $Repo "dist\stage"
$Icon = Join-Path $Repo "installer\whister.ico"
$UpdaterProject = Join-Path $Repo "updater\WhisterUpdater\WhisterUpdater.csproj"
$UpdaterPublish = Join-Path $Repo "dist\updater-publish"

if ([string]::IsNullOrWhiteSpace($AppVersion)) {
    $ChromiumVersionFile = Join-Path (Split-Path (Split-Path $BuildDir -Parent) -Parent) "chrome\VERSION"
    if (-not (Test-Path $ChromiumVersionFile)) {
        throw "AppVersion was not supplied and Chromium version file was not found: $ChromiumVersionFile"
    }
    $VersionParts = @{}
    Get-Content $ChromiumVersionFile | ForEach-Object {
        if ($_ -match '^(MAJOR|MINOR|BUILD|PATCH)=(\d+)$') {
            $VersionParts[$matches[1]] = $matches[2]
        }
    }
    $AppVersion = "$($VersionParts.MAJOR).$($VersionParts.MINOR).$($VersionParts.BUILD).$($VersionParts.PATCH)"
}
if ($AppVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    throw "AppVersion must be a four-part numeric version, for example 158.0.8105.0."
}
if ([string]::IsNullOrWhiteSpace($OutName)) {
    $OutName = "WhisterSetup-$AppVersion.exe"
}

$Makensis = (Get-Command makensis -ErrorAction SilentlyContinue).Source
if (-not $Makensis) {
    foreach ($p in "C:\Program Files (x86)\NSIS\makensis.exe", "C:\Program Files\NSIS\makensis.exe") {
        if (Test-Path $p) { $Makensis = $p; break }
    }
}
if (-not $Makensis) { throw "NSIS not found. Install it with: winget install NSIS.NSIS" }
if (-not (Test-Path "$BuildDir\chrome.exe")) { throw "chrome.exe not found in $BuildDir. Build first." }
if (-not (Test-Path $UpdaterProject)) { throw "Updater project not found: $UpdaterProject" }
if (-not (Test-Path $UpdaterConfigPath)) { throw "Updater configuration not found: $UpdaterConfigPath" }

$RequiredFiles = @(
    "chrome.exe", "chrome.dll", "chrome_elf.dll", "icudtl.dat", "resources.pak",
    "chrome_100_percent.pak", "chrome_200_percent.pak", "v8_context_snapshot.bin",
    "snapshot_blob.bin", "locales\en-US.pak"
)
$MissingFiles = @($RequiredFiles | Where-Object { -not (Test-Path (Join-Path $BuildDir $_)) })
if ($MissingFiles.Count -gt 0) {
    throw "Required installer payload files are missing from ${BuildDir}:`n - $($MissingFiles -join "`n - ")"
}

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null
if (Test-Path $UpdaterPublish) { Remove-Item $UpdaterPublish -Recurse -Force }

& dotnet publish $UpdaterProject -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o $UpdaterPublish
if ($LASTEXITCODE -ne 0) { throw "Whister updater publish failed ($LASTEXITCODE)" }

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
    if (Test-Path $src) { Copy-Item $src $Stage -Force }
}
Get-ChildItem $BuildDir -Filter "*.manifest" -File | Copy-Item -Destination $Stage -Force
foreach ($d in "locales", "MEIPreload", "default_apps", "extensions", "IwaKeyDistribution", "resources", "hyphen-data", "angledata") {
    $src = Join-Path $BuildDir $d
    if (Test-Path $src) { Copy-Item $src (Join-Path $Stage $d) -Recurse -Force }
}
$UBlockBundle = Join-Path $Stage "resources\whister_ubo"
foreach ($requiredBundleFile in "manifest.json", "LICENSE.txt") {
    if (-not (Test-Path (Join-Path $UBlockBundle $requiredBundleFile))) {
        throw "uBlock Origin bundle is incomplete: $UBlockBundle. Build it with prepare_ublock.ps1 before packaging."
    }
}
Copy-Item (Join-Path $UpdaterPublish "WhisterUpdater.exe") $Stage -Force
$UpdaterConfig = Get-Content $UpdaterConfigPath -Raw
$UpdaterConfig.Replace("__APP_VERSION__", $AppVersion) |
    Set-Content (Join-Path $Stage "updater-config.json") -NoNewline

$Out = Join-Path $Repo "dist\$OutName"
& $Makensis /V2 "/DSTAGE=$Stage" "/DICON=$Icon" "/DOUTFILE=$Out" "/DAPPVERSION=$AppVersion" (Join-Path $Repo "installer\whister.nsi")
if ($LASTEXITCODE -ne 0) { throw "makensis failed ($LASTEXITCODE)" }

$Hash = (Get-FileHash $Out -Algorithm SHA256).Hash
"$Hash  $OutName" | Set-Content (Join-Path $Repo "dist\$OutName.sha256")
Write-Host ">>> Built $Out" -ForegroundColor Green
Write-Host ">>> Version $AppVersion" -ForegroundColor Green
Write-Host ">>> SHA256 $Hash"
