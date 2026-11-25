# Native Source Port Plan: Solution layout

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md)

## 3. Solution layout

Owner decision 2026-10-17 (**hybrid**): DW4's few-projects style (`D:\Dev\NativePort\DW4_NativePort2`), with the
engine systems grouped in one project. `tools\scaffold\gen_solution.py` generates the solution, every `.vcxproj`
and its `.vcxproj.filters` from one table. Files are listed explicitly and every folder on disk is a filter, so
Solution Explorer mirrors the tree. Re-run the generator after adding or removing files.

```
VS-NativePort\
  VagrantStory.Native.sln            ← open this, F5 runs VS.Desktop
  Directory.Build.props              ← x64, C++23 / C17, ClangCL (needed for the decomp's GNU C), warnings, output dirs
  build-native.cmd  test-native.cmd  extract-native-assets.cmd  package-native.cmd
  vcpkg.json                         ← manifest: SDL3, Catch2, nlohmann-json (+ dav1d, libwebm, libflac, lodepng as converters land)
  Native\
    Engine\         VS.Engine        engine systems, one folder each under src\, include\ and Legacy\:
                      Core\       types, Fixed/Vec/Mat (GTE replacement), Random, FrameClock, Log, Assert, Arena, Handle<T>
                      Platform\   SDL3 window, file system, timers, paths, crash handler
                      Render\     RenderQueue, sort keys, materials/blend modes, texture page space, capture/readback, SDL3 GPU backend
                      Audio\      sample banks, voices, mixer, reverb, stream player; Audio\Seq is the AKAO sequencer
                      Input\      actions, PS button map for parity, SDL3 gamepad, keyboard, rumble
                      Movie\      Matroska demux (libwebm) + AV1 decode (dav1d) → texture, audio sync
                      Save\       versioned save format, .mcd import
                    Legacy\<System>\  T-shims (libgpu, libspu/libsnd, libpress, libcard/libapi); deleted by 2e
    Content\        VS.Content       loaders for Native\data\ (glTF, PNG, JSON, WAV), AssetId registry, AsyncLoader,
                                     catalogs, schema validation, patch layers; disc decoders (VS.Extract only)
                    Legacy\          T-shims for libcd/libds
                    schemas\         *.schema.json (versioned, committed)
                    patches\         our fixes/enhancements as patch layers (committed)
    Game\           VS.Game          one folder per module under src\ and include\:
                    Boot\  Title\  Field\  Battle\  Actors\  Items\  Menu\  Event\  Effects\  Ending\  State\
    Desktop\        VS.Desktop       executable: main loop, config, CLI (--replay, --capture)
    Tools\Extract\  VS.Extract       CLI: disc → Native\data\ (JSON catalogs, glTF, PNG, WAV/FLAC, AV1 MKV) + manifest + round-trip validation
    Tests\          VS.Tests         unit tests (Core.Math, decoders, catalogs round-trip, replay hashes)
    data\           (git-ignored)    ALL converted disc content, produced up front by VS.Extract. See ASSET_PIPELINE.md
  THIRD_PARTY.md    licenses of vcpkg dependencies and any lifted CC0/MIT code
  tools\            ledger\ (generators, gates), port\ (retired copier, D24), scaffold\ (solution generator), lint\ (boundary checks)
  Docs\             BUILDING.md (clone → build → extract → play), plan, inventory, ledgers, per-system design notes
```

Includes are written `"<System or Module>/<File>.hpp"`: `"Core/Log.hpp"`, `"Render/Device.hpp"`, `"Boot/GameApp.hpp"`,
`"Content/Disc.hpp"`. Engine systems and game modules never share a folder name.

There is no Triage project: every function has a final location (§5 step 3 gate).

