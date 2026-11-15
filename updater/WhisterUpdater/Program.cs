using System.Diagnostics;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

const int ManifestMaximumBytes = 64 * 1024;
const string VersionPattern = @"^\d+\.\d+\.\d+\.\d+$";
var updaterDirectory = AppContext.BaseDirectory;
var configPath = Path.Combine(updaterDirectory, "updater-config.json");
var statePath = Path.Combine(updaterDirectory, "update-state.json");
var mutex = new Mutex(false, @"Local\WhisterBrowserUpdater");

if (!mutex.WaitOne(TimeSpan.Zero))
{
    return 0;
}

try
{
    var config = LoadConfig(configPath);
    if (args.Contains("--check", StringComparer.OrdinalIgnoreCase) && !ShouldCheck(statePath, config.CheckIntervalHours))
    {
        return 0;
    }

    var manifest = await DownloadAndVerifyManifestAsync(config);
    if (CompareVersions(manifest.Version, config.CurrentVersion) <= 0)
    {
        WriteState(statePath);
        return 0;
    }

    var installerPath = await DownloadInstallerAsync(manifest, updaterDirectory);
    VerifyAuthenticodeSignature(installerPath, config.TrustedSignerThumbprint);

    WriteState(statePath);
    await WaitForBrowserExitAsync();
    Process.Start(new ProcessStartInfo(installerPath, "/S")
    {
        UseShellExecute = false,
        WorkingDirectory = Path.GetDirectoryName(installerPath)!,
    });
    return 0;
}
catch (Exception exception)
{
    Console.Error.WriteLine($"Whister update failed: {exception.Message}");
    return 1;
}
finally
{
    mutex.ReleaseMutex();
    mutex.Dispose();
}

static UpdaterConfig LoadConfig(string path)
{
    if (!File.Exists(path))
    {
        throw new InvalidOperationException($"Updater configuration is missing: {path}");
    }

    var config = JsonSerializer.Deserialize<UpdaterConfig>(File.ReadAllText(path))
        ?? throw new InvalidOperationException("Updater configuration is invalid.");

    if (!Uri.TryCreate(config.FeedUrl, UriKind.Absolute, out var feedUri) ||
        feedUri.Scheme != Uri.UriSchemeHttps ||
        !string.Equals(feedUri.Host, "beta-updates.whisterbrowser.com", StringComparison.OrdinalIgnoreCase))
    {
        throw new InvalidOperationException("The update feed must use the Whister HTTPS update domain.");
    }

    if (!Regex.IsMatch(config.CurrentVersion, VersionPattern) ||
        string.IsNullOrWhiteSpace(config.ManifestPublicKeySpkiBase64) ||
        string.IsNullOrWhiteSpace(config.TrustedSignerThumbprint))
    {
        throw new InvalidOperationException(
            "Updates are disabled until a release version, manifest public key, and Authenticode signer thumbprint are configured.");
    }

    return config;
}

static bool ShouldCheck(string statePath, int intervalHours)
{
    if (!File.Exists(statePath))
    {
        return true;
    }

    var state = JsonSerializer.Deserialize<UpdateState>(File.ReadAllText(statePath));
    return state?.LastCheckedUtc is null ||
        DateTimeOffset.UtcNow - state.LastCheckedUtc.Value >= TimeSpan.FromHours(Math.Max(1, intervalHours));
}

static async Task<UpdateManifest> DownloadAndVerifyManifestAsync(UpdaterConfig config)
{
    using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
    client.DefaultRequestHeaders.CacheControl = new System.Net.Http.Headers.CacheControlHeaderValue { NoCache = true };
    using var response = await client.GetAsync(config.FeedUrl, HttpCompletionOption.ResponseHeadersRead);
    response.EnsureSuccessStatusCode();

    if (response.Content.Headers.ContentLength is > ManifestMaximumBytes)
    {
        throw new InvalidOperationException("The update manifest is too large.");
    }

    var manifestText = await response.Content.ReadAsStringAsync();
    if (Encoding.UTF8.GetByteCount(manifestText) > ManifestMaximumBytes)
    {
        throw new InvalidOperationException("The update manifest is too large.");
    }

    var manifest = JsonSerializer.Deserialize<UpdateManifest>(manifestText)
        ?? throw new InvalidOperationException("The update manifest is invalid.");
    ValidateManifest(manifest, config);

    using var key = ECDsa.Create();
    key.ImportSubjectPublicKeyInfo(Convert.FromBase64String(config.ManifestPublicKeySpkiBase64), out _);
    if (!key.VerifyData(Encoding.UTF8.GetBytes(CanonicalManifestPayload(manifest)),
        Convert.FromBase64String(manifest.Signature), HashAlgorithmName.SHA256))
    {
        throw new CryptographicException("The update manifest signature is invalid.");
    }

    return manifest;
}

