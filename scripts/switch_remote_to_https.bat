@echo off
chcp 65001 >nul
cd /d "%~dp0"
cd ..
python scripts\switch_remote_to_https.py
if errorlevel 1 pause