Dependency direction (#43). Game owns domain state, decisions and snapshots; Engine owns generic mechanisms; Content
returns decoded values or handles. Engine and Content therefore never include Game headers: an Engine mechanism
that needs a game decision receives it as an argument, a callback or a narrow interface that Game supplies.
- Project references: `VS.Engine ← VS.Content ← VS.Game ← {VS.Desktop, VS.Tests}`, and `VS.Extract` uses Engine and
  Content.
- Engine's Movie and Save read content, so `VS.Engine` sees Content's headers without a project reference. The
  executables link both static libraries.
- Each Engine system is a layer (`Engine\Legacy\<System>\` belongs to its system; `Engine\Legacy\Psx.hpp`, the PS1
  SDK aggregate, is `Engine.Legacy`), Content is one layer, and Game is one layer. A layer may include:

  | Layer | May include |
  | --- | --- |
  | Engine.Core | Engine.Core |
  | Engine.Platform | Engine.Core, Engine.Platform |
  | Engine.Render, Engine.Audio, Engine.Input, Engine.Movie, Engine.Save, Engine.Legacy | any Engine layer, Content |
  | Content | Engine.Core, Engine.Platform, Content |
  | Game, Desktop, Tools, Tests | any layer |

- Enforcement: `python tools\ledger\boundaries.py --check` (CI) resolves every quoted include in `Native\Engine` and
  `Native\Content` and fails on any crossing not recorded in `Docs\ledgers\BOUNDARY_LEDGER.csv`, on a resolved
  crossing that returns and on a stale ledger. Only the Engine and Content sources with a remaining crossing into
  Game compile with Game's include directories (`VSCopiedCIncludes`; every other legacy source gets
  `VSLegacyIncludes`), so the compiler rejects any other Game include there; `gen_solution.py` picks the property
  from the ledger and `--check` fails when the generated projects disagree with it. Remove a crossing, then run
  `boundaries.py --sync` and `gen_solution.py`.
- Not checked: isolation between Game modules. They talk through `Game\State` and module headers by convention.

<!-- boundaries:begin (generated by tools/ledger/boundaries.py --sync) -->

**11 of 79 crossing includes resolved**, 68 remaining in 14 files.

| From | To | Remaining includes | Remaining files | Resolved includes |
| --- | --- | ---: | ---: | ---: |
| Content | Engine.Audio | 2 | 2 | 0 |
| Content | Engine.Legacy | 3 | 3 | 1 |
| Content | Engine.Render | 5 | 2 | 0 |
| Content | Game | 21 | 3 | 2 |
| Engine.Audio | Game | 17 | 4 | 0 |
| Engine.Core | Engine.Platform | 0 | 0 | 1 |
| Engine.Input | Game | 3 | 1 | 0 |
| Engine.Render | Game | 17 | 5 | 7 |

| Remaining file | Crosses into | Includes |
| --- | --- | ---: |
| `Native/Content/src/ActorAssetsLive.cpp` | Engine.Audio, Engine.Legacy, Engine.Render, Game | 24 |
| `Native/Content/src/CdDriveLive.cpp` | Game | 2 |
| `Native/Content/src/DiscReaderLive.cpp` | Engine.Audio, Engine.Legacy | 2 |
| `Native/Content/src/SystemResourcesLive.cpp` | Engine.Legacy, Engine.Render, Game | 3 |
| `Native/Engine/src/Audio/ActorSoundLive.cpp` | Game | 12 |
| `Native/Engine/src/Audio/AmbienceLive.cpp` | Game | 2 |
| `Native/Engine/src/Audio/MusicLive.cpp` | Game | 2 |
| `Native/Engine/src/Audio/SoundBanksLive.cpp` | Game | 1 |
| `Native/Engine/src/Input/ControllerLive.cpp` | Game | 3 |
| `Native/Engine/src/Render/CameraViewLive.cpp` | Game | 1 |
| `Native/Engine/src/Render/DisplayControlLive.cpp` | Game | 3 |
| `Native/Engine/src/Render/ModelGeometryLive.cpp` | Game | 9 |
| `Native/Engine/src/Render/PaletteAnimationLive.cpp` | Game | 3 |
| `Native/Engine/src/Render/ScreenOverlayLive.cpp` | Game | 1 |

<!-- boundaries:end -->