static void ValidateManifest(UpdateManifest manifest, UpdaterConfig config)
{
    if (manifest.SchemaVersion != 1 ||
        !string.Equals(manifest.Channel, config.Channel, StringComparison.Ordinal) ||
        !Regex.IsMatch(manifest.Version, VersionPattern) ||
        !Uri.TryCreate(manifest.Installer.Url, UriKind.Absolute, out var installerUri) ||
        installerUri.Scheme != Uri.UriSchemeHttps ||
        !string.Equals(installerUri.Host, "beta-updates.whisterbrowser.com", StringComparison.OrdinalIgnoreCase) ||
        !Regex.IsMatch(manifest.Installer.Sha256, "^[a-f0-9]{64}$", RegexOptions.CultureInvariant) ||
        manifest.Installer.Size <= 0 ||
        string.IsNullOrWhiteSpace(manifest.Signature))
    {
        throw new InvalidOperationException("The update manifest contains invalid values.");
    }
}

static string CanonicalManifestPayload(UpdateManifest manifest) => string.Join("\n",
    manifest.SchemaVersion,
    manifest.Channel,
    manifest.Version,
    manifest.PublishedAt.ToUniversalTime().ToString("O"),
    manifest.Installer.Url,
    manifest.Installer.Sha256,
    manifest.Installer.Size) + "\n";

static async Task<string> DownloadInstallerAsync(UpdateManifest manifest, string updaterDirectory)
{
    var updateDirectory = Path.Combine(updaterDirectory, "downloads");
    Directory.CreateDirectory(updateDirectory);
    var installerPath = Path.Combine(updateDirectory, $"WhisterSetup-{manifest.Version}.exe");
    var temporaryPath = installerPath + ".partial";

    using var client = new HttpClient { Timeout = TimeSpan.FromMinutes(30) };
    using var response = await client.GetAsync(manifest.Installer.Url, HttpCompletionOption.ResponseHeadersRead);
    response.EnsureSuccessStatusCode();
    if (response.Content.Headers.ContentLength is long length && length != manifest.Installer.Size)
    {
        throw new InvalidOperationException("The installer size does not match the manifest.");
    }

    await using (var source = await response.Content.ReadAsStreamAsync())
    await using (var destination = File.Create(temporaryPath))
    {
        await source.CopyToAsync(destination);
    }

    if (new FileInfo(temporaryPath).Length != manifest.Installer.Size ||
        !string.Equals(Convert.ToHexString(SHA256.HashData(await File.ReadAllBytesAsync(temporaryPath))).ToLowerInvariant(),
            manifest.Installer.Sha256, StringComparison.Ordinal))
    {
        File.Delete(temporaryPath);
        throw new CryptographicException("The installer checksum does not match the manifest.");
    }

    File.Move(temporaryPath, installerPath, true);
    return installerPath;
}

static void VerifyAuthenticodeSignature(string installerPath, string expectedThumbprint)
{
    if (WinVerifyTrust.VerifyFile(installerPath) != 0)
    {
        throw new CryptographicException("The installer Authenticode signature is not trusted by Windows.");
    }

    using var certificate = X509CertificateLoader.LoadCertificateFromFile(installerPath);
    if (!string.Equals(certificate.Thumbprint?.Replace(" ", string.Empty), expectedThumbprint.Replace(" ", string.Empty),
        StringComparison.OrdinalIgnoreCase))
    {
        throw new CryptographicException("The installer signer does not match the configured Whister release certificate.");
    }
}

static async Task WaitForBrowserExitAsync()
{
    var timeout = DateTimeOffset.UtcNow.AddHours(2);
    while (IsWhisterBrowserRunning() && DateTimeOffset.UtcNow < timeout)
    {
        await Task.Delay(TimeSpan.FromSeconds(15));
    }

    if (IsWhisterBrowserRunning())
    {
        throw new TimeoutException("Whister Browser did not close within two hours.");
    }
}

static bool IsWhisterBrowserRunning()
{
    var whisterExecutable = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "Programs", "Whister", "chrome.exe");

    foreach (var process in Process.GetProcessesByName("chrome"))
    {
        try
        {
            if (string.Equals(process.MainModule?.FileName, whisterExecutable, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }
        }
        catch (InvalidOperationException)
        {
            // A process may exit while its executable path is being read.
        }
        catch (System.ComponentModel.Win32Exception)
        {
            // Ignore browser processes owned by another user or protected by Windows.
        }
        finally
        {
            process.Dispose();
        }
    }

    return false;
}

static int CompareVersions(string left, string right) =>
    new Version(left).CompareTo(new Version(right));

static void WriteState(string path) =>
    File.WriteAllText(path, JsonSerializer.Serialize(new UpdateState(DateTimeOffset.UtcNow)));

internal sealed record UpdaterConfig(
    string FeedUrl,
    string Channel,
    string CurrentVersion,
    string ManifestPublicKeySpkiBase64,
    string TrustedSignerThumbprint,
    int CheckIntervalHours);

internal sealed record UpdateState(DateTimeOffset? LastCheckedUtc);

internal sealed record UpdateManifest(
    int SchemaVersion,
    string Channel,
    string Version,
    DateTimeOffset PublishedAt,
    InstallerInfo Installer,
    string Signature);

internal sealed record InstallerInfo(string Url, string Sha256, long Size);
