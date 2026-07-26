# Session handoff

Written 2026-07-25. Rewrite this file at the end of a session rather than
appending to it.

## State of the tree

`HEAD` is `f1633202 Merge branch 'main' into work` on branch `work`. Nothing is
committed yet; everything below is uncommitted working-tree changes.

```
 M .gitignore                    make.log ignore rule
 D  make.log                     staged for deletion
 M Makefile.native               SOUND_HARVEST, sound_tables/audio_sink/cdxa
 M README.md                     audio status, VS_NO_AUDIO
 M docs/NATIVE_DEVIATIONS.md     section B rewritten, new section F
 M docs/NATIVE_PORT_PLAN.md      new §0 status block
 M native/platform/battle_asm.c  func_80018C30 stub removed; 5 SFX helpers real
 M native/platform/libds.c       decodes XA audio out of form 2 sectors
 M native/platform/libspu.c      SpuGetVoiceEnvelope, time-driven pump, CD vol
 M native/platform/sound_asm.c   +1150 lines: 8 transcribed functions, bss aliases
 M native/platform/spu_core.c    CD input mixing, volume-register fixes
 M native/tools/harvest_data.py  PS-EXE file-offset fix
?? native/platform/sound_tables.c  the three dispatch tables + LFO pointers
?? native/platform/audio_sink.c    SDL2 output
?? native/platform/cdxa.c          CD-XA ADPCM decoder
?? docs/SESSION_HANDOFF.md
```

**`src/` is untouched.** All three verifications pass:

```sh
make -j$(nproc)                      # ✔ All files match  (21/21)
make -f Makefile.native -j$(nproc)   # links
make -f Makefile.native mdectest     # 593/593 frames
```

## The port has sound

Audio plays out of the host's speakers by default, with nothing configured. The
intro movie has its soundtrack, and the title screen's menu SFX sound as you
move through it.

```sh
VS_HEADLESS=1 VS_WATCHDOG=95 VS_AUDIO_WAV=/tmp/a.wav \
VS_PAD_SCRIPT=2400:0x0800,2520:0x1000,2600:0x4000,2700:0x0020 \
  ./native/build/vagrant-native
# [audio] 44100Hz stereo via SDL "pulseaudio"
# /tmp/a.wav: 95s, peak 23883, 31% non-silent
```

Under WSL this reaches Windows through WSLg's PulseAudio socket with no setup,
given the 32-bit `libpulse` and `libSDL2` packages (the build is `-m32`).
`VS_NO_AUDIO=1` turns output off; `VS_AUDIO_WAV` is now independent of it.

Eight separate fixes, none of which showed a symptom until the last:

1. **`func_80018C30`, the driver's command front door, was a no-op stub.**
   Every sound request in the game arrives there. Now transcribed, along with
   the rest of the SFX path that was still assembly: `func_8001653C`,
   `func_800161C4`, `func_80019FC4`, `func_80015220`, `func_80015080`,
   `func_80015970`, `func_800147CC`. That is **8 of the 17 remaining
   `INCLUDE_ASM` functions in `25AC.c`**, in `native/platform/sound_asm.c`.
2. **Three dispatch tables were arrays of zeroes.** Rebuilt as real C in
   `native/platform/sound_tables.c`, with `g_Sound_LfoTable`.
3. **Split `.bss` labels broke struct aliasing.** Ledger F1. Read it.
4. **Sound lookup tables read as zeroes.** 26 of them, now in `SOUND_HARVEST`.
5. **`harvest_data.py` had the wrong file offset for `SLUS_010.40`.** Ledger F2.
6. **The mix was gated on `VS_AUDIO_WAV`.** `spu_render` is what consumes the
   key-on register, so an ordinary run was inert, not merely silent.
7. **The SPU rendered per `VSync` call, not per unit of time.** The game calls
   VSync about fifteen times a second during the movie, not sixty, so a fixed
   735 frames per call produced audio at quarter speed. The vblank counter once
   had this exact bug, and the fix is the same shape.
8. **The volume registers were decoded wrong, twice.** Voice and main volume
   hold *half* the level in a signed 15-bit field, so returning the raw field
   cost 6dB per multiply and 12dB across the mix. The CD input registers are
   not in that format at all (plain signed 16-bit), and reading the driver's
   0x7FFF as a 15-bit field gives -1 -- which is why the movie's soundtrack
   first arrived at a peak amplitude of exactly 1.

### The movie's soundtrack

