# Rood Reverse

This folder is the Windows-maintained matching project. Use the root
`tools/Setup-Windows.ps1` and Visual Studio `Decomp` project, not the historical
Linux quickstart below. See [Windows Migration](../../docs/WINDOWS_MIGRATION.md)
for required Windows historical compilers and verification limits.
The upstream source/configuration is authoritative; `upstream.json` records it.

The source now integrates `noprompt/rood-reverse`'s `decomp/verified-functions`
branch, with their work taking precedence over overlapping local source.
All 289 C units pass the isolated historical compiler sweep, and the latest
WSL build reproduces the retail outputs byte-for-byte. That build includes
inherited assembly and does not certify assembly-free source completion or a
native runtime. Earlier local match counts describe superseded source. See
[Local Progress](DECOMP_PROGRESS.md).
The progress badges below use this Windows snapshot's local objdiff report.
They measure byte matches, include inherited assembly, and are not a measure of
readable C completion. The Linux quickstart remains upstream reference material,
not a Windows build workflow. Build verification does not synchronize source;
explicit sync refuses local changes.

A decompilation project for Vagrant Story for PlayStation 1, with a goal of enabling interoperability with modern hardware and fixing bugs in the original game.

This project does not contain any of the original files from the game disk, you must own a copy of the game.

