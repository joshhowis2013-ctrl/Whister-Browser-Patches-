param(
    [string]$TestDirectory = (Join-Path $env:LOCALAPPDATA "Whister\LocalUpdateTest")
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$ConfigTemplate = Join-Path $Repo "updater\WhisterUpdater\updater-config.json"
$ConfigPath = Join-Path $TestDirectory "updater-config.json"
$PrivateKeyPath = Join-Path $TestDirectory "manifest-private.pem"
$ThumbprintPath = Join-Path $TestDirectory "code-signing-thumbprint.txt"

New-Item -ItemType Directory -Force -Path $TestDirectory | Out-Null

if (Test-Path $ThumbprintPath) {
    $thumbprint = (Get-Content $ThumbprintPath -Raw).Trim()
    $certificate = Get-ChildItem "Cert:\CurrentUser\My\$thumbprint" -ErrorAction SilentlyContinue
}

if (-not $certificate) {
    $certificate = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject "CN=Whister Browser Local Update Test" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyAlgorithm RSA `
        -KeyLength 3072 `
        -HashAlgorithm SHA256 `
        -KeyExportPolicy NonExportable
    $certificate.Thumbprint | Set-Content $ThumbprintPath -NoNewline
}

$trustedCertificate = Get-ChildItem "Cert:\CurrentUser\Root\$($certificate.Thumbprint)" -ErrorAction SilentlyContinue
if (-not $trustedCertificate) {
    $rootStore = [System.Security.Cryptography.X509Certificates.X509Store]::new(
        [System.Security.Cryptography.X509Certificates.StoreName]::Root,
        [System.Security.Cryptography.X509Certificates.StoreLocation]::CurrentUser)
    $rootStore.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
    try {
        $rootStore.Add($certificate)
    }
    finally {
        $rootStore.Close()
    }
}

if (-not (Test-Path $ConfigPath)) {
    Copy-Item $ConfigTemplate $ConfigPath
}
if (-not (Test-Path $PrivateKeyPath)) {
    & (Join-Path $PSScriptRoot "New-WhisterUpdateKey.ps1") `
        -ConfigPath $ConfigPath `
        -PrivateKeyPath $PrivateKeyPath
}

$config = Get-Content $ConfigPath -Raw | ConvertFrom-Json
$config.trustedSignerThumbprint = $certificate.Thumbprint
$config | ConvertTo-Json -Depth 4 | Set-Content $ConfigPath -Encoding utf8

Write-Host "Local test updater configuration: $ConfigPath" -ForegroundColor Green
Write-Host "Test certificate thumbprint: $($certificate.Thumbprint)" -ForegroundColor Green
Write-Host "Use this configuration only with -UpdaterConfigPath; do not copy it into the repository." -ForegroundColor Yellow
