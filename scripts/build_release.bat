@echo off
chcp 65001 >nul
cd /d "%~dp0"
if not exist "build_exe.py" (
    echo 错误: 请在 scripts 目录下运行
    pause
    exit /b 1
)

echo 正在构建 Release 发布包...
python -m pip install pyinstaller watchdog -q 2>nul
python build_release.py
if errorlevel 1 (
    echo 构建失败
    pause
    exit /b 1
)
echo.
echo 发布包已生成在: scripts\release\
dir /b release\*.zip 2>nul
pause