Many thanks to the [Chrono Cross Decompilation](https://github.com/jdperos/chrono-cross-decomp) for much of the sound library code.

Heavily inspired by and indebted to the incredible work of the [sotn-decomp](https://github.com/Xeeynamo/sotn-decomp) team and the [PS1/PS2 decompilation](https://discord.gg/hKx3FJJgrV) Discord server.

Grateful to the authors of Data Crystal’s [Vagrant Story](https://datacrystal.tcrf.net/wiki/Vagrant_Story) page for their excellent work and insights.

Thanks also to the following amazing projects (and their dependencies):
- [splat](https://github.com/ethteck/splat)
- [maspsx](https://github.com/mkst/maspsx)
- [decomp.me](https://github.com/decompme/decomp.me)
- [Decomp permuter](https://github.com/simonlindholm/decomp-permuter)
- [Old GCC](https://github.com/decompals/old-gcc)
- [mkpsxiso](https://github.com/Lameguy64/mkpsxiso)
- [objdiff](https://github.com/encounter/objdiff)
- [decomp.dev](https://decomp.dev/)

## Status

### Windows Snapshot Progress: 2026-10-14

The latest local objdiff report measures **97.81017% matched code overall**
(1,894,184 / 1,936,592 bytes), **98.695786% for BATTLE.PRG**
(510,652 / 517,400), and **98.71365% for the broader BATTLE category**
(517,840 / 524,588). Overall, 4,084 / 4,156 functions are byte-matched.
The two readable C functions temporarily replaced by upstream assembly have
now been restored and freshly matched. Together with `func_8008AC78`, current
BATTLE.PRG progress is 1,412 bytes above the pre-import accepted total. The WSL
build still reproduces all retail outputs; assembly-backed bytes are not
counted as reconstructed C progress.

The badge board below uses local objdiff code-match percentages, not decomp.dev's
source-progress metric. `INCLUDE_ASM` output may still reproduce retail bytes in
the full build, but is not counted as reconstructed C progress. A 100% badge is
therefore not a claim that every function is readable C.

![Overall matched code: 97.81% local byte match](https://img.shields.io/badge/Overall%20matched%20code-97.81%25-blue)

### Core libraries
![Entry point and libraries: 77.39% local byte match](https://img.shields.io/badge/Entry%20point%20and%20libraries-77.39%25-blue)

### Main game engine
![Main game engine: 98.71% local byte match](https://img.shields.io/badge/Main%20game%20engine-98.71%25-blue)
![BATTLE.PRG: 98.70% local byte match](https://img.shields.io/badge/BATTLE.PRG-98.70%25-blue)
![Main engine bootstrapper: 100.00% local byte match](https://img.shields.io/badge/Main%20engine%20bootstrapper-100.00%25-blue)
![Fullscreen effects: 100.00% local byte match](https://img.shields.io/badge/Fullscreen%20effects-100.00%25-blue)

### Effects
![Ability effects: 100.00% local byte match](https://img.shields.io/badge/Ability%20effects-100.00%25-blue)


![Ability effect dispatcher: 100.00% local byte match](https://img.shields.io/badge/Ability%20effect%20dispatcher-100.00%25-blue)
![Analyze: 100.00% local byte match](https://img.shields.io/badge/Analyze-100.00%25-blue)
![Solid Shock: 100.00% local byte match](https://img.shields.io/badge/Solid%20Shock-100.00%25-blue)
![Lightning Bolt: 100.00% local byte match](https://img.shields.io/badge/Lightning%20Bolt-100.00%25-blue)
![Vulcan Lance: 100.00% local byte match](https://img.shields.io/badge/Vulcan%20Lance-100.00%25-blue)
![Spirit Surge: 100.00% local byte match](https://img.shields.io/badge/Spirit%20Surge-100.00%25-blue)
![Dark Chant: 100.00% local byte match](https://img.shields.io/badge/Dark%20Chant-100.00%25-blue)
![Exorcism: 100.00% local byte match](https://img.shields.io/badge/Exorcism-100.00%25-blue)
![Explosion: 100.00% local byte match](https://img.shields.io/badge/Explosion-100.00%25-blue)
![Thunderburst: 100.00% local byte match](https://img.shields.io/badge/Thunderburst-100.00%25-blue)
![Flame Sphere: 100.00% local byte match](https://img.shields.io/badge/Flame%20Sphere-100.00%25-blue)
![Gaea Strike: 100.00% local byte match](https://img.shields.io/badge/Gaea%20Strike-100.00%25-blue)
![Avalanche: 100.00% local byte match](https://img.shields.io/badge/Avalanche-100.00%25-blue)
![Radial Surge: 100.00% local byte match](https://img.shields.io/badge/Radial%20Surge-100.00%25-blue)
![Meteor: 100.00% local byte match](https://img.shields.io/badge/Meteor-100.00%25-blue)
![Tornado: 100.00% local byte match](https://img.shields.io/badge/Tornado-100.00%25-blue)
![Thunderbolt: 100.00% local byte match](https://img.shields.io/badge/Thunderbolt-100.00%25-blue)
![Acid Flow: 100.00% local byte match](https://img.shields.io/badge/Acid%20Flow-100.00%25-blue)
![Judgment: 100.00% local byte match](https://img.shields.io/badge/Judgment-100.00%25-blue)
![Apocalypse: 100.00% local byte match](https://img.shields.io/badge/Apocalypse-100.00%25-blue)
![Numbing Needle: 100.00% local byte match](https://img.shields.io/badge/Numbing%20Needle-100.00%25-blue)
![Bloody Sin: 100.00% local byte match](https://img.shields.io/badge/Bloody%20Sin-100.00%25-blue)
![Last Ascension: 100.00% local byte match](https://img.shields.io/badge/Last%20Ascension-100.00%25-blue)
![Wyrm Scorn: 100.00% local byte match](https://img.shields.io/badge/Wyrm%20Scorn-100.00%25-blue)
![Rending Gale: 100.00% local byte match](https://img.shields.io/badge/Rending%20Gale-100.00%25-blue)
![Cherry Ronde: 100.00% local byte match](https://img.shields.io/badge/Cherry%20Ronde-100.00%25-blue)
![Papillon Reel: 100.00% local byte match](https://img.shields.io/badge/Papillon%20Reel-100.00%25-blue)
![Sunder: 100.00% local byte match](https://img.shields.io/badge/Sunder-100.00%25-blue)
![Swallow Slash: 100.00% local byte match](https://img.shields.io/badge/Swallow%20Slash-100.00%25-blue)
![Advent Sign: 100.00% local byte match](https://img.shields.io/badge/Advent%20Sign-100.00%25-blue)
![Mistral Edge: 100.00% local byte match](https://img.shields.io/badge/Mistral%20Edge-100.00%25-blue)
![Black Nebula: 100.00% local byte match](https://img.shields.io/badge/Black%20Nebula-100.00%25-blue)
![Bear Claw: 100.00% local byte match](https://img.shields.io/badge/Bear%20Claw-100.00%25-blue)
![Accursed Umbra: 100.00% local byte match](https://img.shields.io/badge/Accursed%20Umbra-100.00%25-blue)
![Iron Ripper: 100.00% local byte match](https://img.shields.io/badge/Iron%20Ripper-100.00%25-blue)
![Emetic Bomb: 100.00% local byte match](https://img.shields.io/badge/Emetic%20Bomb-100.00%25-blue)
![Riskbreak: 100.00% local byte match](https://img.shields.io/badge/Riskbreak-100.00%25-blue)
![Gravis Aether: 100.00% local byte match](https://img.shields.io/badge/Gravis%20Aether-100.00%25-blue)
![Trinity Pulse: 100.00% local byte match](https://img.shields.io/badge/Trinity%20Pulse-100.00%25-blue)
![Quickshock: 100.00% local byte match](https://img.shields.io/badge/Quickshock-100.00%25-blue)
![Hex Flux: 100.00% local byte match](https://img.shields.io/badge/Hex%20Flux-100.00%25-blue)
![Scythe Wind: 100.00% local byte match](https://img.shields.io/badge/Scythe%20Wind-100.00%25-blue)
![Giga Tempest: 100.00% local byte match](https://img.shields.io/badge/Giga%20Tempest-100.00%25-blue)
![Spiral Scourge: 100.00% local byte match](https://img.shields.io/badge/Spiral%20Scourge-100.00%25-blue)
![Death Wail: 100.00% local byte match](https://img.shields.io/badge/Death%20Wail-100.00%25-blue)
![Sanctus Flare: 100.00% local byte match](https://img.shields.io/badge/Sanctus%20Flare-100.00%25-blue)
![Lotus Palm: 100.00% local byte match](https://img.shields.io/badge/Lotus%20Palm-100.00%25-blue)
![Vertigo: 100.00% local byte match](https://img.shields.io/badge/Vertigo-100.00%25-blue)
![Vermillion Aura: 100.00% local byte match](https://img.shields.io/badge/Vermillion%20Aura-100.00%25-blue)
![HP-up: 100.00% local byte match](https://img.shields.io/badge/HP-up-100.00%25-blue)
![PLG224: 100.00% local byte match](https://img.shields.io/badge/PLG224-100.00%25-blue)
![PLG232: 100.00% local byte match](https://img.shields.io/badge/PLG232-100.00%25-blue)
![PLG236: 100.00% local byte match](https://img.shields.io/badge/PLG236-100.00%25-blue)
![PLG242: 100.00% local byte match](https://img.shields.io/badge/PLG242-100.00%25-blue)
![PLG246: 100.00% local byte match](https://img.shields.io/badge/PLG246-100.00%25-blue)
![PLG250: 100.00% local byte match](https://img.shields.io/badge/PLG250-100.00%25-blue)
![PLG255: 100.00% local byte match](https://img.shields.io/badge/PLG255-100.00%25-blue)
![PLG260: 100.00% local byte match](https://img.shields.io/badge/PLG260-100.00%25-blue)
![PLG263: 100.00% local byte match](https://img.shields.io/badge/PLG263-100.00%25-blue)
![PLG265: 100.00% local byte match](https://img.shields.io/badge/PLG265-100.00%25-blue)
![PLG266: 100.00% local byte match](https://img.shields.io/badge/PLG266-100.00%25-blue)
![PLG272: 100.00% local byte match](https://img.shields.io/badge/PLG272-100.00%25-blue)
![PLG273: 100.00% local byte match](https://img.shields.io/badge/PLG273-100.00%25-blue)
![PLG291: 100.00% local byte match](https://img.shields.io/badge/PLG291-100.00%25-blue)
![PLG297: 100.00% local byte match](https://img.shields.io/badge/PLG297-100.00%25-blue)
![PLG300: 100.00% local byte match](https://img.shields.io/badge/PLG300-100.00%25-blue)
![PLG301: 100.00% local byte match](https://img.shields.io/badge/PLG301-100.00%25-blue)

### Menus
![Menus: 100.00% local byte match](https://img.shields.io/badge/Menus-100.00%25-blue)


![Top level menu and shared menu functionality: 100.00% local byte match](https://img.shields.io/badge/Top%20level%20menu%20and%20shared%20menu%20functionality-100.00%25-blue)
![Magic menu: 100.00% local byte match](https://img.shields.io/badge/Magic%20menu-100.00%25-blue)
![Break Arts menu: 100.00% local byte match](https://img.shields.io/badge/Break%20Arts%20menu-100.00%25-blue)
![Battle Abilities menu: 100.00% local byte match](https://img.shields.io/badge/Battle%20Abilities%20menu-100.00%25-blue)
![Items menu: 100.00% local byte match](https://img.shields.io/badge/Items%20menu-100.00%25-blue)
![Status menu: 100.00% local byte match](https://img.shields.io/badge/Status%20menu-100.00%25-blue)
![Map menu: 100.00% local byte match](https://img.shields.io/badge/Map%20menu-100.00%25-blue)
![Data menu: 100.00% local byte match](https://img.shields.io/badge/Data%20menu-100.00%25-blue)
![Options menu: 100.00% local byte match](https://img.shields.io/badge/Options%20menu-100.00%25-blue)
![Score menu: 100.00% local byte match](https://img.shields.io/badge/Score%20menu-100.00%25-blue)
![Loot menu: 100.00% local byte match](https://img.shields.io/badge/Loot%20menu-100.00%25-blue)
![Item setup menu: 100.00% local byte match](https://img.shields.io/badge/Item%20setup%20menu-100.00%25-blue)
![Container menu: 100.00% local byte match](https://img.shields.io/badge/Container%20menu-100.00%25-blue)
![Quick Manual menu: 100.00% local byte match](https://img.shields.io/badge/Quick%20Manual%20menu-100.00%25-blue)
![Misc menus: 100.00% local byte match](https://img.shields.io/badge/Misc%20menus-100.00%25-blue)

### Title and Ending Sequence
![Title sequence and menu: 100.00% local byte match](https://img.shields.io/badge/Title%20sequence%20and%20menu-100.00%25-blue)
![Credits: 100.00% local byte match](https://img.shields.io/badge/Credits-100.00%25-blue)

## Editable content
The entirety of the game's menu code and data is decompiled, including all files in `MENU` and `SMALL`, with the exceptions of `MENU/MCDATA.BIN`, `MENU/OPMCIMG1.BIN` and `MENU/OPMCIMG2.BIN`.

Much of the menu code has been refactored and is readable, and the menu executables are completely relocatable. It should be possible to rewrite the menu code arbitrarily, the method and data addresses in other modules will be relinked appropriately.

Due to the presence of a large number of empty sectors at the start of each directory on the disk, it should also be possible to rebuild the disk without affecting sector-based data loading in `BATTLE.PRG`, as long as the number of dummy sectors is adjusted accordingly.

## (Naive) Quickstart
0. n.b.: You may want to consider limiting the jobs spawned by make to something sensible like `-j8`.
1. Fork the project
2. Install system dependencies and copy disk image (see "Setup" below)
3. Run `make -j` to perform an initial split & build
4. Pick a function to decompile, use `make decompme` to create a scratch on decomp.me
5. Claim and decompile the scratch
6. Copy the decompiled code where it belongs 
7. Optionally use `make permute` if needed
8. Run `make remake -j`, and finally `make format` if the code compiles successfully
9. Open a PR

## Setup
The project is mostly self-configuring but requires a minimal amount of setup.
- Make sure the following packages are installed, older versions may also work but this is not tested. This configuration works as-is on Ubuntu 26.04 on WSL; depending on your distro you may need additional packages.

| Package                      | Minimum Version |
|------------------------------|-----------------|
| clang-format                 | 21.1.8          |
| g++                          | 15.2.0          |
| binutils-mipsel-linux-gnu    | 2.45.90         |
| make                         | 4.4.            |
| python3                      | 3.14.4          |
| python3-venv                 | 3.14.4          |
| unzip                        | 6.00            |
| wget                         | 1.25.0          |

Take a look at `./Dockerfile` for a minimal setup on Ubuntu
- Dump your original disk to `disks/SLUS-01040.bin`
- Run `make -j` to finish installation and perform an initial build

## Build targets
- `make -j` should be all that is needed most of the time. The first execution will configure the remaining dependencies in the `tools` directory and extract the files from the disk; from then on it will perform a minimal rebuild. 
- `make decompme TARGET=path/to/nonmatchings/source.s` uploads the target function to a new decomp.me scratch.
- `make permute TARGET=path/to/nonmatchings/source.s` invokes the permuter for the target assembly file. 
- `make remake -j` will delete and re-split the targets; this can be necessary if the dependency management has failed to identify everything that needs rebuilding (in which case a bug report is appreciated!), or if the configuration has changed enough to be no longer valid (e.g. orphaned .d files). It's good practice to run this before submitting a pull request to make sure everything still works.
- `make format` formats the source files and symbol addresses for consistency. Please run this before submitting a PR.
- `make clean` simply deletes the `build` and `nonmatchings` directories
- `make clean-all` will completely reset the project, including dropping any source changes and data files (but not the disk image). Use with caution.
