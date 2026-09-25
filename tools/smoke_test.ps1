param([string]$ExeDir = "D:\proyectos\audio-visualizer\x64\Release")
$ErrorActionPreference = 'Stop'
Set-Location $ExeDir
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT rect);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
}
"@
$outDir = Join-Path $PSScriptRoot "capturas"; New-Item -ItemType Directory -Force $outDir | Out-Null
$WM_KEYDOWN = 0x0100; $WM_KEYUP = 0x0101; $WM_CLOSE = 0x0010
function Shot($proc, $name) {
    $rect = New-Object W+RECT
    [void][W]::GetClientRect($proc.MainWindowHandle, [ref]$rect)
    $w = $rect.Right - $rect.Left; $h = $rect.Bottom - $rect.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
    [void][W]::PrintWindow($proc.MainWindowHandle, $hdc, 0x3)
    $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save((Join-Path $outDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Write-Host ("[{0}] {1}" -f $name, $proc.MainWindowTitle)
}
function Key($proc, [int]$vk) {
    $scan = [int]([W]::MapVirtualKey($vk, 0)) -band 0xFF
    $down = [IntPtr]((($scan -shl 16) -bor 1))
    $up   = [IntPtr]((([int64]0xC0000000) -bor ($scan -shl 16) -bor 1))
    [void][W]::PostMessage($proc.MainWindowHandle, $WM_KEYDOWN, [IntPtr]$vk, $down)
    Start-Sleep -Milliseconds 40
    [void][W]::PostMessage($proc.MainWindowHandle, $WM_KEYUP, [IntPtr]$vk, $up)
}
$player = New-Object System.Media.SoundPlayer "C:\Windows\Media\Ring05.wav"
$player.PlayLooping()
$proc = Start-Process -FilePath ".\audio-visualizer.exe" -WorkingDirectory (Get-Location) -PassThru
Start-Sleep -Seconds 3
$proc.Refresh()
if ($proc.HasExited) { Write-Host "FALLO: termino al arrancar, exit=$($proc.ExitCode)"; $player.Stop(); exit 1 }
Key $proc 0x34; Start-Sleep -Seconds 2; $proc.Refresh(); Shot $proc "v6-cascada"
# Fundido cruzado a radial: captura a mitad de la transicion (~75 ms) y al final.
Key $proc 0x32; Start-Sleep -Milliseconds 35; Shot $proc "v6-fundido-medio"
Start-Sleep -Milliseconds 600; $proc.Refresh(); Shot $proc "v6-radial"
# HUD: captura durante la apertura (~100 ms) y con la apertura completa (material acrilico).
Key $proc 0x48; Start-Sleep -Milliseconds 60; Shot $proc "v6-hud-abriendo"
Start-Sleep -Milliseconds 900; $proc.Refresh(); Shot $proc "v6-hud-material"
# CPU con el HUD abierto y el material activo.
$t0 = @{}; foreach ($t in $proc.Threads) { $t0[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds }
$w0 = Get-Date; Start-Sleep -Seconds 5; $proc.Refresh()
$wall = ((Get-Date) - $w0).TotalMilliseconds
$rows = foreach ($t in $proc.Threads) { $s = if ($t0.ContainsKey($t.Id)) { $t0[$t.Id] } else { 0 }; [pscustomobject]@{ Hilo=$t.Id; Pct=[math]::Round(100.0*($t.TotalProcessorTime.TotalMilliseconds-$s)/$wall,2) } }
Write-Host "CPU por hilo con HUD y material (5 s):"; $rows | Sort-Object Pct -Descending | Select-Object -First 3 | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Titulo: $($proc.MainWindowTitle)"
# Cierre del HUD con fundido y cierre de la aplicacion.
Key $proc 0x48; Start-Sleep -Milliseconds 400
$player.Stop()
$t1 = Get-Date
[void][W]::PostMessage($proc.MainWindowHandle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
if ($proc.WaitForExit(5000)) { Write-Host "Cierre correcto en $([math]::Round(((Get-Date) - $t1).TotalMilliseconds)) ms. ExitCode=$($proc.ExitCode)" }
else { Write-Host "FALLO: no cerro"; Stop-Process -Id $proc.Id -Force -Confirm:$false }
