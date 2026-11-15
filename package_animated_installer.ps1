param(
    [string]$BuildDir = "C:\chromium\src\out\BetaRelease",
    [string]$AppVersion,
    [string]$OutName
)
$ErrorActionPreference = "Stop"
$Repo = $PSScriptRoot
$Dist = Join-Path $Repo "dist"
$AnimatedProject = Join-Path $Repo "installer\animated\WhisterAnimatedInstaller.csproj"
$BackendDirectory = Join-Path $Dist "animated-backend"

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
    $OutName = "WhisterAnimatedSetup-$AppVersion.exe"
}
if (-not (Test-Path $AnimatedProject)) {
    throw "Animated installer project not found: $AnimatedProject"
}

$BackendName = "WhisterBackend-$AppVersion.exe"
$BackendPath = Join-Path $BackendDirectory $BackendName
New-Item -ItemType Directory -Path $BackendDirectory -Force | Out-Null

& (Join-Path $Repo "package_installer.ps1") -BuildDir $BuildDir -AppVersion $AppVersion -OutName "animated-backend\$BackendName"
if ($LASTEXITCODE -ne 0) {
    throw "Whister installer backend packaging failed ($LASTEXITCODE)"
}
if (-not (Test-Path $BackendPath)) {
    throw "Whister installer backend was not created: $BackendPath"
}
$StageDirectory = Join-Path $Repo "dist\stage"
$PayloadFileCount = @(Get-ChildItem $StageDirectory -File -Recurse).Count
if ($PayloadFileCount -eq 0) {
    throw "No installer payload files were staged in $StageDirectory"
}

$OutPath = Join-Path $Dist $OutName
$AnimatedPublishDirectory = Join-Path $Dist "animated-publish"
& dotnet publish $AnimatedProject -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true "-p:InstallerPayloadPath=$BackendPath" `
    "-p:WhisterPayloadFileCount=$PayloadFileCount" "-p:Version=$AppVersion" `
    -o $AnimatedPublishDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Animated Whister installer publish failed ($LASTEXITCODE)"
}

$PublishedInstaller = Join-Path $AnimatedPublishDirectory "WhisterAnimatedSetup.exe"
if (-not (Test-Path $PublishedInstaller)) {
    throw "Animated Whister installer was not published: $PublishedInstaller"
}
Copy-Item $PublishedInstaller $OutPath -Force
$Hash = (Get-FileHash $OutPath -Algorithm SHA256).Hash
"$Hash  $OutName" | Set-Content "$OutPath.sha256"
Write-Host ">>> Built $OutPath" -ForegroundColor Green
Write-Host ">>> Version $AppVersion" -ForegroundColor Green
Write-Host ">>> SHA256 $Hash"

Remove-Item -LiteralPath $BackendPath, "$BackendPath.sha256" -Force
Remove-Item -LiteralPath $AnimatedPublishDirectory -Recurse -Force
