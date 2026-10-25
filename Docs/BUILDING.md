# Building Vagrant Story Native

This file is the single source of truth for getting from a fresh clone to a playable build.
**Keep it current:** any change to prerequisites, scripts, dependencies, output layout or the extraction
step must update this file in the same commit (plan D19).

> **Recorded checkpoint, not a fresh build claim.** See [HANDOFF.md](HANDOFF.md)
> for the source revision, uncommitted work, validation scope and current priority.
> Debug/Release results and exact replay procedures are retained in
> [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md).
> Run the commands in sections 4 and 7 for your revision.
> After the extraction step (§5), `VS.Desktop`
> runs the original boot and title code: NOW LOADING, the publisher and developer logos, the intro movie with the
> opening music, the title screen and its menu (New Game / Continue / Vibration / Sound), driven by gamepad or
> keyboard. Shared room/actor geometry, dialogue, and facial texture animation render in attract and early
> New Game scenes. The manor splash and the reported room-AI/condition-snapshot crashes have focused fixes;
> rendering parity, full cutscene motion, and interactive combat remain incomplete. Audio control timing
> advances. Native audio plays title-menu sounds, prologue music and SFX through sfizz and SDL,
> including the game's stereo/mono setting. Device-demand rendering keeps active voices playing during
> gameplay-thread stalls. All 83 nonempty wave banks import; noise/FM and original reverb presets are
> implemented. Supported effect/dialogue streams and ending XA use native assets and output.
> Full-content listening parity and memory-card support are not established.
> See [HANDOFF.md](HANDOFF.md) for the verified captures, tests, and remaining limits.
> Planned items are marked ⏭ and turn into normal instructions when they land.

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
| Internet on the first build | | vcpkg downloads and builds the dependencies (SDL3, Catch2, nlohmann-json, dav1d, libwebm, dr_libs, sfizz). This takes several minutes the first time and is cached after that. |
| **Your own copy of the game** | Vagrant Story (USA), **SLUS-01040** | See §3. No game data is in this repository. |
| FFmpeg | 6.0+ with `libsvtav1` and the `psxstr` demuxer | Needed only by the extractor, to convert the intro movie (ASSET_PIPELINE §6.7); found on `PATH` or given with `--ffmpeg`. `winget install Gyan.FFmpeg` gives a suitable build. It is never linked or shipped. |

Tested configuration: VS 2026 Community 18.10, MSVC 14.51, Windows SDK 10.0.28000, ffmpeg 8.1.1.

An MSBuild-only installation is insufficient: the selected VS instance must also
contain the C++ MSBuild targets, Windows SDK and ClangCL components above.
The build wrapper selects the latest instance advertising MSBuild; it does not
currently verify that instance's C++ components before invoking it.

### 1.2 Only for working on the port itself (not needed to play)

| Requirement | Used by |
|---|---|
| Python 3.10+ with the `.venv-3` environment and `python -m pip install -r tools\requirements.txt` | Port tools and pinned Google-style/static-analysis checks; see [QUALITY.md](QUALITY.md) |
| Node.js 22+ and `npm ci --prefix tools/lint --ignore-scripts` | Pinned first-party clone detection |
| `resources\rood-reverseWin` (in the repo) | Read-only reference that those tools parse. The build never uses it. |
| Optional: `git submodule update --init` | Fetches `resources\rood-reverse` (upstream decomp) and the Ghidra PSX loader. Neither is needed to build. |
| Optional: external reference tools | See [TOOLS_INVENTORY.md](TOOLS_INVENTORY.md) |

The retained environment on this development machine is `.venv-3` (Python
3.14.3). Reuse it rather than creating `.venv`, `.venv-1` or `.venv-2`:

```powershell
.\.venv-3\Scripts\Activate.ps1
python --version
```

Activation affects only that shell. Without activation, invoke
`.\.venv-3\Scripts\python.exe` explicitly for Python tooling. On a fresh clone
only, create it with `python -m venv .venv-3`, then install the documented
requirements. Select the same interpreter in VS Code; do not recreate it if
it already exists.

