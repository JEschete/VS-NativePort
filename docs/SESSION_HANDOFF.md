# Session handoff

Written 2026-07-26. Rewrite this file at the end of a session rather than
appending to it.

## State of the tree

Branch `work`. The audio session is committed as `5a68f319 Audio working, still
some bugs`, and upstream `main` has since been merged in.

**Uncommitted on top of that: 38 newly matched BATTLE functions, plus two
assembly-only segments converted into C translation units.** See "This session:
matching decomp" below. `make -j$(nproc)` reports **✔ All files match (21/21)**,
so the disc still reproduces byte-for-byte.

```
 M config/BATTLE/BATTLE.PRG/splat.yaml  2EA3C and 6E644 are now `c` subsegments
 M src/BATTLE/BATTLE.PRG/146C.c   func_8008D2C0_t typedef moved out
 M src/BATTLE/BATTLE.PRG/146C.h   func_8008D2C0_t, three camera getters declared
 M src/BATTLE/BATTLE.PRG/3A1A0.c  7 functions
 M src/BATTLE/BATTLE.PRG/3A1A0.h  unk1868 renamed to its real offset unk1878,
                                  unk1800[0x3C] split, unk5CA made volatile
 M src/BATTLE/BATTLE.PRG/40564.c  func_800ACF54 + func_800ACF54_t
 M src/BATTLE/BATTLE.PRG/573B8.c  two hand-written functions transcribed
 M src/BATTLE/BATTLE.PRG/5BF94.c  23 functions, 3 new struct types
 M src/BATTLE/BATTLE.PRG/5BF94.h  vs_battle_cameraPose_t, D_800F54B8_t
 M src/BATTLE/BATTLE.PRG/6E644.h  needs 146C.h for vs_battle_actor
?? src/BATTLE/BATTLE.PRG/2EA3C.c  new unit, 31 functions
?? src/BATTLE/BATTLE.PRG/6E644.c  new unit, 217 functions, 5 already matched
?? tools/dev/dcmp.py              the fast compare loop
```

The file list below is what the audio commit contains, kept as a map of where
that work lives.

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

Verifications, as of the audio commit:

```sh
make -j$(nproc)                      # ✔ All files match  (21/21)
make -f Makefile.native -j$(nproc)   # links
make -f Makefile.native mdectest     # 593/593 frames
```

The native build has **not** been re-run since the decomp work above; nothing in
it touches a native stub, but confirm before relying on that.

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

### Matching decomp

1. **`6E644` is now the long pole and it is open for business.** 212 of its 217
   functions are still `INCLUDE_ASM`, and roughly 128 of them have ordinary
   compiler stack frames, so most of it really is recoverable C. Start at the
   small end: `python3 /tmp/…/survey.py` equivalents aside, `grep -c INCLUDE_ASM
   src/BATTLE/BATTLE.PRG/6E644.c` and work up by the `nonmatching <name>, 0x<sz>`
   header in each `.s`.
2. **Do not try to decompile `2EA3C` into C.** It is now a `c` subsegment so the
   translation unit exists, but all 31 functions are hand-written: zero stack
   frames across the whole segment, callee-saved registers spilled to absolute
   scratchpad addresses (0x1F800200…), 205 GTE ops, 12 jumps through a register,
   and self-modifying code (`func_80098160` stores halfwords into the middle of
   `func_800980F8`). The right end state is a file of `__asm__` blocks in the
   style of `573B8.c`, which is a 2500-line transcription job, not decomp.
3. **Keep working `5BF94` and `3A1A0`.** Both still have small self-contained
   functions, and the struct types this session added (`vs_battle_cameraPose_t`,
   `D_800F54B8_t`, `D_800F55F4_t`, `func_800D6298_t`, `func_800D5D74_t`) unblock
   their neighbours: `func_800D21C0` and `func_800D236C` are the camera-pose
   interpolators that consume `D_800F5520[2]`, and `func_800D2560` and
   `func_800D2A38` are next to the node pool.
4. **`func_800D4FB4` and `func_800D6448`** (5BF94) are both fully understood and
   were left only because they need `D_800F569C_t.unk0[0xC]` split so the pointer
   at offset 8 can be named, and `D_800F569C_t` itself moved out of `5BF94.c`
   into a header. That refactor unblocks both plus anything else reaching
   `D_800F569C->unk8`.

### Audio and native

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

## This session: matching decomp

**38 BATTLE functions moved out of `INCLUDE_ASM`, all byte-exact**, 36 of them
into C and 2 into `__asm__` blocks because they are hand-written. Per unit:
`5BF94` 96 → 73, `3A1A0` 33 → 26, `40564` 21 → 20, `573B8` 9 → 7 (that file now
has none left), and 5 in the brand-new `6E644.c`.

**Two assembly-only segments became C translation units.** `2EA3C` (31
functions) and `6E644` (217 functions) were `asm` subsegments with no `.c` file
at all; both are now `c` subsegments. That is 70KB of game code that went from
unreachable to editable. Read the two traps in "Converting an asm segment"
below before doing another one.

