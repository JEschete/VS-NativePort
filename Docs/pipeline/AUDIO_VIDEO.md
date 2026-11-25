# Asset Pipeline: audio and movies

[Asset pipeline index](../ASSET_PIPELINE.md)

### 6.5 Sequenced audio (owner choice: **sequenced**)
The game plays music and sound effects by sequencing sampled instruments at runtime. Tempo, volume, fades and battle transitions change live, so the content is converted into equivalent modern pieces and our ported sequencer (`Audio::Seq`, copied from the decomp `Audio\Seq`) plays it:

| Piece | Format | Detail |
|---|---|---|
| Samples (implemented first slice) | `audio\banks\<bank>\<n>.wav` | 4-bit ADPCM decoded to mono PCM16 at 44.1 kHz. Console waveform parity is not established. |
| Instruments | `audio\banks\<bank>\<n>.sfz`, `bank.json` | SFZ supplies a sample/loop region to sfizz. Version-1 JSON retains original sample/loop offsets, tuning, ADSR, availability and optional `source_record` bytes (16 or 64). The existing sequencer computes pitch/pan; native envelopes run separately, so SFZ key/velocity tracking and envelope shaping are neutral. Pitch bend has no smoothing and spans 12 octaves in either direction. |
| Music (planned) | `audio\music\<id>.seq.json` | Decoded AKAO `{tick, op, args}` events with semantic jump targets; currently the original bytecode still drives the ported sequencer. |
| Sound effects (planned) | `audio\sfx\<id>.seq.json` | Same future event format; current effects retain original AKAO programs and native sample-bank bindings. |
| Index (planned) | `catalogs\music.json`, `catalogs\sfx.json` | Semantic IDs with original track/SE provenance. |

- The semantic event schema/round-trip encoder remains planned, not implemented by the sample importer.
- **Bank import:** `VS.Extract banks Native\data` (also called by `extract`) converts every bank through the
  `wave-bank` converter; one-byte zero files are placeholders (unused bank slots) that it and the runtime skip.
  Each bank is a manifest output (`audio.bank.<wave file>`) listing its `bank.json`, `.wav` and `.sfz` files with
  the source hash, so a rerun keeps unchanged banks, `--force` rewrites them, and two banks writing one bank id are
  refused. Exact-end records and equal start/loop offsets in erased, final DMA padding are explicitly unavailable,
  not fabricated silence. Active ADPCM remains strictly validated. WAVE0032's older 64-byte articulation is
  recognized by its payload layout, pitch table and envelope fields. Q20 base pitch is converted to native root/fine
  tuning; the complete twelve-pitch table and unpacked envelope survive in `source_record`. Its all-zero pitch tables
  are unavailable records. Original data/provenance remain import sources, not a new authoring-format requirement.
  Reimport after upgrading: existing SFZ files need the unsmoothed, extended pitch-bend range for FM.
