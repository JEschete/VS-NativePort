# Windows Build Migration

## Ownership

- `resources/rood-reverse` is the upstream reference. Its `upstream` remote points
  to `ser-pounce/rood-reverse`. Do not change that reference to accommodate this port.
- `resources/rood-reverseWin` owns the maintained matching-project snapshot.
  Source, headers, asset definitions and decompilation configuration from
  ser-pounce always replace local versions during synchronization.
- Windows recipes and native compatibility transformations live outside that
  replacement set. `upstream.json` records the authoritative commit and file hashes.
- The root owns the Windows native runtime, tests, Windows orchestration and docs.
  Retail data, disc images and memory-card saves remain local and ignored.

## Removed Dependencies

The maintained root and Windows build no longer use WSL, Linux ELF host binaries,
Bash, Nix, Docker, Linux multilib packages, Unix sockets, POSIX memory mapping,
`dlopen`, Unix signal watchdogs or shell `grep`/`nm` build pipelines.

The retired root matching Makefiles, Nix shell/lock/environment, duplicated
game/configuration/asset/header trees and legacy tool submodules are removed.
Copied Linux Docker/CI/build recipes in the Windows snapshot are also retired.
The unmodified upstream reference may still contain Linux tooling; it is not
an executable dependency of this Windows build.

Windows equivalents are Visual Studio Win32 with Clang, native Windows GNU Make,
Windows MIPS tools, `mkpsxiso.exe`, structured Python build operations,
`QueryPerformanceCounter`, `VirtualAlloc`, `LoadLibrary`, Windows exception
reporting and an externally bounded boot probe. SDL2.dll supplies direct Windows
keyboard/controller/video/audio access; the WSL controller bridge is removed.

## Setup And Build

Install Visual Studio 2022 Desktop development with C++, its Clang compiler
component, CMake 3.28 or newer, and Python 3.10 or newer. The compiler integration
uses the installed Clang binary through v143; it does not require the separate
ClangCL platform-toolset integration.

Run `tools/Setup-Windows.ps1` from PowerShell. It creates an isolated environment,
installs Python dependencies, fetches Windows tools and generates
`build/windows/VagrantStory.sln`.

Open that solution, select Debug/Win32 and build `NativePort`. The executable is
`build/windows/Debug/vagrant-native.exe`. `Decomp` invokes the matching pipeline.
`WindowsTools` bootstraps its native Windows tool dependencies.

Place your own NTSC-U disc image at `disks/SLUS-01040.bin`. Existing `data/` is
preserved; extraction only runs if it is absent. Native inputs and retail-derived
sources are regenerated under ignored `build/`, not committed as game assets.

## Upstream Updates

Use the reference repository to fetch ser-pounce's work. Then run:

```powershell
build/python/Scripts/python.exe tools/windows.py sync
```

Alternatively add `--fetch-upstream` to fetch the canonical repository explicitly.
The sync command uses the upstream ref, not local commits on the JEschete fork,
replaces authoritative trees and records their hashes. Windows build files remain.
New upstream C implementations supersede assembly-specific native adapters.
Unsupported source-shape changes fail preparation instead of silently replacing
upstream behavior. Direct edits to authoritative files are overwritten on sync.

## Verification And Gaps

Verified Windows Win32 executables: host timing/DLL/memory tests, 891 GTE checks,
18 GPU checks and inline-GTE translation tests. Python tests cover native input
isolation, upstream precedence, COFF parsing, file utilities, atomic downloads,
disc LBA sectors and Form-2 media.

The native executable compiles and links through Visual Studio. A bounded
headless startup enters the game and initializes the disc and audio layers.
Its eight-second watchdog exits normally. The startup regression in MSVC
`setvbuf` is fixed by checked unbuffered output; zero-sized line buffering was
invalid and opened a Debug CRT assertion dialog.

Subsequent SDL-window validation exposed and fixed missing image symbol views:
the upstream indexed-image encoders export palette and pixel offsets, and sized
RGB images export header and data offsets. Emitting only a base asset name left
`publisher_clut`, `_saveMenuBg_clut` and `_nowLoading_header` as zero-filled stubs.
Native generation now preserves those offsets into the unchanged retail bytes.
`NativeStubs` rescans the current object definitions on each build and avoids
rewriting an unchanged result. Bounded runs now render the loading text and
publisher logo; both publisher display buffers contain 3,090 nonblack pixels.
The watchdog also saves VRAM for pixel-based validation.

Full title/menu/movie and game-world execution have not been certified on
Windows. The existing named-abort bring-up behavior remains for missing game
functions; successful linking is not proof of complete gameplay.

**Matching build prerequisite:** old-gcc release 0.17 provides Linux/macOS
compilers, not Windows executables. Supply compatible Windows `cc1.exe` builds
for 2.7.2-psx, 2.7.2-cdk and 2.8.1-psx under
`build/tools/old-gcc/<version>/cc1.exe`. The `Decomp` project fails clearly when
they are missing and rejects ELF/Linux binaries. No WSL/Docker fallback exists.
Byte-identical matching is unverified until those genuine compilers are supplied.

Existing historical Linux observations in the other docs are retained as
research history, not as current setup instructions. This document governs
the maintained Windows environment.
