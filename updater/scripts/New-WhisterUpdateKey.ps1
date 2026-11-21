param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,
    [Parameter(Mandatory = $true)]
    [string]$PrivateKeyPath
)

$ErrorActionPreference = "Stop"

if (Test-Path $PrivateKeyPath) {
    throw "Refusing to overwrite existing private key: $PrivateKeyPath"
}

$config = Get-Content $ConfigPath -Raw | ConvertFrom-Json
$key = [System.Security.Cryptography.ECDsa]::Create(
    [System.Security.Cryptography.ECCurve]::CreateFromFriendlyName("nistP256"))

try {
    $privateKeyDirectory = Split-Path $PrivateKeyPath -Parent
    New-Item -ItemType Directory -Force -Path $privateKeyDirectory | Out-Null
    [System.IO.File]::WriteAllText(
        $PrivateKeyPath,
        $key.ExportPkcs8PrivateKeyPem(),
        [System.Text.UTF8Encoding]::new($false))

    $config.manifestPublicKeySpkiBase64 = [Convert]::ToBase64String($key.ExportSubjectPublicKeyInfo())
    $config | ConvertTo-Json -Depth 4 |
        Set-Content $ConfigPath -NoNewline -Encoding utf8

    Write-Host "Created private key at $PrivateKeyPath" -ForegroundColor Green
    Write-Host "The public key was written to $ConfigPath. Keep the private key outside this repository." -ForegroundColor Green
}
finally {
    $key.Dispose()
}
