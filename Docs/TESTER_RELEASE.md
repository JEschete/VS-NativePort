# Vagrant Story Native - Windows tester build

This is an experimental port, not a finished replacement for the original game.
Requires Windows 10 version 1903 or later, or Windows 11 (64-bit).
Expect gameplay, cutscene and presentation defects. No game data or BIOS is
included. You must provide your own **USA SLUS-01040 single-track MODE2/2352
CUE/BIN dump**.
No BIOS is required. This unsigned experimental build may trigger a Windows
publisher warning; only run packages you trust. Do not disable antivirus.

## Required disc identity

The launcher rejects a different region, modified image, incomplete dump or
unsupported CUE layout. A matching filename or `SLUS-01040` label is not enough.
The approved BIN is exactly **750,643,152 bytes**:

- SHA-256: `25221D7F8FDB2C4490AF12D9E82F0E58BDD6ACB43387F9082B3C5E79DFE090C0`
- SHA-1 (for comparison with your dump records): `38C63DB6C49A06B91D87CD5A4E31B1EC68FE212C`

CUE and BIN filenames may differ from these examples, but the CUE must reference
the actual BIN and declare one `BINARY` file, `TRACK 01 MODE2/2352` and
`INDEX 01 00:00:00`. Metadata/comments and UTF-8 BOMs are supported; multi-track,
gapped, UTF-16 or malformed descriptors are rejected. There is no checksum-bypass
option. The complete BIN is verified again before every Play, including when
prepared assets already exist. The image is held read-only during setup.

## Start here

1. Extract the **entire ZIP** into a folder. Do not run programs inside the ZIP.
2. Double-click **Start-Game.cmd**. It checks the bundled runtime files and opens
   the launcher. No Visual Studio, Python or Node.js installation is needed.
   On first launch all path fields are blank. No developer configuration or
   personal paths are included. Only paths you select are remembered locally.
3. Use **Choose CUE...** beside the disc field to select your `.cue`. Its referenced
   `.bin` must exist with the name recorded inside the CUE; selecting an unrelated
   BIN with a matching-looking name is not sufficient.
   The launcher verifies it in the background. **Play stays disabled** until
   the full checksum matches. Use **Verify disc** after editing the path;
   **Disc details / View error** shows the expected and detected identity.
4. Choose a writable **game save folder**. Existing saves are not moved or deleted
   when you change this setting.
5. On first play, provide **FFmpeg**. Click **Detect** to search your own machine,
   browse to `ffmpeg.exe`, use the official download link, or choose the
   optional **winget** installation of `Gyan.FFmpeg`. Installing software is
   optional and may require Windows approval. After installing, click **Detect
   FFmpeg**. A build supporting `psxstr` and `libsvtav1` is required.
6. Press **Play**. First-time setup extracts disc files, converts the intro and
   streamed audio, imports music/sound banks, and checks the outputs before
   launching the game. This can take several minutes. Do not close setup.
7. Later plays check and reuse prepared assets; they do not extract again unless
   the source disc, pipeline version or required asset files have changed.
   Assets from the earlier unverified-checksum launcher require one preparation
   update; previous assets are retained rather than silently trusted.

## Data, saves and failures

- Settings and prepared assets are stored in the per-user
  `%APPDATA%\VS-NativePort\VagrantStory` directory supplied by SDL. The chosen
  save folder is independent of the installed game and prepared assets.
- The launcher remembers all three paths. Keep the original CUE/BIN available
  for source validation. The game itself launches against the prepared assets.
- Remembered paths are in your own per-user configuration, never the tester ZIP.
  Detection is an explicit action; no save or converter location is preselected.
- The interface scales with monitor DPI. Resize or scroll the window on smaller
  displays; all setup fields and the Play button remain reachable.
- A failed setup does **not** start the game or mark incomplete assets ready.
  Correct the displayed error and retry. **Open setup log** opens `setup.log`
  in the per-user directory. This is installation output, not gameplay logging.
- Failed staging folders and replaced asset folders are retained for diagnosis
  and recovery; they are named `assets-staging-*` and `assets-previous-*`.
  After successful setup you may remove obsolete ones, never your save folder.
- Release game builds disable normal gameplay logging and detailed trace files.
  Startup errors are still displayed. Use a developer Debug build when detailed
  diagnostic evidence is specifically needed.
- Do not upload disc images, extracted assets, saves containing game data,
  screenshots or audio as public issue attachments. Describe the problem,
  room/action, build version and reproduction steps.

## Controls

Keyboard defaults: arrows move, **Z = Cross**, **X = Circle**, **Return = Start**.
Gamepads use the existing production input support.

FFmpeg is external and is not included in this ZIP:
https://ffmpeg.org/download.html#build-windows

Project: https://github.com/JEschete/VS-NativePort
