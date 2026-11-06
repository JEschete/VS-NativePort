// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomLoading.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include "Field/RoomEffectsCore.hpp"
#include "Render/TimImage.hpp"

namespace VS::Field {
namespace {
using room_effects::Fits;
using room_effects::Load;
using room_effects::Store;

constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kHalfwordBytes = 2;

// The zone file header (Field_ZndHeader).
constexpr std::size_t kZoneRoomFilesOffsetAt = 0x00;
constexpr std::size_t kZoneRoomFilesSizeAt = 0x04;
constexpr std::size_t kZoneEnemiesOffsetAt = 0x08;
constexpr std::size_t kZoneTexturesOffsetAt = 0x10;
constexpr std::size_t kZoneMusicIdAt = 0x18;

// The enemy section: the count, then the ZUD files.
constexpr std::size_t kZoneEnemyFilesAt = 4;
// A disc file entry: its sector, then its byte size.
constexpr std::size_t kFileEntrySizeAt = 4;

// The texture section: four table sizes, then the tables.
constexpr std::size_t kTextureTableCount = 4;
constexpr std::size_t kTextureTablesAt = kTextureTableCount * kWordBytes;

// The room file header (Field_MpdHeader): word offsets, then halfword sizes
// for the first four sections and word sizes for the last two.
constexpr std::size_t kRoomSectionsOffsetAt = 0x00;
constexpr std::size_t kClearedOffsetAt = 0x08;
constexpr std::size_t kClearedSizeAt = 0x0C;
constexpr std::size_t kScriptOffsetAt = 0x10;
constexpr std::size_t kScriptSizeAt = 0x14;
constexpr std::size_t kDoorOffsetAt = 0x18;
constexpr std::size_t kDoorSizeAt = 0x1C;
constexpr std::size_t kEnemyOffsetAt = 0x20;
constexpr std::size_t kEnemySizeAt = 0x24;
constexpr std::size_t kLootOffsetAt = 0x28;
constexpr std::size_t kLootSizeAt = 0x2C;

// A placement record (Field_MpdEnemySpawn and Field_MpdObjectSpawn).
constexpr std::size_t kPlacementKindAt = 0x00;
constexpr std::size_t kPlacementHiddenAt = 0x02;
constexpr std::size_t kEnemyIdAt = 0x04;
constexpr std::size_t kEnemySlotAt = 0x05;
constexpr std::size_t kEnemyFlagTestAt = 0x06;
constexpr std::size_t kEnemyStatTestAt = 0x07;
constexpr std::size_t kEnemyFlagValueAt = 0x08;
constexpr std::size_t kEnemyStatValueAt = 0x0A;
constexpr std::size_t kEnemyTileXAt = 0x0C;
constexpr std::size_t kEnemyDirectionAt = 0x0D;
constexpr std::size_t kEnemyTileZAt = 0x0E;
constexpr std::size_t kEnemyHeightAt = 0x0F;
constexpr std::size_t kEnemyFacingAt = 0x23;
constexpr std::size_t kEnemyEngagedAt = 0x24;
constexpr std::size_t kEnemySceneTestAt = 0x27;
constexpr std::size_t kObjectFlagAt = 0x04;
constexpr std::size_t kObjectFlagMinAt = 0x06;
constexpr std::size_t kObjectFlagMaxAt = 0x07;
constexpr std::size_t kObjectSlotAt = 0x08;
constexpr std::size_t kObjectTileXAt = 0x0A;
constexpr std::size_t kObjectTileZAt = 0x0B;
constexpr std::size_t kObjectHeightAt = 0x0C;
constexpr std::size_t kObjectDirectionAt = 0x0D;
constexpr std::size_t kObjectModelAt = 0x0E;

// The placement tests (146C.c:14936-15016): the scene test reads script
// flags 1 and 2, the flag test flag 0x82, and the stat test Ashley's hit
// points or the flag in the stat value's low 10 bits against its top 6.
constexpr int kSceneFirstFlag = 1;
constexpr int kSceneSecondFlag = 2;
constexpr int kPlacementFlag = 0x82;
constexpr std::uint16_t kStatValueMask = 0x3FF;
constexpr int kStatThresholdShift = 10;
constexpr std::uint16_t kObjectFlagMask = 0x3FF;
// The test codes; 0 and any other code pass.
constexpr std::uint8_t kSceneFirstFlagClear = 1;
constexpr std::uint8_t kSceneFirstFlagSet = 2;
constexpr std::uint8_t kSceneSecondFlagSet = 3;
constexpr std::uint8_t kFlagEqual = 1;
constexpr std::uint8_t kFlagAtMost = 2;
constexpr std::uint8_t kFlagAtLeast = 3;
constexpr std::uint8_t kStatHpAtMost = 1;
constexpr std::uint8_t kStatHpAtLeast = 2;
constexpr std::uint8_t kStatFlagEqual = 3;
constexpr std::uint8_t kStatFlagAtMost = 4;
constexpr std::uint8_t kStatFlagAtLeast = 5;

// The enemy spawn flags (146C.c:15020-15036): facing codes 1 and 2 pick
// their own, every other code the default.
constexpr std::uint8_t kFacingCodeOne = 1;
constexpr std::uint8_t kFacingCodeTwo = 2;
constexpr int kDefaultFacingSpawnFlags = 6;
constexpr int kFacingOneSpawnFlags = 0;
constexpr int kFacingTwoSpawnFlags = 1;
constexpr int kEngagedSpawnFlag = 0x80;

// The object model word: the model in bits 12-15 and 8-11, the material in
// bits 5-7 (as 2-4).
constexpr int kObjectModelHighShift = 12;
constexpr int kObjectModelLowMask = 0xF00;
constexpr int kObjectMaterialShift = 3;
constexpr int kObjectMaterialMask = 0x1C;

// The collision map header (Field_MpdCollisionHeader).
constexpr std::size_t kCollisionWidthAt = 0;
constexpr std::size_t kCollisionHeightAt = 2;
constexpr std::size_t kCollisionCellCountAt = 6;
// Section E (Field_MpdSectionE).
constexpr std::size_t kSecondArrayOffsetAt = 4;
constexpr std::size_t kHasSecondArrayAt = 8;
constexpr std::size_t kFirstArrayAt = 0x0C;
constexpr std::size_t kSecondArrayAt = 0x10;
// The environment section (Field_MpdEnvironment).
constexpr std::size_t kWorkshopAt = 0x30;
constexpr std::size_t kEnvironmentFlagsAt = 0x31;
constexpr std::size_t kTrapLevelAt = 0x32;
constexpr std::size_t kSceneIdAt = 0x50;
constexpr std::size_t kFallZoneAt = 0x52;
constexpr std::size_t kFallRoomAt = 0x54;
constexpr std::size_t kEnvironmentParameterAt = 0x56;
constexpr int kFallZoneShift = 16;

// The collision map's packed dimensions: the width in tile units (16 per
// tile) in the low half, the rows in the top 12 bits. The original's
// rounded shift divides toward zero.
constexpr std::uint32_t kDimensionWidthMask = 0xFFFF;
constexpr std::uint32_t kTileUnits = 16;
constexpr int kDimensionRowsUnit = 0x100000;
// Section E's second array offset, rounded down to a word.
constexpr std::uint32_t kWordAlignMask = ~std::uint32_t{kWordBytes - 1};
// The room record the field loads its room into.
constexpr int kCurrentRoomOwner = 0;
// The entrance elevations that take a step (0-3): even ones step along x,
// odd ones along z.
constexpr std::uint8_t kSteppingElevations = 4;

// The room enemy memory's most recent room.
constexpr std::uint8_t kMostRecentRoom = 4;
// The raised camera's room (zone 27, room 1).
constexpr int kRaisedCameraZone = 27;
constexpr int kRaisedCameraRoom = 1;
// The screen transition steps a room starts with (Field_screenTransitionStep).
constexpr int kFadeInTransitionEffect = 3;
constexpr int kFadeInStartStep = 0;
constexpr int kCoverStep = 3;
constexpr int kExploringStep = 4;

[[nodiscard]] std::int32_t LoadWord(std::span<const std::byte> bytes,
                                    std::size_t offset) {
  return Load<std::int32_t>(bytes, offset);
}

[[nodiscard]] std::span<const std::byte> Section(
    std::span<const std::byte> file, const ByteRange& range) {
  return file.subspan(range.offset, range.bytes);
}

// A section from `offset` to the file's end; nothing when the offset is
// negative or past the end.
[[nodiscard]] std::optional<ByteRange> RangeToEnd(std::size_t file_bytes,
                                                  std::int32_t offset) {
  if (offset < 0 || static_cast<std::size_t>(offset) > file_bytes) {
    return std::nullopt;
  }
  const auto start = static_cast<std::size_t>(offset);
  return ByteRange{.offset = start, .bytes = file_bytes - start};
}

// A sized section; an empty one is empty wherever its offset points, since
// no reader follows it.
[[nodiscard]] std::optional<ByteRange> SizedRange(std::size_t file_bytes,
                                                  std::int32_t offset,
                                                  std::int64_t bytes) {
  if (bytes == 0) {
    return ByteRange{};
  }
  if (offset < 0 || bytes < 0 ||
      !Fits(file_bytes, static_cast<std::size_t>(offset),
            static_cast<std::size_t>(bytes))) {
    return std::nullopt;
  }
  return ByteRange{.offset = static_cast<std::size_t>(offset),
                   .bytes = static_cast<std::size_t>(bytes)};
}

[[nodiscard]] std::vector<Content::DiscFileExtent> DecodeFileEntries(
    std::span<const std::byte> entries, std::size_t count) {
  std::vector<Content::DiscFileExtent> files;
  files.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t at = index * kRoomFileEntryBytes;
    files.push_back({.sector = LoadWord(entries, at),
                     .bytes = LoadWord(entries, at + kFileEntrySizeAt)});
  }
  return files;
}
}  // namespace

