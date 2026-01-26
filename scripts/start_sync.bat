@echo off
chcp 65001 >nul
echo Starting Auto Git Sync Script...
echo Press Ctrl+C to stop the script
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is not installed or not in PATH
    pause
    exit /b 1
)

REM Change to parent directory (project root)
cd ..

REM Run simplified script (no extra dependencies required)
echo Running simplified auto git sync script...
echo Monitoring directory: %CD%
echo.
python scripts\auto_git_sync_simple.py

pause