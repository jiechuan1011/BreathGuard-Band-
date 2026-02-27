@echo off
rem Build script for port_tester.exe using project virtualenv Python when available
cd /d %~dp0

set VENV_PY=D:\大创\Diabetes_Screening_System\.venv\Scripts\python.exe

if exist "%VENV_PY%" (
  echo Using virtualenv python: %VENV_PY%
  "%VENV_PY%" -m pip install -r requirements.txt
  echo Ensuring PySimpleGUI is the upstream implementation...
  "%VENV_PY%" -m pip install --force-reinstall git+https://github.com/PySimpleGUI/PySimpleGUI.git
  echo Building one-file windowed executable with PyInstaller (via module)...
  "%VENV_PY%" -m PyInstaller --noconfirm --onefile --windowed port_tester_tk.py
  set ERR=%ERRORLEVEL%
  if "%ERR%"=="0" (
    echo Build finished. See ..\..\dist\port_tester.exe
  ) else (
    echo Build failed with code %ERR%
  )
) else (
  echo Virtualenv python not found at %VENV_PY%. Falling back to system python.
  python -m pip install -r requirements.txt
  python -m pip install --upgrade pyinstaller
  python -m PyInstaller --noconfirm --onefile --windowed port_tester_tk.py
  if %ERRORLEVEL% EQU 0 (
    echo Build finished. See ..\..\dist\port_tester.exe
  ) else (
    echo Build failed with code %ERRORLEVEL%
  )
)