// --- Reading the zone and room files

void QueueFileRead(RoomFileReadHost& host, LoadedFile file,
                   Content::DiscFileExtent extent, RoomLoadDiagnostic pending,
                   RoomLoadDiagnostic held) {
  if (host.ReadPending(file)) {
    host.Diagnostic(pending);
  }
  if (host.DataHeld(file)) {
    host.Diagnostic(held);
  }
  host.BeginRead(file, extent);
}

bool AwaitFileRead(RoomFileReadHost& host, LoadedFile file) {
  for (;;) {
    const DiscReadState state = host.PollRead(file);
    if (state == DiscReadState::kLoaded) {
      break;
    }
    if (state == DiscReadState::kFailed) {
      host.ReleaseRead(file);
      host.FreeLoadedData(file);
      host.Refuse(file == LoadedFile::kZone ? RoomLoadRefusal::kZoneReadFailed
                                            : RoomLoadRefusal::kRoomReadFailed,
                  0);
      return false;
    }
    host.UpdateWhileLoading();
  }
  host.ReleaseRead(file);
  return true;
}

// --- The zone file

std::optional<ZoneFileLayout> DecodeZoneFile(std::span<const std::byte> file) {
  if (file.size() < kZoneHeaderBytes) {
    return std::nullopt;
  }
  const auto room_files =
      SizedRange(file.size(), LoadWord(file, kZoneRoomFilesOffsetAt),
                 LoadWord(file, kZoneRoomFilesSizeAt));
  const auto enemies =
      RangeToEnd(file.size(), LoadWord(file, kZoneEnemiesOffsetAt));
  const auto textures =
      RangeToEnd(file.size(), LoadWord(file, kZoneTexturesOffsetAt));
  if (!room_files || !enemies || !textures) {
    return std::nullopt;
  }
  return ZoneFileLayout{.room_files = *room_files,
                        .enemies = *enemies,
                        .textures = *textures,
                        .music_id = LoadWord(file, kZoneMusicIdAt)};
}

std::vector<Content::DiscFileExtent> DecodeRoomFileTable(
    std::span<const std::byte> table) {
  return DecodeFileEntries(table, table.size() / kRoomFileEntryBytes);
}

std::optional<ZoneEnemyTables> DecodeZoneEnemyTables(
    std::span<const std::byte> section) {
  if (section.size() < kWordBytes) {
    return std::nullopt;
  }
  const std::int32_t count = LoadWord(section, 0);
  if (count < 0) {
    return std::nullopt;
  }
  const auto templates_at =
      kZoneEnemyFilesAt +
      (static_cast<std::size_t>(count) * kRoomFileEntryBytes);
  const auto template_bytes =
      static_cast<std::size_t>(count) * kZoneEnemyTemplateBytes;
  if (!Fits(section.size(), templates_at, template_bytes)) {
    return std::nullopt;
  }
  return ZoneEnemyTables{
      .zud_files = DecodeFileEntries(section.subspan(kZoneEnemyFilesAt),
                                     static_cast<std::size_t>(count)),
      .templates = section.subspan(templates_at, template_bytes)};
}

