# Native Source Port Plan: Work packages M5–M10

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · §14 contract and M0–M4: [WORK_PACKAGES.md](WORK_PACKAGES.md)

### M5 — Sequenced music/SFX and native audio output

**Scope/files:** `Engine/Audio/Seq`, `SoundEffects`, `Driver`, `Mixer`, `AudioDevice`, `Legacy/Audio/LibSpu.cpp`,
Content audio conversion; EC-02/04/06. Owner decision D23 selects sfizz/SFZ+WAV. M5 defines the supported fidelity
rules and explicitly lists unsupported SPU modes; no competing synthesis backend is authorized.

1. Separate musical/game rules from physical voice bits and register writes. Define timestamped note/voice operations,
   sample/bank handles, priorities, cancellation, envelopes, loops, pitch/volume/pan and reverb behavior.
2. Decode sample banks and sequence metadata with bounds/version checks. Cover expected loop points and tuning;
   keep behavior in the ported sequencer, not in opaque hardware-register snapshots.
3. Connect the selected voice engine/mix path to SDL output. Specify audio-thread ownership, source disposal,
   underrun/error handling and the relationship between simulation ticks and sample time.
4. Migrate effects, music transitions and streaming sources; preserve key-on/off timing and stealing/release rules.
   Define missing noise/FM/reverb behavior explicitly rather than returning success for unsupported operations.
5. Verify event traces plus decoded output for one title track, menu SFX, overlapping voices, fades and battle transition.

**Current implementation (contracts):**
- All nonempty disc sample banks import to versioned JSON plus per-instrument WAV/SFZ, keeping source articulation
  bytes in metadata. The existing AKAO sequencer commits typed controls to sfizz in 200-frame ticks at 48 kHz;
  controls migrate through typed native APIs, not SDK register shims. No second sequencer is introduced.
- Counter advancement uses the host's rational field rate with an integer remainder (59.94 Hz, not 60).
- Demand-driven output: the simulation owns the sequencer and program preparation and publishes complete control
  ticks through a bounded SPSC queue; the SDL callback owns active sfizz programs, renders on demand and returns
  retired programs to the control thread. File I/O, logging, instrument loading and control-thread waits stay out
  of the callback. Accelerated desktop replay keeps synchronous output.
- Noise follows the original clock/parity rules; FM uses the envelope-scaled preceding voice; the ten reverb presets
  run a native float delay network (not bit-exact SPU arithmetic). Mono folds down the full dry+wet mix.
- In-game menu effects, effect streams and ending XA (FLAC per D18, played through dr_flac on a separate SDL source)
  use the same voice engine and lifecycle (reservation, pause/resume, fades, EOF, cleanup). Saved music layers copy
  and swap complete native objects.
- Not yet accepted: synchronous first-use loading, full music/stream transitions, unused SDK/DMA helpers, and
  reference listening. Open work: #253.

**Gate:** required title audio is audible and reference-checked; supported paths no longer read/write SPU register or
sample-memory blocks. Output-device creation and sample-transfer completion are insufficient. Close EC-02 after all
remaining audio consumers migrate, with a defined source/clock contract available to M6.

### M6 — Converted movie playback and soundtrack lifetime

**Scope/files:** `Tools/Extract`, Content movie assets, `Engine/src/Movie/MoviePlayer.cpp`, `Movie/Intro/Player.cpp`,
the former `Legacy/Movie/LibPress.cpp`, Render and Audio interfaces; EC-04/08.

1. Implement the agreed STR conversion and versioned movie metadata, including frame count/rate, duration and audio.
   Validate supported input and conversion failure; external-tool absence must produce a usable diagnostic.
2. Implement decoding behind the existing player interface with explicit frame ownership and bounded buffering;
   replace page-space coordinates in the modern presentation contract. A player stub is not a decoder.
3. Synchronize picture to a defined audio/clock source. Preserve intro skip rules and the soundtrack tail under the
   title; define restart, natural completion, stop and resource cleanup independently.
4. Test invalid/missing assets, A/V drift, picture completion versus audio completion, skip at several points,
   re-entry, attract playback and exit during decoding. Use reference frame/time checks from the asset plan.

**Gate:** decoded picture/audio, duration, synchronization, skip and tail behavior pass. Black placeholders or bypassing
the intro cannot satisfy the movie gate. Delete unused MDEC-facing dependencies as consumers migrate.

**Verified title adoption:** #53 adopts an owned streaming session, bounded audio-clock presentation and GPU frame
output for the title movie. Other movie-asset parity checks and broader compatibility retirement remain separate.

### M7 — One complete field and actor slice

