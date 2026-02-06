using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

class Program
{
    static async Task<int> Main(string[] args)
    {
        var root = AppContext.BaseDirectory; // exe folder
        var repoRoot = Directory.GetParent(root)!.Parent!.Parent!.Parent!.FullName; // heuristics for src/DevTools/bin...
        var logDir = Path.Combine(repoRoot, "logs");
        Directory.CreateDirectory(logDir);
        var logFile = Path.Combine(logDir, "devrunner.log");

        await Log(logFile, "DevRunner start");

        // 默认顺序：dotnet build -> run scripts/build_and_run.ps1 (non-interactive) -> git add/commit
        if (args.Length == 0 || args[0].Equals("runall", StringComparison.OrdinalIgnoreCase))
        {
            if (!await RunProcess("dotnet", "build", repoRoot, logFile)) return 1;

            // 如果存在 scripts/build_and_run.ps1，则以 PowerShell 非交互执行
            var psPath = Path.Combine(repoRoot, "scripts", "build_and_run.ps1");
            if (File.Exists(psPath))
            {
                await Log(logFile, "Running build_and_run.ps1 via PowerShell...");
                if (!await RunProcess("powershell", $"-NoProfile -ExecutionPolicy Bypass -File \"{psPath}\"", repoRoot, logFile))
                {
                    await Log(logFile, "scripts build failed or exited non-zero");
                }
            }

            // 轻量 git commit（如果有改动）
            if (await HasGitChanges(repoRoot))
            {
                await RunProcess("git", "add -A", repoRoot, logFile);
                var msg = "Automated dev runner commit";
                await RunProcess("git", $"commit -m \"{msg}\"", repoRoot, logFile);
            }

            await Log(logFile, "DevRunner finished");
            return 0;
        }

        await Log(logFile, $"Unknown args: {string.Join(' ', args)}");
        return 2;
    }

    static async Task Log(string logFile, string text)
    {
        var line = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {text}";
        Console.WriteLine(line);
        await File.AppendAllTextAsync(logFile, line + Environment.NewLine, Encoding.UTF8);
    }

    static async Task<bool> RunProcess(string fileName, string args, string workingDir, string logFile)
    {
        await Log(logFile, $"> {fileName} {args}");
        try
        {
            var psi = new ProcessStartInfo(fileName, args)
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = workingDir,
            };

            using var p = Process.Start(psi)!;
            var sbOut = new StringBuilder();
            p.OutputDataReceived += (s, e) => { if (e.Data != null) sbOut.AppendLine(e.Data); };
            var sbErr = new StringBuilder();
            p.ErrorDataReceived += (s, e) => { if (e.Data != null) sbErr.AppendLine(e.Data); };
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            await p.WaitForExitAsync();

            if (sbOut.Length > 0) await File.AppendAllTextAsync(logFile, sbOut.ToString());
            if (sbErr.Length > 0) await File.AppendAllTextAsync(logFile, sbErr.ToString());

            await Log(logFile, $"Process exit code: {p.ExitCode}");
            return p.ExitCode == 0;
        }
        catch (Exception ex)
        {
            await Log(logFile, "Process failed: " + ex.Message);
            return false;
        }
    }

    static async Task<bool> HasGitChanges(string workingDir)
    {
        try
        {
            var psi = new ProcessStartInfo("git", "status --porcelain")
            {
                RedirectStandardOutput = true,
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = workingDir,
            };
            using var p = Process.Start(psi)!;
            var outStr = await p.StandardOutput.ReadToEndAsync();
            await p.WaitForExitAsync();
            return !string.IsNullOrWhiteSpace(outStr);
        }
        catch { return false; }
    }
}
