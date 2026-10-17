# Runtime tracing (deferred work)

Status: **not started**. Do this after the copy step compiles (it does not block copying), and before the
affected systems are modernized.

## Why

Static reading named everything it could (NAMING_CONVENTIONS.md §11.3). What remains `low` is low because the
code alone does not show what a value means:

- a member is set and tested, but no reader shows what it stands for;
- it is only ever cleared;
- the decompiled declaration disagrees with how the code indexes it.

Renaming these again from the same source would be guessing. The way forward is to watch the original game run.

## The queue

`python tools\naming\trace_queue.py` regenerates [ledgers/TRACE_QUEUE.csv](ledgers/TRACE_QUEUE.csv) from the
ledgers. Each row has:

- `touched_by`: the functions that use the item, as catalog names with RAM addresses, for breakpoints;
- `watch`: where the item lives, for read/write watchpoints. A fixed address (`name=0x800F1A00`), or
  `*pointer+offset` when the struct is reached through a pointer global.

Counts at creation (2026-10-17):

| Priority | Kind | Rows | With breakpoints | With watch address |
|---:|---|---:|---:|---:|
| 1 | `opaque`: read by code, meaning unknown | 377 | 348 | 74 |
| 2 | `low-named`: named from limited evidence | 378 | 268 | 155 |
| 2 | `global`: low-confidence globals | 52 | 47 | 51 |
| 3 | `type`: low-confidence types (named by their members) | 73 | n/a | 5 |
| 4 | `reserved`: never read in decompiled code | 1,434 | 38 | 426 |

Most rows are in Battle (476 opaque/low-named), concentrated in `Actors::ActorRecord`, `Battle::Ai::ActorAiState`
and its views, and `Field::GimmickInstance`. Many AI views are heap-allocated, so they have no watch address. For
those, break on a `touched_by` function and read the struct pointer from its argument register (`a0`–`a3`).

## Priority 0: layout and identity questions (do these first)

These change the shape of native types and arrays, not just names. Each is described in NAMING_CONVENTIONS.md
§11.2/§11.3. Resolve them from assembly (Ghidra MCP) or a trace before the affected code is modernized:

- `Battle::ActorTargetWord` (vs_battle_actor +0x10): pending Z movement offset vs last targeted actor/limb.
- `Input::VibrationPatternSlot` (T00581): the interpreter reads 48 bytes from a 56-byte element; the real element
  probably starts 8 bytes later.
- `Event::CameraState` (T00259): shake oscillators are read at +0x1A6, two bytes past the declared field.
- G00603: seven declared bounce offsets, but index 7 is read. G00820: ripple grid extent.
- G00805/G00806: overlap between the ending atlas and the shared-header instance.
- G00335/G00337: pause flag vs platform array indexing.
- G01961/G01963/G01970 and the other "cleared on room load, no reader" words: confirm they are write-only (then
  drop) or find the reader (asm or another overlay).
- `Field::MpdTileCell` (T00333): bits 5, 7–9, 15 and 19–30 are still `reserved`. Bit 6 is
  `blocksSphereTerrain` and bit 18 is `occlusionAndFloorFlags`; both are med and need confirming.
- `Audio::CutsceneStreamState.auxiliaryMask` and the other "meaning unresolved" audio flags.

## Tools

- **DuckStation** (`C:\Tools\Systems\PSX\Emulators\DuckStation`): installed. Its CPU debugger has breakpoints and
  memory read/write watchpoints. Use it to answer single questions by hand and to make save states.
- **PCSX-Redux** (`C:\Tools\Systems\PSX\Emulators\PCSX-Redux`): source only, not built yet. It is Lua-scriptable,
  so it is the tool for batch tracing: walk the queue, set breakpoints/watchpoints, log values per frame.
  Build it when the queue is worked in bulk.
- **Ghidra MCP**: settles layout questions from the matched assembly without running anything.
- Disc image: `disks\` (SLUS-01040).

Caveats:

- **Overlays share address ranges.** BATTLE/MENU/EFFECT/TITLE code and data at the same address are different
  objects depending on which overlay is loaded. Condition breakpoints on the loaded overlay (check a known word
  of the overlay, or break only after `Menu::Framework::LoadMenuOverlay` / the effect loader loads the one you
  want).
- Effect plugins (`PLGnnn.BIN`) are loaded on demand. Trace them from a save state taken just before the
  ability or spell that loads them.

## Scenarios (save states to make once)

Title menu; field walk; door and lock; trap tile; pit fall; chest; gimmick push/stack/fall; battle start;
targeting sphere; chain ability; defense ability; Break Art; enemy AI turn (several enemy kinds and a boss); loot
screen; each main menu screen (Items, Status, Map, Setup/workshop, Container, Score, Manual, Data); save/load;
game over; time attack result; ending.

## Recording results

1. For each settled row, add an entry to `tools\naming\sanity_fixes.py` (or the batch generator): the new name,
   meaning, confidence, and evidence.
2. Trace evidence is not a source line. Before the first trace-based fix, extend `merge_symbols.py`'s `EVIDENCE`
   pattern with a `trace:<scenario>:<what was observed>` token, and keep a short log per scenario under
   `tools\naming\traces\`. Prefer citing the source line the trace explained when there is one.
3. Run `python tools\naming\sanity_fixes.py`, the per-batch `--check`, the unit tests, and
   `python tools\naming\trace_queue.py` to shrink the queue. Commit per scenario.
4. A `reserved` member confirmed never read at runtime in its scenarios becomes a padding/`drop` candidate in the
   native type. One that is read gets a name.
