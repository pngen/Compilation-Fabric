$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $root "build\tools\compile_fabric.exe"
$drv = Join-Path $root "build\tests\atomic_driver.exe"
$port = 48050 + (Get-Random -Maximum 200)
if (-not (Test-Path $cli)) { Write-Output "atomic: cli not found"; exit 1 }
$so = Join-Path $root "build\atomic_out.log"; $se = Join-Path $root "build\atomic_err.log"
Remove-Item $so,$se -ErrorAction SilentlyContinue
$coord = Start-Process -FilePath $cli -ArgumentList "serve $port" -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardError $se -RedirectStandardOutput $so -PassThru
Start-Sleep -Milliseconds 900
$w1 = Start-Process -FilePath $cli -ArgumentList "worker 127.0.0.1 $port 1" -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardError $se -RedirectStandardOutput $so -PassThru
$w2 = Start-Process -FilePath $cli -ArgumentList "worker 127.0.0.1 $port 2" -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardError $se -RedirectStandardOutput $so -PassThru
Start-Sleep -Milliseconds 1500
$procs = Get-Process compile_fabric -ErrorAction SilentlyContinue
Write-Output "procs=$($procs.Count)"
try {
  & $drv $port
  $code = $LASTEXITCODE
} finally {
  Stop-Process -Id $coord.Id,$w1.Id,$w2.Id -Force -ErrorAction SilentlyContinue
}
Write-Output "atomic_driver exit: $code"
exit $code
