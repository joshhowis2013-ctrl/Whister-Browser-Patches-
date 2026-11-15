param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [string]$TestDirectory = (Join-Path $env:LOCALAPPDATA "Whister\LocalUpdateTest")
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $InstallerPath -PathType Leaf)) {
    throw "Installer not found: $InstallerPath"
}

$ThumbprintPath = Join-Path $TestDirectory "code-signing-thumbprint.txt"
if (-not (Test-Path $ThumbprintPath)) {
    throw "Local test certificate was not initialized. Run Initialize-WhisterLocalUpdateTest.ps1 first."
}

$thumbprint = (Get-Content $ThumbprintPath -Raw).Trim()
$certificate = Get-ChildItem "Cert:\CurrentUser\My\$thumbprint" -ErrorAction Stop
$signature = Set-AuthenticodeSignature -FilePath $InstallerPath -Certificate $certificate -HashAlgorithm SHA256
if ($signature.Status -ne "Valid") {
    throw "Signing failed: $($signature.Status) $($signature.StatusMessage)"
}

Write-Host "Signed local test installer: $InstallerPath" -ForegroundColor Green