**Scope/files:** `Game/Field`, `Actors`, `Event`, `State`, Content room/model/animation decoders and required renderer;
EC-01/03/04/05. Select one named room/transition/actor fixture in M2's verification ledger before implementation.

1. Decode the required room/zone, actor, texture and animation data with stable IDs and validated references.
2. Integrate camera, room/actor rendering, animation, collision, movement, doors and event triggers in that slice.
3. Give load requests, scratch/workspace buffers and actors explicit owners. Replace original slot offsets for migrated
   data; specify cancellation and destruction order, stale-handle behavior and reset versus persistent room state.
4. Exercise enter, explore, leave and re-enter with canonical state, image checkpoints and required audio/events.
   Add malformed content and unload-during-load tests; use memory/lifetime instrumentation where supported.

**Gate:** the complete room loop passes without required stubs or original-layout dependence in migrated consumers.
The opening event and other Phase 1c content remain separate coverage entries until exercised.

### M8 — One deterministic combat slice

**Scope/files:** `Game/Battle`, `Battle/Ai`, `Items`, `Effects`, `State`, action/enemy catalogs and audio/render consumers.

1. Select a reproducible encounter with explicit enemy, equipment, initial state and RNG seed.
2. Connect targeting, prerequisites, damage/hit calculations, risk, action animation, AI, status and recovery/rewards.
   Resolve dispatch through typed behavior registrations; reject unsupported required action/effect IDs before use.
3. Test formulas independently with boundaries and known inputs, then the whole encounter with fixed inputs.
   Add effect/animation/sound checkpoints rather than verifying numerical damage alone.
4. Verify defeat, interruption, reward application and return to field, including repeated encounters and saved state.

**Gate:** canonical encounter outcomes and audiovisual checkpoints match the defined reference. Explicitly list
uncovered abilities/enemies/effects for M10; one encounter is not proof of full combat parity.

### M9 — Menus, inventory and recoverable persistence

**Scope/files:** `Game/Menu`, `Items`, `State`, `Title` Continue flow, `Engine/Save`, `Legacy/Save/LibCard.cpp`; EC-04/05/07/09.
Deliver menu/inventory and persistence as separately verified subpackages. Q6 selects versioned JSON for the
initial native-only milestone; PS1 import remains gated on supported variants and separate decoder fixtures.

1. Cover each menu family: navigation, equipment/inventory changes, storage, workshops, rename and options. Test
   repeated open/close and cancellation; replace reload-dependent statics with owned screen state.
2. Define a versioned semantic save schema with stable IDs and migrations, excluding pointers, padding and GPU/audio
   handles. Specify invalid versions/references and import support rather than serializing native structs.
3. Implement validated load and recoverable writes: temporary output, commit/replace semantics, backup/recovery and
   explicit handling of interrupted writes or unavailable storage. Never report success merely because a slot exists.
4. Decode supported user-supplied PS1 save variants separately from native persistence; compare inventory, flags,
   location and progression against fixtures. Keep card hardware behavior out of the new API.
5. Test round trips, corrupt/truncated files, unsupported versions, missing IDs, interruptions and repeated Continue.

**Gate:** each menu family has evidence; save/load/import preserve defined semantic state and recover predictably
from failures. Delete card/file-event adapters after the last caller migrates; helper functions alone do not pass.

### M10 — Full-content verification and retirement closure

**Scope:** all game/content ledgers, EC issues, boundary checks, generated ownership and release documentation.

1. Expand slice coverage to every zone, boss, menu, effect/plugin, ending and New Game+; identify all reachable
   assembly-only stubs and rewrite/verify them in the owning package before accepting coverage.
2. Complete format conversion and schema/reference validation for all shipped content, including streams and tables.
   Run with the original disc unavailable: the shipping runtime consumes converted assets, not sector requests or PRGs.
3. Remove each remaining bridge as its last consumer migrates: register/page models, GTE-style transactions, original
   scratch/overlay offsets, SDK timing/input/card interfaces and module-address relocation. A renamed helper is not removal.
4. Complete typed state ownership and C++ promotion with full diagnostics. Keep valid native arenas and faithful
   game algorithms where useful; their existence is not evidence of emulation.
5. Run full-playthrough replays plus interactive checks, content validation and final EC/dependency audit. Update
   instructions and unresolved limitations from actual outputs. Any Q4-approved retained backend must have explicit
   isolation, test coverage and a corresponding amended release gate.

**Gate:** Phase 1e behavior/content obligations and Phase 2e architecture obligations are each evidenced separately.
No required path contains a stub, unresolved dispatch or unconverted asset. No unexplained runtime exception remains,
and no count/percentage alone substitutes for the playthrough and subsystem verification artifacts.
