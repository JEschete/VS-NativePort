#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Actors/ActorSpawning.hpp"
#include "Actors/RoomEnemyMemory.hpp"
#include "Content/CdDrive.hpp"
#include "Event/RoomTriggers.hpp"
#include "Field/RoomState.hpp"
#include "Render/DisplayControl.hpp"

// The zone and room loading rules (BATTLE.PRG 146C.c:14766-15774 and
// 16031-17331): reading a zone file (ZND) for its room file table, enemy
// templates, textures and music; reading a room file (MPD) for its room
// sections, events, loot and actor placements; spawning the room's enemies
// and objects, placing Ashley at the entrance, setting up the room's camera
// and play, and unloading the room again. Each file is read through a
// bounded span and each record is decoded from its little-endian bytes;
// RoomLoadingLive.cpp binds the session's zone context, room record and
// heap.
namespace VS::Field {
// The zone files the location table names (Field_legacyZoneDiscFiles).
inline constexpr int kZoneCount = 256;
// The zone file header: seven words (room file table, enemy templates and
// textures, each as offset and size, then the zone music id).
inline constexpr std::size_t kZoneHeaderBytes = 0x1C;
// A room file table entry: the room file's sector and byte size.
inline constexpr std::size_t kRoomFileEntryBytes = 8;
// An enemy template (Field_ZndEnemyRecord, an Actors_CharacterInit).
inline constexpr std::size_t kZoneEnemyTemplateBytes = 0x464;
// The room file header: the room, cleared, script, door, enemy and loot
// sections, each as offset and size.
inline constexpr std::size_t kRoomFileHeaderBytes = 0x30;
// A room placement record (Field_MpdEnemySpawn or Field_MpdObjectSpawn).
inline constexpr std::size_t kRoomPlacementBytes = 0x28;
// The room sections: a header of 24 byte lengths, then the sections.
inline constexpr std::size_t kRoomSectionCount = 24;
inline constexpr std::size_t kRoomSectionHeaderBytes = kRoomSectionCount * 4;
// The actor slots a room fills: 0 is Ashley and 1 a scripted actor; the
// room's enemies take 2-9 (a second body four slots on) and its objects
// 2-15.
inline constexpr int kRoomActorSlots = 16;
inline constexpr int kFirstRoomActorSlot = 2;
inline constexpr int kRoomEnemySlotEnd = 10;
inline constexpr int kSecondaryBodySlotOffset = 4;
// The placements whose hidden flags the room enemy memory records.
inline constexpr std::size_t kRememberedHiddenFlags = 32;
// An enemy placement without a zone template: a plain actor.
inline constexpr int kUntemplatedEnemyId = 0x7F;
// An object placement that takes the first free slot.
inline constexpr std::uint8_t kAnyObjectSlot = 0xFF;
// The statuses entering a room clears from Ashley.
inline constexpr int kRoomEntryClearedStatuses = 0x21000;

// The original's debug reports (vs_main_nop9, which does nothing).
enum class RoomLoadDiagnostic : std::uint8_t {
  kNoFreeActorSlot = 0x86,
  kZoneReadPending = 0x87,
  kZoneDataHeld = 0x88,
  kRoomReadPending = 0x89,
  kRoomDataHeld = 0x8A,
  kEmptyRoomFileTable = 0x8B,
};

// What the typed loader refuses where the original read past its data.
enum class RoomLoadRefusal : std::uint8_t {
  kZoneId,
  kRoomFileId,
  kZoneReadFailed,
  kRoomReadFailed,
  kMalformedZoneFile,
  kMalformedZoneEnemies,
  kMalformedZoneTextures,
  kMalformedRoomFile,
  kMalformedRoomSections,
  kEnemyTemplate,
  kActorSlot,
  kNoEnvironment,
  kRoomEventLoad,
};

// Reports for every host: the original's diagnostics and the refusals.
class RoomLoadReporter {
 public:
  RoomLoadReporter() = default;
  RoomLoadReporter(const RoomLoadReporter&) = delete;
  RoomLoadReporter& operator=(const RoomLoadReporter&) = delete;
  RoomLoadReporter(RoomLoadReporter&&) = delete;
  RoomLoadReporter& operator=(RoomLoadReporter&&) = delete;
  virtual ~RoomLoadReporter() = default;

  virtual void Diagnostic(RoomLoadDiagnostic code) = 0;
  // `value` names what was refused (an id, slot or placement index).
  virtual void Refuse(RoomLoadRefusal refusal, int value) = 0;
};

// A byte range of a file.
struct ByteRange {
  std::size_t offset = 0;
  std::size_t bytes = 0;