void LoadZoneRoomFileTable(ZoneTableHost& host,
                           std::span<const std::byte> table) {
  if (table.empty()) {
    host.Diagnostic(RoomLoadDiagnostic::kEmptyRoomFileTable);
  }
  host.StoreRoomFiles(DecodeRoomFileTable(table));
}

void LoadZoneEnemyTemplates(ZoneTableHost& host,
                            std::span<const std::byte> section) {
  const auto tables = DecodeZoneEnemyTables(section);
  if (!tables) {
    host.Refuse(RoomLoadRefusal::kMalformedZoneEnemies, 0);
    host.StoreZoneEnemies({}, {});
    return;
  }
  host.StoreZoneEnemies(tables->zud_files, tables->templates);
}
namespace {
// A palette TIM's lines in the room palette rows; lines outside the rows
// are passed over.
void CopyPaletteRows(ZoneTextureHost& host, std::span<const std::byte> pixels,
                     const Render::ScreenRect& area) {
  const auto line_bytes = static_cast<std::size_t>(area.width) * kHalfwordBytes;
  std::size_t at = 0;
  for (int line = area.y; line < area.y + area.height; ++line) {
    const int row = line - kZonePaletteLine;
    if (row >= 0 && row < kZonePaletteRows) {
      host.CopyPaletteRow(pixels.subspan(at, line_bytes),
                          {.row = row,
                           .first_color = area.x - kZonePaletteColumn,
                           .colors = area.width});
    }
    at += line_bytes;
  }
}

void UploadZoneTexture(ZoneTextureHost& host,
                       std::span<const std::byte> tim_bytes) {
  const auto tim = Render::ParseTim(tim_bytes);
  if (!tim) {
    return;
  }
  const Render::TimBlock& image = tim->image;
  const auto pixels = tim_bytes.subspan(image.pixel_offset, image.pixel_bytes);
  if (image.area.x >= kZonePaletteColumn) {
    CopyPaletteRows(host, pixels, image.area);
    return;
  }
  Render::ScreenRect area = image.area;
  area.x = static_cast<std::int16_t>(area.x - kZoneTextureColumnShift);
  area.y = static_cast<std::int16_t>(area.y + kZoneTextureLineShift);
  host.UploadImage(area, pixels);
}

// decomp: BATTLE.PRG _finishLoadZnd (146C.c:14862-14873)
void SwitchZoneMusic(ZoneLoadHost& host, int music_id) {
  host.StopAndFreeCurrentMusic();
  for (int slot = 1; slot <= kZoneMusicSlots; ++slot) {
    host.FreeMusicSlot(slot);
  }
  host.SetMusicSlot(0);
  if (music_id != 0) {
    host.LoadWaveBank(music_id);
    host.SetMusicSlot(host.LoadMusicSlot(music_id, kZoneMusicTargetSlot));
  }
}
}  // namespace

void UploadZoneTextures(ZoneTextureHost& host,
                        std::span<const std::byte> section) {
  if (section.size() < kTextureTablesAt) {
    host.Refuse(RoomLoadRefusal::kMalformedZoneTextures, 0);
    return;
  }
  const std::int32_t first_table_bytes = LoadWord(section, 0);
  if (first_table_bytes <= 0) {
    return;
  }
  const auto table_bytes = static_cast<std::size_t>(first_table_bytes);
  if (table_bytes < kWordBytes ||
      !Fits(section.size(), kTextureTablesAt, table_bytes)) {
    host.Refuse(RoomLoadRefusal::kMalformedZoneTextures, 0);
    return;
  }
  const auto table = section.subspan(kTextureTablesAt, table_bytes);
  const std::int32_t count = LoadWord(table, 0);
  std::size_t at = kWordBytes;
  for (std::int32_t index = 0; index < count; ++index) {
    if (!Fits(table.size(), at, kWordBytes)) {
      host.Refuse(RoomLoadRefusal::kMalformedZoneTextures, index);
      return;
    }
    const std::int32_t tim_size = LoadWord(table, at);
    at += kWordBytes;
    const std::size_t advance =
        tim_size < 0
            ? 0
            : (static_cast<std::size_t>(tim_size) / kWordBytes) * kWordBytes;
    if (tim_size < 0 || !Fits(table.size(), at, advance)) {
      host.Refuse(RoomLoadRefusal::kMalformedZoneTextures, index);
      return;
    }
    UploadZoneTexture(host, table.subspan(at));
    at += advance;
  }
}

bool QueueZoneLoad(ZoneLoadHost& host, int zone_id) {
  if (zone_id < 0 || zone_id >= kZoneCount) {
    host.Refuse(RoomLoadRefusal::kZoneId, zone_id);
    return false;
  }
  QueueFileRead(host, LoadedFile::kZone, host.ZoneFile(zone_id),
                RoomLoadDiagnostic::kZoneReadPending,
                RoomLoadDiagnostic::kZoneDataHeld);
  if (host.MusicFadeEnabled() &&
      host.CurrentMusicId() != host.ZoneMusicId(zone_id)) {
    host.FadeOutCurrentMusic();
  }
  return true;
}

void FinishZoneLoad(ZoneLoadHost& host, int zone_id) {
  if (!host.ReadPending(LoadedFile::kZone) && !QueueZoneLoad(host, zone_id)) {
    return;
  }
  if (!AwaitFileRead(host, LoadedFile::kZone)) {
    return;
  }
  if (host.SceneTimerResetScope() == SceneTimerScope::kZone) {
    host.ResetSceneTimer();
  }
  const auto file = host.LoadedData(LoadedFile::kZone);
  const auto layout = DecodeZoneFile(file);
  if (!layout) {
    host.Refuse(RoomLoadRefusal::kMalformedZoneFile, zone_id);
    host.FreeLoadedData(LoadedFile::kZone);
    return;
  }
  LoadZoneEnemyTemplates(host, Section(file, layout->enemies));
  UploadZoneTextures(host, Section(file, layout->textures));
  LoadZoneRoomFileTable(host, Section(file, layout->room_files));
  const int music_id = layout->music_id;
  host.FreeLoadedData(LoadedFile::kZone);

  if (!host.NativeResumePending()) {
    host.ResetZoneScriptFlags(host.GameEntryMode() == kNewGameEntryMode);
  }
  if (host.CurrentMusicId() != music_id && host.MusicFadeEnabled()) {
    SwitchZoneMusic(host, music_id);
  }
  host.FinishZoneMusic(music_id);
}

void ReleaseZoneData(ZoneReleaseHost& host) {
  host.ReleaseZoneEventResources();
  host.FreeZoneTables();
}

