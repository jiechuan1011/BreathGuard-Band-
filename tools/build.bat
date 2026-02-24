@echo off
REM 一键安装依赖并使用 pyinstaller 打包 GUI 程序

REM 输出当前 python 信息，方便排查环境
echo Python 路径：
where python
python --version
echo.

REM 1. 安装必要的 Python 包
python -m pip install --upgrade pip
python -m pip install pyserial pyinstaller platformio
echo.

REM 查看 pyinstaller 模块是否可见
python -c "import sys,pkgutil; print('sys.path=',sys.path); print('pyinstaller loader=',pkgutil.find_loader('pyinstaller')); print('PyInstaller loader=',pkgutil.find_loader('PyInstaller'))"
echo.

REM 2. 切换到当前脚本所在目录（tools）
cd /d "%~dp0"

REM 清理旧的打包目录，避免使用缓存exe
if exist build rd /s /q build
if exist dist rd /s /q dist

REM 3. 使用 PyInstaller 生成单文件窗口程序。模块名区分大小写。
python -m PyInstaller --onefile --windowed flash_gui.py

echo.
echo 打包完成！请在 dist \ 目录中找到 flash_gui.exe
echo.
echo 如需重打包，可先删除 build 和 dist 目录然后重新运行本批处理。
pause
