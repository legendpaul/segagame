param(
    [int]$ReturnCount = 0,
    [string]$InputKeys = "",
    [int]$VirtualKey = 0,
    [string]$Output = "fusion_capture.png"
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class FusionWindow {
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out Rect rect);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
"@

$fusion = Get-Process | Where-Object ProcessName -eq "Fusion" | Select-Object -First 1
if (-not $fusion -or $fusion.MainWindowHandle -eq 0) { throw "Fusion window not found" }

[FusionWindow]::SetForegroundWindow($fusion.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 250
for ($i = 0; $i -lt $ReturnCount; $i++) {
    [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
    Start-Sleep -Milliseconds 900
}
if ($InputKeys) {
    [System.Windows.Forms.SendKeys]::SendWait($InputKeys)
    Start-Sleep -Milliseconds 500
}
if ($VirtualKey -gt 0) {
    [FusionWindow]::keybd_event([byte]$VirtualKey, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
    [FusionWindow]::keybd_event([byte]$VirtualKey, 0, 2, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}

$rect = New-Object FusionWindow+Rect
[FusionWindow]::GetWindowRect($fusion.MainWindowHandle, [ref]$rect) | Out-Null
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
$outputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Output))
$bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
Write-Output $outputPath
