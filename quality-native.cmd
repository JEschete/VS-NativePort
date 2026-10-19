@echo off
setlocal
pushd "%~dp0" || exit /b 2
set "PYTHON=python"
if exist ".venv-3\Scripts\python.exe" set "PYTHON=.venv-3\Scripts\python.exe"
"%PYTHON%" tools\lint\quality.py %*
set RC=%ERRORLEVEL%
popd
exit /b %RC%