  friend bool operator==(const ByteRange&, const ByteRange&) = default;
};

// --- Reading the zone and room files

// Which loads reset the scene timer (State_sceneTimer.resetScope); kNone
// for any other scope, which no load resets.
enum class SceneTimerScope : std::uint8_t { kRoom, kZone, kScene, kNone };
enum class DiscReadState : std::uint8_t { kPending, kLoaded, kFailed };
enum class LoadedFile : std::uint8_t { kZone, kRoom };

class RoomFileReadHost : public virtual RoomLoadReporter {
 public:
  [[nodiscard]] virtual bool ReadPending(LoadedFile file) const = 0;
  [[nodiscard]] virtual bool DataHeld(LoadedFile file) const = 0;
  // Takes the file's block from the heap's high end and starts its read, or
  // reads it at once when the disc queue is full. A pending request or held
  // block is replaced, as the original replaced it.
  virtual void BeginRead(LoadedFile file, Content::DiscFileExtent extent) = 0;
  [[nodiscard]] virtual DiscReadState PollRead(LoadedFile file) const = 0;
  // Runs the play clock and the disc queue while a read waits.
  virtual void UpdateWhileLoading() = 0;
  virtual void ReleaseRead(LoadedFile file) = 0;
  // The read file's bytes, bounded by its size; empty when none is held.
  [[nodiscard]] virtual std::span<const std::byte> LoadedData(
      LoadedFile file) const = 0;
  virtual void FreeLoadedData(LoadedFile file) = 0;
  [[nodiscard]] virtual SceneTimerScope SceneTimerResetScope() const = 0;
  virtual void ResetSceneTimer() = 0;
};

// Starts a read of `extent` into a new block, reporting a pending request
// (`pending`) and a held block (`held`) first.
void QueueFileRead(RoomFileReadHost& host, LoadedFile file,
                   Content::DiscFileExtent extent, RoomLoadDiagnostic pending,
                   RoomLoadDiagnostic held);
// Waits for the file's read and releases its request. False, with the block
// freed and the failure refused, when the read failed; the original waited
// forever.
bool AwaitFileRead(RoomFileReadHost& host, LoadedFile file);

// --- The zone file

struct ZoneFileLayout {
  ByteRange room_files{};
  // From the section's offset to the file's end: the original ignored the
  // enemy and texture section sizes.
  ByteRange enemies{};
  ByteRange textures{};
  int music_id = 0;

