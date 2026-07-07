@echo off
set PYTHONUTF8=1
set CCACHE_DISABLE=1
call F:\Espressif\frameworks\esp-idf-v5.3.2\export.bat
cd /d F:\head_code
idf.py build
