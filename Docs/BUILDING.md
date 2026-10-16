# Building Vagrant Story Native

This file is the single source of truth for getting from a fresh clone to a playable build.
**Keep it current:** any change to prerequisites, scripts, dependencies, output layout or the extraction
step must update this file in the same commit (plan D19).

> **Current state (Phase 1a).** The solution builds and the tests pass. `VS.Desktop` opens a window, reads the
> disc, and logs controller input. It is **not playable yet**. Each section says what works **today**
> and what is **planned**. Planned items are marked ⏭ and turn into normal instructions when they land.

---

## 1. Requirements

### 1.1 To build and play

| Requirement | Version | Notes |
|---|---|---|
| Windows | 10 or 11, x64 | x64 only (plan D4) |
| Visual Studio | **2026 (18.x)**, any edition | The VS 2022 Clang toolset is not tested. |
| VS workload | **Desktop development with C++** | |
| VS components | **C++ Clang Compiler for Windows**, **MSBuild support for LLVM (clang-cl) toolset**, **vcpkg package manager** | All projects use the `ClangCL` toolset. vcpkg is the copy bundled with VS (`$(VCInstallDir)vcpkg`); you do not need a separate vcpkg install or `vcpkg integrate`. |
| Git | any recent | |
| Internet on the first build | | vcpkg downloads and builds the dependencies (SDL3, Catch2, nlohmann-json). This takes several minutes the first time and is cached after that. |
| **Your own copy of the game** | Vagrant Story (USA), **SLUS-01040** | See §3. No game data is in this repository. |
| ffmpeg ⏭ | 6.0+ with `libsvtav1` | Needed only by the asset extractor, to convert the intro movie (ASSET_PIPELINE §6.7). `winget install Gyan.FFmpeg` gives a suitable build. It is never linked or shipped. |

Tested configuration: VS 2026 Community 18.10, MSVC 14.51, Windows SDK 10.0.28000, ffmpeg 8.1.1.

### 1.2 Only for working on the port itself (not needed to play)

| Requirement | Used by |
|---|---|
| Python 3.10+ and `pip install -r tools\requirements.txt` (libclang 18.1.1, PyYAML) | `tools\ledger`, `tools\naming`, `tools\port`, `tools\scaffold` |
| `resources\rood-reverseWin` (in the repo) | Read-only reference that those tools parse. The build never uses it. |
| Optional: `git submodule update --init` | Fetches `resources\rood-reverse` (upstream decomp) and the Ghidra PSX loader. Neither is needed to build. |
| Optional: external reference tools | See [TOOLS_INVENTORY.md](TOOLS_INVENTORY.md) |

---

## 2. Get the source

```cmd
git clone https://github.com/JEschete/VS-NativePort.git
cd VS-NativePort
```

---

## 3. Provide the game disc

You need a 1:1 dump of **Vagrant Story (USA), SLUS-01040**: one `.bin` (MODE2/2352, raw sectors) plus its `.cue`.
Put both files here (the folder is git-ignored):

```
disks\SLUS-01040.bin
disks\SLUS-01040.cue
```

The `.cue` must name the `.bin` by file name, e.g. `FILE "SLUS-01040.bin" BINARY` / `TRACK 01 MODE2/2352`.
The dump this project is developed against has `SLUS-01040.bin` **SHA-1 `38C63DB6C49A06B91D87CD5A4E31B1EC68FE212C`**
(750,643,152 bytes). Check yours in PowerShell:

```powershell
Get-FileHash disks\SLUS-01040.bin -Algorithm SHA1
```

Other dumps of the same release should work. The extractor warns, but does not stop, if the hash differs.
A `.bin` containing only cooked 2048-byte sectors, or an extracted file folder, is **not** enough: the intro movie and
the ending audio live in raw form-2 sectors.

---

## 4. Build

### 4.1 Command line

```cmd
build-native.cmd            :: Debug
build-native.cmd Release
```

