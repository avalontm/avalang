# Fase 8 -- Optimizacion: helper de measure_release.bat.
# No se invoca a mano normalmente -- measure_release.bat ya localiza los
# binarios y llama a este script con las rutas resueltas.

param(
    [string]$AvaCli,
    [string]$AvalangDll,
    [string]$AvalangUiDll
)

$ErrorActionPreference = "Stop"

function Measure-Run {
    param([string]$Exe, [string]$ScriptPath, [int]$Runs = 5)

    $times = @()
    $peakMem = 0
    for ($i = 0; $i -lt $Runs; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $p = Start-Process -FilePath $Exe -ArgumentList $ScriptPath -NoNewWindow -PassThru
        while (-not $p.HasExited) {
            try {
                $p.Refresh()
                if ($p.PeakWorkingSet64 -gt $peakMem) { $peakMem = $p.PeakWorkingSet64 }
            } catch {
                # el proceso ya termino entre HasExited y Refresh() -- normal
                # en scripts que corren en menos de 1ms, se ignora.
            }
        }
        $sw.Stop()
        $times += $sw.Elapsed.TotalMilliseconds
    }
    return @{ Times = $times; PeakMemBytes = $peakMem }
}

Write-Host ""
Write-Host "====================================================================="
Write-Host "6. Tamano de binarios"
Write-Host "====================================================================="
if ($AvalangDll -and (Test-Path $AvalangDll)) {
    $size = (Get-Item $AvalangDll).Length
    Write-Host ("avalang.dll       {0:N0} bytes  ({1})" -f $size, $AvalangDll)
} else {
    Write-Host "avalang.dll       no encontrado (build estatico? ver AVA_BUILD_SHARED)"
}
if ($AvalangUiDll -and (Test-Path $AvalangUiDll)) {
    $size = (Get-Item $AvalangUiDll).Length
    Write-Host ("avalang_ui.dll    {0:N0} bytes  ({1})" -f $size, $AvalangUiDll)
} else {
    Write-Host "avalang_ui.dll    no encontrado (build estatico o AVA_BUILD_UI=OFF)"
}
if ($AvaCli -and (Test-Path $AvaCli)) {
    $size = (Get-Item $AvaCli).Length
    Write-Host ("ava_cli.exe       {0:N0} bytes  ({1})" -f $size, $AvaCli)
}

Write-Host ""
Write-Host "====================================================================="
Write-Host "7/8. Memoria de VM y startup (samples\test\main.ava, 5 corridas)"
Write-Host "====================================================================="
$startupScript = "samples\test\main.ava"
if (-not (Test-Path $startupScript)) {
    Write-Host "[WARN] $startupScript no existe, se omite esta seccion."
} else {
    $r = Measure-Run -Exe $AvaCli -ScriptPath $startupScript -Runs 5
    $avg = ($r.Times | Measure-Object -Average).Average
    $formatted = ($r.Times | ForEach-Object { "{0:N1}" -f $_ }) -join ", "
    Write-Host ("startup promedio: {0:N1} ms  ({1})" -f $avg, $formatted)
    if ($r.PeakMemBytes -gt 0) {
        Write-Host ("memoria pico (working set): {0:N0} KB" -f ($r.PeakMemBytes / 1KB))
    } else {
        Write-Host "memoria pico: no se pudo muestrear (el proceso termino demasiado rapido para el polling)"
    }
}

Write-Host ""
Write-Host "====================================================================="
Write-Host "9. Rendimiento (scripts\benchmark.ava -- fib(27) recursivo)"
Write-Host "====================================================================="
$benchScript = "scripts\benchmark.ava"
if (-not (Test-Path $benchScript)) {
    Write-Host "[WARN] $benchScript no existe, se omite esta seccion."
} else {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $AvaCli $benchScript | Out-Null
    $sw.Stop()
    Write-Host ("fib(27): {0:N1} ms" -f $sw.Elapsed.TotalMilliseconds)
}

Write-Host ""
Write-Host "====================================================================="
Write-Host "Listo. Estos numeros son una linea base local, no un benchmark"
Write-Host "formal -- comparalos build a build (antes/despues de un cambio de"
Write-Host "optimizacion) en la misma maquina, no contra numeros de otra PC."
Write-Host "====================================================================="
