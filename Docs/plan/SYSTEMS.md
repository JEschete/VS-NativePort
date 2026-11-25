# Native Source Port Plan: Per-system designs (engine)

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · §6.6–§6.8.2 continue in [SYSTEMS_2.md](SYSTEMS_2.md)

## 6. Per-system designs

Each system lists: responsibility, API sketch, ledger rows it absorbs, Phase 1 work, Phase 2+ work and tests.
These are target designs, not an implemented-feature inventory. See §0.2 and §15 for current behavior;
§14 specifies the migration steps, dependencies and evidence needed to claim implementation.

### 6.1 Core (`Engine\Core`)
- **Responsibility:** fixed-point math, random, timing, logging, assertions, memory arenas, typed handles.
- **API:**
  - `Fixed12` (4.12) and `Fixed16`;
  - `Vec3s`, `Vec3i`, `Mat3x3s`, `Transform`;
  - `RotTrans`, `RotTransPers`, `NormalClip`, `AverageZ`, `Project`, with explicit rounding that matches the original where gameplay depends on it;
  - `Random` (reproduces the original `rand` LCG sequence for replay parity);
  - `FrameClock` (logical ticks; the original's field-based 60/30/20 fps steps).
- **Absorbs:** libgte (41 symbols, 1,489 uses), libc/crt, libetc VSync, `Core.Math` (41 rows), `Core` (44 rows).
- **Tests:** a math oracle against the PsyCross GTE and ps1-tests GTE vectors; LCG sequence; transform round-trips.

### 6.2 Graphics (`Engine\Render`)
- **Responsibility:** everything visible. The game submits *what* to draw, and the backend decides *how*.
- **Model:**
  - `RenderQueue` per frame. Draw items carry a `SortKey { layer, depth, sequence }`, which replaces OT index + `AddPrim` order. Reverse-depth ordering is reproduced by the key, not by a linked list.
  - Draw items: `Triangle/Quad` (flat, gouraud, textured, textured-gouraud), `Sprite`, `Tile`, `Line`, `Fill`.
  - Materials: `BlendMode { Opaque, Half, Add, Sub, AddQuarter }`, `TextureRef`, `Palette`, semi-transparency flags.
  - **Texture page space:** the original data refers to textures by tpage/clut coordinates. `Engine\Render` keeps a *logical* 1024×512 texture-page space that maps those coordinates to real GPU textures and palettes. It is a lookup structure (rect → texture + palette), not emulated VRAM. TIM uploads (`LoadImage`) register textures in that space.
  - **Capture API:** `Capture(rect) → TextureHandle` replaces `StoreImage`/`MoveImage` readbacks (title menu background, screen effects, battle transitions).
  - Display: `SetDisplayArea`, `SetClearColor`, fades, letterbox, all as typed calls.
- **Absorbs:** libgpu (78 symbols, 1,742 uses) via `Legacy\gpu_*.cpp` shims, then migrated; `Graphics` (121 rows).
- **Phase 1:** native resolution 320×240 (and the 512/640 modes the game uses), point sampling, affine mapping, dithering optional. Parity first.
- **Phase 2:** resolution scale, widescreen, perspective-correct mapping, sub-pixel vertices, 60 fps interpolation (the queue stores previous transforms).
- **Tests:** capture comparison of title, menu and first room against DuckStation reference images with a tolerance metric.

### 6.3 Audio (`Engine\Audio`, `Audio::Seq`)
- **Responsibility:** music, SFX, voice/XA streams.
- **Model:**
  - sfizz (D23) renders instruments from SFZ regions and decoded 16-bit PCM WAV samples produced by `VS.Extract`.
  - `Audio::Seq` retains AKAO timing, ordering, channel state, priorities and transitions; it submits ordered note/control operations to sfizz instead of driving SPU registers.
  - SDL3 output receives the native float mix at 48 kHz. Audio-thread ownership, buffering and the simulation-to-sample clock are part of M5's acceptance.
  - Reverb presets and unsupported noise/FM behavior must be explicitly mapped or listed; no success-shaped hardware-register shim.
  - `StreamPlayer` plays the FLAC streams converted from XA.
- **Sequencer:** the decomp's AKAO sequence interpreter (in `Audio`, 254 rows) is copied and becomes `Audio::Seq`. It
  retains its bytecode and ordering while binding native sample assets and voice controls. Decoded `*.seq.json` event
  lists remain planned; no second event sequencer is introduced.
- **Absorbs:** libspu (36), libsnd (2) via typed native sample, voice, mixer and stream APIs; SPU register and RAM compatibility surfaces are retired as their callers migrate.
- **Phase 2:** better resampling, optional interpolation modes, volume channels, replacement music hooks.
- **Backend:** D23 selects SFZ + WAV instruments played by sfizz through the ported AKAO sequencer. M5 defines the supported fidelity and explicitly lists unsupported behaviors.
- **Tests:** sequence-event traces versus a reference capture; ADPCM decoder versus known vectors; rendered samples and lifecycle/transition checks.

### 6.4 Video (`Engine\Movie`)
- **Responsibility:** FMV playback (the movies in `TITLE.STR`, converted to `Native\data\movies\*.mkv`).
- **Model:** libwebm demuxes the Matroska file, dav1d decodes AV1 to YUV planes that a shader converts on an SDL3 GPU texture, and the FLAC audio goes through `Audio::StreamPlayer`. Presentation is clocked to the audio. VS.Extract does the STR → AV1 conversion up front with ffmpeg (extract time only; [ASSET_PIPELINE.md](../ASSET_PIPELINE.md) §6.7).
- **As built (1b):** `Movie_PlayerOpen/DrawFrame/Close` (`Movie/MoviePlayer.h`). The frame due at the game's tick is converted to RGB on the CPU and written as 24-bit pixels into the page space where the MDEC output went, so the copied display code shows it unchanged. The soundtrack is decoded at open and queued on its own audio-device source. The file is `Native\data\disc\MOV\TITLE.STR.mkv` (AV1 CRF 18, full range; FLAC). Current state: EC-08.
- **Absorbs:** libpress (13 symbols) via shim → migrated; `Video` (15 rows); the title movie functions (`_initIntroMovie`, `_playIntroMovie`).
- **Reuse:** none at runtime. jpsxdec decodes are the reference used to validate the conversion.
- **Tests:** PSNR ≥ 45 dB of decoded MKV frames against a jpsxdec decode; frame count and duration equal.

### 6.5 Input (`Engine\Input`)
- **Responsibility:** devices → actions.
- **Model:**
  - `Action` enum (the sixteen PS1 controls, `Input/Actions.hpp`) and `ActionSet`/`ActionState`, kept separate from physical keys and gamepad controls.
  - A `PadState` with the original PS button bitmask for Phase 1 parity (the game logic reads it).
  - SDL3 gamepad and keyboard bindings (`Input/Bindings.hpp`, #52): a versioned JSON file, `config\input.json` beside
    the executable by default (`--input-config` overrides it), shared by `VS.Desktop` and the Combat Lab. A missing
    file is created with the original default layout. Any problem (invalid JSON, a duplicate key, an unknown action,
    key or control, a key or control bound to two actions, a host-reserved key, or a bad version or trigger
    threshold) rejects the whole file: the defaults apply, the file is never overwritten, and every problem is logged
    and recorded as an `input.bindings` trace event; Release Desktop also shows them in a warning box. Bindings load
    only while the devices are enabled, so `--scripted` runs never read the file. Limits: the analog sticks are not
    bindable, and the launcher does not forward `--input-config`.
  - Rumble (the title's Vibration option) maps to SDL rumble.
  - Typed device settings (`Input/Settings.hpp`, #52): `DeviceSettings` holds a `SoundOutput` and a `Vibration`,
    decoded from the packed record's bytes the way the devices applied them. `Sound_SetSpeakerMode` selects mono only
    for 1 (25AC.c:451), and the motors run for any nonzero vibration byte (main.c:1653). `LiveDeviceSettings` reports
    a byte other than 0 or 1 once per distinct pair, and writes store 0 or 1. A soft reset restores
    `DeviceSettings{}`, stereo with vibration on (main.c:1764-1765). Divergence: a corrupt mono byte of 2 stays stereo,
    as `Sound_SetSpeakerMode` applied it; the original title's `initEnvironment` normalized it to mono (22C.c:1487),
    and the Options menu opened on an out-of-range row (88.c:454).
  - Consumers are split. **Immediate device consumers** apply a change at once: the audio output
    (`Audio::LiveSetSoundOutput`, from the title Sound menu, the Options menu and a loaded save) and the pad motors
    (`LiveRumble`, gated by vibration and applied through `InputSystem::RequestRumble`). **Session/gameplay consumers**
    read the Options flags (`Menu::Options::Settings`: puzzle mode, ability timing, weapon and armour status, cursor
    memory, information, simple map) when they act; these persist with the save. Gameplay still reads those flags
    through the packed record (#489).
  - The title menu reads `LiveActions()` (player 1's held and newly pressed `ActionState`) and turns it into
    `Title::MenuCommands` through `MenuCommandsFor`, so it reads no legacy controller globals. The other copied
    consumers move off `ControllerLiveState.hpp` under #489.
- **Absorbs:** libpad (25 symbols, 544 uses), rewritten at copy time (R); `Input` (17 rows).
- **Phase 2:** remapping UI, per-device prompts.
- **Tests:** binding resolution and file validation (`[input-bindings]`); pad edge detection (pressed/held/repeat) matching the original repeat timings.
