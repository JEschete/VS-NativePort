@echo off
rem Build VagrantStory.Native.sln.  Usage: build-native.cmd [Debug^|Release] [extra msbuild args]
setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
if not defined MSBUILD (
  echo MSBuild not found. Install Visual Studio with the C++ workload and the Clang toolset.
  exit /b 1
)
"%MSBUILD%" "%~dp0VagrantStory.Native.sln" /m /restore /nologo /v:minimal /p:Configuration=%CONFIG% /p:Platform=x64 %2 %3 %4 %5
exit /b %ERRORLEVEL%