  friend bool operator==(const ZoneFileLayout&,
                         const ZoneFileLayout&) = default;
};

// Nothing when the header or a section lies outside the file.
[[nodiscard]] std::optional<ZoneFileLayout> DecodeZoneFile(
    std::span<const std::byte> file);

// The room file table: each entry's sector and byte size. A partial last
// entry is dropped.
[[nodiscard]] std::vector<Content::DiscFileExtent> DecodeRoomFileTable(
    std::span<const std::byte> table);

// The enemy section: a count, the count's ZUD files, then the count's
// templates.
struct ZoneEnemyTables {
  std::vector<Content::DiscFileExtent> zud_files{};
  std::span<const std::byte> templates{};
};

// Nothing when the count is negative or the tables leave the section.
[[nodiscard]] std::optional<ZoneEnemyTables> DecodeZoneEnemyTables(
    std::span<const std::byte> section);

class ZoneTableHost : public virtual RoomLoadReporter {
 public:
  // Copies the room file table into the zone (a new low-end block).
  virtual void StoreRoomFiles(
      std::span<const Content::DiscFileExtent> room_files) = 0;
  // Copies the ZUD files and templates into the zone (new low-end blocks);
  // empty tables clear them.
  virtual void StoreZoneEnemies(
      std::span<const Content::DiscFileExtent> zud_files,
      std::span<const std::byte> templates) = 0;
};

// decomp: BATTLE.PRG _loadMpdLbas (146C.c:14774)
// An empty table is reported and stored, as the original stored it.
void LoadZoneRoomFileTable(ZoneTableHost& host,
                           std::span<const std::byte> table);
// decomp: BATTLE.PRG _loadZndEnemies (146C.c:14783)
// A malformed section is refused and clears the tables.
void LoadZoneEnemyTemplates(ZoneTableHost& host,
                            std::span<const std::byte> section);

// A palette row a zone texture fills (the CLUT rows at VRAM line 224).
struct ZonePaletteRow {
  int row = 0;
  int first_color = 0;
  int colors = 0;
};

// The VRAM position zone textures are stored from; textures at or right of
// kZonePaletteColumn are palette rows, kept in the room palettes.
inline constexpr int kZonePaletteColumn = 768;
inline constexpr int kZonePaletteLine = 224;
inline constexpr int kZonePaletteRows = 4;
inline constexpr int kZoneTextureColumnShift = 320;
inline constexpr int kZoneTextureLineShift = 256;

class ZoneTextureHost : public virtual RoomLoadReporter {
 public:
  virtual void CopyPaletteRow(std::span<const std::byte> colors,
                              const ZonePaletteRow& row) = 0;
  virtual void UploadImage(const Render::ScreenRect& area,
                           std::span<const std::byte> pixels) = 0;
};

// decomp: BATTLE.PRG _loadZndTims (146C.c:15608)
// The section starts with four table sizes; only the first table, a count
// and that many sized TIMs, is read. Palette TIMs fill the room palette rows
// they cover; the others are uploaded 320 columns left and 256 lines down.
// A missing first table (a size of 0 or less, which the original read
// through a null pointer) uploads nothing. A TIM that does not parse is
// skipped; a section shorter than its sizes or a TIM size leaving the table
// is refused and ends the upload.
void UploadZoneTextures(ZoneTextureHost& host,
                        std::span<const std::byte> section);

// The music slots a zone change frees (1-4) and the slot the zone's music
// loads into (146C.c:14862-14873).
inline constexpr int kZoneMusicSlots = 4;
inline constexpr int kZoneMusicTargetSlot = 3;

class ZoneLoadHost : public RoomFileReadHost,
                     public ZoneTableHost,
                     public ZoneTextureHost {
 public:
  [[nodiscard]] virtual Content::DiscFileExtent ZoneFile(int zone_id) const = 0;
  [[nodiscard]] virtual int ZoneMusicId(int zone_id) const = 0;
  [[nodiscard]] virtual int CurrentMusicId() const = 0;
  [[nodiscard]] virtual bool MusicFadeEnabled() const = 0;
  // Fades the playing music out ahead of the new zone's.
  virtual void FadeOutCurrentMusic() = 0;
  [[nodiscard]] virtual bool NativeResumePending() const = 0;
  [[nodiscard]] virtual int GameEntryMode() const = 0;
  virtual void ResetZoneScriptFlags(bool new_game) = 0;
  virtual void StopAndFreeCurrentMusic() = 0;
  virtual void FreeMusicSlot(int slot) = 0;
  virtual void LoadWaveBank(int music_id) = 0;
  // Loads the music into `target_slot`; returns the music slot to play.
  [[nodiscard]] virtual int LoadMusicSlot(int music_id, int target_slot) = 0;
  virtual void SetMusicSlot(int slot) = 0;
  // Stores the zone's music id and enables the music fade.
  virtual void FinishZoneMusic(int music_id) = 0;
};

// decomp: BATTLE.PRG _loadZnd (146C.c:14803)
// An id outside the zone table is refused (false).
bool QueueZoneLoad(ZoneLoadHost& host, int zone_id);
// decomp: BATTLE.PRG _finishLoadZnd (146C.c:14826)
void FinishZoneLoad(ZoneLoadHost& host, int zone_id);

class ZoneReleaseHost {
 public:
  ZoneReleaseHost() = default;
  ZoneReleaseHost(const ZoneReleaseHost&) = delete;
  ZoneReleaseHost& operator=(const ZoneReleaseHost&) = delete;
  ZoneReleaseHost(ZoneReleaseHost&&) = delete;
  ZoneReleaseHost& operator=(ZoneReleaseHost&&) = delete;
  virtual ~ZoneReleaseHost() = default;

  virtual void ReleaseZoneEventResources() = 0;
  // Frees the templates, ZUD files and room file table and clears them.
  virtual void FreeZoneTables() = 0;
};

// decomp: BATTLE.PRG func_80089098 (146C.c:14886)
// The original's empty cleanup hook (_vsBattleNop2, 146C.c:16031) is
// VS::Core::NoOperation and is not called here.
void ReleaseZoneData(ZoneReleaseHost& host);

// decomp: BATTLE.PRG func_80089104 (146C.c:14899)
// The zone's music id (the original's D_800F18A8 view).
[[nodiscard]] int ZoneSoundFileId(int zone_music_id);

// --- Room placements

// An enemy placement (Field_MpdEnemySpawn).
struct EnemyPlacement {
  std::uint8_t enemy_id = 0;
  // 0 takes the first free enemy slot.
  std::uint8_t slot = 0;
  std::uint8_t flag_test = 0;
  std::uint8_t stat_test = 0;
  std::uint16_t flag_value = 0;
  std::uint16_t stat_value = 0;
  Actors::SpawnPlacement placement{};
  std::uint8_t facing = 0;
  bool starts_engaged = false;
  std::uint8_t scene_test = 0;

