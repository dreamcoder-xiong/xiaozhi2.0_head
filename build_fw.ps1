# 清除 msys 环境变量
Remove-Item Env:OSTYPE -ErrorAction SilentlyContinue
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYSTEM_PREFIX -ErrorAction SilentlyContinue

$env:PYTHONUTF8 = "1"
$env:CCACHE_DISABLE = "1"
. F:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1

Set-Location F:\head_code
Write-Host "=== Full clean ==="
idf.py fullclean
Write-Host "=== Build ==="
idf.py build
