# =========================================================================
# WHISTER BROWSER AUTOMATED MULTI-BRANCH BUILDER & CONFIG INJECTOR
# =========================================================================
param (
    [Parameter(Mandatory=$true)]
    [ValidateSet("stable", "beta")]
    [string]$Channel
)

# 1. CONFIGURE DIRECTORIES AND MILESTONE VERSIONS
$ChromiumSrcDir = "C:\chromium\src"
$MyRepoDir = "C:\whister-browser"

if ($Channel -eq "stable") {
    $TargetChromiumVersion = "refs/tags/152.0.7977.65"
    $BuildOutPath = "out\StableRelease"
    $ConfigFile = "$MyRepoDir\config\stable_args.gn"
} else {
    $TargetChromiumVersion = "refs/tags/155.0.8044.0"
    $BuildOutPath = "out\BetaRelease"
    $ConfigFile = "$MyRepoDir\config\beta_args.gn"
}

Write-Host ">>> Initiating Whister Browser [$Channel] Deployment Sequence..." -ForegroundColor Cyan

# Ensure core repository exists
if (-not (Test-Path $ChromiumSrcDir)) {
    Write-Error "Core engine repository missing. Run your 'fetch chromium' process first."
    Exit
}

# 2. SANITISE CHROMIUM REPOSITORY WORKSPACE
Set-Location $ChromiumSrcDir
Write-Host ">>> Wiping previous channel compilation artifacts..." -ForegroundColor Yellow
git reset --hard
git clean -fd

# 3. SWAP CORE ENGINE HEAD TAG
Write-Host ">>> Switching engine branch to $TargetChromiumVersion..." -ForegroundColor Cyan
git fetch origin --tags
git checkout $TargetChromiumVersion
gclient sync --with_branch_heads --with_tags --jobs=4 --force --no-history


# 4. APPLY LAYERED PATCH MODULES
Write-Host ">>> Merging shared features..." -ForegroundColor Cyan
$CommonPatches = Get-ChildItem -Path "$MyRepoDir\patches\common\*.patch" -ErrorAction SilentlyContinue
$ChannelPatches = Get-ChildItem -Path "$MyRepoDir\patches\$Channel\*.patch" -ErrorAction SilentlyContinue

# Layer A: Shared Code Base Modifications
if ($CommonPatches) {
    foreach ($Patch in $CommonPatches) {
        Write-Host "Applying Base: $($Patch.Name)" -ForegroundColor Green
        # Adding --3way allows Git to automatically resolve microscopic formatting anomalies
        git apply --3way "$($Patch.FullName)"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Warning: Patch $($Patch.Name) failed to merge cleanly. Skipping to avoid build locks." -ForegroundColor Yellow
        }
    }
}

# Layer B: Unique Channel Configurations
if ($ChannelPatches) {
    foreach ($Patch in $ChannelPatches) {
        Write-Host "Applying $Channel Edge: $($Patch.Name)" -ForegroundColor Green
        git apply --3way "$($Patch.FullName)"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Warning: Patch $($Patch.Name) failed to merge cleanly. Skipping to avoid build locks." -ForegroundColor Yellow
        }
    }
}
# Layer C: Custom Brand Image Asset Overlays (PNG + ICO)
$CustomAssetsDir = "$MyRepoDir\branding\common\assets"
$GoogleThemeDir = "$ChromiumSrcDir\chrome\app\theme\chromium"

if (Test-Path $CustomAssetsDir) {
    Write-Host ">>> Overwriting default logo assets and Windows binaries with Whister branding graphics..." -ForegroundColor Green
    # Copy all PNGs and the Windows .ico file directly over the defaults
    Copy-Item -Path "$CustomAssetsDir\*.*" -Destination $GoogleThemeDir -Force
}

# 5. DYNAMICALLY CREATE OUTPUT FOLDER AND INJECT THE ARGS FILE
Write-Host ">>> Injecting configuration args profile..." -ForegroundColor Cyan
$FullOutDirectory = "$ChromiumSrcDir\$BuildOutPath"

if (-not (Test-Path $FullOutDirectory)) {
    New-Item -ItemType Directory -Path $FullOutDirectory | Out-Null
}
# Inject Native uBlock Origin policy configurations
$ExtensionSrc = "$MyRepoDir\branding\common\extensions\cjpalhdlnbpafiamejdnhcphjbkeiagm.json"
$ExtensionDestDir = "$ChromiumSrcDir\$BuildOutPath\default_apps"

if (-not (Test-Path $ExtensionDestDir)) {
    New-Item -ItemType Directory -Path $ExtensionDestDir | Out-Null
}

Copy-Item -Path $ExtensionSrc -Destination $ExtensionDestDir -Force
Write-Host ">>> Built-in uBlock Origin profile staged for deployment." -ForegroundColor Green

# Copy the corresponding .gn file directly into the compilation target root
Copy-Item -Path $ConfigFile -Destination "$FullOutDirectory\args.gn" -Force
Write-Host ">>> Configurations successfully applied to $BuildOutPath\args.gn" -ForegroundColor Green

# 6. GENERATE BUILD TARGET ARCHITECTURE FILES AND COMPILE
Write-Host ">>> Running GN Build File Generator..." -ForegroundColor Cyan
cmd /c "gn gen $BuildOutPath"

Write-Host ">>> Launching Autoninja compiler..." -ForegroundColor Cyan
cmd /c "autoninja -C $BuildOutPath mini_installer"

Write-Host ">>> Compilation finished! Check $BuildOutPath\mini_installer.exe" -ForegroundColor Green