  friend bool operator==(const EnemyPlacement&,
                         const EnemyPlacement&) = default;
};

// An object placement (Field_MpdObjectSpawn).
struct ObjectPlacement {
  std::uint16_t flag = 0;
  std::uint8_t flag_min = 0;
  std::uint8_t flag_max = 0;
  // kAnyObjectSlot takes the first free slot.
  std::uint8_t slot = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t height = 0;
  std::uint8_t direction = 0;
  std::uint16_t model = 0;

  friend bool operator==(const ObjectPlacement&,
                         const ObjectPlacement&) = default;
};

struct RoomPlacement {
  // 0 places an enemy; any other kind an object.
  std::uint8_t kind = 0;
  std::uint8_t hidden = 0;
  EnemyPlacement enemy{};
  ObjectPlacement object{};

  [[nodiscard]] bool IsEnemy() const { return kind == 0; }
  // The slot the placement names (the enemy or object slot).
  [[nodiscard]] std::uint8_t NamedSlot() const {
    return IsEnemy() ? enemy.slot : object.slot;
  }

  friend bool operator==(const RoomPlacement&, const RoomPlacement&) = default;
};

// The placement records; a partial last record is dropped.
[[nodiscard]] std::vector<RoomPlacement> DecodeRoomPlacements(
    std::span<const std::byte> records);

// The flags and Ashley's hit points the placement tests read.
class PlacementTestHost {
 public:
  PlacementTestHost() = default;
  PlacementTestHost(const PlacementTestHost&) = delete;
  PlacementTestHost& operator=(const PlacementTestHost&) = delete;
  PlacementTestHost(PlacementTestHost&&) = delete;
  PlacementTestHost& operator=(PlacementTestHost&&) = delete;
  virtual ~PlacementTestHost() = default;

  [[nodiscard]] virtual std::uint8_t ScriptFlag(int flag) const = 0;
  [[nodiscard]] virtual int AshleyCurrentHp() const = 0;
};

// Whether an enemy placement shows: not hidden and its scene, flag and stat
// tests passing (146C.c:14936-15016).
[[nodiscard]] bool EnemyPlacementVisible(const RoomPlacement& placement,
                                         const PlacementTestHost& host);
// The spawn flags of an enemy placement: its facing rule, with 0x80 when it
// starts engaged (146C.c:15020-15036). A facing past 3, which the original
// left unset, faces as 0 does.
[[nodiscard]] int EnemySpawnFlags(const EnemyPlacement& enemy);
// Whether an object placement shows: its script flag within its range.
[[nodiscard]] bool ObjectPlacementVisible(const ObjectPlacement& object,
                                          const PlacementTestHost& host);

class RoomActorSpawnHost : public virtual RoomLoadReporter,
                           public PlacementTestHost {
 public:
  [[nodiscard]] virtual std::span<const std::byte> PlacementRecords() const = 0;
  [[nodiscard]] virtual bool ActorSlotOccupied(int slot) const = 0;
  [[nodiscard]] virtual int ZoneEnemyCount() const = 0;
  [[nodiscard]] virtual bool TemplateUsesSecondaryBody(int enemy_id) const = 0;
  // Spawns placement `placement_index` from template `enemy_id`.
  virtual void SpawnEnemy(int slot, std::size_t placement_index, int enemy_id,
                          int flags) = 0;
  // Spawns a plain actor (kUntemplatedEnemyId) without equipment.
  virtual void SpawnUntemplatedEnemy(int slot,
                                     const Actors::SpawnPlacement& placement,
                                     int flags) = 0;
  virtual void SpawnObject(int slot, int model,
                           const Actors::SpawnPlacement& placement,
                           int material) = 0;
  virtual void RememberHidden(int spawn_index, std::uint8_t hidden) = 0;
  // Frees the cached ZUD and forgets it.
  virtual void FreeCachedZud() = 0;
};

// decomp: BATTLE.PRG func_80089114 (146C.c:14901)
// Spawns each shown placement, then records the placements' hidden flags
// (at most kRememberedHiddenFlags) and frees the cached ZUD. An enemy
// template outside the zone and an explicit slot outside the actor slots
// are refused and skipped; an untemplated enemy past the zone's templates
// has no second body. The original read past its tables for these and
// recorded 32 hidden flags whatever the placement count.
void SpawnRoomActors(RoomActorSpawnHost& host);

// --- Ashley's entrance

// The pending door's entrance (Field_pendingDestinationDoor).
struct RoomEntrance {
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t direction = 0;
  std::uint8_t elevation = 0;
  bool has_entry_step = false;
};

// A step into the room: its world offset.
struct EntryStep {
  int x = 0;
  int z = 0;

