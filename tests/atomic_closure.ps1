$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $root "build\tools\compile_fabric.exe"
$drv = Join-Path $root "build\tests\atomic_driver.exe"
$state = Join-Path $root "build\atomic_state.json"
$port = 48050 + (Get-Random -Maximum 200)
if (-not (Test-Path $cli)) { Write-Output "atomic: cli not found"; exit 1 }
Remove-Item $state -ErrorAction SilentlyContinue
$coord = Start-Process -FilePath $cli -ArgumentList "serve $port" -WorkingDirectory $root -WindowStyle Hidden -PassThru
Start-Sleep -Milliseconds 900
$w1 = Start-Process -FilePath $cli -ArgumentList "worker 127.0.0.1 $port 1" -WorkingDirectory $root -WindowStyle Hidden -PassThru
$w2 = Start-Process -FilePath $cli -ArgumentList "worker 127.0.0.1 $port 2" -WorkingDirectory $root -WindowStyle Hidden -PassThru
try {
  Start-Sleep -Milliseconds 1200
  & $drv 1 $port $state; $c1 = $LASTEXITCODE
  # Terminate worker-1 as a real OS process, then restart it as a NEW OS process (new WorkerBootId).
  Stop-Process -Id $w1.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 300
  $w1n = Start-Process -FilePath $cli -ArgumentList "worker 127.0.0.1 $port 1" -WorkingDirectory $root -WindowStyle Hidden -PassThru
  Start-Sleep -Milliseconds 800
  & $drv 2 $port $state; $c2 = $LASTEXITCODE
} finally {
  Stop-Process -Id $coord.Id,$w1.Id,$w1n.Id,$w2.Id -Force -ErrorAction SilentlyContinue
}
Write-Output "atomic_driver phase1=$c1 phase2=$c2"
if ($c1 -eq 0 -and $c2 -eq 0) { exit 0 } else { exit 1 }