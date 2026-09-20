# Copilot instructions for Whister Browser Patches

## Repository purpose and architecture

This repository is a Chromium overlay, not the browser source tree itself.
Chromium must be checked out separately at `C:\chromium\src`; this repository
supplies the changes and build inputs that are applied to it.

The integration has four layers:

1. `patches/common/*.patch` contains the shared Chromium source changes:
   branding, the `whister://` scheme and resources, privacy/telemetry changes,
   settings styling, and omnibox/search defaults. These are applied with
   `git apply --3way`.
2. `branding/common/` contains the new-tab HTML/CSS/JavaScript, extension
   policy JSON, and image assets. The new-tab resources are registered in
   Chromium by the resource patches; the image assets are copied over the
   Chromium theme directory by the build script.
3. `config/stable_args.gn` and `config/beta_args.gn` provide channel-specific
   GN settings. Stable is an official, stripped-symbol build; beta is a
   non-official build with more symbol information and additional de-bloating
   flags.
4. `update_and_build.ps1` selects the Chromium tag and output directory for
   `stable` or `beta`, resets/synchronizes the Chromium checkout, applies
   patches, stages branding and extension files, runs `gn gen`, and builds
   `mini_installer`.

Keep changes scoped to the correct layer. Changes to Chromium source behavior
belong in a patch file rather than being copied into this repository as source
files. Changes to the dashboard presentation belong under
`branding/common/new_tab_page/` and must remain consistent with the resource
registration patches.

## Build commands

### Local Windows build

Prerequisites are a Chromium checkout at `C:\chromium\src`, Chromium
`depot_tools` available on `PATH` (`gclient`, `gn`, and `autoninja`), and a
PowerShell session:

```powershell
.\update_and_build.ps1 -Channel stable
.\update_and_build.ps1 -Channel beta
```

The script is destructive to the Chromium checkout: it runs `git reset --hard`
and `git clean -fd` before switching tags. Do not run it against a checkout
containing unrelated Chromium work.

Outputs are written under the Chromium tree:

- Stable: `C:\chromium\src\out\StableRelease\mini_installer.exe`
- Beta: `C:\chromium\src\out\BetaRelease\mini_installer.exe`

The GitHub Actions build is manual (`Actions` → `Build Whister Browser`) and
accepts a `stable` or `beta` channel. It runs on Windows and uploads the
installer as an artifact. Do not make the full Chromium build run automatically
for ordinary pushes or pull requests.

### Validation and tests

There is no unit-test or lint suite in this repository. The required automated
checks are in `.github/workflows/validate.yml` and run on pushes to `master` and
pull requests:

- required files and directories exist;
- every patch has `diff --git`, `---`, and `+++` headers;
- the extension policy JSON parses;
- `update_and_build.ps1` parses without PowerShell syntax errors.

Run equivalent checks locally when possible:

```powershell
# PowerShell syntax check
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
  (Join-Path $PWD 'update_and_build.ps1'),
  [ref]$null,
  [ref]$errors
) | Out-Null
if ($errors.Count -gt 0) { $errors | Format-List; exit 1 }

# JSON check
Get-Content branding\common\extensions\cjpalhdlnbpafiamejdnhcphjbkeiagm.json -Raw |
  ConvertFrom-Json | Out-Null

# Check one patch file
$patch = 'patches\common\disable_telemetry.patch'
$text = Get-Content $patch -Raw
if ($text -notmatch '(?m)^diff --git a/' -or
    $text -notmatch '(?m)^--- ' -or
    $text -notmatch '(?m)^\+\+\+ ') {
  throw "Invalid patch format: $patch"
}
```

On GitHub Actions, the JSON check uses Python (`python -m json.tool`) and the
PowerShell syntax check uses `pwsh`; these tools are available on the hosted
runner even if they are not installed on the local Windows machine.

## Repository-specific conventions

- Use `stable` and `beta` exactly as the channel values. They select different
  Chromium tags, output directories, and GN argument files.
- Shared patches go in `patches/common/`. Channel-specific patches, if added,
  go in `patches/stable/` or `patches/beta/`; the script applies common patches
  first and then channel patches.
- Preserve unified Git patch format and make patch paths target the Chromium
  source root. Validate new or edited patches before committing.
- Keep resource IDs, resource registration, and the files staged into Chromium
  synchronized. A new dashboard asset is not usable unless the corresponding
  GRD/resource and factory/scheme patches are updated too.
- The new-tab page is a self-contained static bundle. Its JavaScript updates
  the clock, routes direct destinations versus search queries, selects among
  DuckDuckGo/Brave/Google, and periodically requests weather data. Keep its
  element IDs and script/style filenames aligned with `index.html` and the GRD
  entries.
- Privacy-related changes are intentionally split between GN flags and source
  patches. Do not assume disabling a GN feature replaces the corresponding
  Chromium patch.
- The build script assumes Windows absolute paths and a case-insensitive
  filesystem. Keep checked-in path casing consistent because Linux CI and Git
  preserve case.
- Do not silently treat a failed patch as success. The current script logs a
  warning and continues after `git apply --3way` failure; any change to this
  behavior should be deliberate because it can produce a partially patched
  Chromium tree.
- Keep workflow permissions read-only unless a workflow genuinely needs
  additional access. The repository validation workflow should remain fast;
  expensive Chromium builds are manual.