int ZoneSoundFileId(int zone_music_id) { return zone_music_id; }

// --- Room placements

namespace {
[[nodiscard]] std::uint8_t LoadByte(std::span<const std::byte> bytes,
                                    std::size_t offset) {
  return Load<std::uint8_t>(bytes, offset);
}

[[nodiscard]] std::uint16_t LoadHalfword(std::span<const std::byte> bytes,
                                         std::size_t offset) {
  return Load<std::uint16_t>(bytes, offset);
}

[[nodiscard]] EnemyPlacement DecodeEnemyPlacement(
    std::span<const std::byte> record) {
  return {.enemy_id = LoadByte(record, kEnemyIdAt),
          .slot = LoadByte(record, kEnemySlotAt),
          .flag_test = LoadByte(record, kEnemyFlagTestAt),
          .stat_test = LoadByte(record, kEnemyStatTestAt),
          .flag_value = LoadHalfword(record, kEnemyFlagValueAt),
          .stat_value = LoadHalfword(record, kEnemyStatValueAt),
          .placement = {.tile_x = LoadByte(record, kEnemyTileXAt),
                        .direction = LoadByte(record, kEnemyDirectionAt),
                        .tile_z = LoadByte(record, kEnemyTileZAt),
                        .height = LoadByte(record, kEnemyHeightAt)},
          .facing = LoadByte(record, kEnemyFacingAt),
          .starts_engaged = LoadByte(record, kEnemyEngagedAt) != 0,
          .scene_test = LoadByte(record, kEnemySceneTestAt)};
}

[[nodiscard]] ObjectPlacement DecodeObjectPlacement(
    std::span<const std::byte> record) {
  return {.flag = LoadHalfword(record, kObjectFlagAt),
          .flag_min = LoadByte(record, kObjectFlagMinAt),
          .flag_max = LoadByte(record, kObjectFlagMaxAt),
          .slot = LoadByte(record, kObjectSlotAt),
          .tile_x = LoadByte(record, kObjectTileXAt),
          .tile_z = LoadByte(record, kObjectTileZAt),
          .height = LoadByte(record, kObjectHeightAt),
          .direction = LoadByte(record, kObjectDirectionAt),
          .model = LoadHalfword(record, kObjectModelAt)};
}

[[nodiscard]] bool SceneTestPasses(std::uint8_t test,
                                   const PlacementTestHost& host) {
  switch (test) {
    case kSceneFirstFlagClear:
      return host.ScriptFlag(kSceneFirstFlag) == 0;
    case kSceneFirstFlagSet:
      return host.ScriptFlag(kSceneFirstFlag) == 1;
    case kSceneSecondFlagSet:
      return host.ScriptFlag(kSceneSecondFlag) == 1;
    default:
      return true;
  }
}

[[nodiscard]] bool FlagTestPasses(const EnemyPlacement& enemy,
                                  const PlacementTestHost& host) {
  switch (enemy.flag_test) {
    case kFlagEqual:
      return host.ScriptFlag(kPlacementFlag) == enemy.flag_value;
    case kFlagAtMost:
      return host.ScriptFlag(kPlacementFlag) <= enemy.flag_value;
    case kFlagAtLeast:
      return host.ScriptFlag(kPlacementFlag) >= enemy.flag_value;
    default:
      return true;
  }
}

[[nodiscard]] bool StatTestPasses(const EnemyPlacement& enemy,
                                  const PlacementTestHost& host) {
  const int value = enemy.stat_value & kStatValueMask;
  const int threshold = enemy.stat_value >> kStatThresholdShift;
  switch (enemy.stat_test) {
    case kStatHpAtMost:
      return host.AshleyCurrentHp() <= value;
    case kStatHpAtLeast:
      return host.AshleyCurrentHp() >= value;
    case kStatFlagEqual:
      return host.ScriptFlag(value) == threshold;
    case kStatFlagAtMost:
      return host.ScriptFlag(value) <= threshold;
    case kStatFlagAtLeast:
      return host.ScriptFlag(value) >= threshold;
    default:
      return true;
  }
}

// The actor slots the room's placements name, which the free slot searches
// pass over, plus the second bodies already given.
using ReservedSlots = std::array<bool, kRoomActorSlots>;

enum class SlotChoice : std::uint8_t { kChosen, kNoFreeSlot, kRefused };

struct EnemySlot {
  SlotChoice choice = SlotChoice::kChosen;
  int slot = 0;
};

[[nodiscard]] bool SlotFree(const RoomActorSpawnHost& host,
                            const ReservedSlots& reserved, int slot) {
  return !host.ActorSlotOccupied(slot) &&
         !reserved.at(static_cast<std::size_t>(slot));
}

// The first free enemy slot (2-9) whose second body's slot, when it has
// one, is free too; that slot is reserved.
[[nodiscard]] EnemySlot FirstFreeEnemySlot(const RoomActorSpawnHost& host,
                                           ReservedSlots& reserved,
                                           bool secondary) {
  for (int slot = kFirstRoomActorSlot; slot < kRoomEnemySlotEnd; ++slot) {
    if (!SlotFree(host, reserved, slot)) {
      continue;
    }
    if (secondary) {
      const int body = slot + kSecondaryBodySlotOffset;
      if (!SlotFree(host, reserved, body)) {
        continue;
      }
      reserved.at(static_cast<std::size_t>(body)) = true;
    }
    return {.choice = SlotChoice::kChosen, .slot = slot};
  }
  return {.choice = SlotChoice::kNoFreeSlot};
}

// decomp: BATTLE.PRG func_80089114 (146C.c:15041-15100)
[[nodiscard]] EnemySlot ChooseEnemySlot(RoomActorSpawnHost& host,
                                        ReservedSlots& reserved, int named,
                                        bool secondary) {
  if (named == 0) {
    return FirstFreeEnemySlot(host, reserved, secondary);
  }
  const int body = named + kSecondaryBodySlotOffset;
  if (named >= kRoomActorSlots || (secondary && body >= kRoomActorSlots)) {
    host.Refuse(RoomLoadRefusal::kActorSlot, named);
    return {.choice = SlotChoice::kRefused, .slot = named};
  }
  if (secondary) {
    if (!SlotFree(host, reserved, body)) {
      return {.choice = SlotChoice::kNoFreeSlot, .slot = named};
    }
    reserved.at(static_cast<std::size_t>(body)) = true;
  }
  return {.choice = SlotChoice::kChosen, .slot = named};
}

// False when no slot is free, which ends the spawning.
bool SpawnEnemyPlacement(RoomActorSpawnHost& host, ReservedSlots& reserved,
                         const RoomPlacement& placement, std::size_t index) {
  const EnemyPlacement& enemy = placement.enemy;
  const int enemy_id = enemy.enemy_id;
  const bool templated = enemy_id != kUntemplatedEnemyId;
  const int templates = host.ZoneEnemyCount();
  if (templated && enemy_id >= templates) {
    host.Refuse(RoomLoadRefusal::kEnemyTemplate, static_cast<int>(index));
    return true;
  }
  const bool secondary =
      enemy_id < templates && host.TemplateUsesSecondaryBody(enemy_id);
  const EnemySlot slot = ChooseEnemySlot(host, reserved, enemy.slot, secondary);
  if (slot.choice == SlotChoice::kNoFreeSlot) {
    host.Diagnostic(RoomLoadDiagnostic::kNoFreeActorSlot);
    host.FreeCachedZud();
    return false;
  }
  if (slot.choice == SlotChoice::kRefused ||
      !EnemyPlacementVisible(placement, host)) {
    return true;
  }
  const int flags = EnemySpawnFlags(enemy);
  if (templated) {
    host.SpawnEnemy(slot.slot, index, enemy_id, flags);
    return true;
  }
  const Actors::SpawnPlacement& at = enemy.placement;
  host.SpawnUntemplatedEnemy(
      slot.slot,
      {.tile_x = at.tile_x,
       .direction = at.direction,
       .tile_z = at.tile_z,
       .height = static_cast<std::uint8_t>(at.height * kElevationHeightUnits)},
      flags);
  return true;
}

// False when no slot is free, which ends the spawning.
bool SpawnObjectPlacement(RoomActorSpawnHost& host,
                          const ReservedSlots& reserved,
                          const ObjectPlacement& object) {
  if (!ObjectPlacementVisible(object, host)) {
    return true;
  }
  int slot = object.slot;
  if (object.slot == kAnyObjectSlot) {
    slot = kRoomActorSlots;
    for (int free = kFirstRoomActorSlot; free < kRoomActorSlots; ++free) {
      if (SlotFree(host, reserved, free)) {
        slot = free;
        break;
      }
    }
    if (slot == kRoomActorSlots) {
      host.Diagnostic(RoomLoadDiagnostic::kNoFreeActorSlot);
      return false;
    }
  } else if (slot >= kRoomActorSlots) {
    host.Refuse(RoomLoadRefusal::kActorSlot, slot);
    return true;
  }
  // The object's height and direction swap places in its placement.
  const Actors::SpawnPlacement at{
      .tile_x = object.tile_x,
      .direction = object.height,
      .tile_z = object.tile_z,
      .height =
          static_cast<std::uint8_t>(object.direction * kElevationHeightUnits)};
  const int model = object.model;
  host.SpawnObject(
      slot, (model >> kObjectModelHighShift) | (model & kObjectModelLowMask),
      at,
      ((model >> kObjectMaterialShift) & kObjectMaterialMask) |
          (at.height / kElevationHeightUnits));
  return true;
}

void RememberHiddenFlags(RoomActorSpawnHost& host) {
  const auto records = host.PlacementRecords();
  const std::size_t count =
      std::min(records.size() / kRoomPlacementBytes, kRememberedHiddenFlags);
  for (std::size_t index = 0; index < count; ++index) {
    host.RememberHidden(
        static_cast<int>(index),
        LoadByte(records, (index * kRoomPlacementBytes) + kPlacementHiddenAt));
  }
}
}  // namespace

