# Native Source Port Plan: Per-system designs (content, save, game)

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · §6 and §6.1–§6.5 are in [SYSTEMS.md](SYSTEMS.md)

### 6.6 Content (`VS.Content`)
- **Responsibility:** the disc (for VS.Extract only), every format on it, and loading the converted content in `Native\data\` (D18).
- **Model:**
  - `Disc` opens `.bin/.cue` (mode 2 raw sectors) and parses ISO9660 itself. **Only VS.Extract uses it**; the game never reads the disc.
  - `AssetId` registry: every file the game references by LBA or index becomes a named ID (`asset.movie.title`),
    mapped to its converted file(s) in `Native\data\`. **Status (#47):** `Content::AssetCatalog`
    (`catalogs\assets.json`, schema `vs.assets` v1) names every `raw` extracted file
    `asset.<lowercase disc path, '/' → '.'>`, with size and original path/sector as provenance only; `catalogs\audio.json`
    (`vs.audio` v1) maps the music, SFX-bank, wave-bank and menu sound tables. `AssetHandle` is
    `{record, generation}`; reopening the data issues a new generation, so an old handle is refused rather than reread.
    `DataStore::ReadAsset` and the disc queue's asset requests read by handle at the whole-sector load size. VS.Extract
    (`AssetCatalogImport`) is the only code that turns sectors and executable tables into IDs. Converted forms (`mkv`,
    `stream`) and the remaining sector-addressed consumers are EC-04 work.
  - `AsyncLoader` does background loads with completion handles, replacing `DsRead` callbacks.
    **Actor queue ([#48](https://github.com/JEschete/VS-NativePort/issues/48)):** the actor/model request ownership slice
    is adopted. Each queue entry owns its parser phases, buffers, cursors, file-read identities, ticket and record-memory resolver; actor, weapon and
    sequence loads resume and cancel from the entry, not module globals. Queue shifts move this state, and clearing
    the queue (used by the battle initializer) discards it. Legacy parser symbols are output-only diagnostic snapshots;
    a raw ETM queue entry imports its legacy phase and buffer once when first processed, while that unowned producer
    remains #207. File-read starts distinguish success, a full request queue
    and failure (#518): only a full queue permits the synchronous read fallback, and a converted-file error or failed
    fallback frees the loader's allocation from the same heap end and fails the request before parsing, binding or
    texture upload. File-read locations and request pointers are also entry-owned; the legacy stream symbols are
    output-only snapshots for submitted requests. Failed or replaced reads fail the owned request after releasing its
    actual allocation, without releasing a foreign read generation. Owned requests check their target again when
    processing starts. Actor, weapon and gimmick instances own private mutable geometry; queued loads and menu previews retain
    immutable model definitions and animation banks instead of source actor slots. Resource lifetimes include texture
    leases and read-only bank extents, and reset invalidates old resource epochs. Reused model instances enter their
    initial queued-animation boundary explicitly, without copying a peer's playback state. Menu previews own their
    records and tickets; partial submission, failure, replacement and session cleanup cancel before releasing those
    resources, without releasing foreign records. Status rendering starts after its switcher acquires the owned
    preview, without borrowing a previous entry's record during initialization. Auxiliary creation resolves its gimmick
    record through its owner and publishes the actor only after successful asset completion; scripted actors likewise
    publish only after their owned model ticket reports success, and equipped actors only after all owned body and
    equipment loads succeed. Both cancel or drain pending loads before freeing the allocation (#55). Ashley's script
    model command stays non-blocking, refuses missing ownership or submission failure, and leaves eventual
    completion reporting and ticket cleanup to the actor load table. Refused
    publication and partial creation release that owner's geometry and occupancy without clearing a foreign slot.
    Gimmick instances retain immutable compiled templates,
    not mutable peer prefixes. A room change invalidates template cache selection without freeing geometry still
    needed for transition rendering; actor removal and model/session reset retire the instance bindings.
    Compiled SHP/WEP/SEQ rendering and playback layouts remain compatibility adapters, not retired authoring formats.
  - **Decoders** (used by VS.Extract and the round-trip tests) produce typed objects: TIM, MPD, ZND, SHP, SEQ, WEP, ARM, ZUD, SYD, FBT/FBC, EVT, AKAO, XA, MCDATA, ITEMNAME/ITEMHELP text, HF0/HF1 help, EFFECT data.
  - **Loaders** (used by the game) read glTF, PNG, WAV, FLAC and JSON from `Native\data\` and build the same typed objects. The round-trip rule in ASSET_PIPELINE §7 proves `decode(raw) == load(converted)`.
  - **Catalogs** are loaded from `Native\data\catalogs\`, validated against `schemas\`, and frozen into immutable registries (§9).
- **Absorbs:** libds (33), libcd (1) via shim → migrated; `Content` (56 rows).
- **Tests:** a decoder round-trip for every format against the GodHands / Unity-parser interpretations; the converter round-trips (ASSET_PIPELINE §7); catalog round-trip (§9.8).

### 6.7 Save (`Engine\Save`)
- **Model:** a versioned JSON save with string IDs for every item/location. The native milestone implements native
  save/load first; PS1 `.mcd`/`.mcr` import is a separate decoder and is not yet supported.
- **Absorbs:** libcard (4), libapi card events, `Save` (68 rows).
- **Tests:** save → load round-trip; import of a known PS1 save gives the expected inventory.
- **Storage and codec contract (2026-10-24):** Engine slot storage is payload-agnostic and requires Game validation,
  bounded reads, recoverable writes and explicit replacement consent for an unverified primary. The Game JSON codec
  encodes named semantic fields and stable string references, not native object bytes; source glyph buffers remain
  byte arrays. Its canonical-JSON FNV-1a64 checksum detects accidental corruption, not tampering. Schema/integrity
  decoding is staging only: content resolution, cross-reference checks and safe runtime application are separate
  required gates before production load.
- **Runtime and menus:** schema3, content validation and staged resume are connected to Data Save/Load and title
  Continue, including backup recovery/replacement and failed-commit retry. Open: PS1 import, old-schema migration,
  clear saves, unsupported transient/support states and final card-adapter retirement (EC-09); this is not the complete
  M9 gate.
- **Validated-document ownership (2026-10-25):** synchronous Game validation may retain the successfully decoded
  primary/backup document for that load; it is not decoded again after acceptance. Confirmation still freshly validates
  schema, integrity and content, then compares all modeled wire fields through the shared semantic encoder, not native
  padding or only a checksum. No asynchronous lifetime or cached load/overwrite consent is introduced.
- **Lazy inventory-cache capture (#180):** original item consumption clears an exhausted slot without rebuilding menu
  display indices. Semantic capture projects only ordered occupied survivors on its owned copy, preserving lazy missing
  entries and leaving live gameplay unchanged. Range/duplicate errors stay explicit, and saved-file validation still
  rejects dead references.
- **Paused grounded capture (#181):** the original field-menu pause freezes ordinary horizontal walking/turning intent.
  Capture accepts that intent only with control5/6 and an actual interaction pause, saves the current static
  position/facing without changing live state, and resumes through the normal idle room spawn. Active movement,
  vertical motion, jump, attachment, moving-support, route, transition, placement and camera gates remain.

### 6.8 Game modules (`VS.Game`)

| Module | Origin | Notes |
|---|---|---|
| `Boot` | SLUS `main` boot path | `Boot::RunSession` runs the modes **Boot → Title → Gameplay → Ending → Title**; field, battle and the menus share the gameplay mode's loop. `JumpToTitle`/`JumpToGameLoop`/`JumpToEnding` throw a `SessionTransfer` to the runner, so the frames they leave unwind before the destination is prepared (#44). |
| `Title` | TITLE.PRG (98 functions, all named) | Dev credits, intro movie, title menu, attract loop. **1b target.** |
| `Field` | SLUS + BATTLE field parts | Room/zone loading, camera, collision, doors. |
| `Battle` | BATTLE.PRG (1,850 functions) | Combat, risk, chains, AI. Largest naming job. |
| `Actors` | SLUS/BATTLE | Composition: `Transform`, `Model`, `Animator`, `Stats`, `Equipment`, `AI`, `Collider` components. |
| `Items` | BATTLE/MENU | Inventory, equipment, workshops, combine. Uses Defs from catalogs + Instances (§9). |
| `Menu` | MAINMENU + MENU0–F | Each MENUx becomes a screen class registered with a menu stack. |
| `Event` | SLUS/BATTLE event VM | Scripted scenes, cameras, dialog. |
| `Effects` | EFFECT + PLG*.BIN + SCREFF2 + EFFPURGE | **Template kinds** (tiledRings, particleSetup, trails9, …) + an **effects catalog** of parameters per original plugin ID (480 instantiations become data rows). |
| `Ending` | ENDING.PRG | Credits/ending sequences. |
| `State` | globals | `GameState` (party, flags, inventory, location, timers) as one owned, serializable object. |

Each mode is a `Boot::ModeModule` (#44) whose `Run()` runs on the game fiber until it returns the next mode or a jump
transfers out; the fiber's per-tick wait is its update, and its ordered initialization (`InitializeTitleSession`,
`InitializeGameplayOverlay`) and scoped teardown are its lifecycle. Statics that the original re-initialized by
reloading an overlay must be reset by the mode's initialization.

### 6.8.1 Actors and their assets: one shared path (owner, 2026-10-17)

Every actor (Ashley, NPCs, enemies, bosses, menu previews, the attract demos) goes through the same few
components, and every fix lands there, never in a scene:

| Component | Today (copied, x64-fixed) | Target |
|---|---|---|
| **Actor asset loader**: SHP models, SEQ animation banks, WEP weapons, ETM | `Content\src\ActorAssets.cpp`: the asset request queue (`Content_QueueActorAssetRequest` → `Content_UpdateActorAssetQueue`) and one parser per format (`Content_ParseActorModel`, `Content_ParseActorSequence`, `Content_ParseWeaponModel`) | `Content::ShpModel`, `Content::SeqBank`, `Content::WepModel` typed objects, decoded by VS.Extract (glTF + animation JSON) and loaded by Content (§6.6); the queue becomes `AsyncLoader` handles |
| **Actor handler**: records, spawn/despawn, equipment, placement | `ActorInstanceTable` explicitly registers generations, immutable creation definitions, component bindings and allocation ownership; lookups never adopt foreign slots. `ActorSpawning` publishes loaded instances and refuses new owned loads during removal. Room persistence and semantic snapshots read/write retained components. Destruction retires owned AI even without room maps; ordinary room exit retains Ashley's AI. Finalization refuses remaining loads or AI. Heap replacement cancels or drains owned loads and invalidates instances before reclaiming storage. Roster links, retained AI cursors and room special targets resolve generation slots, not mutable IDs ([#55](https://github.com/JEschete/VS-NativePort/issues/55)). | `Actors::ActorSystem` owning actors as components (`Transform`, `Model`, `Animator`, `Stats`, `Equipment`) |
| **Sequence handler**: playing SEQ animations, animation events | `Actors\Animation.c` (`Actors_PlayActorAnimation`, `Actors_FindActorAnimationHeader`, `Actors_InitializeActorAnimationPose`, `Actors_RunAnimationEvents`) | `Actors::Animator` over `Content::SeqBank`, with the same keyframe decoding and event script |

The stable identity, component ownership and instance/resource teardown slice is complete
([#55](https://github.com/JEschete/VS-NativePort/issues/55)); compiled model/animation/collision adapters and their
remaining EC-04/05 retirement gates are separate.

x64 rule for these formats until the typed decoders replace them: a loaded file keeps its own layout. Pointers the PS1
loader relocated in place stay 32-bit offsets (`copy_fixups.ONDISC_OFFSETS`; each use adds the offset to the record's
address), and records the code addresses by PS1 offset are reached by named field (`tools\port\field_at.py`). Each fix
lands once in the shared function, so it holds for every actor and every caller; `Native\Tests\src\LayoutAsserts.cpp`
checks the layouts they rely on.

### 6.8.2 State ownership and reset boundaries (#45)

Every piece of game state has one of four lifetimes, and it is reset only at that lifetime's boundary. The table
lists the owners as built; `GameState` as one owned object (§6.8 `State`) is still the target.

| Lifetime | Lasts | Owners | Reset or load point |
|---|---|---|---|
| Persistent | across saves | `State::stateFlags` (typed `StateFlags`), read and written by id through `State::ScriptFlags` (`LiveScriptFlag`, `SetLiveScriptFlag`); `State::scoreData`, `artsStatus`, `roomVisitProgress`, `learnedActionFlags`, `playTime`, `recentRoomEnemyStates`; `Items::inventory` and `inventoryIndices` (the original's misnamed `Audio_Seq_unused*` storage); `Input::settings` | New Game (`Title::InitializeNewGameData`); `Game::Save::Capture` and its restore in `Save/RuntimeSnapshot.cpp` |
| Session | from one title entry to the next | `Boot::SessionState`, `SoftResetState`, the save resume, and the workspaces that replaced the PS1 scratchpad: `Event::LiveEventCameraScratch()` (shake envelope and offsets), `Effects::EmitterWorkspace()` (emitter flags carried between emissions) and `Battle::LiveSphereWorkspace()` (sphere bases, sine samples, staged volumes) | `Boot::ResetLiveSessionState()`, called by every `ResetNativeSession` (`InitializeTitleSession`, `ResetToTitle`, the title transfer's preparation in `Boot::RunSession`, and the session's end); the jump into the game loop keeps them |
| Scene | one room or battle | `Field::RoomVisitState`, `Event::cameraWorkspace`, `Battle::targetSphere`, the AI room state and its borrowed bindings | room load; the AI session invalidation (`Battle/Ai/RoomState.cpp`) |
| Temporary | one frame or one call | `Render::FrameScratchpad()` (the frame setup rewrites it every frame), the AI `PlanningWorkspace` (one per call), `Render` model workspaces, event frame locals | none |

Rules:
- A soft reset must not leak session state into the next run. The PS1 kept scratchpad values across a soft reset
  only because nothing cleared them, and the title overlay overwrote them anyway; native code clears them at the title.
- A workspace that the original shared through the scratchpad gets its own typed struct with fixed capacity, so
  writers cannot reach each other's state. `Native\Tests\src\EventSessionTests.cpp` (`[session-reset][workspaces]`)
  checks that the workspaces survive the game-loop jump and are cleared at the title.

Open work: #57 (the planner still publishes its call-local workspace through globals while it runs), #488 (the overlay
arena keeps the original's overlapping slot offsets, EC-05).
