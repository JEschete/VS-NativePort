@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Check-Dependencies.ps1"
if errorlevel 1 (
  pause
  exit /b 1
)
start "" "%~dp0VS.Launcher.exe"