std::vector<RoomPlacement> DecodeRoomPlacements(
    std::span<const std::byte> records) {
  const std::size_t count = records.size() / kRoomPlacementBytes;
  std::vector<RoomPlacement> placements;
  placements.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const auto record =
        records.subspan(index * kRoomPlacementBytes, kRoomPlacementBytes);
    placements.push_back({.kind = LoadByte(record, kPlacementKindAt),
                          .hidden = LoadByte(record, kPlacementHiddenAt),
                          .enemy = DecodeEnemyPlacement(record),
                          .object = DecodeObjectPlacement(record)});
  }
  return placements;
}

bool EnemyPlacementVisible(const RoomPlacement& placement,
                           const PlacementTestHost& host) {
  const EnemyPlacement& enemy = placement.enemy;
  return placement.hidden == 0 && SceneTestPasses(enemy.scene_test, host) &&
         FlagTestPasses(enemy, host) && StatTestPasses(enemy, host);
}

int EnemySpawnFlags(const EnemyPlacement& enemy) {
  int flags = kDefaultFacingSpawnFlags;
  if (enemy.facing == kFacingCodeOne) {
    flags = kFacingOneSpawnFlags;
  } else if (enemy.facing == kFacingCodeTwo) {
    flags = kFacingTwoSpawnFlags;
  }
  if (enemy.starts_engaged) {
    flags |= kEngagedSpawnFlag;
  }
  return flags;
}

bool ObjectPlacementVisible(const ObjectPlacement& object,
                            const PlacementTestHost& host) {
  const int value = host.ScriptFlag(object.flag & kObjectFlagMask);
  return value >= object.flag_min && value <= object.flag_max;
}

void SpawnRoomActors(RoomActorSpawnHost& host) {
  const auto placements = DecodeRoomPlacements(host.PlacementRecords());
  if (placements.empty()) {
    return;
  }
  ReservedSlots reserved{};
  for (const RoomPlacement& placement : placements) {
    if (placement.NamedSlot() < kRoomActorSlots) {
      reserved.at(placement.NamedSlot()) = true;
    }
  }
  for (std::size_t index = 0; index < placements.size(); ++index) {
    const RoomPlacement& placement = placements[index];
    const bool spawning =
        placement.IsEnemy()
            ? SpawnEnemyPlacement(host, reserved, placement, index)
            : SpawnObjectPlacement(host, reserved, placement.object);
    if (!spawning) {
      return;
    }
  }
  RememberHiddenFlags(host);
  host.FreeCachedZud();
}

// --- Ashley's entrance

EntryStep EntranceStep(const RoomEntrance& entrance) {
  if (entrance.elevation >= kSteppingElevations) {
    return {};
  }
  if (entrance.elevation % 2 == 0) {
    return {.x = kEntryStepDistance, .z = 0};
  }
  return {.x = 0, .z = kEntryStepDistance};
}