`native/platform/cdxa.c` decodes the CD-XA ADPCM interleaved into `TITLE.STR`
(1402 of 11216 sectors; coding 0x01 = stereo, 37800Hz, 4-bit; all on channel 1,
so no XA filter is needed). It sits **behind libds, not behind libstream**,
because that is where the hardware does it: the drive decodes matching sectors
itself and feeds the SPU's CD input, and the CPU never sees them. Putting it
there is what makes the game's own `UpdateCdVolume` fades apply to the
soundtrack without any special-casing.

Two things worth knowing if you touch it:

- **The sound-group header layout was measured, not recalled.** Each 128-byte
  group starts with 16 bytes holding eight shift/filter parameters stored
  twice. Checking which of those bytes are always equal across all 25,236
  groups in the real file settles the layout: `[0]==[4]`, `[1]==[5]`, `[2]==[6]`,
  `[3]==[7]`, `[8]==[12]`, `[9]==[13]`, `[10]==[14]`, `[11]==[15]`. `/tmp` is
  gone by now, but the probe was ~40 lines of Python and is worth rewriting
  before trusting any spec.
- **The ring buffer must be a power of two.** It was `OUT_RATE * 2` (88200) at
  first, which is not, so the `& (RING_FRAMES - 1)` wrap mangled every index and
  the mixer read frames that had never been written. Symptom: a decoder that is
  provably perfect in isolation, and output that is 0.2% non-silent.

## Where the two hard bugs came from, because they will recur

Both are in ledger section F, and both are silent. They are worth internalising
before touching any other subsystem.

**F1, split `.bss`.** splat cuts a new `dlabel` at every referenced address, so
one C struct arrives as several symbols and the stub generator gives each its
own blob. `func_80019A58` gates the per-tick SPU flush on
`D_80039B00 | ...ActiveNoteMask | D_80037898`, where those two names *are*
`g_Sound_GlobalFlags.UpdateFlags` and `g_Sound_VoiceSchedulerState.KeyedFlags`.
The VM wrote the structs; the test read two blobs that were always zero. Fixed
with an `ALIAS` macro at the top of `sound_asm.c` that equates each split label
to base + its true offset. The sound driver is done; **no other subsystem has
been audited**, and the audit is mechanical. Compare `.bss` addresses in
`build/src/**/*.bss.s` against struct sizes in `src/include/*.h`.

**F2, harvested data.** A wrong-offset harvest is not a crash, it is a table of
plausible wrong numbers. It surfaced here as the semitone pitch table starting
with 0, so every note keyed on correctly, mixed correctly, and played at pitch
zero. Eyeball a harvested table once: the semitone table should read
`0x1000, 0x10F3, 0x11F5, …`.

## Next steps, in the order I would take them

1. **Music.** The only audio thing left, and it is structural rather than a
   driver problem: nothing on the boot path loads a `MUSIC0xx.DAT`
   (`_musicLBAs[]` in main.c indexes them by LBA), and `src/MENU/MENUF.PRG/3B8.c`,
   the one place that starts music from a menu, is in `NATIVE_EXCLUDE` in
   `Makefile.native`. A 95-second boot with scripted title input issues commands
   0x80 and 0x20 only, never 0x10/0x12/0x16/0x19/0x1A. Those are implemented and
   dispatch correctly. Start by finding out why MENUF is excluded.
2. **Audit the rest of the `.bss` aliasing** (ledger F1). Mechanical, and it
   will prevent a repeat of this session's debugging in another subsystem.
3. **Reverb, noise voices, pitch/amplitude modulation.** `spu_core.c` ignores
   the registers for all three, and the driver does ask for them.

The remaining `25AC.c` assembly, if audio needs more: `func_80012F10`,
`func_8001436C`, `Sound_LoadAkaoSequence`, `Sound_SetMusicSequence`,
`func_8001733C` (VM command 0x22), `func_80015BAC`, `func_8001CDD0`,
`func_800135D8` and `func_8001369C` (the last two already have native
transcriptions). `Sound_LoadAkaoSequence` and `Sound_SetMusicSequence` are the
ones music will need.

## Matching decomp: unchanged, and the trap in it

Nothing was matched this session. The state from last time still stands:

Three functions matched and committed earlier: `func_800BFD9C`,
`func_800C0150` (573B8), `func_800D18B4` (5BF94). Three more got close and were
reverted, all with correct instruction count and control flow and one register
allocated differently:

