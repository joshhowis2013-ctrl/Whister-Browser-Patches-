param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string]$Version,
    [Parameter(Mandatory = $true)]
    [string]$PrivateKeyPath,
    [string]$Channel = "beta",
    [string]$BaseUrl = "https://beta-updates.whisterbrowser.com/win/beta",
    [string]$OutputPath = "manifest.json"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $InstallerPath -PathType Leaf)) {
    throw "Installer not found: $InstallerPath"
}
if (-not (Test-Path $PrivateKeyPath -PathType Leaf)) {
    throw "Private key not found: $PrivateKeyPath"
}

$baseUri = [Uri]$BaseUrl
if ($baseUri.Scheme -ne "https" -or $baseUri.Host -ne "beta-updates.whisterbrowser.com") {
    throw "BaseUrl must use https://beta-updates.whisterbrowser.com."
}

$installerName = Split-Path $InstallerPath -Leaf
$installerUrl = "$($BaseUrl.TrimEnd('/'))/$installerName"
$publishedAt = [DateTimeOffset]::UtcNow.ToString("O")
$hash = (Get-FileHash $InstallerPath -Algorithm SHA256).Hash.ToLowerInvariant()
$size = (Get-Item $InstallerPath).Length
$payload = "1`n$Channel`n$Version`n$publishedAt`n$installerUrl`n$hash`n$size`n"

$privateKeyPem = Get-Content $PrivateKeyPath -Raw
$key = $null
try {
    $importFromPem = [System.Security.Cryptography.ECDsa].GetMethod("ImportFromPem")
    if ($importFromPem) {
        $key = [System.Security.Cryptography.ECDsa]::Create()
        $key.ImportFromPem($privateKeyPem)
    }
    else {
        $base64 = [regex]::Match(
            $privateKeyPem,
            "-----BEGIN PRIVATE KEY-----(?<key>[\s\S]+?)-----END PRIVATE KEY-----").Groups["key"].Value -replace "\s"
        if ([string]::IsNullOrWhiteSpace($base64)) {
            throw "The manifest private key is not a PKCS#8 PEM file."
        }
        $cngKey = [System.Security.Cryptography.CngKey]::Import(
            [Convert]::FromBase64String($base64),
            [System.Security.Cryptography.CngKeyBlobFormat]::Pkcs8PrivateBlob)
        $key = [System.Security.Cryptography.ECDsaCng]::new($cngKey)
    }
    $signature = [Convert]::ToBase64String(
        $key.SignData([System.Text.Encoding]::UTF8.GetBytes($payload),
            [System.Security.Cryptography.HashAlgorithmName]::SHA256))
}
finally {
    $key.Dispose()
}

[ordered]@{
    schemaVersion = 1
    channel = $Channel
    version = $Version
    publishedAt = $publishedAt
    installer = [ordered]@{
        url = $installerUrl
        sha256 = $hash
        size = $size
    }
    signature = $signature
} | ConvertTo-Json -Depth 4 | Set-Content $OutputPath -NoNewline -Encoding utf8

Write-Host "Wrote signed update manifest to $OutputPath" -ForegroundColor Green
