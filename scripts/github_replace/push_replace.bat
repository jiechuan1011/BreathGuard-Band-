@echo off
REM Usage: push_replace.bat owner repo src_folder [branch] [commit_msg]

SETLOCAL
if "%1"=="" (
  echo Usage: push_replace.bat owner repo src_folder [branch] [commit_msg]
  exit /b 1
)

set OWNER=%1
set REPO=%2
set SRC=%3
set BRANCH=%4
set COMMIT=%5

if "%BRANCH%"=="" set BRANCH=main
if "%COMMIT%"=="" set COMMIT=Replace files via batch

REM Allow passing token via env GITHUB_TOKEN or as --token argument to python
echo Owner: %OWNER%
echo Repo: %REPO%
echo Source folder: %SRC%
echo Branch: %BRANCH%
echo Commit message: %COMMIT%

python "%~dp0push_replace.py" %OWNER% %REPO% "%SRC%" %BRANCH% "%COMMIT%"
if ERRORLEVEL 1 (
  echo Script finished with errors.
  exit /b 1
)

echo Done.
ENDLOCAL