void PlaceAshleyAtRoomEntrance(RoomEntranceHost& host) {
  const RoomEntrance entrance = host.PendingEntrance();
  host.PlaceAshley({.tile_x = entrance.tile_x,
                    .direction = entrance.direction,
                    .tile_z = entrance.tile_z,
                    .height = static_cast<std::uint8_t>(
                        entrance.elevation * kElevationHeightUnits)});
  if (entrance.has_entry_step) {
    host.StepAshley(EntranceStep(entrance));
  }
  host.SettleAshley();
}

std::vector<HiddenFlag> RestoreRoomEnemyCache(
    Actors::RoomEnemyMemory& memory, const CurrentRoom& room,
    std::span<const RoomPlacement> placements) {
  std::vector<HiddenFlag> flags;
  auto& rooms = memory.rooms;
  const auto remembered = std::find_if(
      rooms.begin(), rooms.end(), [&room](const Actors::RememberedRoom& at) {
        return at.recency != 0 && at.zone == room.zone_id &&
               at.map == room.room_id;
      });
  if (remembered != rooms.end()) {
    memory.current_room =
        static_cast<std::int32_t>(std::distance(rooms.begin(), remembered));
    flags.reserve(placements.size());
    for (std::size_t index = 0; index < placements.size(); ++index) {
      const Actors::RememberedEnemy* enemy =
          Actors::FindRememberedEnemy(memory, static_cast<int>(index));
      flags.push_back({.placement_index = index,
                       .hidden = enemy != nullptr ? enemy->hidden
                                                  : placements[index].hidden});
    }
  } else {
    const auto oldest =
        std::min_element(rooms.begin(), rooms.end(),
                         [](const Actors::RememberedRoom& left,
                            const Actors::RememberedRoom& right) {
                           return left.recency < right.recency;
                         });
    memory.current_room =
        static_cast<std::int32_t>(std::distance(rooms.begin(), oldest));
    *oldest = Actors::RememberedRoom{};
  }
  Actors::RememberedRoom& current =
      rooms.at(static_cast<std::size_t>(memory.current_room));
  for (Actors::RememberedRoom& other : rooms) {
    if (current.recency < other.recency) {
      --other.recency;
    }
  }
  current.recency = kMostRecentRoom;
  current.zone = static_cast<std::uint8_t>(room.zone_id);
  current.map = static_cast<std::uint16_t>(room.room_id);
  return flags;
}

void LoadRoomEnemyPlacements(RoomPlacementHost& host,
                             std::span<const std::byte> section) {
  host.StorePlacements(host.EncounterPlacements(section));
}

// --- The room sections

std::optional<RoomSectionLayout> DecodeRoomSectionLayout(
    std::span<const std::byte> data) {
  if (data.size() < kRoomSectionHeaderBytes) {
    return std::nullopt;
  }
  RoomSectionLayout layout;
  for (std::size_t index = 0; index < kRoomSectionCount; ++index) {
    const std::int32_t length = LoadWord(data, index * kWordBytes);
    if (length < 0) {
      return std::nullopt;
    }
    layout.lengths.at(index) = static_cast<std::uint32_t>(length);
    layout.offsets.at(index) = layout.total_bytes;
    layout.total_bytes += static_cast<std::size_t>(length);
  }
  if (!Fits(data.size(), kRoomSectionHeaderBytes, layout.total_bytes)) {
    return std::nullopt;
  }
  return layout;
}

bool ResolveSectionEArrays(std::span<std::byte> section_e) {
  if (section_e.size() < kSectionEHeaderBytes) {
    return false;
  }
  const std::span<const std::byte> header = section_e;
  constexpr auto kHeaderBytes =
      static_cast<std::uint32_t>(kSectionEHeaderBytes);
  std::uint32_t second_array = 0;
  if (LoadWord(header, kHasSecondArrayAt) > 0) {
    second_array =
        kHeaderBytes +
        (static_cast<std::uint32_t>(LoadWord(header, kSecondArrayOffsetAt)) &
         kWordAlignMask);
  }
  Store<std::uint32_t>(section_e, kFirstArrayAt, kHeaderBytes);
  Store<std::uint32_t>(section_e, kSecondArrayAt, second_array);
  return true;
}

bool ResolveCollisionCellCount(std::span<std::byte> collision_map) {
  if (collision_map.size() < kCollisionCellCountAt + kHalfwordBytes) {
    return false;
  }
  const std::span<const std::byte> header = collision_map;
  const int cells = Load<std::int16_t>(header, kCollisionWidthAt) *
                    Load<std::int16_t>(header, kCollisionHeightAt);
  Store<std::int16_t>(collision_map, kCollisionCellCountAt,
                      static_cast<std::int16_t>(cells));
  return true;
}

namespace {
void ResolveRoomSectionHeaders(RoomSectionHost& host) {
  const auto section_e = host.RoomSection(RoomSectionId::kSectionE);
  if (!section_e.empty() && !ResolveSectionEArrays(section_e)) {
    host.Refuse(RoomLoadRefusal::kMalformedRoomSections,
                static_cast<int>(RoomSectionId::kSectionE));
  }
  const auto collision_map = host.RoomSection(RoomSectionId::kCollisionMap);
  if (!collision_map.empty() && !ResolveCollisionCellCount(collision_map)) {
    host.Refuse(RoomLoadRefusal::kMalformedRoomSections,
                static_cast<int>(RoomSectionId::kCollisionMap));
  }
}

void MarkMinimapEntrance(RoomSectionHost& host) {
  host.PrepareMinimapScene();
  const int door = host.PendingEnteredDoor();
  if (door == 0) {
    host.MarkEntranceDoor(host.EnteredDoorId());
    return;
  }
  host.MarkEntranceDoor(door - 1);
  host.ClearPendingEnteredDoor();
}

[[nodiscard]] std::span<std::byte> SectionOf(const RoomSections& sections,
                                             RoomSectionId section) {
  return sections.at(static_cast<std::size_t>(section));
}
}  // namespace

bool LoadRoomSections(RoomSectionHost& host, int owner_room_index,
                      std::span<const std::byte> data) {
  host.ClearRoomTint();
  const auto layout = DecodeRoomSectionLayout(data);
  if (!layout) {
    host.Refuse(RoomLoadRefusal::kMalformedRoomSections, owner_room_index);
    return false;
  }
  if (layout->total_bytes == 0) {
    return false;
  }
  if (!host.StoreRoomSections(
          owner_room_index, *layout,
          data.subspan(kRoomSectionHeaderBytes, layout->total_bytes))) {
    return false;
  }
  if (!host.RoomSection(RoomSectionId::kGeometry).empty()) {
    host.OffsetRoomTextureIds();
  }
  ResolveRoomSectionHeaders(host);
  if (!host.RoomSection(RoomSectionId::kMinimapScene).empty()) {
    MarkMinimapEntrance(host);
  }
  host.InitializeRoomSystems();
  if (!host.RoomSection(RoomSectionId::kSectionE).empty()) {
    host.WaitForOverlayReady();
  }
  return true;
}

