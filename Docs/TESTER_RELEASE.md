# Vagrant Story Native - Windows tester build

This is an experimental port, not a finished replacement for the original game.
Requires Windows 10 version 1903 or later, or Windows 11 (64-bit).
Expect gameplay, cutscene and presentation defects. No game data or BIOS is
included. You must provide your own **USA SLUS-01040 single-track MODE2/2352
dump**, as a BIN, a CUE/BIN or a CHD.
No BIOS is required. This unsigned experimental build may trigger a Windows
publisher warning; only run packages you trust. Do not disable antivirus.

## Required disc identity

The launcher rejects a different region, modified image or incomplete dump. A
matching filename or `SLUS-01040` label is not enough. The approved BIN is exactly
**750,643,152 bytes**:

- SHA-256: `25221D7F8FDB2C4490AF12D9E82F0E58BDD6ACB43387F9082B3C5E79DFE090C0`
- SHA-1 (for comparison with your dump records): `38C63DB6C49A06B91D87CD5A4E31B1EC68FE212C`

File names do not matter. You can select any of these:

- **`.bin`**: no CUE is needed.
- **`.cue`**: it must declare one `BINARY` file, `TRACK 01 MODE2/2352` and
  `INDEX 01 00:00:00`. Metadata/comments and UTF-8 BOMs are supported; multi-track,
  gapped, UTF-16 or malformed descriptors are rejected. If the BIN named inside it
  is missing, for example after renaming, the launcher uses the BIN with the CUE's
  own name, or else the only BIN in that folder.
- **`.chd`**: one data track, with no parent CHD. It must decompress to exactly the
  BIN above, so the checksums above apply to its contents, not to the `.chd` file.

There is no checksum-bypass option. The complete image is verified again before
every Play, including when prepared assets already exist; a CHD takes a few seconds
longer because it is decompressed to check it. The image is held read-only during
setup.

## Start here

1. Extract the **entire ZIP** into a folder. Do not run programs inside the ZIP.
2. Double-click **Start-Game.cmd**. It checks the bundled runtime files and opens
   the launcher. No Visual Studio, Python or Node.js installation is needed.
   On first launch all path fields are blank. No developer configuration or
   personal paths are included. Only paths you select are remembered locally.
3. Use **Choose disc...** beside the disc field to select your `.cue`, `.bin` or
   `.chd`. The launcher verifies it in the background. **Play stays disabled** until
   the full checksum matches. Use **Verify disc** after editing the path;
   **Disc details / View error** shows the expected and detected identity.
4. Choose a writable **game save folder**. Existing saves are not moved or deleted
   when you change this setting.
5. On first play, provide **FFmpeg**. Click **Detect** to search your own machine,
   browse to `ffmpeg.exe`, use the official download link, or choose the
   optional **winget** installation of `Gyan.FFmpeg`. Installing software is
   optional and may require Windows approval. After installing, click **Detect**.
   Not every FFmpeg build can be used; see [Which FFmpeg to use](#which-ffmpeg-to-use).
6. Press **Play**. First-time setup extracts disc files, converts the intro and
   streamed audio, imports music/sound banks, and checks the outputs before
   launching the game. This can take several minutes. Do not close setup.
7. Later plays check and reuse prepared assets; they do not extract again unless
   the source disc, pipeline version or required asset files have changed.
   Selecting a different image file, such as moving from a BIN to a CHD, prepares
   the assets once more.
   Assets from the earlier unverified-checksum launcher require one preparation
   update; previous assets are retained rather than silently trusted.

## Which FFmpeg to use

FFmpeg is not part of this game and cannot be distributed with it, so you supply your
own. Setup uses it once, to convert the intro movie to AV1, which needs a build with
an AV1 encoder. **Not all Windows builds have the fast one, `libsvtav1`**:

| Build | Has `libsvtav1` | Use it? |
| --- | --- | --- |
| **Install with winget** (`Gyan.FFmpeg`) | Yes | Yes, this is the easiest |
| gyan.dev `ffmpeg-release-full` | Yes | Yes |
| BtbN `win64-gpl` or `win64-lgpl` | Yes | Yes |
| gyan.dev `ffmpeg-release-essentials` | **No** | See below |

The **essentials** build has only `libaom-av1`. Setup uses that instead, which takes
a little longer. **Launcher versions up to 0.0.10 cannot use it** and stop with
`Asset conversion did not complete: MOV/TITLE.STR`. If you see that message, install a
build from the table and press **Play** again; your disc is not the problem.

To check a copy of FFmpeg, run this in Command Prompt in its folder. It should list
`libsvtav1` or `libaom-av1`:

```
ffmpeg -hide_banner -encoders | findstr av1
```

A build with neither is refused before setup starts, and the error names it.

## Updates

Each time the launcher opens, it asks GitHub for the latest release. **Check for
updates** at the top of the launcher does the same on demand. No account or
personal data is sent. If the check fails, for example when you are offline, the
launcher opens normally and does not interrupt you.

If a newer version exists, the launcher asks before downloading anything. If you
choose **Yes**, it downloads the update, replaces the program files in this
folder and restarts itself. It cannot run while a disc check or game session is
active, and asks once that has finished.

Before changing anything, it checks the download against the SHA-256 checksum
GitHub publishes, checks every unpacked file, and confirms the new launcher can
start. If replacing the files fails, the previous version is restored. If your PC
loses power partway through, the next launch restores it. If the launcher itself
will not start after an interruption, extract the release ZIP over this folder.

Only program files listed in the release are replaced. Saves, settings and
prepared assets are not touched, and anything else you keep in this folder is
left alone. The folder must be writable, so install the game somewhere other
than `Program Files`. Updates come from this project's GitHub releases. These
builds are not code-signed.

## Data, saves and failures

- Settings and prepared assets are stored in the per-user
  `%APPDATA%\VS-NativePort\VagrantStory` directory supplied by SDL. The chosen
  save folder is independent of the installed game and prepared assets.
- The launcher remembers all three paths. Keep your disc image available for
  source validation. The game itself launches against the prepared assets.
- Remembered paths are in your own per-user configuration, never the tester ZIP.
  Detection is an explicit action; no save or converter location is preselected.
- The interface scales with monitor DPI. Resize or scroll the window on smaller
  displays; all setup fields and the Play button remain reachable.
- A failed setup does **not** start the game or mark incomplete assets ready.
  Correct the displayed error and retry. **Open setup log** opens `setup.log`
  in the per-user directory. It keeps the output of every setup step, including
  FFmpeg's own errors. This is installation output, not gameplay logging.
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

FFmpeg is external and is not included in this ZIP; see
[Which FFmpeg to use](#which-ffmpeg-to-use):
https://ffmpeg.org/download.html#build-windows

Project: https://github.com/JEschete/VS-NativePort
