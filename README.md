# Whister browser patches

This repository contains the patches, branding assets, build configuration, and
PowerShell automation for the Whister Browser stable and beta channels.

## Workflows

- **Validate repository** runs on pushes and pull requests. It checks required
  files, patch structure, JSON configuration, and PowerShell syntax.
- **Build Whister Browser** is started manually from the **Actions** tab. Select
  `stable` or `beta`; it fetches Chromium on a Windows runner, applies the
  patches, builds the installer, and uploads it as a workflow artifact.

## Local build

Run the build script from PowerShell after fetching Chromium into
`C:\chromium\src`:

```powershell
.\update_and_build.ps1 -Channel stable
.\update_and_build.ps1 -Channel beta
```

## Experimental animated installer

The animated setup is a separate, experimental installer; it does not replace
the standard NSIS installer. After building Chromium, package it with:

```powershell
.\package_animated_installer.ps1 -BuildDir C:\chromium\src\out\BetaRelease
```

This writes `dist\WhisterAnimatedSetup-<version>.exe`. Its animated wizard
hands installation to the same tested NSIS backend, preserving the existing
per-user install, updater, desktop-shortcut choice, and uninstall behavior.

On first run, Whister opens a five-step setup flow: welcome, default-browser
settings, search-engine selection, import settings, and finish. uBlock Origin
is bundled as the browser's content blocker. The default-browser and import
actions open Chromium’s corresponding Settings pages; they do not report
completion until those pages perform their own work.

## Beta auto-updates

The packaged installer includes a per-user updater that checks the Cloudflare
beta feed at logon. It refuses to install an update until all browser windows
close, and verifies both the signed manifest and the Authenticode
signature of the installer.

Before creating the first release, acquire an Authenticode certificate, then set
its SHA-1 thumbprint in `updater\WhisterUpdater\updater-config.json` and create
the update-manifest signing key outside the repository:

```powershell
.\updater\scripts\New-WhisterUpdateKey.ps1 `
  -ConfigPath .\updater\WhisterUpdater\updater-config.json `
  -PrivateKeyPath "$env:USERPROFILE\.whister\update-manifest-private.pem"
```

Sign the installer with the release certificate, generate a signed manifest,
and upload both to R2 using the instructions in `cloudflare-updates\README.md`.
Do not publish an unsigned installer or store the manifest private key in this
repository.

### Local-only updater test

To test the update flow before purchasing a certificate, create a self-signed
certificate trusted only by the current Windows user:

```powershell
.\updater\scripts\Initialize-WhisterLocalUpdateTest.ps1
```

Use the configuration path it prints with `package_installer.ps1`, then sign
the test package with `Sign-WhisterLocalUpdatePackage.ps1`. Never upload this
test certificate or use its configuration for public releases.