`build-native.cmd` finds MSBuild with `vswhere`, restores the vcpkg packages (into `vcpkg_installed\`), and builds
`VagrantStory.Native.sln` for x64. Any extra arguments are passed to MSBuild.

### 4.2 Visual Studio

1. Open `VagrantStory.Native.sln`.
2. Pick **Debug | x64** or **Release | x64**.
3. Set **VS.Desktop** as the startup project (it is the default).
4. Press F5. The first build restores the vcpkg packages automatically.

### 4.3 Output

Everything goes to `build\<Config>\` (git-ignored):

| File | What it is |
|---|---|
| `VS.Desktop.exe` + `SDL3.dll` | the game |
| `VS.Extract.exe` | the asset extractor (§5) |
| `VS.Tests.exe` | unit tests |
| `vs-native.log` | the log written by `VS.Desktop` at run time |

Warnings are errors (`/W4 /WX`). A clean build has zero warnings.

---

## 5. Convert the game data (asset extraction)

**Planned ⏭ (Docs\ASSET_PIPELINE.md):**

```cmd
extract-native-assets.cmd                      :: uses disks\SLUS-01040.cue, writes Native\data\
extract-native-assets.cmd --disc D:\path\to\SLUS-01040.cue --ffmpeg C:\path\to\ffmpeg.exe
```

- This converts **every** file on the disc into modern formats under `Native\data\` (git-ignored): JSON, glTF,
  PNG, WAV, FLAC and AV1 MKV. It writes `Native\data\manifest.json`.
- Run it once after cloning, and again whenever the extractor changes. The game tells you when that is needed.
- Runs are incremental. `--force` rebuilds everything.

**Today:** the extractor can list the disc and copy its regular files. These are diagnostic commands only; the
game does not need their output.

```cmd
build\Debug\VS.Extract.exe list    disks\SLUS-01040.cue
build\Debug\VS.Extract.exe extract disks\SLUS-01040.cue build\disc
```

---

## 6. Run

```cmd
build\Release\VS.Desktop.exe
```

| Option | Meaning |
|---|---|
| `--disc <path>` | Use this disc (`.cue` or extracted folder). By default the exe looks for the `VS_DISC` environment variable, then for `disks\SLUS-01040.cue` (or `SLUS-01040\`) in its own folder or any parent folder, which finds the repo copy in a dev build. |

**Today:** it opens a window, reads the disc, clears the screen, and logs controller and keyboard presses to
`vs-native.log`.

⏭ **After extraction lands:** the game reads only `Native\data\` (plus `Native\Content\patches\` and `mods\`)
and never reads the disc. `--disc` moves to `extract-native-assets.cmd`. If `Native\data\` is missing or out of
date, the game stops with a message that says to run the extractor.

### Controls (today)

| Input | Notes |
|---|---|
| Any SDL3-supported gamepad | Mapped to the PlayStation pad layout (Cross/Circle/…). |
| Keyboard | Arrows = D-pad · X = Cross · C = Circle · Z = Square · S = Triangle · Q/E = L1/R1 · 1/3 = L2/R2 · Enter = Start · Backspace = Select (`Native\Input`). The remapping UI is a Phase 2 item. |

---

## 7. Test

```cmd
test-native.cmd             :: builds Debug, then runs VS.Tests.exe
test-native.cmd Release
test-native.cmd Debug "[core]"   :: any Catch2 arguments after the config ([core], [input], [content], [disc], ...)
```

⏭ The round-trip tests for the extractor are tagged `[extract]` and need the disc (ASSET_PIPELINE §7).

---

## 8. Port tooling (developers)

These scripts regenerate committed files. They are not part of the build.

| Command | What it does |
|---|---|
| `python tools\ledger\build_ledgers.py` | Rebuild `Docs\ledgers\` from the decomp (it keeps the status columns you filled in) |
| `python tools\port\decomp_index.py` | libclang index of the decomp → `build\port\index.json` |
| `python tools\port\data_inventory.py` | Global data inventory → `build\port\data.json` |
| `python tools\port\gen_decomp_headers.py` | Generated headers that `decomp_index.py` needs |
| `python tools\naming\merge_names.py --check-all` | Validate the function catalog naming |
| `python tools\scaffold\gen_solution.py` | Regenerate the `.sln` / `.vcxproj` files. **Run it after any project change**; do not hand-edit the project files. |

**Legacy, do not use:** `tools\Setup-Windows.ps1`, `tools\Probe-Native.ps1` and `tools\windows.py` belong to the
earlier, abandoned emulator-style port (VS 2022/CMake, `vagrant-native.exe`). They are not part of this build.

---

## 9. Troubleshooting

| Symptom | Fix |
|---|---|
| `MSBuild not found` | Install VS 2026 with the **Desktop development with C++** workload. |
| `The build tools for ClangCL cannot be found` | Add the two Clang components from §1.1 in the Visual Studio Installer. |
| `SDL3/SDL.h` not found, or vcpkg errors | Add the **vcpkg package manager** component, check the internet connection, delete `vcpkg_installed\`, and build again. |
| `no disc found` in `vs-native.log` | Put the files at `disks\SLUS-01040.cue/.bin`, or pass `--disc`. |
| `cue sheet has no FILE entry` / the bin cannot be opened | The `.cue` must reference the `.bin` by a name that exists next to it. |
| The window opens and closes | Read `build\<Config>\vs-native.log`. |

---

## 10. Clean

```cmd
rmdir /s /q build             :: binaries, intermediates, build\port tool output
rmdir /s /q vcpkg_installed   :: forces the dependencies to be restored again
rmdir /s /q Native\data       :: ⏭ converted game data (re-run the extractor)
```
