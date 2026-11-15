using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Threading;

namespace WhisterAnimatedInstaller;

public partial class MainWindow : Window
{
    private readonly DispatcherTimer _progressTimer = new() { Interval = TimeSpan.FromMilliseconds(350) };
    private readonly Stopwatch _installClock = new();
    private readonly int _payloadFileCount;
    private string? _installLogPath;
    private string _lastLogContent = string.Empty;
    private bool _detailedMode;
    private bool _isInstalling;

    public MainWindow()
    {
        InitializeComponent();
        InstallPathBox.Text = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Programs", "Whister");
        var fileCountMetadata = Assembly.GetExecutingAssembly()
            .GetCustomAttributes<AssemblyMetadataAttribute>()
            .FirstOrDefault(attribute => attribute.Key == "WhisterPayloadFileCount")
            ?.Value;
        _payloadFileCount = int.TryParse(fileCountMetadata, out var fileCount)
            ? fileCount
            : 0;
        var badgePulse = new DoubleAnimation(1, 1.06, TimeSpan.FromSeconds(1.8))
        {
            AutoReverse = true,
            RepeatBehavior = RepeatBehavior.Forever,
            EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut }
        };
        WelcomeBadgeScale.BeginAnimation(ScaleTransform.ScaleXProperty, badgePulse);
        WelcomeBadgeScale.BeginAnimation(
            ScaleTransform.ScaleYProperty,
            new DoubleAnimation(1, 1.06, TimeSpan.FromSeconds(1.8))
            {
                AutoReverse = true,
                RepeatBehavior = RepeatBehavior.Forever,
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut }
            });
        VersionText.Text = $"EXPERIMENTAL SETUP  ·  {Assembly.GetExecutingAssembly().GetName().Version?.ToString(3)}";
        _progressTimer.Tick += AnimateProgress;
        Loaded += (_, _) => ShowPage(WelcomePage);
        Closing += (_, e) => e.Cancel = _isInstalling;
    }

    private async void OptionsButton_Click(object sender, RoutedEventArgs e)
    {
        await TransitionToAsync(OptionsPage);
    }

    private async void BackButton_Click(object sender, RoutedEventArgs e)
    {
        await TransitionToAsync(WelcomePage);
    }

    private async void BrowseButton_Click(object sender, RoutedEventArgs e)
    {
        using var dialog = new System.Windows.Forms.FolderBrowserDialog
        {
            Description = "Choose where to install Whister",
            SelectedPath = Directory.Exists(InstallPathBox.Text)
                ? InstallPathBox.Text
                : Path.GetDirectoryName(InstallPathBox.Text) ?? InstallPathBox.Text,
            ShowNewFolderButton = true
        };
        if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
        {
            InstallPathBox.Text = Path.Combine(dialog.SelectedPath, "Whister");
        }
    }

    private async void InstallButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryGetInstallDirectory(out var installDirectory, out var error))
        {
            ValidationMessage.Text = error;
            ValidationMessage.Visibility = Visibility.Visible;
            return;
        }

        ValidationMessage.Visibility = Visibility.Collapsed;
        await TransitionToAsync(InstallingPage);
        _isInstalling = true;
        CloseButton.IsEnabled = false;
        InstallStatus.Text = "Preparing installation…";
        _detailedMode = DetailedProgressBox.IsChecked == true;
        InstallDetailsPanel.Visibility = _detailedMode ? Visibility.Visible : Visibility.Collapsed;
        InstallActivityText.Text = "Starting the embedded installer…";
        InstallElapsedText.Text = "Elapsed 00:00";
        InstallLogTextBox.Text = "Waiting for installer activity…";
        _lastLogContent = string.Empty;
        InstallationProgressBar.Value = 0;
        InstallationProgressBar.IsIndeterminate = true;
        InstallPercent.Text = _payloadFileCount > 0 ? "Preparing…" : string.Empty;
        var tempDirectory = Path.Combine(Path.GetTempPath(), $"WhisterSetup-{Guid.NewGuid():N}");
        var installLogPath = Path.Combine(tempDirectory, "install.log");
        _installLogPath = installLogPath;
        _installClock.Restart();
        StartInstallAnimations();
        _progressTimer.Start();

        try
        {
            var createDesktopShortcut = DesktopShortcutBox.IsChecked == true;
            var result = await Task.Run(() =>
                RunEmbeddedInstaller(
                    installDirectory, createDesktopShortcut, tempDirectory, installLogPath));
            _progressTimer.Stop();
            _installClock.Stop();
            StopInstallAnimations();
            _isInstalling = false;
            CloseButton.IsEnabled = true;

            if (result.ExitCode == 0)
            {
                InstallationProgressBar.BeginAnimation(RangeBase.ValueProperty, null);
                InstallationProgressBar.IsIndeterminate = false;
                InstallationProgressBar.Value = 100;
                InstallPercent.Text = "100%";
                InstallStatus.Text = "Installation complete.";
                await TransitionToAsync(CompletePage);
            }
            else
            {
                var details = $"The installer exited with code {result.ExitCode}. No success was reported. " +
                              "Check that the install location is writable, then try again.";
                if (!string.IsNullOrWhiteSpace(result.LogTail))
                {
                    details += Environment.NewLine + Environment.NewLine + result.LogTail;
                }

                ErrorDetails.Text = details;
                await TransitionToAsync(ErrorPage);
            }
        }
        catch (Exception ex) when (ex is IOException or InvalidOperationException or UnauthorizedAccessException
                                   or System.ComponentModel.Win32Exception)
        {
            _progressTimer.Stop();
            _installClock.Stop();
            StopInstallAnimations();
            _isInstalling = false;
            CloseButton.IsEnabled = true;
            ErrorDetails.Text = ex.Message;
            await TransitionToAsync(ErrorPage);
        }
    }

    private (int ExitCode, string? LogTail) RunEmbeddedInstaller(
        string installDirectory,
        bool createDesktopShortcut,
        string tempDirectory,
        string logPath)
    {
        const string payloadResourceName = "WhisterAnimatedInstaller.Payload.exe";
        var assembly = Assembly.GetExecutingAssembly();
        using var payload = assembly.GetManifestResourceStream(payloadResourceName)
            ?? throw new InvalidOperationException("The installer package is missing its embedded browser payload.");

        Directory.CreateDirectory(tempDirectory);
        var backendPath = Path.Combine(tempDirectory, "WhisterPayloadSetup.exe");

        try
        {
            using (var destination = File.Create(backendPath))
            {
                payload.CopyTo(destination);
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = backendPath,
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = tempDirectory
            };
            startInfo.ArgumentList.Add("/S");
            startInfo.ArgumentList.Add(createDesktopShortcut
                ? "/COMPONENTS=Whister Browser,Desktop shortcut"
                : "/COMPONENTS=Whister Browser");
            startInfo.ArgumentList.Add($"/LOG={logPath}");
            startInfo.ArgumentList.Add($"/D={installDirectory}");

            using var process = Process.Start(startInfo)
                ?? throw new InvalidOperationException("Windows could not start the Whister installer.");
            process.WaitForExit();
            var logTail = process.ExitCode != 0 && File.Exists(logPath)
                ? string.Join(Environment.NewLine, File.ReadAllLines(logPath).TakeLast(15))
                : null;
            return (process.ExitCode, logTail);
        }
        finally
        {
            try
            {
                Directory.Delete(tempDirectory, recursive: true);
            }
            catch (IOException)
            {
                Trace.TraceWarning("Could not remove temporary Whister installer files from {0}.", tempDirectory);
            }
            catch (UnauthorizedAccessException)
            {
                Trace.TraceWarning("Could not remove temporary Whister installer files from {0}.", tempDirectory);
            }
        }
    }

    private bool TryGetInstallDirectory(out string installDirectory, out string error)
    {
        installDirectory = string.Empty;
        error = string.Empty;
        var enteredPath = InstallPathBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(enteredPath))
        {
            error = "Choose a folder for Whister.";
            return false;
        }

        try
        {
            installDirectory = Path.GetFullPath(enteredPath);
            var root = Path.GetPathRoot(installDirectory);
            if (string.Equals(installDirectory.TrimEnd(Path.DirectorySeparatorChar),
                    root?.TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase))
            {
                error = "Choose a folder inside a drive, not the drive itself.";
                return false;
            }

            return true;
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
        {
            error = "Enter a valid Windows folder path.";
            return false;
        }
    }

    private void AnimateProgress(object? sender, EventArgs e)
    {
        InstallElapsedText.Text = $"Elapsed {_installClock.Elapsed:mm\\:ss}";
        if (string.IsNullOrEmpty(_installLogPath) || !File.Exists(_installLogPath))
        {
            var waitingForInstaller = _installClock.Elapsed.TotalSeconds < 3;
            InstallStatus.Text = waitingForInstaller
                ? "Preparing installation…"
                : "Starting installer…";
            if (_detailedMode)
            {
                InstallActivityText.Text = waitingForInstaller
                    ? "Preparing the embedded browser installer…"
                    : "Starting the installer and waiting for its activity log…";
            }
            return;
        }

        string logContent;
        try
        {
            using var logFile = new FileStream(
                _installLogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var reader = new StreamReader(logFile, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
            logContent = reader.ReadToEnd();
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            Trace.TraceWarning("Could not read the Whister install log: {0}", ex.Message);
            InstallStatus.Text = "Installing Whister…";
            return;
        }

        if (_detailedMode && logContent != _lastLogContent)
        {
            _lastLogContent = logContent;
            var visibleLog = string.Join(
                Environment.NewLine,
                logContent.Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries)
                    .TakeLast(80));
            InstallLogTextBox.Text = string.IsNullOrWhiteSpace(visibleLog)
                ? "Installer started. Waiting for file activity…"
                : visibleLog;
            InstallLogTextBox.ScrollToEnd();
        }

        var logLines = logContent.Split(new[] { "\r\n", "\n" }, StringSplitOptions.None);
        var latestActivity = logLines.LastOrDefault(line => !string.IsNullOrWhiteSpace(line));
        var completedFiles = logLines.Count(
            line => line.StartsWith("File: wrote ", StringComparison.Ordinal));
        if (completedFiles == 0)
        {
            completedFiles = logLines.Count(
                line => line.StartsWith("Output: ", StringComparison.Ordinal));
        }
        if (_payloadFileCount > 0 && completedFiles > 0)
        {
            var totalFiles = _payloadFileCount + 2;
            var progress = Math.Clamp(
                5 + (int)(85.0 * completedFiles / totalFiles), 5, 95);
            InstallationProgressBar.IsIndeterminate = false;
            AnimateProgressTo(Math.Max(InstallationProgressBar.Value, progress));
            InstallPercent.Text = $"{(int)InstallationProgressBar.Value}%";
            InstallStatus.Text = completedFiles >= totalFiles
                ? "Finishing setup…"
                : $"Installing files ({Math.Min(completedFiles, totalFiles)} of {totalFiles})…";
            if (_detailedMode)
            {
                InstallActivityText.Text = string.IsNullOrWhiteSpace(latestActivity)
                    ? $"Files written: {Math.Min(completedFiles, totalFiles)} of {totalFiles}"
                    : $"Files written: {Math.Min(completedFiles, totalFiles)} of {totalFiles}. Latest: {latestActivity}";
            }
        }
        else
        {
            InstallationProgressBar.IsIndeterminate = true;
            InstallPercent.Text = _payloadFileCount > 0 ? "Preparing…" : string.Empty;
            InstallStatus.Text = "Installing Whister…";
            if (_detailedMode)
            {
                InstallActivityText.Text = string.IsNullOrWhiteSpace(latestActivity)
                    ? "Installer is running; waiting for file progress…"
                    : $"Latest installer activity: {latestActivity}";
            }
        }
    }

    private void AnimateProgressTo(double value)
    {
        var currentValue = InstallationProgressBar.Value;
        InstallationProgressBar.BeginAnimation(RangeBase.ValueProperty, null);
        InstallationProgressBar.Value = value;
        if (Math.Abs(currentValue - value) < 0.1)
        {
            return;
        }

        InstallationProgressBar.BeginAnimation(
            RangeBase.ValueProperty,
            new DoubleAnimation(currentValue, value, TimeSpan.FromMilliseconds(320))
            {
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
            });
    }

    private void StartInstallAnimations()
    {
        SpinnerRotation.Angle = 0;
        SpinnerRotation.BeginAnimation(
            RotateTransform.AngleProperty,
            new DoubleAnimation(0, 360, TimeSpan.FromSeconds(1.3))
            {
                RepeatBehavior = RepeatBehavior.Forever
            });
    }

    private void StopInstallAnimations()
    {
        SpinnerRotation.BeginAnimation(RotateTransform.AngleProperty, null);
        SpinnerRotation.Angle = 0;
    }

    private async void FinishButton_Click(object sender, RoutedEventArgs e)
    {
        if (LaunchBrowserBox.IsChecked == true)
        {
            var installDirectory = Path.GetFullPath(InstallPathBox.Text.Trim());
            try
            {
                Process.Start(new ProcessStartInfo(Path.Combine(installDirectory, "chrome.exe"))
                {
                    UseShellExecute = true
                });
            }
            catch (Exception ex) when (ex is IOException or InvalidOperationException or UnauthorizedAccessException
                                       or System.ComponentModel.Win32Exception)
            {
                ErrorDetails.Text = $"Whister installed, but could not be opened: {ex.Message}";
                await TransitionToAsync(ErrorPage);
                return;
            }
        }

        Close();
    }

    private async void BackFromErrorButton_Click(object sender, RoutedEventArgs e)
    {
        await TransitionToAsync(OptionsPage);
    }

    private async Task TransitionToAsync(Grid nextPage)
    {
        var currentPage = new[] { WelcomePage, OptionsPage, InstallingPage, CompletePage, ErrorPage }
            .FirstOrDefault(page => page.Visibility == Visibility.Visible);

        if (currentPage == nextPage)
        {
            return;
        }

        if (currentPage is not null)
        {
            var exit = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(130))
            {
                EasingFunction = new CubicEase { EasingMode = EasingMode.EaseIn }
            };
            currentPage.BeginAnimation(OpacityProperty, exit);
            await Task.Delay(exit.Duration.TimeSpan);
            currentPage.Visibility = Visibility.Collapsed;
        }

        nextPage.Visibility = Visibility.Visible;
        nextPage.Opacity = 0;
        var transform = (TranslateTransform)nextPage.RenderTransform;
        transform.Y = 12;

        var enterOpacity = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(280))
        {
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
        };
        var enterPosition = new DoubleAnimation(12, 0, TimeSpan.FromMilliseconds(360))
        {
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
        };
        nextPage.BeginAnimation(OpacityProperty, enterOpacity);
        transform.BeginAnimation(TranslateTransform.YProperty, enterPosition);
    }

    private void ShowPage(Grid page)
    {
        page.Visibility = Visibility.Visible;
        page.Opacity = 1;
        foreach (var other in new[] { WelcomePage, OptionsPage, InstallingPage, CompletePage, ErrorPage })
        {
            if (other != page)
            {
                other.Visibility = Visibility.Collapsed;
            }
        }
    }

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ButtonState == MouseButtonState.Pressed)
        {
            DragMove();
        }
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        Close();
    }
}
