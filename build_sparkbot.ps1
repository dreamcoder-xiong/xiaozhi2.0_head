$ErrorActionPreference = "Stop"
$env:MSYSTEM = $null
$env:MSYSTEM_PREFIX = $null
$env:MSYSTEM_CHOST = $null
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYSTEM_PREFIX -ErrorAction SilentlyContinue
Remove-Item Env:MSYSTEM_CHOST -ErrorAction SilentlyContinue

# 官方 1.8.6 要求 IDF >= 5.4.0，使用已装的 5.5.5
$env:IDF_PYTHON_ENV_PATH = "F:\Espressif\python_env\idf5.5_py3.11_env"
Set-Location "F:\Company\xiaozhi-esp32-1.8.6-sparkbot"

& "F:\Espressif\frameworks\esp-idf-v5.5.5\export.ps1" | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] export.ps1 failed"; exit 1 }

Write-Host "[STEP 1/3] set-target esp32s3 ..."
idf.py set-target esp32s3
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] set-target failed"; exit 1 }

Write-Host "[STEP 2/3] build ..."
idf.py build
if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] build failed"; exit 1 }

Write-Host "[STEP 3/3] build success!"