bool ReleaseOwnedRoomSections(RoomSectionHost& host, int owner_room_index) {
  const RoomDisplay room = host.LoadedRoom();
  if (!room.loaded || room.owner_room_index != owner_room_index) {
    return false;
  }
  host.ReleaseRoomResources();
  host.FreeRoomSections();
  return true;
}

std::span<std::byte> RoomAuxiliaryData(const RoomSections& sections) {
  return SectionOf(sections, RoomSectionId::kAuxiliary);
}

std::span<std::byte> RoomSupplementalRecord(const RoomSections& sections,
                                            int index) {
  const auto section = SectionOf(sections, RoomSectionId::kSupplemental);
  if (index < 0) {
    return {};
  }
  const std::size_t at =
      static_cast<std::size_t>(index) * kSupplementalRecordBytes;
  if (!Fits(section.size(), at, kSupplementalRecordBytes)) {
    return {};
  }
  return section.subspan(at, kSupplementalRecordBytes);
}

bool IsRoomAudioSequenceAbsent(const RoomSections& sections) {
  return SectionOf(sections, RoomSectionId::kSoundData).empty();
}

std::optional<RoomEnvironmentProperties> DecodeRoomEnvironmentProperties(
    std::span<const std::byte> section) {
  if (section.size() < kRoomEnvironmentBytes) {
    return std::nullopt;
  }
  return RoomEnvironmentProperties{
      .workshop = LoadByte(section, kWorkshopAt),
      .flags = LoadByte(section, kEnvironmentFlagsAt),
      .trap_level = LoadByte(section, kTrapLevelAt),
      .scene_id = Load<std::int16_t>(section, kSceneIdAt),
      .fall_zone = Load<std::int16_t>(section, kFallZoneAt),
      .fall_room = Load<std::int16_t>(section, kFallRoomAt),
      .parameter = Load<std::int16_t>(section, kEnvironmentParameterAt)};
}

int IsCurrentRoomWorkshop(const RoomEnvironmentProperties& environment) {
  return environment.workshop;
}

int RoomEnvironmentFlags(const RoomEnvironmentProperties& environment) {
  return environment.flags;
}

int CurrentSceneId(const RoomEnvironmentProperties& environment) {
  return environment.scene_id;
}

int RoomFallDestination(const RoomEnvironmentProperties& environment) {
  const auto zone = static_cast<std::uint32_t>(
      static_cast<std::uint16_t>(environment.fall_zone));
  const auto room =
      static_cast<std::uint32_t>(static_cast<int>(environment.fall_room));
  return static_cast<int>((zone << kFallZoneShift) | room);
}

int RoomEnvironmentParameter(const RoomEnvironmentProperties& environment) {
  return environment.parameter;
}

int RoomTrapLevel(const RoomEnvironmentProperties& environment) {
  return environment.trap_level;
}

// --- Loading and unloading the room

std::optional<RoomFileLayout> DecodeRoomFile(std::span<const std::byte> file) {
  if (file.size() < kRoomFileHeaderBytes) {
    return std::nullopt;
  }
  const std::size_t size = file.size();
  const auto room_sections =
      RangeToEnd(size, LoadWord(file, kRoomSectionsOffsetAt));
  const auto cleared = SizedRange(size, LoadWord(file, kClearedOffsetAt),
                                  LoadHalfword(file, kClearedSizeAt));
  const auto script = SizedRange(size, LoadWord(file, kScriptOffsetAt),
                                 LoadHalfword(file, kScriptSizeAt));
  const auto doors = SizedRange(size, LoadWord(file, kDoorOffsetAt),
                                LoadHalfword(file, kDoorSizeAt));
  const auto enemies = SizedRange(size, LoadWord(file, kEnemyOffsetAt),
                                  LoadWord(file, kEnemySizeAt));
  const auto loot = SizedRange(size, LoadWord(file, kLootOffsetAt),
                               LoadWord(file, kLootSizeAt));
  if (!room_sections || !cleared || !script || !doors || !enemies || !loot) {
    return std::nullopt;
  }
  return RoomFileLayout{.room_sections = *room_sections,
                        .cleared = *cleared,
                        .script = *script,
                        .doors = *doors,
                        .enemies = *enemies,
                        .loot = *loot};
}

ZoneTileExtent ZoneTileExtentFromDimensions(int dimensions) {
  const std::uint32_t width =
      static_cast<std::uint32_t>(dimensions) & kDimensionWidthMask;
  return {.columns = static_cast<std::int16_t>(width / kTileUnits),
          .rows = static_cast<std::int16_t>(dimensions / kDimensionRowsUnit)};
}

RoomEntryCamera EntryCamera(const CurrentRoom& room, int initial_distance) {
  const bool raised =
      room.zone_id == kRaisedCameraZone && room.room_id == kRaisedCameraRoom;
  return {.pitch = raised ? kRaisedEntryCameraPitch : kDefaultEntryCameraPitch,
          .mode = initial_distance == kNearEntryCameraDistance
                      ? kNearEntryCameraMode
                      : kFarEntryCameraMode};
}

void BeginSceneActorTransition(RoomLoadHost& host) {
  if (host.SceneTimerResetScope() == SceneTimerScope::kScene) {
    host.ResetSceneTimer();
  }
  host.ClearRoomEnemyMemory();
}

bool QueueRoomLoad(RoomLoadHost& host, int room_id) {
  if (room_id < 0 || room_id >= host.RoomFileCount()) {
    host.Refuse(RoomLoadRefusal::kRoomFileId, room_id);
    return false;
  }
  QueueFileRead(host, LoadedFile::kRoom, host.RoomFile(room_id),
                RoomLoadDiagnostic::kRoomReadPending,
                RoomLoadDiagnostic::kRoomDataHeld);
  return true;
}

