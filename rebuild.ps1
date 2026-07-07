Remove-Item Env:OSTYPE -ErrorAction SilentlyContinue
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYSTEM_PREFIX -ErrorAction SilentlyContinue
$env:PYTHONUTF8 = "1"
$env:CCACHE_DISABLE = "1"
. F:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1
Set-Location F:\head_code
Write-Host "=== Building ==="
idf.py build
Write-Host "=== Flashing ==="
Set-Location F:\head_code\build
python -m esptool --chip esp32s3 -p COM10 -b 460800 --before default_reset --after hard_reset write_flash "@flash_args"
Write-Host "=== Done ==="
