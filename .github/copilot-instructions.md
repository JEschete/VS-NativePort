# Copilot Instructions

## Project Guidelines
- Never attribute changes to the assistant in commits. Do not add assistant authorship, co-author trailers, signatures, or AI attribution to commit messages or metadata.

## Local environment (development machine)
- **Build:** `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe VagrantStory.Native.sln /m /p:Configuration=Debug /p:Platform=x64`; tests `test-native.cmd Debug`. Regenerate projects with `python tools\scaffold\gen_solution.py` (never hand-edit `.vcxproj`). See `Docs\BUILDING.md`.
- **Game data:** disc image `disks\SLUS-01040.cue/.bin`; extracted data `Native\data` (`build\Debug\VS.Extract.exe extract disks\SLUS-01040.cue Native\data`, needs FFmpeg on PATH: winget Gyan.FFmpeg).
- **External tools:** `C:\Tools` (shown in the solution as `Tools\C-Tools`; inventory in `Docs\TOOLS_INVENTORY.md`). Repository tools in `tools\` (`Tools\tools`).
- **DuckStation (reference emulator):** installed at `C:\Tools\Systems\PSX\Emulators\DuckStation\duckstation-qt-x64-ReleaseLTCG.exe`. Never run or reconfigure the installed copy for tests: `tools\test\duckstation_setup.ps1` makes a portable copy in `build\duckstation` (git-ignored) with its own `settings.ini`.
- **PS1 BIOS:** `F:\Emulators\PlayStation\bios\SCPH1001.BIN` (NTSC-U, used for SLUS-01040; `scph5501.bin` also there). Copies exist under `F:\Emulators\LaunchBox\Emulators\RetroArch\system\`. Never copy a BIOS into the repository.
- **Reference captures:** `tools\test\duckstation_capture.ps1 -SkipIntro -Keys '40:Down,42:X' -Seconds 45,50 -Out build\ds_x` (keys briefly take focus; Z = Cross, X = Circle, Return = Start).
- **Native test runs:** `tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 1800 -At '1500' -Press '1200:Down,1300:Circle'`. Ticks are 1/60 s. **Skip the intro movie by pressing Start (`-SkipIntro` = `--press 1000:Start`)**: the title menu is then up by tick ~1100 instead of ~3300. Crash stacks are in `build\Debug\vs-native.log`.