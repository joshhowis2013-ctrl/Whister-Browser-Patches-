# Whister beta update feed

The Worker exposes only the beta manifest and versioned installers from a private
R2 bucket. Bind `beta-updates.whisterbrowser.com/*` to this Worker, create the
`whister-beta-updates` R2 bucket, then deploy:

```powershell
cd cloudflare-updates
npx wrangler login
npx wrangler deploy
```

Upload a signed installer and its generated manifest after code-signing the
installer:

```powershell
npx wrangler r2 object put whister-beta-updates/win/beta/WhisterSetup-158.0.8105.0.exe --file ..\dist\WhisterSetup-158.0.8105.0.exe
npx wrangler r2 object put whister-beta-updates/win/beta/manifest.json --file ..\manifest.json
```

Generate `manifest.json` only with `..\updater\scripts\New-WhisterUpdateManifest.ps1`.
The updater rejects any manifest whose signature, HTTPS host, version, installer
size, SHA-256 hash, Windows Authenticode trust, or signer thumbprint is invalid.
