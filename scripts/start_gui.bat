@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist "auto_git_sync_gui.py" (
    echo 错误: 未找到 auto_git_sync_gui.py
    pause
    exit /b 1
)
python auto_git_sync_gui.py
if errorlevel 1 pause