| Function | Unit | Remaining delta |
|---|---|---|
| `func_800688D4` | ENDING D4 | `value` needs `a1`, gcc gives `v1` |
| `func_800C02A8` | BATTLE 573B8 | gcc CSEs `&D_1F800014_mat` into `s0` |
| `func_800CFC48` | BATTLE 5BF94 | control flow right, allocation wrong |

**The permuter works** (`binutils-mips-linux-gnu` was the missing piece):

```sh
make permute TARGET=build/src/ENDING/ENDING.PRG/nonmatchings/D4/func_800688D4.s
```

**Result on `func_800688D4`: ~128,000 iterations, best score 55, no match.**
Candidates are under `nonmatchings/func_800688D4/output-<score>-<n>/`.

**Do not repeat this mistake:** a one-register diff was read as "nearly there,
just needs the permuter". 128k iterations without reaching 0 says the target was
compiled from a structurally different source, not merely allocated differently.
Treat a stubborn one-register diff as evidence of the wrong shape.

The lead in `output-55-1/diff.txt` is that the permuter assigns a mask into a
temporary (`if (new_var = 0x200 & arg0)`), matching the target's use of distinct
registers (`v1`, `a2`) where the naive C reuses `v0`. Applying named temporaries
to both mask tests had not been evaluated when that session ended.

The work-in-progress `D4.c` from that attempt is kept at
`nonmatchings/wip/D4.c.wip` (the whole `nonmatchings/` tree is in
`.git/info/exclude`). `src/ENDING/ENDING.PRG/D4.c` itself is reverted, which is
why the matching build is green.

`func_800688D4` is a sine lookup on a 0x1000-per-turn angle folded into the
table's first quadrant. `_trig_table` holds pairs, so an index steps four bytes
and the odd member is reached by nudging the base pointer. `0x400` mirrors about
the half turn, `0x200` mirrors within the quadrant and selects the pair's second
value, `0x800` flips the sign.

## Decomp status

2672 / 3702 functions, 72.18%. Code coverage 60.79%. (Unchanged; this session
was native-side only.)

```
battle   1282/1856   574 left     the long pole, gates the game world
lib       551/1003   452 left     mostly PSYQ libs the port replaces
ending     50/54       4 left     func_800688D4, func_8006AF64, _renderText,
                                  _renderPhantomPain
menus     672/672     0 left  ✅
title     117/117     0 left  ✅
```

## Useful things that are easy to forget

- **Regenerate the stub layer after adding a native definition.**
  `make -f Makefile.native stubs` before the build, or the link fails with
  multiple definitions. The `stubs` target is not a dependency of `all`.
- **Audio needs a power-of-two ring and a time-driven pump.** Both were got
  wrong once here; see "The movie's soundtrack" above.
- **Fast matching loop.** A full build only tells you which `.PRG` differs. Read
  the object directly:
  `mipsel-linux-gnu-objdump -d --no-show-raw-insn build/src/<unit>.o` and compare
  against `build/src/.../nonmatchings/<unit>/<func>.s`.
- **Diagnostic from which files fail.** If a change makes every MENU overlay
  fail, the new code is the wrong LENGTH, because they link against BATTLE.PRG.
  If only BATTLE.PRG fails, the length is right and the content is wrong.
- **Single-object builds do not work** for reverting to `INCLUDE_ASM`, because
  splat has to regenerate the `nonmatchings/*.s` set.
- **The menu harness segfaults** (`make -f Makefile.native menu`). Pre-existing
  and not diagnosed. The full boot path is unaffected.
- **`make -f Makefile.native test` does not link**: the GTE/GPU test harness
  pulls in `libgpu.c` without `present.c`, so `psx_present_set_display` is
  undefined. Also pre-existing, and confirmed so by reproducing it with this
  session's changes stashed. `mdectest` is the regression target that works.
- **Run git inside WSL.** Windows git against `\\wsl.localhost\…` has no SSH key
  and writes with the wrong file-mode and line-ending behaviour.
- **`.git/info/exclude`** carries the ignore rules for generated files, the
  user's memory card, and `nonmatchings/`, so they stay ignored on `main` too.

## Trace flags

`VS_HEADLESS`, `VS_WATCHDOG=N`, `VS_CD_SPEED`, `VS_CD_TRACE`, `VS_STR_TRACE`,
`VS_GPU_TRACE`, `VS_CARD_TRACE`, `VS_RCNT_TRACE`, `VS_SND_TRACE`,
`VS_AUDIO_WAV`, `VS_VRAM_RAW`, `VS_PAD_SCRIPT=<frame>:<mask>`, `VS_DISC`,
`VS_DATA_DIR`.