  friend bool operator==(const EntryStep&, const EntryStep&) = default;
};

inline constexpr int kEntryStepDistance = 0x40;
inline constexpr int kElevationHeightUnits = 64;

// The step an entrance takes into the room (146C.c:15158-15183). An
// elevation past 3, whose step the original left unset, takes none.
[[nodiscard]] EntryStep EntranceStep(const RoomEntrance& entrance);

class RoomEntranceHost {
 public:
  RoomEntranceHost() = default;
  RoomEntranceHost(const RoomEntranceHost&) = delete;
  RoomEntranceHost& operator=(const RoomEntranceHost&) = delete;
  RoomEntranceHost(RoomEntranceHost&&) = delete;
  RoomEntranceHost& operator=(RoomEntranceHost&&) = delete;
  virtual ~RoomEntranceHost() = default;

  [[nodiscard]] virtual RoomEntrance PendingEntrance() const = 0;
  virtual void PlaceAshley(const Actors::SpawnPlacement& placement) = 0;
  virtual void StepAshley(const EntryStep& step) = 0;
  // Restores Ashley's idle pose at the view, clears her collision bypass
  // and sets up the room's navigation.
  virtual void SettleAshley() = 0;
};

// decomp: BATTLE.PRG func_80089888 (146C.c:15142)
void PlaceAshleyAtRoomEntrance(RoomEntranceHost& host);

// decomp: BATTLE.PRG func_80089A00 (146C.c:15190)
// Selects the remembered room for `room` (or the oldest, cleared) and makes
// it the most recent. A remembered room returns each placement's hidden
// flag, its remembered one where it has one; the oldest room returns none.
struct HiddenFlag {
  std::size_t placement_index = 0;
  std::uint8_t hidden = 0;

  friend bool operator==(const HiddenFlag&, const HiddenFlag&) = default;
};
[[nodiscard]] std::vector<HiddenFlag> RestoreRoomEnemyCache(
    Actors::RoomEnemyMemory& memory, const CurrentRoom& room,
    std::span<const RoomPlacement> placements);

class RoomPlacementHost {
 public:
  RoomPlacementHost() = default;
  RoomPlacementHost(const RoomPlacementHost&) = delete;
  RoomPlacementHost& operator=(const RoomPlacementHost&) = delete;
  RoomPlacementHost(RoomPlacementHost&&) = delete;
  RoomPlacementHost& operator=(RoomPlacementHost&&) = delete;
  virtual ~RoomPlacementHost() = default;

  // An encounter's placements in place of the room's, or `room` unchanged.
  [[nodiscard]] virtual std::span<const std::byte> EncounterPlacements(
      std::span<const std::byte> room) = 0;
  // Copies the records into the zone (a new low-end block); empty records
  // clear them.
  virtual void StorePlacements(std::span<const std::byte> records) = 0;
};

// decomp: BATTLE.PRG _loadMpdEnemySection (146C.c:15238)
void LoadRoomEnemyPlacements(RoomPlacementHost& host,
                             std::span<const std::byte> section);

// --- The room sections

enum class RoomSectionId : std::uint8_t {
  kGeometry,
  kCollisionMap,
  kTileCells,
  kDoors,
  kLighting,
  kAuxiliary,
  kSupplemental,
  kSection8,
  kFloorTriggers,
  kLockTriggers,
  kEnvironment,
  kTextureEffects,
  kEffectScripts,
  kSectionE,
  kMinimapScene,
  kTiledBackground,
  kViewTriggers,
  kMovingPlatforms,
  kContainers,
  kSoundData,
  kSection15,
  kSection16,
  kSection17,
  kZoneBounds,
};

struct RoomSectionLayout {
  std::array<std::uint32_t, kRoomSectionCount> lengths{};
  // Each section's offset in the section block (the lengths before it).
  std::array<std::size_t, kRoomSectionCount> offsets{};
  std::size_t total_bytes = 0;