Three coherent subsystems came out of the function work, and the struct types
they needed are the durable part of it:

- **Camera interpolation** (`5BF94`). `vs_battle_cameraPose_t` in `5BF94.h` is
  the 0x40-byte pose `func_800D1B18` captures: position, lookAt, an unidentified
  vector at 0x20, then roll (biased +0x8000), nearClip, projectionDistance and
  farClip. `D_800F5520[2]` is the from/to pair and `D_800F54D8` the interpolated
  result; `func_800D2518` is the struct assignment between them. `func_800D169C`
  / `func_800D17A8` lerp an `SVECTOR` / `VECTOR` componentwise, and
  `func_800D1718` / `func_800D1824` are their cosine-eased wrappers.
- **The four-slot table at `D_800F54B8`** (`5BF94`). `D_800F54B8_t` is six bytes
  with a 5-bit kind and two indirection flags packed into byte 1; `func_800D1EF0`
  arms slots from a mask, `func_800D1FEC` releases them, `func_800D1E2C` /
  `func_800D1E78` read the kind and roll a count.
- **The `D_800F55F4` node pool** (`5BF94`). `D_800F55F4_t` is a 0x78-byte
  doubly-linked node; `D_800F53B8_t.unk18` is now typed as its list head.
  `func_800D2888` (unlink) and `func_800D2904` (bulk free) match.

Also matched, `5BF94`: `func_800C58A4` (status-effect name lookup),
`func_800C64D0` (OT splice), `func_800CF92C`, `func_800CFC8C`, `func_800D1104`,
`func_800D12D8`, `func_800D5D74`, `func_800D6298`, `func_800D6310`,
`func_800D65D8`. `3A1A0`: `func_800A47C4`, `func_800A4A24`, `func_800A2FBC`
(bleeds the unk34 residual into the position over the remaining frames),
`func_800A3054`, `func_800A3310`, `func_800A36E0`, `func_800A3500`. `40564`:
`func_800ACF54`. `6E644`: `func_800D7FFC`, `func_800D8038`, `func_800D8054`,
`func_800D8200`, `func_800E4CE8`.

### Technique that came out of this, now in docs/DECOMP_NOTES.md

**New file: `docs/DECOMP_NOTES.md`.** Everything reusable this session produced
lives there rather than here, because this file gets rewritten each time. It
covers the hand-written-assembly test, eight gcc shape rules, the struct-layout
dump recipe, and the asm-segment conversion procedure. Read it before touching
another function; it is short.

The single most valuable item: **a function with an 8-byte stack frame that makes
a call was written by hand**, because gcc always lays down the 0x10
outgoing-argument area. That test settles `func_800C02A8` (573B8), which a
previous session spent time on and recorded as a one-register near-miss. It has
an 8-byte frame and two `jal`s, so no C will ever match it. It and
`vs_battle_memcpy` are now `__asm__` blocks in the style of the four already in
that file, and `573B8.c` has no `INCLUDE_ASM` left.

**Related lead, not yet settled.** `func_800688D4` in ENDING `D4`, the one that
survived 128,000 permuter iterations, is the same `_trig_table` sine lookup as
`func_80098AB8` in BATTLE `2EA3C`, which is hand-written and has a fall-through
`func_80098AB4` cos entry point that just adds 0x400. The ENDING copy keeps its
working value in `$a1` and only moves it to `$v0` in the epilogue, which is not
how gcc allocates a return value. The frame test cannot decide it because both
are leaves. Worth an hour before anyone points the permuter at it again.

### Which shape rule cracked which function

All eight rules are written up in `docs/DECOMP_NOTES.md`. The worked examples,
if you want to see one in situ:

| Rule | Function | What it looked like before |
|---|---|---|
| `&&` bit tests get folded | `func_800A47C4` | one `andi 0xF01` where the target has two `andi`s |
| where the `u_char` truncation lands | `func_800D1E2C`, `func_800D1E78` | `andi 0xFF` on the wrong side of the merge |
| index-first `addu` means integer arithmetic | `func_800D5D74` | one instruction, operands swapped |
| a constant live across blocks was a local | `func_800C64D0` | `0xFFFFFF` rematerialised three times |
| `lw lw sw sw` is a struct assignment | `func_800ACF54` | 15 instructions against 19, no `nop`s |
| split `x <<= n; x >>= n;` | `func_800A3500` | a consistent permutation of three registers |
| word reads of a byte-typed struct | `func_800A3310`, `func_800A36E0` | `lbu`+`andi 0x20` where the target has `lw`+`andi 0x200000` |
| `volatile` on a re-read field | `func_800A2FBC` | one divisor load where the target has three |

The struct-layout dump recipe in that file also closed out the item this
session inherited: `D_800F4538_t` was **not** approximate around 0x1878. Its
field named `unk1868` genuinely sits at 0x1878, because the preceding
`D_800F4538_unk1864` is 0x14 bytes. It is now renamed `unk1878`, which unblocked
`func_800A36E0` and `func_800ACF54` immediately.

