# Vagrant Story Native

A Windows x64 native source port of *Vagrant Story* (PS1, SLUS-01040), written in modern C++. It does not emulate the
PlayStation; the game's logic lives in the C++ systems under `Native\`. You must supply your own copy of the game.

- **Build and play:** [Docs/BUILDING.md](Docs/BUILDING.md)
- **Plan and status:** [Docs/NATIVE_SOURCE_PORT_PLAN.md](Docs/NATIVE_SOURCE_PORT_PLAN.md)
- **Game data conversion:** [Docs/ASSET_PIPELINE.md](Docs/ASSET_PIPELINE.md)

Quick start (details and prerequisites are in BUILDING.md):

```cmd
git clone https://github.com/JEschete/VS-NativePort.git
cd VS-NativePort
:: copy your SLUS-01040.bin / .cue into disks\
build-native.cmd Release
build\Release\VS.Desktop.exe
```