  friend bool operator==(const RoomSectionLayout&,
                         const RoomSectionLayout&) = default;
};

// Nothing when the header is short, a length is negative as the original's
// signed read, or the sections leave the data.
[[nodiscard]] std::optional<RoomSectionLayout> DecodeRoomSectionLayout(
    std::span<const std::byte> data);

// The section-E header's array offsets (146C.c:15697-15705).
inline constexpr std::size_t kSectionEHeaderBytes = 0x14;
// Writes section E's array offsets; false when the header is short.
bool ResolveSectionEArrays(std::span<std::byte> section_e);
// Writes the collision map's cell count (width x height, 16 bits); false
// when the header is short.
bool ResolveCollisionCellCount(std::span<std::byte> collision_map);

class RoomSectionHost : public virtual RoomLoadReporter {
 public:
  virtual void ClearRoomTint() = 0;
  // Copies the section lengths and sections into the room record (a new
  // low-end block) and marks it loaded by `owner_room_index`, undisplayed;
  // false when the block cannot be taken.
  virtual bool StoreRoomSections(int owner_room_index,
                                 const RoomSectionLayout& layout,
                                 std::span<const std::byte> sections) = 0;
  // The loaded room's section, bounded by its length; empty when absent.
  [[nodiscard]] virtual std::span<std::byte> RoomSection(
      RoomSectionId section) = 0;
  virtual void OffsetRoomTextureIds() = 0;
  // Resolves the minimap scene, restores its visits and marks its first
  // room visited.
  virtual void PrepareMinimapScene() = 0;
  // The door the last transition entered through plus one, or 0.
  [[nodiscard]] virtual int PendingEnteredDoor() const = 0;
  virtual void ClearPendingEnteredDoor() = 0;
  [[nodiscard]] virtual int EnteredDoorId() const = 0;
  virtual void MarkEntranceDoor(int door) = 0;
  // Resets the gimmick queue and sets up the room effects, chests, moving
  // platforms, floating labels, weather particles, background colours and
  // palettes, in the original's order, then clears the reserved byte.
  virtual void InitializeRoomSystems() = 0;
  virtual void WaitForOverlayReady() = 0;

  [[nodiscard]] virtual RoomDisplay LoadedRoom() const = 0;
  // Releases the tiled background, the weather particles and the room
  // effects.
  virtual void ReleaseRoomResources() = 0;
  // Frees the section block and clears the room record.
  virtual void FreeRoomSections() = 0;
};

// decomp: BATTLE.PRG _loadMpdRoomSection (146C.c:15663)
// False when every section is empty or the sections are malformed (refused).
// A short section-E or collision map header is refused and left unresolved.
bool LoadRoomSections(RoomSectionHost& host, int owner_room_index,
                      std::span<const std::byte> data);
// decomp: BATTLE.PRG func_8008ABF0 (146C.c:15774)
bool ReleaseOwnedRoomSections(RoomSectionHost& host, int owner_room_index);

// The loaded room's sections, each bounded by its length.
using RoomSections = std::array<std::span<std::byte>, kRoomSectionCount>;

// decomp: BATTLE.PRG func_8008B6FC (146C.c:16061)
[[nodiscard]] std::span<std::byte> RoomAuxiliaryData(
    const RoomSections& sections);
// decomp: BATTLE.PRG func_8008C478 (146C.c:16467)
// The 8-byte record `index`; empty when it lies outside the section.
inline constexpr std::size_t kSupplementalRecordBytes = 8;
[[nodiscard]] std::span<std::byte> RoomSupplementalRecord(
    const RoomSections& sections, int index);
// decomp: BATTLE.PRG func_8008E470 (146C.c:17321)
[[nodiscard]] bool IsRoomAudioSequenceAbsent(const RoomSections& sections);

// The room environment section's fields (Field_MpdEnvironment).
inline constexpr std::size_t kRoomEnvironmentBytes = 0x58;

struct RoomEnvironmentProperties {
  std::uint8_t workshop = 0;
  std::uint8_t flags = 0;
  std::uint8_t trap_level = 0;
  std::int16_t scene_id = 0;
  std::int16_t fall_zone = 0;
  std::int16_t fall_room = 0;
  std::int16_t parameter = 0;

  friend bool operator==(const RoomEnvironmentProperties&,
                         const RoomEnvironmentProperties&) = default;
};

// Nothing when the section is shorter than the environment.
[[nodiscard]] std::optional<RoomEnvironmentProperties>
DecodeRoomEnvironmentProperties(std::span<const std::byte> section);

// decomp: BATTLE.PRG vs_battle_inWorkshop (146C.c:16798)
// The workshop the room is (0 for none).
[[nodiscard]] int IsCurrentRoomWorkshop(
    const RoomEnvironmentProperties& environment);
// decomp: BATTLE.PRG func_8008D3E8 (146C.c:16800)
[[nodiscard]] int RoomEnvironmentFlags(
    const RoomEnvironmentProperties& environment);
// decomp: BATTLE.PRG vs_battle_getCurrentSceneId (146C.c:16802)
[[nodiscard]] int CurrentSceneId(const RoomEnvironmentProperties& environment);
// decomp: BATTLE.PRG func_8008D418 (146C.c:16804)
// The zone in the high half and the room, sign-extended, ORed in.
[[nodiscard]] int RoomFallDestination(
    const RoomEnvironmentProperties& environment);
// decomp: BATTLE.PRG func_8008E4AC (146C.c:17329)
[[nodiscard]] int RoomEnvironmentParameter(
    const RoomEnvironmentProperties& environment);
// decomp: BATTLE.PRG _getRoomTrapLevel (146C.c:17331)
[[nodiscard]] int RoomTrapLevel(const RoomEnvironmentProperties& environment);

// --- Loading and unloading the room

struct RoomFileLayout {
  // From the section's offset to the file's end.
  ByteRange room_sections{};
  ByteRange cleared{};
  ByteRange script{};
  ByteRange doors{};
  ByteRange enemies{};
  ByteRange loot{};

