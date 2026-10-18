# =========================================================================
# WHISTER BROWSER AUTOMATED MULTI-BRANCH BUILDER & CONFIG INJECTOR
# =========================================================================
param (
    [Parameter(Mandatory=$true)]
    [ValidateSet("stable", "beta")]
    [string]$Channel,
    [switch]$UseStockNewTab
)

# 1. CONFIGURE DIRECTORIES AND MILESTONE VERSIONS
$ChromiumSrcDir = "C:\chromium\src"
$MyRepoDir = "C:\whister-browser"

if ($Channel -eq "stable") {
    $TargetChromiumVersion = "refs/tags/154.0.8037.98"
    $BuildOutPath = "out\StableRelease"
    $ConfigFile = "$MyRepoDir\config\stable_args.gn"
} else {
    $TargetChromiumVersion = "refs/tags/158.0.8105.0"
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
git fetch origin "$TargetChromiumVersion`:$TargetChromiumVersion" --no-tags
git checkout $TargetChromiumVersion
gclient sync --with_branch_heads --with_tags --jobs=4 --force --no-history


# 4. APPLY LAYERED PATCH MODULES
Write-Host ">>> Merging shared features..." -ForegroundColor Cyan
$EnabledCommonPatchNames = @(
    "custom_omnibox_placeholder.patch",
    "degoogle_settings_interface.patch",
    "theme_native_top_bar.patch",
    "inject_whister_theme.patch",
    "whister_settings_page.patch"
)
if (-not $UseStockNewTab) {
    $EnabledCommonPatchNames += "whister_new_tab.patch"
} else {
    Write-Host ">>> Using Chromium's stock new-tab page as fallback." -ForegroundColor Yellow
}
$CommonPatches = foreach ($PatchName in $EnabledCommonPatchNames) {
    $PatchPath = Join-Path "$MyRepoDir\patches\common" $PatchName
    if (-not (Test-Path $PatchPath)) {
        throw "Required compatible patch is missing: $PatchPath"
    }
    Get-Item $PatchPath
}
$DeferredCommonPatches = Get-ChildItem -Path "$MyRepoDir\patches\common\*.patch" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notin $EnabledCommonPatchNames }
if ($DeferredCommonPatches) {
    $DeferredNames = ($DeferredCommonPatches.Name -join ", ")
    Write-Host ">>> Deferring unported patches: $DeferredNames" -ForegroundColor Yellow
}
$ChannelPatches = Get-ChildItem -Path "$MyRepoDir\patches\$Channel\*.patch" -ErrorAction SilentlyContinue

# Layer A: Shared Code Base Modifications
if ($CommonPatches) {
    foreach ($Patch in $CommonPatches) {
        Write-Host "Applying Base: $($Patch.Name)" -ForegroundColor Green
        git apply --unidiff-zero --recount --ignore-space-change "$($Patch.FullName)"
        if ($LASTEXITCODE -ne 0) {
            throw "Patch $($Patch.Name) failed to apply. Stopping so an incomplete Whister build is not produced."
        }
    }
}

# Layer B: Unique Channel Configurations
if ($ChannelPatches) {
    foreach ($Patch in $ChannelPatches) {
        Write-Host "Applying $Channel Edge: $($Patch.Name)" -ForegroundColor Green
        git apply --unidiff-zero --recount --ignore-space-change "$($Patch.FullName)"
        if ($LASTEXITCODE -ne 0) {
            throw "Patch $($Patch.Name) failed to apply. Stopping so an incomplete Whister build is not produced."
        }
    }
}
# Layer C: Custom Brand Image Asset Overlays (PNG + ICO)
$CustomAssetsDir = "$MyRepoDir\branding\common\Assets"
$GoogleThemeDir = "$ChromiumSrcDir\chrome\app\theme\chromium"

if (Test-Path $CustomAssetsDir) {
    Write-Host ">>> Overwriting default logo assets and Windows binaries with Whister branding graphics..." -ForegroundColor Green
    # Copy all PNGs and the Windows .ico file directly over the defaults
    Copy-Item -Path "$CustomAssetsDir\*.*" -Destination $GoogleThemeDir -Force
}

$WhisterNewTabSrc = "$MyRepoDir\branding\common\new_tab_page"
$WhisterNewTabDest = "$ChromiumSrcDir\chrome\browser\resources\whister_new_tab"
New-Item -ItemType Directory -Path $WhisterNewTabDest -Force | Out-Null
foreach ($FileName in @("settings.html", "settings.css", "settings.js", "setup.html", "index.html", "style.css", "app.js")) {
    $SourceFile = Join-Path $WhisterNewTabSrc $FileName
    if (-not (Test-Path $SourceFile)) {
        throw "Required Whister settings asset is missing: $SourceFile"
    }
    Copy-Item -Path $SourceFile -Destination $WhisterNewTabDest -Force
}

if (-not $UseStockNewTab) {
    Write-Host ">>> Whister new-tab UI staged for chrome://newtab." -ForegroundColor Green
}

# Copy new Whister source files (settings WebUI) into the tree.
Copy-Item -Path "$MyRepoDir\overlay\*" -Destination $ChromiumSrcDir -Recurse -Force

# 5. DYNAMICALLY CREATE OUTPUT FOLDER AND INJECT THE ARGS FILE
Write-Host ">>> Injecting configuration args profile..." -ForegroundColor Cyan
$FullOutDirectory = "$ChromiumSrcDir\$BuildOutPath"

if (-not (Test-Path $FullOutDirectory)) {
    New-Item -ItemType Directory -Path $FullOutDirectory | Out-Null
}
$ExtensionId = "ddkjiahejlhfcafbddmgiahcphecmpfh"
$ExtensionDestDir = "$ChromiumSrcDir\$BuildOutPath\extensions"
$LegacyExtensionManifests = @(
    "$ChromiumSrcDir\$BuildOutPath\default_apps\cjpalhdlnbpafiamejdnhcphjbkeiagm.json",
    "$ChromiumSrcDir\$BuildOutPath\default_apps\$ExtensionId.json",
    "$ExtensionDestDir\cjpalhdlnbpafiamejdnhcphjbkeiagm.json",
    "$ExtensionDestDir\$ExtensionId.json"
)

foreach ($LegacyManifest in $LegacyExtensionManifests) {
    if (Test-Path $LegacyManifest) {
        Remove-Item $LegacyManifest -Force
    }
}

# Copy the corresponding .gn file directly into the compilation target root
Copy-Item -Path $ConfigFile -Destination "$FullOutDirectory\args.gn" -Force
Write-Host ">>> Configurations successfully applied to $BuildOutPath\args.gn" -ForegroundColor Green

# 6. GENERATE BUILD TARGET ARCHITECTURE FILES AND COMPILE
Write-Host ">>> Running GN Build File Generator..." -ForegroundColor Cyan
cmd /c "gn gen $BuildOutPath"

Write-Host ">>> Launching Autoninja compiler..." -ForegroundColor Cyan
cmd /c "autoninja -C $BuildOutPath mini_installer"

Write-Host ">>> Compilation finished! Check $BuildOutPath\mini_installer.exe" -ForegroundColor Green