`resources\rood-reverse-noprompt` is an optional, ignored local-only reference
checkout. It is not a submodule and is not fetched by the command above; its
own Git history and local edits are retained. Neither it nor the registered
references are runtime dependencies.

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
build-native.cmd
build-native.cmd Release
```

`build-native.cmd` finds MSBuild with `vswhere`, restores the vcpkg packages (into `vcpkg_installed\`), and builds
`VagrantStory.Native.sln` for x64. The default configuration is Debug. Up to four
additional arguments after the configuration are passed to MSBuild.
In PowerShell, prefix repository scripts with `.\`: use `.\build-native.cmd`
and `.\test-native.cmd`.

If the wrapper selects an MSBuild-only instance while a complete C++ instance is
also installed, invoke the latter directly. For example, for the documented
Community installation (adjust the path to your actual complete instance):

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' .\VagrantStory.Native.sln /m /restore /p:Configuration=Debug /p:Platform=x64
```

This does not bypass missing prerequisites; the selected instance must have the
C++ targets, ClangCL, Windows SDK and bundled vcpkg components from section 1.

### 4.2 Visual Studio

1. Open `VagrantStory.Native.sln`.
2. Pick **Debug | x64** or **Release | x64**.
3. Set **VS.Desktop** as the startup project (it is the default).
4. Press F5. The first build restores the vcpkg packages automatically.

### 4.3 Output