  friend bool operator==(const RoomFileLayout&,
                         const RoomFileLayout&) = default;
};

// Nothing when the header or a section lies outside the file.
[[nodiscard]] std::optional<RoomFileLayout> DecodeRoomFile(
    std::span<const std::byte> file);

// The zone's tile extent from the collision map's packed dimensions
// (146C.c:15318-15334).
struct ZoneTileExtent {
  std::int16_t columns = 0;
  std::int16_t rows = 0;

  friend bool operator==(const ZoneTileExtent&,
                         const ZoneTileExtent&) = default;
};
[[nodiscard]] ZoneTileExtent ZoneTileExtentFromDimensions(int dimensions);

// The camera's bounds (Field_ZoneBounds).
struct ZoneBounds {
  std::int16_t min_x = 0;
  std::int16_t min_z = 0;
  std::int16_t max_x = 0;
  std::int16_t max_z = 0;

  friend bool operator==(const ZoneBounds&, const ZoneBounds&) = default;
};

// The sections the room's events load from (Event_LoadRoomEventSections).
struct RoomEventSections {
  std::span<const std::byte> cleared{};
  std::span<const std::byte> script{};
  std::span<const std::byte> doors{};
  bool first_entry = false;
};

// The orbit camera a room starts with (Field_sphericalCamera).
struct RoomEntryCamera {
  int pitch = 0;
  int mode = 0;
};

inline constexpr int kDefaultEntryCameraPitch = -384;
inline constexpr int kRaisedEntryCameraPitch = -128;
inline constexpr int kNearEntryCameraDistance = 0x600;
inline constexpr int kNearEntryCameraMode = 1;
inline constexpr int kFarEntryCameraMode = 3;
// The game entry modes FinishRoomLoad reads (Boot_gameEntryMode).
inline constexpr int kNewGameEntryMode = 1;
inline constexpr int kResumedNewGameEntryMode = 5;

// The camera a room starts with: the raised pitch in zone 27's room 1, the
// near mode at the near distance.
[[nodiscard]] RoomEntryCamera EntryCamera(const CurrentRoom& room,
                                          int initial_distance);

class RoomLoadHost : public RoomFileReadHost,
                     public RoomSectionHost,
                     public RoomActorSpawnHost,
                     public RoomEntranceHost,
                     public RoomPlacementHost {
 public:
  virtual void ResetGameplayInspection() = 0;
  [[nodiscard]] virtual Content::DiscFileExtent RoomFile(int room_id) const = 0;
  [[nodiscard]] virtual int RoomFileCount() const = 0;
  // Takes the two primitive buffers and sets up the CLUT animation.
  virtual void AllocatePrimitiveBuffers() = 0;

  // The zone context's room and scene.
  [[nodiscard]] virtual CurrentRoom PendingDestination() const = 0;
  virtual void SetCurrentRoom(const CurrentRoom& room) = 0;
  [[nodiscard]] virtual int LoadedSceneId() const = 0;
  virtual void SetLoadedSceneId(int scene_id) = 0;
  virtual void ClearRoomEnemyMemory() = 0;
  [[nodiscard]] virtual Actors::RoomEnemyMemory LoadRoomEnemyMemory() const = 0;
  virtual void StoreRoomEnemyMemory(const Actors::RoomEnemyMemory& memory) = 0;
  virtual void SetPlacementHidden(std::size_t placement_index,
                                  std::uint8_t hidden) = 0;
  virtual void MarkRoomVisitPending() = 0;
  [[nodiscard]] virtual int CollisionMapDimensions() const = 0;
  [[nodiscard]] virtual std::optional<ZoneBounds> RoomCameraBounds() const = 0;
  virtual void SetZoneExtent(const ZoneTileExtent& extent,
                             const ZoneBounds& bounds) = 0;
  // Clears the zone context's reset words (the original's D_800F18C8
  // view): the words at 0x48 and 0x50 and the low halves at 0x4C and 0x54.
  virtual void ClearRoomResetWords() = 0;
  virtual void CaptureBackgroundColors() = 0;
  virtual void LoadChestLoot(std::span<const std::byte> loot) = 0;

  // The room's events: the encounter's, or the room file's.
  [[nodiscard]] virtual bool EncounterUsesRoomEvents() const = 0;
  [[nodiscard]] virtual Event::RoomEventLoad LoadEncounterRoomEvents() = 0;
  [[nodiscard]] virtual Event::RoomEventLoad LoadRoomEvents(
      const RoomEventSections& sections) = 0;
  [[nodiscard]] virtual int GameEntryMode() const = 0;
  virtual void SetGameEntryMode(int mode) = 0;

  virtual void SnapActorsToSupport() = 0;
  // Queues the labels of Ashley's statuses in `statuses` and removes
  // `statuses`.
  virtual void ClearAshleyStatuses(int statuses) = 0;
  // Resets the room drawing, view, input, effect and actor switches the
  // original clears on entering a room.
  virtual void ResetRoomEntryState() = 0;
  [[nodiscard]] virtual int ScreenTransitionEffect() const = 0;
  // Starts the fade in at `step`, clearing the fade when `clear_alpha`.
  virtual void SetScreenTransition(int step, bool clear_alpha) = 0;
  [[nodiscard]] virtual int InitialCameraDistance() const = 0;
  virtual void ResetCamera(const RoomEntryCamera& camera) = 0;
  // Aims the camera at Ashley and sets up the field display.
  virtual void FrameCameraOnAshley() = 0;

  [[nodiscard]] virtual bool NativeResumePending() const = 0;
  [[nodiscard]] virtual bool FinishNativeResume() = 0;
  virtual void JumpToTitle() = 0;
  [[nodiscard]] virtual bool RoomEventTriggered() = 0;
  virtual void RequestEventStart() = 0;
  virtual void InitializeWeather(bool event_running) = 0;
  virtual void PrepareCutsceneActors() = 0;
  // Shows the room and its actors, starts the weather without an event,
  // restores the effect sounds and enters exploration with the HUD shown.
  virtual void EnterExploration() = 0;
  [[nodiscard]] virtual int RoomMusicSlot() const = 0;
  virtual void PlayRoomMusic(int slot) = 0;
  virtual void ShowDisplay() = 0;
  virtual void InvalidateControllerConnections() = 0;
};

// decomp: BATTLE.PRG func_80088CAC (146C.c:14766)
// Clears the room enemy memory, first resetting the scene timer when it
// resets per scene.
void BeginSceneActorTransition(RoomLoadHost& host);
// decomp: BATTLE.PRG _loadMpd (146C.c:15254)
// An id outside the zone's room file table is refused (false).
bool QueueRoomLoad(RoomLoadHost& host, int room_id);
// decomp: BATTLE.PRG func_80089DC0 (146C.c:15272)
// A room id outside the table, a failed read or a malformed file is refused
// once the primitive buffers are taken (so the unload stays balanced) and
// leaves the room unloaded. A room without an environment section is
// refused and keeps the loaded scene. A refused event load frees the room
// sections and file without advancing game-entry state or starting play.
// False tells the gameplay loop to reset rather than run an unloaded room.
bool FinishRoomLoad(RoomLoadHost& host, int room_id);

class RoomActorRemovalHost {
 public:
  RoomActorRemovalHost() = default;
  RoomActorRemovalHost(const RoomActorRemovalHost&) = delete;
  RoomActorRemovalHost& operator=(const RoomActorRemovalHost&) = delete;
  RoomActorRemovalHost(RoomActorRemovalHost&&) = delete;
  RoomActorRemovalHost& operator=(RoomActorRemovalHost&&) = delete;
  virtual ~RoomActorRemovalHost() = default;

