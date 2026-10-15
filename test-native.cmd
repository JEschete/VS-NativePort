@echo off
rem Build and run the unit tests.  Usage: test-native.cmd [Debug^|Release] [catch2 args]
setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug
call "%~dp0build-native.cmd" %CONFIG% || exit /b 1
pushd "%~dp0"
"build\%CONFIG%\VS.Tests.exe" %2 %3 %4 %5
set RC=%ERRORLEVEL%
popd
exit /b %RC%
