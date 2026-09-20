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
