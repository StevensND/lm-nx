# Run only when requested by the user; build tools never invoke this script.
param([ValidateSet('Nativo','Dawn')][string]$Renderer='Nativo',[switch]$Metricas,[switch]$Capturas,[long]$Volcado=-1)
$ErrorActionPreference='Stop'
$raiz=Split-Path -Parent $PSScriptRoot
$carpeta=Join-Path $raiz $(if($Renderer -eq 'Dawn'){'build/pc-dawn-nmake'}else{'build/pc-nativo-nmake'})
$exe=Join-Path $carpeta $(if($Renderer -eq 'Dawn'){'ProjectLM-NX-PC-Dawn.exe'}else{'ProjectLM-NX-PC.exe'})
if(-not(Test-Path -LiteralPath $exe)){throw "Build $exe first"}
$datos=Join-Path $carpeta 'datos'
foreach($nombre in @('config','guardados','cache')){New-Item -ItemType Directory -Force -Path (Join-Path $datos $nombre)|Out-Null}
$overlay=Join-Path $raiz 'build/discos-switch/hidden_mansion'
if(Test-Path -LiteralPath (Join-Path $overlay 'manifest.txt')){
    $destino=Join-Path $datos 'guardados/hidden_mansion'
    New-Item -ItemType Directory -Force -Path $destino|Out-Null
    Copy-Item -Path (Join-Path $overlay '*') -Destination $destino -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $raiz 'data/pipelines.bin') -Destination (Join-Path $carpeta 'pipelines.bin') -Force
$anteriorBackend=$env:LM_BACKEND;$anteriorLog=$env:LMNX_PERF_LOG;$anteriorDump=$env:LM_FRAME_DUMP
$anteriorVolcado=$env:LMNX_VOLCADO
try{
    $env:LM_BACKEND='vulkan'
    if($Volcado -ge 0){$env:LMNX_VOLCADO="$Volcado"}
    if($Metricas -and $Renderer -eq 'Nativo'){$env:LMNX_PERF_LOG=Join-Path $carpeta ("metricas-"+(Get-Date -Format 'yyyyMMdd-HHmmss')+".csv")}else{Remove-Item Env:LMNX_PERF_LOG -ErrorAction SilentlyContinue}
    if($Capturas){$env:LM_FRAME_DUMP=(Join-Path $carpeta 'capturas')+',300,330,10'}
    $marca=Get-Date -Format 'yyyyMMdd-HHmmss'
    $salida=Join-Path $carpeta ("salida-"+$marca+'.log')
    $errores=Join-Path $carpeta ("errores-"+$marca+'.log')
    Write-Host "Game logs: $salida and $errores"
# User-invoked only. Keep the SDL window visible and wait for it to close. File redirection avoids PowerShell
    # NativeCommandError for normal stderr diagnostics and preserves UTF-8 bytes.
    $proceso=Start-Process -FilePath $exe -ArgumentList @('--window','1280x720','--disc',('"'+(Join-Path $raiz 'orig\GLME01.iso')+'"')) -WorkingDirectory $carpeta -NoNewWindow -RedirectStandardOutput $salida -RedirectStandardError $errores -PassThru -Wait
    if($proceso.ExitCode -ne 0){Write-Host "The game exited with code $($proceso.ExitCode); check the logs."}
}finally{$env:LM_BACKEND=$anteriorBackend;$env:LMNX_PERF_LOG=$anteriorLog;$env:LM_FRAME_DUMP=$anteriorDump;$env:LMNX_VOLCADO=$anteriorVolcado}
