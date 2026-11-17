@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
if not exist build\scratch\status-terminal mkdir build\scratch\status-terminal
clang-cl /nologo /std:c++latest /W4 /WX /DNOMINMAX /EHsc /MD /Gy /Gw /O1 ^
 /I Native\Game\include /I Native\Game\src /I Native\Engine /I Native\Content ^
 /I Native\Engine\include /I Native\Engine\src /I Native\Content\include /I Native\Content\src ^
 /I Native\Tests\include /I vcpkg_installed\x64-windows\include ^
 tools\port\differentials\status_terminal_live_stubs.cpp ^
 Native\Tests\src\Main.cpp Native\Tests\src\StatusTerminalLiveTests.cpp ^
 Native\Tests\src\StatusTerminalEquipmentTests.cpp Native\Tests\src\StatusTerminalRenderingTests.cpp ^
 Native\Tests\src\StatusTerminalScreenTests.cpp ^
 Native\Game\src\Menu\Status\TerminalEquipment.cpp ^
 Native\Game\src\Menu\Status\TerminalRendering.cpp ^
 Native\Game\src\Menu\Status\TerminalController.cpp ^
 Native\Game\src\Menu\Status\TerminalControllersLive.cpp ^
 Native\Game\src\Menu\Status\TerminalEquipmentLive.cpp ^
 Native\Game\src\Menu\Status\TerminalScreenLive.cpp ^
 Native\Game\src\Menu\Status\ActorSwitching.cpp ^
 Native\Game\src\Menu\Status\ComponentDescriptions.cpp ^
 Native\Game\src\Menu\Status\ComponentDescriptionsLive.cpp ^
 Native\Game\src\Menu\Status\DetailTransitions.cpp ^
 Native\Game\src\Menu\Status\DetailTransitionsLive.cpp ^
 Native\Game\src\Menu\Status\LimbControllers.cpp ^
 Native\Game\src\Menu\Status\LimbControllersLive.cpp ^
 Native\Game\src\Menu\Framework\ItemRows.cpp Native\Game\src\Menu\Framework\ItemRowsLive.cpp ^
 Native\Game\src\Items\EquipmentStats.cpp Native\Game\src\Items\EquipmentAffinity.cpp ^
 Native\Game\src\Battle\HudState.cpp Native\Game\src\Battle\HudStateLive.cpp ^
 Native\Engine\src\Core\Memory\Heap.cpp Native\Engine\src\Core\Memory\HeapLive.cpp ^
 Native\Engine\src\Core\Math\ScalarMath.cpp Native\Engine\src\Core\Math\Distance.cpp ^
 Native\Engine\src\Core\String\GameText.cpp Native\Engine\src\Core\Scratch.cpp ^
 Native\Engine\src\Input\Controller.cpp ^
 Native\Engine\src\Render\EncodedText.cpp Native\Engine\src\Render\EncodedTextLive.cpp ^
 Native\Engine\src\Render\Sprites.cpp Native\Engine\src\Render\SpritesLive.cpp ^
 Native\Engine\src\Render\UiPackets.cpp Native\Engine\src\Render\UiPacketsLive.cpp ^
 vcpkg_installed\x64-windows\lib\Catch2.lib ^
 /Fe:build\scratch\status-terminal\terminal-tests.exe /Fo:build\scratch\status-terminal\ ^
 /link /OPT:REF /ERRORLIMIT:0
if errorlevel 1 exit /b %errorlevel%
set "PATH=%CD%\vcpkg_installed\x64-windows\bin;%PATH%"
build\scratch\status-terminal\terminal-tests.exe "[status-terminal]" %*
exit /b %errorlevel%
