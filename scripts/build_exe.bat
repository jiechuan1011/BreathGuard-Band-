@echo off
chcp 65001 >nul
echo ============================================================
echo 正在构建 Auto Git Sync 完整版 / 本地版 exe
echo ============================================================

REM 进入 scripts 目录（与 build_exe.py 同目录）
cd /d "%~dp0"
if not exist "auto_git_sync_simple.py" (
    echo 错误: 未找到 auto_git_sync_simple.py，请确保在 scripts 目录下运行本脚本
    pause
    exit /b 1
)

REM 检查 Python
python --version >nul 2>&1
if errorlevel 1 (
    echo 错误: 未找到 Python，请先安装 Python 并加入 PATH
    pause
    exit /b 1
)

REM 安装打包依赖（若未安装）
echo.
echo 检查/安装打包依赖...
pip install pyinstaller watchdog -q
if errorlevel 1 (
    echo 安装依赖失败，可手动执行: pip install -r scripts/requirements-build.txt
    pause
    exit /b 1
)

REM 执行打包
echo.
python build_exe.py
if errorlevel 1 (
    echo 打包失败
    pause
    exit /b 1
)

echo.
echo 生成的 exe 位于: scripts\dist\
dir /b dist\*.exe 2>nul
pause
