param(
    [Parameter(Mandatory = $true)]
    [string]$ChromiumBuildDir
)

$ErrorActionPreference = "Stop"
$UBlockRoot = Join-Path $PSScriptRoot "third_party\uBlockOrigin"
$AssetsRoot = Join-Path $UBlockRoot "dist\build\uAssets"
$ExtensionSource = Join-Path $UBlockRoot "dist\build\uBlock0.chromium"
$ExtensionDestination = Join-Path $ChromiumBuildDir "resources\whister_ubo"
$ExpectedExtensionId = "hoganoibjngfmjnnniladdmgmkkigmaj"
$Git = (Get-Command git -ErrorAction Stop).Source
$Bash = "C:\Program Files\Git\bin\bash.exe"

if (-not (Test-Path (Join-Path $UBlockRoot "tools\make-chromium.sh"))) {
    throw "The local uBlock Origin fork is missing: $UBlockRoot"
}
if (-not (Test-Path $Bash)) {
    throw "Git Bash is required to build the uBlock Origin extension: $Bash"
}

foreach ($asset in @(
    @{ Branch = "master"; Directory = "main" },
    @{ Branch = "gh-pages"; Directory = "prod" }
)) {
    $assetPath = Join-Path $AssetsRoot $asset.Directory
    if (-not (Test-Path $assetPath)) {
        New-Item -ItemType Directory -Path $AssetsRoot -Force | Out-Null
        & $Git clone --depth 1 --branch $asset.Branch `
            "https://github.com/uBlockOrigin/uAssets.git" $assetPath
        if ($LASTEXITCODE -ne 0) {
            throw "Could not fetch the uBlock Origin $($asset.Branch) filter assets."
        }
    }
}

Push-Location $UBlockRoot
try {
    & $Bash "./tools/make-chromium.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "The uBlock Origin Chromium extension build failed ($LASTEXITCODE)."
    }
} finally {
    Pop-Location
}

$ManifestPath = Join-Path $ExtensionSource "manifest.json"
if (-not (Test-Path $ManifestPath)) {
    throw "The uBlock Origin build did not produce a manifest: $ManifestPath"
}
$Manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json
if (-not $Manifest.key) {
    throw "The uBlock Origin manifest must have a stable public key."
}
$PublicKey = [Convert]::FromBase64String($Manifest.key)
$sha256 = [System.Security.Cryptography.SHA256]::Create(); try { $Hash = $sha256.ComputeHash($PublicKey) } finally { $sha256.Dispose() }
$ExtensionId = -join ($Hash[0..15] | ForEach-Object {
    $byte = $_
    [char]([int][char]'a' + ($byte -shr 4))
    [char]([int][char]'a' + ($byte -band 15))
})
if ($ExtensionId -ne $ExpectedExtensionId) {
    throw "uBlock Origin ID mismatch: expected $ExpectedExtensionId, got $ExtensionId."
}

if (Test-Path $ExtensionDestination) {
    Remove-Item -LiteralPath $ExtensionDestination -Recurse -Force
}
New-Item -ItemType Directory -Path $ExtensionDestination -Force | Out-Null
Copy-Item -Path (Join-Path $ExtensionSource "*") `
    -Destination $ExtensionDestination -Recurse -Force
$StagedManifest = Join-Path $ExtensionDestination "manifest.json"
if (-not (Test-Path $StagedManifest)) {
    throw "The uBlock Origin manifest was not staged."
}
$StagedManifestData = Get-Content $StagedManifest -Raw | ConvertFrom-Json
$StagedManifestData.name = "uBlock Origin"
$StagedManifestData.short_name = "uBlock Origin"
$StagedManifestData.browser_action.default_title = "uBlock Origin"
[System.IO.File]::WriteAllText(
    $StagedManifest,
    ($StagedManifestData | ConvertTo-Json -Depth 100),
    [System.Text.UTF8Encoding]::new($false)
)
if (-not (Test-Path (Join-Path $ExtensionDestination "LICENSE.txt"))) {
    throw "The bundled uBlock Origin license was not copied."
}
(Get-Item $StagedManifest).LastWriteTimeUtc = [DateTime]::UtcNow
$BundleStamp = Join-Path $ChromiumBuildDir `
    "gen\chrome\installer\mini_installer\whister_ubo.stamp"
if (Test-Path $BundleStamp) {
    Remove-Item -LiteralPath $BundleStamp -Force
}

Write-Host ">>> uBlock Origin staged at $ExtensionDestination ($ExtensionId)" `
    -ForegroundColor Green