### Three near-misses, deliberately left as assembly

Each reproduces the target's instruction sequence except as noted. The C is kept
verbatim in a comment above its `INCLUDE_ASM` so the next attempt starts there.

| Function | Unit | Remaining delta |
|---|---|---|
| `func_800D1930` | 5BF94 | target materialises `0xFF` before the loop counter, every shape tried emits it after (2 insns swapped) |
| `func_800D2698` | 5BF94 | 61 instructions, identical bar a consistent permutation of five registers |
| `func_800D27F0` | 5BF94 | target keeps the `unk8` increment in `v0` and moves the return value in last; every shape moves it first and increments in `v1` |

### The fast compare loop: `tools/dev/dcmp.py`

```sh
python3 tools/dev/dcmp.py src/BATTLE/BATTLE.PRG/5BF94.c func_800D2518
python3 tools/dev/dcmp.py src/BATTLE/BATTLE.PRG/5BF94.c   # everything pending
```

With no function names it checks everything you have written in C whose target
`.s` is still on disk, which is the set that matters: splat clears a function's
`.s` only on a full `make`, and building one object does not rerun it. So the
loop is *edit C, run dcmp, repeat*, and only then a full `make`. It refuses to
compare a function that is still `INCLUDE_ASM`, because that would compare the
assembled target against itself and always say MATCH.

It rebuilds the one unit and diffs by **encoding** rather than by disassembly
text. objdump prints `move`/decimal where spimdisasm prints `addu`/hex, so a
textual diff is all false positives. It masks any field a relocation fills in
(`%hi` /
`%lo` immediates, `jal` targets) on *both* sides when *either* side relocates
it: `_camera` is a `--defsym` in `config/BATTLE/BATTLE.PRG/Makefile`, so it is a
relocation in our object and a baked-in `0x1F800064` in the disassembly, and
comparing those naively reports a diff that is not one.

Turns a 3-minute full build into a 15-second check. A full `make` is still the
arbiter before you believe a unit.

## Matching decomp carried over from earlier sessions

Three functions matched and committed earlier: `func_800BFD9C`,
`func_800C0150` (573B8), `func_800D18B4` (5BF94). Three more got close and were
reverted, all with correct instruction count and control flow and one register
allocated differently:

| Function | Unit | Remaining delta |
|---|---|---|
| `func_800688D4` | ENDING D4 | `value` needs `a1`, gcc gives `v1`. **Probably hand-written**, see the lead above |
| ~~`func_800C02A8`~~ | BATTLE 573B8 | **Closed.** Hand-written; now an `__asm__` block |
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

2710 / 3702 functions, 73.20%. Code coverage 61.42%. (Was 2674 / 72.23% / 61.05%
at the start of this session; `make objdiff` regenerates `progress.json`.)

```
battle   1320/1856   536 left     the long pole, gates the game world
lib       551/1003   452 left     mostly PSYQ libs the port replaces
ending     50/54       4 left     func_800688D4, func_8006AF64, _renderText,
                                  _renderPhantomPain
menus     672/672     0 left  ✅
title     117/117     0 left  ✅
```

Where the remaining battle code is, by bytes still in assembly:

```
BATTLE.PRG/6E644   70464   217 fns,   5 matched   ~128 have real stack frames
BATTLE.PRG/146C    54660   663 fns, 623 matched
BATTLE.PRG/5BF94   ~48000  286 fns, 213 matched
BATTLE.PRG/4A0A8   28472   258 fns, 233 matched
BATTLE.PRG/3A1A0   24028    40 fns,  14 matched
BATTLE.PRG/2842C   22440    86 fns,  47 matched
BATTLE.PRG/2EA3C    8920    31 fns,   0 matched   all hand-written, see above
```

Both former `asm` segments are now `c` subsegments, so every one of those 248
functions is reachable from the source tree. `6E644` is where the remaining
value is; `2EA3C` holds none.

Upstream `main` was merged on 2026-07-26 (three commits: `5bc69a6b`, `7e3c3859`,
`a00a5550`). It brought `func_80086754` and `func_800863A4` in BATTLE `146C.c`,
taking that unit from 42 remaining `INCLUDE_ASM` to 40, plus a refactor that
moves several struct definitions into `58578.h` / `38C1C.h` and switches
`146C.c` to repo-root-relative includes. No conflicts, and no native-side
changes were needed: both new functions are battle code nothing reaches yet, so
neither was a native stub before or after.

## Useful things that are easy to forget

- **`make format` reformats two files nobody touched.** `src/SLUS_010.40/main.c`
  and `src/MENU/MENU7.PRG/260.c` are committed in a shape the current
  clang-format disagrees with, so the formatter rewrites them. Revert those two
  with `git checkout --` after running it, or the diff carries unrelated churn.
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