namespace {
// The room's scene; a new one clears the room enemy memory.
void EnterRoomScene(RoomLoadHost& host, int room_id) {
  const auto environment = DecodeRoomEnvironmentProperties(
      host.RoomSection(RoomSectionId::kEnvironment));
  if (!environment) {
    host.Refuse(RoomLoadRefusal::kNoEnvironment, room_id);
    return;
  }
  const int scene_id = CurrentSceneId(*environment);
  if (scene_id != host.LoadedSceneId()) {
    host.SetLoadedSceneId(scene_id);
    BeginSceneActorTransition(host);
  }
}

void SetRoomExtent(RoomLoadHost& host) {
  const ZoneTileExtent extent =
      ZoneTileExtentFromDimensions(host.CollisionMapDimensions());
  host.SetZoneExtent(extent, host.RoomCameraBounds().value_or(
                                 ZoneBounds{.min_x = 0,
                                            .min_z = 0,
                                            .max_x = extent.columns,
                                            .max_z = extent.rows}));
}

// The room's events: the encounter's, or the room file's sections. A new
// game's first room is then marked entered.
[[nodiscard]] int LoadRoomFileEvents(RoomLoadHost& host,
                                     std::span<const std::byte> file,
                                     const RoomFileLayout& layout) {
  int result = 0;
  if (!host.EncounterUsesRoomEvents()) {
    result = host.LoadEncounterRoomEvents();
  } else {
    result = host.LoadRoomEvents(
        {.cleared = Section(file, layout.cleared),
         .script = Section(file, layout.script),
         .doors = Section(file, layout.doors),
         .first_entry = host.GameEntryMode() == kNewGameEntryMode});
  }
  if (host.GameEntryMode() == kNewGameEntryMode) {
    host.SetGameEntryMode(kResumedNewGameEntryMode);
  }
  return result;
}

struct LoadedRoomFile {
  CurrentRoom room;
  int event_result = 0;
};

// Loads the read room file's sections, scene, extent, loot, events and
// placements, then frees it; nothing when it is malformed (refused).
[[nodiscard]] std::optional<LoadedRoomFile> LoadRoomFileContents(
    RoomLoadHost& host, int room_id) {
  const auto file = host.LoadedData(LoadedFile::kRoom);
  const auto layout = DecodeRoomFile(file);
  if (!layout) {
    host.Refuse(RoomLoadRefusal::kMalformedRoomFile, room_id);
    host.FreeLoadedData(LoadedFile::kRoom);
    return std::nullopt;
  }
  LoadRoomSections(host, kCurrentRoomOwner,
                   Section(file, layout->room_sections));
  const CurrentRoom room = host.PendingDestination();
  host.SetCurrentRoom(room);
  EnterRoomScene(host, room_id);
  if (host.SceneTimerResetScope() == SceneTimerScope::kRoom) {
    host.ResetSceneTimer();
  }
  host.MarkRoomVisitPending();
  SetRoomExtent(host);
  host.ClearRoomResetWords();
  host.CaptureBackgroundColors();
  if (layout->loot.bytes != 0) {
    host.LoadChestLoot(Section(file, layout->loot));
  }
  const int event_result = LoadRoomFileEvents(host, file, *layout);
  LoadRoomEnemyPlacements(host, Section(file, layout->enemies));
  host.FreeLoadedData(LoadedFile::kRoom);
  return LoadedRoomFile{.room = room, .event_result = event_result};
}

void RestoreRoomEnemies(RoomLoadHost& host, const CurrentRoom& room) {
  Actors::RoomEnemyMemory memory = host.LoadRoomEnemyMemory();
  const auto placements = DecodeRoomPlacements(host.PlacementRecords());
  const auto flags = RestoreRoomEnemyCache(memory, room, placements);
  host.StoreRoomEnemyMemory(memory);
  for (const HiddenFlag& flag : flags) {
    host.SetPlacementHidden(flag.placement_index, flag.hidden);
  }
}

void StartRoomCamera(RoomLoadHost& host, const CurrentRoom& room) {
  host.ResetRoomEntryState();
  if (host.ScreenTransitionEffect() == kFadeInTransitionEffect) {
    host.SetScreenTransition(kFadeInStartStep, true);
  } else {
    host.SetScreenTransition(kCoverStep, false);
  }
  host.ResetCamera(EntryCamera(room, host.InitialCameraDistance()));
  host.FrameCameraOnAshley();
}

// An event the room's events started (in the result's low half) or a
// triggered one runs; otherwise the room is explored.
void StartRoomPlay(RoomLoadHost& host, int event_result) {
  if (static_cast<std::int16_t>(event_result) != 0) {
    host.InitializeWeather(true);
    host.PrepareCutsceneActors();
    return;
  }
  if (host.RoomEventTriggered()) {
    host.RequestEventStart();
    host.InitializeWeather(true);
    host.PrepareCutsceneActors();
    return;
  }
  host.EnterExploration();
  const int music_slot = host.RoomMusicSlot();
  if (music_slot != 0) {
    host.PlayRoomMusic(music_slot);
  }
  host.SetScreenTransition(kExploringStep, false);
  host.ShowDisplay();
}
}  // namespace

void FinishRoomLoad(RoomLoadHost& host, int room_id) {
  host.ResetGameplayInspection();
  const bool read =
      (host.ReadPending(LoadedFile::kRoom) || QueueRoomLoad(host, room_id)) &&
      AwaitFileRead(host, LoadedFile::kRoom);
  // Taken even when the read is refused, so the unload frees what it took.
  host.AllocatePrimitiveBuffers();
  if (!read) {
    return;
  }
  const auto loaded = LoadRoomFileContents(host, room_id);
  if (!loaded) {
    return;
  }
  RestoreRoomEnemies(host, loaded->room);
  PlaceAshleyAtRoomEntrance(host);
  SpawnRoomActors(host);
  host.SnapActorsToSupport();
  host.ClearAshleyStatuses(kRoomEntryClearedStatuses);
  StartRoomCamera(host, loaded->room);
  if (host.NativeResumePending() && !host.FinishNativeResume()) {
    host.JumpToTitle();
    return;
  }
  StartRoomPlay(host, loaded->event_result);
  host.InvalidateControllerConnections();
}

void RemoveRoomActors(RoomActorRemovalHost& host) {
  for (int slot = kFirstRoomActorSlot; slot < kRoomActorSlots; ++slot) {
    host.RemoveActor(slot);
  }
}

void UnloadCurrentRoom(RoomUnloadHost& host) {
  host.ResetGameplayInspection();
  for (int frame = 0; frame < kUnloadTransitionFrames; ++frame) {
    host.DrawTransitionFrame();
  }
  host.UpdateWhileLoading();
  host.FreeRoomEventSections();
  host.MarkCurrentRoomVisited();
  ReleaseOwnedRoomSections(host, kCurrentRoomOwner);
  host.FreePlacements();
  host.FreeScriptedActorAi();
  RemoveRoomActors(host);
  host.ResetRoomActorState();
  host.FreePrimitiveBuffers();
}
}  // namespace VS::Field
