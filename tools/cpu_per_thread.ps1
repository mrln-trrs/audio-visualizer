param([string]$ExeDir = "D:\proyectos\audio-visualizer\x64\Release", [int]$Seconds = 10)
$ErrorActionPreference = 'Stop'
Set-Location $ExeDir
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W2 {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
$player = New-Object System.Media.SoundPlayer "C:\Windows\Media\Ring05.wav"
$player.PlayLooping()
$proc = Start-Process -FilePath ".\audio-visualizer.exe" -WorkingDirectory (Get-Location) -PassThru
Start-Sleep -Seconds 2   # dejar pasar el arranque y la deteccion del limitador
$proc.Refresh()
$t0 = @{}
foreach ($t in $proc.Threads) { $t0[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds }
$wall0 = Get-Date
Start-Sleep -Seconds $Seconds
$proc.Refresh()
$wall = ((Get-Date) - $wall0).TotalMilliseconds
Write-Host ("Ventana de medida: {0:N0} ms. Titulo: {1}" -f $wall, $proc.MainWindowTitle)
$rows = foreach ($t in $proc.Threads) {
    $start = if ($t0.ContainsKey($t.Id)) { $t0[$t.Id] } else { 0 }
    $cpu = $t.TotalProcessorTime.TotalMilliseconds - $start
    [pscustomobject]@{ Hilo = $t.Id; CpuMs = [math]::Round($cpu, 1); PorcentajeNucleo = [math]::Round(100.0 * $cpu / $wall, 2) }
}
$rows | Sort-Object CpuMs -Descending | Select-Object -First 6 | Format-Table -AutoSize | Out-String | Write-Host
$total = ($rows | Measure-Object CpuMs -Sum).Sum
Write-Host ("Total proceso: {0:N0} ms de CPU en {1:N0} ms = {2:N1} % de un nucleo" -f $total, $wall, (100.0 * $total / $wall))
$player.Stop()
[void][W2]::PostMessage($proc.MainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
if (-not $proc.WaitForExit(5000)) { Write-Host "NO CERRO"; Stop-Process -Id $proc.Id -Force -Confirm:$false } else { Write-Host "Cierre correcto, ExitCode=$($proc.ExitCode)" }