Everything goes to `build\<Config>\` (git-ignored):

| File | What it is |
|---|---|
| `VS.Desktop.exe` + `SDL3.dll`, `dav1d.dll`, `webm.dll`, `sfizz.dll` | the game |
| `VS.Extract.exe` | the asset extractor (§5) |
| `VS.Tests.exe` | unit tests |
| `VS.CombatLab.exe` | standalone live encounter editor and production combat host |
| `vs-native.log` | the log written by `VS.Desktop` at run time |

C++ sources use warnings as errors (`/W4 /WX`). The solution generator currently overrides `.c` items
to warning level 1 with non-fatal warnings, including hand-maintained C ports. Compatibility options are in
[Directory.Build.props](../Directory.Build.props). A successful build is therefore not necessarily
warning-free: pointer/ABI, bounds, and uninitialized-value warnings still need source and runtime review.

### 4.4 Source quality

Native sources use Google C++ conventions. Run `quality-native.cmd` for the fast
no-new-debt gate and `quality-native.cmd --changed --tidy` for compilation-based
analysis of changed owned code. Reports go to `build/quality`; generated and
reference sources are excluded from the default style scope. See
[QUALITY.md](QUALITY.md) for setup, precise scope, baseline policy, and controls.

### 4.5 Standalone Combat Lab

After extracting the data, run `build\Debug\VS.CombatLab.exe` (or Release) for a
live room/enemy/quantity/placement editor without launching `VS.Desktop` or its
title/prologue. Combat uses the shared production code. See
[COMBAT_LAB.md](COMBAT_LAB.md) for controls, presets, extensive per-run logs,
verified scenarios and current same-zone/loadout/coverage limits. This is not a
claim that all enemies, items, spells or abilities are complete.

### 4.6 Local state and evidence retention

The existing `build\` tree contains both reproducible outputs and retained
evidence. It is not a disposable directory as a whole. This cleanup leaves
its contents and existing evidence paths unchanged.

| Local contents | Retention rule |
| --- | --- |
| `build\Debug\obj`, `build\Release\obj`, compiler intermediates | Rebuildable when no build/debug/run is using them; never clean an active configuration. |
| Executables, DLLs and PDBs under `build\<Config>` | Rebuildable, but retain the matching binary/symbol set for unresolved crashes or evidence that cannot be reproduced from its recorded revision and dirty state. |
| `build\port` and module-data/index outputs | Regenerable tooling state, not an excuse to regenerate over conflicting edits; preserve inputs and finish active generator work first. |
| `build\quality`, named replay/capture directories and reference captures | Retain cited logs, manifests, metrics, PCM/images and before/after failure evidence with revision, configuration and inputs. A passing run does not make earlier failure evidence disposable. |
| `build\<Config>\combat-lab` | Retain per-run logs and user presets, including `last-scenario.json`; these are not compiler intermediates. |
| `build\duckstation` | Local portable reference setup, settings and possible captures/state; do not confuse it with a throwaway compiler cache or modify the installed emulator. |
| `Native\data`, `disks`, `SLUS-01040`, local reference checkouts and mods | User-owned/imported data or references; not part of build-output cleanup. |
| `vcpkg_installed` and package caches | Restorable dependency outputs, but leave them intact during active audio/build work. |

Use a dedicated `build\scratch\<experiment>` path for future explicitly
disposable runs, separate from named evidence runs. Before removing an older
run, check its citations in [HANDOFF_HISTORY.md](HANDOFF_HISTORY.md),
[COMBAT_LAB.md](COMBAT_LAB.md) and the relevant issue. Preserve or deliberately
archive referenced evidence first; unknown provenance means retain it.
Do not commit or attach disc-derived captures, audio, dumps or BIOS files.

---

## 5. Convert the game data (asset extraction)

**Planned ⏭ (Docs\ASSET_PIPELINE.md):**

```cmd
extract-native-assets.cmd
extract-native-assets.cmd --disc D:\path\to\SLUS-01040.cue --ffmpeg C:\path\to\ffmpeg.exe
```

- This converts **every** file on the disc into modern formats under `Native\data\` (git-ignored): JSON, glTF,
  PNG, WAV, FLAC and AV1 MKV. It writes `Native\data\manifest.json`.
- Run it once after cloning, and again whenever the extractor changes. The game tells you when that is needed.
- Runs are incremental. `--force` rebuilds everything.

**Today (required before running):**

```cmd
build\Debug\VS.Extract.exe extract disks\SLUS-01040.cue Native\data
build\Debug\VS.Extract.exe extract disks\SLUS-01040.cue Native\data --ffmpeg C:\path\to\ffmpeg.exe
rem Diagnostic: list the disc's files.
build\Debug\VS.Extract.exe list    disks\SLUS-01040.cue
build\Debug\VS.Extract.exe audio   Native\data\disc Native\data
build\Debug\VS.Extract.exe streams disks\SLUS-01040.cue Native\data
```

- Writes `Native\data\disc\<disc path>` for every file and `Native\data\files.tsv` (sector, size, form, path).
  Files not converted yet are copied as they are (form `raw`); the intro movie becomes `MOV\TITLE.STR.mkv` (AV1 +
  FLAC, form `mkv`). Effect and ending audio are imported separately under `audio\streams`;
  the ending's legacy sector-data row remains `stream`, not a raw readable disc file.
- Takes about a minute. Run it again after an extractor change; the converters replace one `raw` form at a time.
- The separate `audio` command imports all 83 nonempty wave banks into versioned JSON, mono PCM16 WAV
  and SFZ under `Native\data\audio\banks`. Use it after the ordinary extraction, including for an existing
  data directory. WAVE0069 supplies MUSIC016, WAVE0091 supplies MUSIC110, and WAVE0011 supplies MUSIC054
  on entry to the dungeon; menu banks alone do not supply these music tracks.
  Missing banks are reported, not replaced with fabricated audio.
  Optional numbers are **wave-file IDs**, not instrument IDs. The older WAVE0032 articulation layout,
  exact-end records and erased final DMA padding are supported; unavailable slots are logged explicitly.
  Reimport after the fidelity update so SFZ pitch-bend range/smoothing matches native FM rendering.
  Successful import is not evidence that every music track has been listened to.
- The `streams` command updates existing installations without reconverting the movie. It imports
  113 effect streams as WAV/SFZ plus metadata, and the ending XA as FLAC. It requires the original
  raw-sector disc for XA and FFmpeg on PATH (or `--ffmpeg <path>`). Full `extract` also runs this step.
  The XA importer preserves the original 443.52-second cadence rather than concatenating duplicate
  sectors. Missing or invalid native stream assets are reported at playback.
- sfizz 1.2.3 is pinned in `tools\vcpkg\ports\sfizz`. vcpkg builds its DLL with MSVC; native projects retain
  ClangCL and consume the C API. The overlay statically includes its bundled WavPack dependency.
  No separate sfizz checkout or standalone dependency-build experiment is required.

### 5.1 Native audio assets

Run the separate bank import after ordinary extraction. For an existing data directory,
the stream command updates effect/XA assets without reconverting the movie:

```cmd
build\Debug\VS.Extract.exe audio   Native\data\disc Native\data
build\Debug\VS.Extract.exe streams disks\SLUS-01040.cue Native\data
```

- `audio` imports all 83 nonempty wave banks into versioned JSON, mono PCM16 WAV
  and SFZ under `Native\data\audio\banks`. WAVE0069 supplies MUSIC016, WAVE0091
  supplies MUSIC110, and WAVE0011 supplies MUSIC054 on dungeon entry; menu banks
  alone do not supply these tracks. Missing banks are reported, not fabricated.
  Optional numbers are wave-file IDs, not instrument IDs. Reimport after audio
  converter changes. Successful import is not full-content listening evidence.
- `streams` imports 113 effect streams as WAV/SFZ plus metadata and the ending XA
  as FLAC. Full `extract` also runs this stream step. XA requires the original
  raw-sector disc and FFmpeg on PATH (or `--ffmpeg <path>`); an extracted folder
  cannot supply the ending's raw form-2 audio sectors. The importer preserves
  the original 443.52-second cadence. Missing/invalid assets are reported.
- sfizz 1.2.3 is pinned in `tools\vcpkg\ports\sfizz`. vcpkg builds its DLL with
  MSVC; native projects retain ClangCL and consume the C API. The overlay
  statically includes bundled WavPack. No separate sfizz checkout is needed.

**Planned ⏭:** `extract-native-assets.cmd` wrapping this, with every file in its modern format (JSON, glTF, PNG,
WAV, FLAC) and `--force` / incremental runs.

---

## 6. Run

```cmd
build\Release\VS.Desktop.exe
```

| Option | Meaning |
|---|---|
| `--disc <path>` | Use this disc (`.cue` or extracted folder). By default the exe looks for the `VS_DISC` environment variable, then for `disks\SLUS-01040.cue` (or `SLUS-01040\`) in its own folder or any parent folder, which finds the repo copy in a dev build. |
| `--data <dir>` | The extracted data (§5). By default `Native\data` in the exe's folder or any parent folder. |
| `--save-dir <dir>` | Override the native save directory. Use a dedicated ignored directory for tests; the default is `saves` inside SDL's per-user VagrantStory directory. Failure to obtain that directory is an explicit error, never a current-directory fallback. |
| `--frames <n>` | Stop after `n` logical ticks (NTSC fields: 60000/1001, approximately 59.94 per second). |
| `--capture <dir> --at <t1,first:last,...>` | Write `frame_<t>.bmp` at individual ticks or every tick in positive inclusive ranges, e.g. `--at 100,200:500`. Ranges are stored without expanding the command line or allocating one entry per tick. A selected tick with no display image records the host's black presentation instead of silently omitting the frame. For continuous comparisons use `--speed 1`; large selections produce one BMP per selected tick, so bound the run and preserve verified lossless video before removing redundant frames. |
| `--press <t:Buttons,...>` | Scripted presses, each held 3 ticks, e.g. `3400:Down,3500:Circle` (names: Up Down Left Right Cross Circle Square Triangle L1 R1 L2 R2 L3 R3 Start Select, joined with `+`). |
| `--scripted` | Ignore keyboard and gamepad (only `--press` input). |
| `--speed <n>` | Run `n` logical ticks per real tick (e.g. 8 to reach the title attract scenes, about tick 5300, quickly). |
| `--log-level <level>` | Minimum severity: `trace`, `debug`, `info` (default), `warn`, `error`, or `off`. `off` suppresses all normal logging, including crash messages. Invalid/missing values fail startup. |
| `--log-channels <names>` | Exact, case-sensitive comma-separated channels, without spaces; default `*` selects all. Filters trace/debug/info only; warnings/errors remain visible unless excluded by severity or `off`. |

Native file-menu integration remains under verification in #18. Native slots
are `slot00.vssave` through `slot99.vssave`, with recoverable `.bak` files;
these are versioned semantic JSON, not PS1 memory-card images. Import and old
schema migration are not implemented. Recovery/replacement requires explicit
confirmation, and invalid files without a valid backup are not overwritten.
Ending clear-save conversion is not modeled yet; that entry reports its limit
instead of invoking the old card backend or writing a fabricated checkpoint.
Do not use owner saves for replay tests or treat an empty-picker test as a
successful gameplay round trip.

Logs go to the console and `build\<Config>\vs-native.log` (truncated each run, including an empty log with `off`).
Each line includes `[tick=N]`: zero is startup, otherwise the logical simulation tick, not elapsed wall time.
For a local native-mix recording, set `VS_AUDIO_CAPTURE` to an absolute file path before launching.
The file is raw **48,000 Hz, two-channel f32le** PCM submitted to SDL; it excludes the independently bound
movie and XA sources. For example, `ffmpeg -f f32le -ar 48000 -ac 2 -i capture.f32 capture.wav` converts it for listening.
Remove the environment variable afterward. Captures and disc-derived assets must remain uncommitted.
The current output is stereo or dual-mono, not 5.1. Native noise, preceding-voice FM and the original
reverb presets are implemented; bit-exact console output and full-content listening parity remain unverified.
For AI diagnosis use `--log-level debug --log-channels ai,actors,boot,input,content`.
`--log-level trace --log-channels frame,trace` enables per-tick begin/end markers and legacy C trace sites;
this is intentionally verbose. Disabled messages skip formatting. C ports can use `Core_Debugf(channel, format, ...)`;
existing `Core_Tracef` sites now require trace level. Do not use `off` when collecting crash evidence.
The capture helper accepts the same controls as `-LogLevel debug -LogChannels 'ai,actors,boot,input,content'`
and fails on build/run errors, timeouts, logged crashes, or missing requested captures within the tick limit.

**Today:** the original boot and title run (see Current state); the game reads `Native\data\` for its files, and
the disc only to report its label. The title screen appears about 55 s after start; **Start (or Circle) skips the
intro movie**, so test runs press it: `--press 1000:Start` puts the menu up by tick ~1100.
`tools\test\run_native.ps1` wraps copy, build and a captured run (`-SkipIntro`), and
`tools\test\duckstation_capture.ps1` makes the matching DuckStation reference (see `.github\copilot-instructions.md`
for the emulator and BIOS locations).

The game reads only `Native\data\` (⏭ plus `Native\Content\patches\` and `mods\`). The disc is optional at run
time; if it is found, its label is logged. If `Native\data\` is missing, the game stops with a message that says to
run the extractor.

### 6.1 Audio diagnostics

For a local native-mix recording, set `VS_AUDIO_CAPTURE` to an absolute file path
before launching. The file is raw **48,000 Hz, two-channel f32le** PCM submitted
to SDL; it excludes independently bound movie and XA sources. For example,
`ffmpeg -f f32le -ar 48000 -ac 2 -i capture.f32 capture.wav` converts it for
listening. Remove the environment variable afterward. Captures and disc-derived
assets must remain uncommitted.

Output is stereo or dual-mono, not 5.1. Native noise, preceding-voice FM and
original reverb presets are implemented; bit-exact output and full-content
listening parity remain unverified.

### 6.2 Controls (today)

| Input | Notes |
|---|---|
| Any SDL3-supported gamepad | Mapped to the PlayStation pad layout (Cross/Circle/…). |
| Keyboard | Arrows = D-pad · X = Cross · C = Circle · Z = Square · S = Triangle · Q/E = L1/R1 · 1/3 = L2/R2 · Enter = Start · Backspace = Select (`Native\Input`). The remapping UI is a Phase 2 item. |

---

## 7. Test

```cmd
test-native.cmd
test-native.cmd Release
test-native.cmd Debug "[core]"
test-native.cmd Release "[audio],[clock]"
```

The default configuration is Debug. The wrapper builds before running tests;
up to four Catch2 arguments follow the configuration. Inspect available cases
with `build\Debug\VS.Tests.exe --list-tests` after a successful build. Skipped
disc-dependent cases are not release acceptance evidence.

Audio tests include actual SDL postmix, stream/XA lifecycle and native music-state
regressions. The `[audio][xa-playback][disc]` case also exercises the installed
`Native\data\audio\streams\xa\ENDING\ENDING.XA.flac` through production SDL output:
initial playback, a 30-second sector seek, stop/re-entry and natural EOF/drain.
It skips explicitly if the converted asset is absent; synthetic XA tests still
run. This is audio-path coverage, not a complete ending-credit navigation replay.

`Native\Tests\data\audio` contains original synthetic FLAC fixtures, not disc
audio. Tests locate them relative to the standard `build\<Config>` executable
directory. To reproduce them with FFmpeg, use fresh destination files (`-n`
refuses to overwrite existing fixtures):

```powershell
ffmpeg -hide_banner -loglevel error -n -f lavfi -i 'sine=frequency=400:sample_rate=37800:duration=3' -af 'volume=3,pan=stereo|c0=c0|c1=-0.5*c0' -sample_fmt s16 -c:a flac -compression_level 12 -fflags +bitexact -flags:a +bitexact -map_metadata -1 Native\Tests\data\audio\stereo-tone.flac
ffmpeg -hide_banner -loglevel error -n -f lavfi -i 'sine=frequency=400:sample_rate=37800:duration=0.2' -af 'volume=3' -sample_fmt s16 -c:a flac -compression_level 12 -fflags +bitexact -flags:a +bitexact -map_metadata -1 Native\Tests\data\audio\mono-tone.flac
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
| `python tools\port\gen_decomp_headers.py [--data Native\data]` | Generated headers that `decomp_index.py` and the copy need (disc layout; string-table offsets read from the extracted data) |
| `python tools\port\copy_to_native.py` | Regenerate the copied code in `Native\` from the decomp (never edit its output; `tools\port\own.py` takes an item over by hand) |
| `python tools\port\understood.py vs_title_exec@TITLE.PRG [--mark]` | Check (and mark) `understood` for every function reachable from a root |
| `python tools\naming\merge_names.py --check-all` | Validate the function catalog naming |
| `python tools\scaffold\gen_solution.py` | Regenerate the `.sln` / `.vcxproj` files, including the `Tools` solution folder that shows `tools\`, `C:\Tools` and `D:\Dev\Modding\VS-Workshop` (source, docs, scripts; set `VS_EXTERNAL_TOOLS` / `VS_WORKSHOP` for other locations; both are optional: on a machine without them the projects list nothing and regenerating leaves them alone) and `Solution Items`, discovered from the repository (root files, all of `Docs\` with its subfolders, `.github\`; new files appear on the next run). **Run it after any project change**; do not hand-edit the project files. |

**Legacy, do not use:** `tools\Setup-Windows.ps1`, `tools\Probe-Native.ps1` and `tools\windows.py` belong to the
earlier, abandoned emulator-style port (VS 2022/CMake, `vagrant-native.exe`). They are not part of this build.

---

## 9. Troubleshooting

| Symptom | Fix |
|---|---|
| `MSBuild not found` | Install VS 2026 with the **Desktop development with C++** workload. |
| `MSB4019`, `Microsoft.Cpp.Default.props` not found | The selected VS/Build Tools instance lacks C++ targets, or an environment override points to the wrong targets. Add the C++ workload, ClangCL components and Windows SDK to that instance through Visual Studio Installer; check `VCTargetsPath` if explicitly set. With multiple instances, invoke MSBuild from a fully provisioned instance directly. Do not hand-edit generated project imports. Restore warnings such as `NU1503` may accompany this prerequisite failure; they do not establish a source-code compile defect. |
| `The build tools for ClangCL cannot be found` | Add the two Clang components from §1.1 in the Visual Studio Installer. |
| `SDL3/SDL.h` not found, or vcpkg errors | Add the **vcpkg package manager** component, check the internet connection, delete `vcpkg_installed\`, and build again. |
| `no disc found` in `vs-native.log` | Put the files at `disks\SLUS-01040.cue/.bin`, or pass `--disc`. |
| `no extracted data` in `vs-native.log` | Run the extractor (§5) into `Native\data`, or pass `--data`. |
| The intro is black, `no converted movie` in the log | FFmpeg was not found during extraction; install it (§1.1) and extract again. |
| `cue sheet has no FILE entry` / the bin cannot be opened | The `.cue` must reference the `.bin` by a name that exists next to it. |
| The window opens and closes | Read `build\<Config>\vs-native.log`. |

---

## 10. Clean

```cmd
rem Binaries, intermediates and port-tool output:
rmdir /s /q build
rem Dependency cache; the next build must restore it:
rmdir /s /q vcpkg_installed
rem Extracted game data; extraction must be repeated:
rmdir /s /q Native\data
```

These are optional destructive cleanup commands, not build or test prerequisites.
Do not remove local captures, extracted assets or dependency caches merely to
run verification. Rebuild and re-extract the corresponding outputs after cleaning.