- **Effect-package sample streams are not sample banks:** the `audio\streams\akao\<id>\` outputs come from block 13
  of the `EFFECT\*.P` packages. They are ADPCM samples, not AKAO sequences, and they stay outside the manifest until
  the effect-package conversion (#216) registers each package as one output. A separate converter for block 13 alone
  would mark the packages converted while their other blocks are not.
- **Mixer/voices:** behavioral, at the envelope level, at 48 kHz (plan §6.3), never SPU registers. Final dry+wet
  output (after any Mono fold-down) saturates to the original signed-PCM range, [-1, 32767/32768], before delivery to
  SDL. Quiet mixes remain additive; source voice gains are unchanged, including the deliberately doubled mono-stream
  pair. This is not normalization or compression, and intentional source overload may still sound distorted.
  Independent XA/movie sources are not part of this primary-mix boundary.
- **Clock/ownership:** the existing 240-Hz sequencer publishes complete, ordered 200-frame control ticks through a
  bounded single-producer/single-consumer queue. SDL demand renders the native voices at 48 kHz, so active
  samples/envelopes continue while gameplay stalls; the callback never runs the sequencer.
  - Instrument files load into separate sfizz programs on the control thread, then ownership transfers to the
    renderer; consumed queue cells return retired programs to the control thread for destruction. A failed load
    stops the requested voice explicitly; queue exhaustion is a logged fatal error. Pending-note envelope feedback
    prevents voice allocation from mistaking queued notes for finished voices.
  - Reset invalidates queued controls before clearing/repriming the native stream, without touching movies. The
    device must stop its callback before the voice engine is destroyed. Open/reset retains 2,400 frames (50 ms) of
    native-stream priming.
  - VSync advances counter 2 using the host clock's exact 60000/1001 field-rate ratio, retaining fractional clocks in
    an integer remainder (assuming 60 fields/s under-feeds and gradually exhausts the 48-kHz output buffer).
    Stop/resume preserves the remainder; counter setup resets it.
  - SFZ loading remains synchronous for gameplay, not for the renderer. Long stalls can delay new musical events;
    active samples continue rather than exhausting pre-rendered PCM. Tick boundaries are retained when queued
    controls catch up, not collapsed or rendered simultaneously.
  - No file I/O, instrument loading, logging or control-thread waits occur in the render callback. Debug logs report
    shortages, rendered frames, callback duration and control-thread gaps.
  - Accelerated desktop replays (`--speed > 1`) retain synchronous rendering and the 250-ms stale-output cap;
    real-time listening acceptance applies at speed 1.
  - `VS_AUDIO_CAPTURE` records rendered native PCM through a two-second bounded staging buffer, drained on the
    control thread. Overflow explicitly fails capture, not playback. Captures exclude queue padding, physical output
    gaps and independent movie/XA sources; their length follows device demand, not logical tick count.
- **Music state:** G00248-G00250 are views of G00246's `VS::Field::ZoneContext` fields, not separate storage. This
  lets normal zone initialization/load and opcode `0x92` reach the populated music slot. Save/layer/crossfade/restore
  use complete native config/channel objects, not PS1 byte extents. Saved-layer conflict resolution detaches
  displaced assignments while protecting active SFX and stream voices.
- **In-game menus:** the five owned wrappers dispatch original open/invalid/leave/select/change effects
  (10/7/6/5/4), preserving channel tags, centered pan and volume. These differ from the existing title-menu calls.
- **Effect streams:** `VS.Extract streams disks\SLUS-01040.cue Native\data` imports block 13 from every
  stream-bearing effect package (mono or stereo). The 64-byte source header, native frame loop and source pitch
  remain in `stream.json`; the ID is a stable FNV content identity, not an authenticity check. Stereo uses separate
  predictor histories and 0x800-byte source-channel chunks. Buffer-end ADPCM flags are not treated as logical sample
  endpoints. Runtime binding validates the bounded source and native assets, uses canonical room slot 3, and
  preserves native command-pointer width. The existing sfizz engine supplies the original reserved voice pair,
  controls, pause/resume, fades, completion and cleanup. A paused voice remains active even before its first
  rendered sample.
- **Fidelity modes:** a shared 44.1-kHz noise sequence uses the original clock/parity rules, independently of voice
  pitch. FM changes sfizz sample pitch using the preceding voice's envelope-scaled, pre-pan signal; voice zero is not
  modulated, pitch is bounded to the original range, and zero pitch holds sample position. Noise-only notes require
  no sample file. Mode changes, envelopes and effect controls share complete-tick publication and renderer reset
  semantics.
  - Reverb uses all ten original presets, named native delay/filter state and signed stereo wet gains. Its
    22.05-kHz reflection/comb/all-pass network uses the documented 39-tap input/output filters; linear interpolation
    bridges the 44.1-kHz filter clock and 48-kHz mixer. Disabling writes preserves existing reads/tails; preset
    changes clear the delay state and reset wet depth as the original SDK did. Tails continue after voice stops and
    are cleared by playback reset. The network uses floating-point arithmetic, not bit-exact SPU rounding, register
    storage or sample RAM.
  - The existing Mono choice folds down the complete dry+wet output at the same control-tick boundary, not just
    per-voice pan. Stereo delay state is retained when switching back.
- **Limits:** complete bank/effect-stream import is not all-track or all-scene listening acceptance. Every
  music/scene transition, perceptual reference parity and 5.1 remain separate acceptance work. Unused DMA/refill
  APIs, the unused actor interpolation wrapper and unproduced stream slots 1/2 are not promoted to supported runtime
  paths. sfizz interpolation and the float effect network are not claims of bit-identical console output. Measured
  evidence is on [#7](https://github.com/JEschete/VS-NativePort/issues/7).
- **Format/DSP sources:** [PSX-SPX SPU documentation](https://psx-spx.consoledev.net/soundprocessingunitspu/)
  and [AKAO articulation format research](https://github.com/vgmtrans/vgmtrans/blob/master/src/main/formats/Akao/AkaoInstr.cpp);
  preset ordering and control/reset semantics were checked against the read-only original libspu source.
- **Rendered previews:** none (owner choice). Phase 2 can add a `--render-previews` flag if wanted.

### 6.6 XA streams → FLAC
- **Source:** the shipped standalone XA track is `ENDING\ENDING.XA`. Raw mode-2 sectors are required; a cooked
  ISO-extracted folder loses the necessary subheaders. Original playback selects file 1/channel 0 at 75 sectors/s.
  The second `BeginXaStream` argument is initial CD volume, not a byte count.
- **Decode:** the `streams` command (also called by `extract`) validates sector/subheader bounds, coding and playback
  cadence before using the existing FFmpeg XA decoder and FLAC encoder. This track repeats each stereo payload four
  times; selecting one per playback period (not concatenating all copies) produces **443.52 seconds, 37800 Hz,
  stereo, 16-bit**. Consecutive identical playback periods are retained; this is cadence selection, not content
  deduplication. Missing channels/timing gaps, changing coding, mismatched redundant payloads and partial periods
  fail explicitly.
- **Output:** `audio\streams\xa\ENDING\ENDING.XA.flac`; `audio\streams\index.json` records its source LBA/extent,
  selected file/channel and rate. The legacy `files.tsv` row remains `stream`: it describes sector-addressed data
  access, not availability of the separate native audio asset. The manifest records the FLAC as output
  `audio.xa.<name>` of converter `xa-flac`, with the hash of the file's raw 2352-byte sectors (§8); an unchanged
  source keeps its FLAC on the next extraction.
- **Runtime:** the owned `XaPlayback` service uses the existing dr_flac dependency to decode on the control thread,
  then supplies an independent SDL source on demand (about 67 MB of PCM while this track plays); there is no
  callback file I/O, decoder work or gameplay-thread refill. Signed CD gain, fades and original
  stereo/attenuated-mono mixing feed that source. EOF, requested stop, driver/disc reset, device close and re-entry
  release its buffers and update the disc-request state. Missing/malformed/truncated assets become logged disc
  errors, not a successful silent completion.
- **Acceptance boundary:** API-level coverage (SDL postmix, seek, stop/re-entry and final-sector EOF/drain through
  the production disc queue) is distinct from full ending-scene/reference listening.

#### Audio producer and cleanup audit (#7)

| Surface | Production route | Boundary |
| --- | --- | --- |
| Title and in-game menus | Title calls and `Menu/Sounds.cpp` -> centered SFX commands -> AKAO/sfizz | Original IDs/tags. |
| Music and event transitions | `Event/ScriptAudio.c`, `Audio/Music.c`, zone state and `MusicState.cpp` | Native-sized save/restore/swap; not every scene crossfade. |
| Actor clips, weapon sounds and footsteps | Actor animation/movement commands -> actor/model/surface SFX helpers -> `Audio/Sfx.c` | No new speech synthesis or claim of voiced text dialogue. |
| Room, weather, doors and interactions | `Field/Environment.c`, `RoomEffects.c`, `Doors.c`, `Interactions.c` -> ordinary/bank SFX, pan and fades | Existing producer paths retain the common renderer; not every weather/room variant. |
| Event sound commands | `Event/ScriptAudio.c` -> bank loading, play/stop, pitch/pan/volume slides and saved music | Native-width stream parameters and typed callbacks preserve the shared dispatcher; no second sequencer. |
| Effect-package streams | Block 13 -> room slot 3 -> effect timeline -> `StreamControl.cpp` / `StreamControlLive.cpp` | Pause, EOF, reset, unbind and re-entry through the unbind route. |
| Movies | Matroska/FLAC -> existing `MoviePlayer` -> independent SDL source | Native-mix clear/reset leaves independent sources intact; primary-mix capture is not movie-output evidence. |
| Ending credits | Credits start/stop -> disc queue -> `XaPlayback.cpp` -> independent SDL source | Full credits navigation/listening is not established. |

Cleanup happens at voice stop/EOF, package unbind, driver/disc reset and device close. The existing unused
`Audio_UpdateActorSoundTransition` has no discovered producer/caller and is marked unused upstream. Stream slots 1/2
have no discovered producers; shipped effects use slot 3. The unused generic `Audio_FreeCutsceneStreamSlot` still
assumes owned storage: it must not be called on borrowed slot-3 package data. Normal production effects use the
unbind route. These unused APIs and DMA/IRQ helpers remain explicit EC-02 migration debt, not demonstrated gameplay
failures or claims of completed compatibility retirement. General discontinuous/multichannel XA, all-content
perceptual parity and 5.1 are outside this bounded acceptance.

Issue #7 owns functional output acceptance, not EC-02 retirement. Disc-derived audio/captures are not committed or
attached to issues.

### 6.7 Movies (STR → AV1 Matroska) (owner choice: **AV1**)
- **Source:** `MOV\TITLE.STR` (26 MB, raw mode-2 sectors: MDEC video + interleaved XA audio). It is read from the `.bin`, like XA. Whether it holds more than one movie (intro, attract) is settled from the decomp's movie code (`_initIntroMovie`, `_playIntroMovie`) and recorded in CONTENT_LEDGER.
- **Pipeline** (extract time only):
  1. `VS.Extract` writes the raw 2352-byte sectors of the movie to a temporary file.
  2. ffmpeg decodes it with its `psxstr` demuxer and `mdec` decoder, and encodes:
     - video with **SVT-AV1** (`libsvtav1`), or **libaom** (`libaom-av1`) in FFmpeg builds without it, at the source resolution and frame rate (no upscaling in Phase 1);
     - audio as **FLAC**.

     The settings are `-c:v libsvtav1 -preset 6 -svtav1-params lp=2 -crf 18` or `-c:v libaom-av1 -cpu-used 6 -row-mt 1 -threads 2 -crf 18 -b:v 0`, each with `-pix_fmt yuv420p -color_range pc -colorspace bt470bg -c:a flac` (`Content/MovieEncoder`). The encoder writes a random container ID, so two conversions differ as files while decoding to identical frames.
  3. The result is muxed into Matroska: `disc\MOV\TITLE.STR.mkv` (`files.tsv` lists the movie with form `mkv`). WebM cannot carry FLAC, so the container is MKV. The manifest records it as output `movie.<name>` of converter `str-mkv`, with the hash of the movie's raw sectors (§8); an unchanged source keeps its MKV, so a second extraction does not re-encode it.
- **Encoder dependency:** ffmpeg is an **extract-time tool only**. It is never linked or shipped. `VS.Extract` finds it on `PATH` or through `--ffmpeg <path>`. The launcher and `VS.Extract` both stop with an error naming the FFmpeg if it has neither encoder, and `VS.Extract` stops if a conversion fails. Some builds (Gyan's "essentials") have `libaom-av1` but no `libsvtav1` (#254); both encoders' output plays through the game's dav1d path.
- **Runtime (`Engine\Movie`):**
  - Matroska demux: libwebm `mkvparser`, BSD.
  - AV1 decode: **dav1d**, BSD.
  - FLAC decode: dr_flac or libFLAC.
  - The YUV planes are uploaded to an SDL3 GPU texture and converted in a shader. Presentation is clocked to the audio, and skipping is the game's original behavior.
- **Validation:** frame N of the decoded MKV against a jpsxdec decode of the same frame, with a PSNR threshold of ≥ 45 dB (near-lossless), for the first 60 frames and every 300th frame after that. The disc-corpus test checks only that the MKV keeps every frame FFmpeg's `psxstr` demuxer completes from the STR, at 15 frames a second; the PSNR comparison is not yet a test.