  virtual void RemoveActor(int slot) = 0;
};

// decomp: BATTLE.PRG func_8008A364 (146C.c:15480)
// Removes the actors in slots 2-15.
void RemoveRoomActors(RoomActorRemovalHost& host);

// The frames the unload draws while the screen stays covered.
inline constexpr int kUnloadTransitionFrames = 2;

class RoomUnloadHost : public RoomSectionHost, public RoomActorRemovalHost {
 public:
  virtual void ResetGameplayInspection() = 0;
  virtual void DrawTransitionFrame() = 0;
  virtual void UpdateWhileLoading() = 0;
  virtual void FreeRoomEventSections() = 0;
  virtual void MarkCurrentRoomVisited() = 0;
  // Frees the room's placements and clears them (the original freed
  // D_800F18E4, the placement view, and left it set).
  virtual void FreePlacements() = 0;
  // Frees the scripted actor's (slot 1) AI state (the original's
  // D_800F192C view of the actor table).
  virtual void FreeScriptedActorAi() = 0;
  // Frees the room AI, clears Ashley's marker, stops every actor's movement
  // and clears the room's loot.
  virtual void ResetRoomActorState() = 0;
  virtual void FreePrimitiveBuffers() = 0;
};

// decomp: BATTLE.PRG func_8008A3A0 (146C.c:15489)
void UnloadCurrentRoom(RoomUnloadHost& host);
}  // namespace VS::Field
