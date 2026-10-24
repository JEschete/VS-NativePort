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

## License

Except where otherwise noted, the project's original contributions are free software: you can redistribute
them and/or modify them under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version
(`GPL-3.0-or-later`). See [LICENSE](LICENSE) for the full terms.

This project is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

Third-party code, dependencies and reference repositories retain their respective licenses. This includes
upstream CC0 material; the GPL grant for original project contributions does not revoke those upstream
permissions. See [THIRD_PARTY.md](THIRD_PARTY.md) and [Docs/TOOLS_INVENTORY.md](Docs/TOOLS_INVENTORY.md).

This grant covers only rights held by the project contributors. It does not grant rights held by the
original game's rights holders in its code, assets, names or trademarks. Game data is not included;
you must supply your own copy of the game.
