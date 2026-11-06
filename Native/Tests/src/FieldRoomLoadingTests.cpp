// Hand-maintained tests for the typed zone and room loading rules
// (PROMOTION_PLAN row 25c, #276, #311, #312).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorSpawning.hpp"
#include "Actors/RoomEnemyMemory.hpp"
#include "Content/CdDrive.hpp"
#include "Field/RoomLoading.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomState.hpp"
#include "Legacy/Psx.h"
#include "Render/DisplayControl.hpp"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLiveState.h"
}

namespace VS::Field {
namespace {
// Copies a global's bytes and puts them back when the test ends.
class SavedGlobal {
 public:
  template <typename T>
  explicit SavedGlobal(T& global) : where_(&global), bytes_(sizeof global) {
    std::memcpy(bytes_.data(), where_, bytes_.size());
  }
  ~SavedGlobal() { std::memcpy(where_, bytes_.data(), bytes_.size()); }
  SavedGlobal(const SavedGlobal&) = delete;
  SavedGlobal& operator=(const SavedGlobal&) = delete;
  SavedGlobal(SavedGlobal&&) = delete;
  SavedGlobal& operator=(SavedGlobal&&) = delete;

 private:
  void* where_;
  std::vector<std::byte> bytes_;
};

using Bytes = std::vector<std::byte>;

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
const T& Required(const std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}
template <typename T>
const T& Required(const std::optional<T>&& value) = delete;

template <typename T>
void Put(Bytes& bytes, std::size_t at, T value) {
  if (bytes.size() < at + sizeof value) {
    bytes.resize(at + sizeof value);
  }
  std::memcpy(bytes.data() + at, &value, sizeof value);
}

template <typename T>
[[nodiscard]] T Get(std::span<const std::byte> bytes, std::size_t at) {
  T value{};
  std::memcpy(&value, bytes.subspan(at, sizeof value).data(), sizeof value);
  return value;
}

void Append(Bytes& bytes, std::span<const std::byte> more) {
  bytes.insert(bytes.end(), more.begin(), more.end());
}

[[nodiscard]] Bytes Filled(std::size_t count, std::uint8_t value) {
  return Bytes(count, std::byte{value});
}

[[nodiscard]] Bytes Counting(std::size_t count, std::uint8_t first) {
  Bytes bytes;
  for (std::size_t index = 0; index < count; ++index) {
    bytes.push_back(static_cast<std::byte>((first + index) & 0xFFU));
  }
  return bytes;
}

struct Refusal {
  RoomLoadRefusal refusal = RoomLoadRefusal::kZoneId;
  int value = 0;

  friend bool operator==(const Refusal&, const Refusal&) = default;
};

// Records the reports and a log of the host calls.
template <typename Base>
class Recording : public Base {
 public:
  void Diagnostic(RoomLoadDiagnostic code) override {
    diagnostics.push_back(code);
  }
  void Refuse(RoomLoadRefusal refusal, int value) override {
    refusals.push_back({.refusal = refusal, .value = value});
  }
  void Log(std::string call) { log.push_back(std::move(call)); }
  [[nodiscard]] bool Called(std::string_view call) const {
    return std::ranges::find(log, call) != log.end();
  }

  std::vector<RoomLoadDiagnostic> diagnostics;
  std::vector<Refusal> refusals;
  std::vector<std::string> log;
};

// The file reads: a pending count of polls before the read's result.
template <typename Base>
class FileReads : public Base {
 public:
  [[nodiscard]] bool ReadPending(LoadedFile /*file*/) const override {
    return read_pending;
  }
  [[nodiscard]] bool DataHeld(LoadedFile /*file*/) const override {
    return data_held;
  }
  void BeginRead(LoadedFile /*file*/, Content::DiscFileExtent extent) override {
    this->Log("BeginRead");
    reads.push_back(extent);
    read_pending = true;
  }
  [[nodiscard]] DiscReadState PollRead(LoadedFile /*file*/) const override {
    if (pending_polls > 0) {
      --pending_polls;
      return DiscReadState::kPending;
    }
    return read_result;
  }
  void UpdateWhileLoading() override { ++loading_updates; }
  void ReleaseRead(LoadedFile /*file*/) override {
    this->Log("ReleaseRead");
    read_pending = false;
  }
  [[nodiscard]] std::span<const std::byte> LoadedData(
      LoadedFile /*file*/) const override {
    return file;
  }
  void FreeLoadedData(LoadedFile /*file*/) override {
    this->Log("FreeLoadedData");
  }
  [[nodiscard]] SceneTimerScope SceneTimerResetScope() const override {
    return timer_scope;
  }
  void ResetSceneTimer() override { this->Log("ResetSceneTimer"); }

  bool read_pending = false;
  bool data_held = false;
  mutable int pending_polls = 0;
  DiscReadState read_result = DiscReadState::kLoaded;
  int loading_updates = 0;
  std::vector<Content::DiscFileExtent> reads;
  Bytes file;
  SceneTimerScope timer_scope = SceneTimerScope::kNone;
};

// The room record: the sections the last store copied.
template <typename Base>
class SectionStore : public Base {
 public:
  void ClearRoomTint() override { this->Log("ClearRoomTint"); }
  bool StoreRoomSections(int owner_room_index, const RoomSectionLayout& layout,
                         std::span<const std::byte> sections) override {
    this->Log("StoreRoomSections");
    if (!store_succeeds) {
      return false;
    }
    layout_ = layout;
    sections_.assign(sections.begin(), sections.end());
    room = {.loaded = true,
            .displayed = false,
            .owner_room_index = owner_room_index};
    return true;
  }
  [[nodiscard]] std::span<std::byte> RoomSection(
      RoomSectionId section) override {
    const auto index = static_cast<std::size_t>(section);
    return std::span<std::byte>(sections_).subspan(layout_.offsets.at(index),
                                                   layout_.lengths.at(index));
  }
  void OffsetRoomTextureIds() override { this->Log("OffsetRoomTextureIds"); }
  void PrepareMinimapScene() override { this->Log("PrepareMinimapScene"); }
  [[nodiscard]] int PendingEnteredDoor() const override { return pending_door; }
  void ClearPendingEnteredDoor() override {
    this->Log("ClearPendingEnteredDoor");
    pending_door = 0;
  }
  [[nodiscard]] int EnteredDoorId() const override { return entered_door; }
  void MarkEntranceDoor(int door) override {
    this->Log("MarkEntranceDoor " + std::to_string(door));
  }
  void InitializeRoomSystems() override { this->Log("InitializeRoomSystems"); }
  void WaitForOverlayReady() override { this->Log("WaitForOverlayReady"); }
  [[nodiscard]] RoomDisplay LoadedRoom() const override { return room; }
  void ReleaseRoomResources() override { this->Log("ReleaseRoomResources"); }
  void FreeRoomSections() override {
    this->Log("FreeRoomSections");
    room = {};
  }

  bool store_succeeds = true;
  int pending_door = 0;
  int entered_door = 0;
  RoomDisplay room;

 private:
  RoomSectionLayout layout_;
  Bytes sections_;
};

// The room sections block: the 24 lengths, then each section's bytes.
[[nodiscard]] Bytes SectionBlock(
    const std::vector<std::pair<RoomSectionId, Bytes>>& sections) {
  std::array<Bytes, kRoomSectionCount> parts{};
  for (const auto& [section, bytes] : sections) {
    parts.at(static_cast<std::size_t>(section)) = bytes;
  }
  Bytes block;
  for (std::size_t index = 0; index < kRoomSectionCount; ++index) {
    Put<std::uint32_t>(block, index * 4,
                       static_cast<std::uint32_t>(parts.at(index).size()));
  }
  for (const Bytes& part : parts) {
    Append(block, part);
  }
  return block;
}

// The environment section's offsets (Field_MpdEnvironment).
constexpr std::size_t kWorkshopAt = 0x30;
constexpr std::size_t kEnvironmentFlagsAt = 0x31;
constexpr std::size_t kTrapLevelAt = 0x32;
constexpr std::size_t kSceneIdAt = 0x50;
constexpr std::size_t kFallZoneAt = 0x52;
constexpr std::size_t kFallRoomAt = 0x54;
constexpr std::size_t kParameterAt = 0x56;

[[nodiscard]] Bytes Environment(std::int16_t scene_id) {
  Bytes environment(kRoomEnvironmentBytes);
  Put<std::int16_t>(environment, kSceneIdAt, scene_id);
  return environment;
}

// A placement record's bytes (Field_MpdEnemySpawn).
void PutEnemy(Bytes& records, std::size_t index, const EnemyPlacement& enemy,
              std::uint8_t hidden = 0) {
  const std::size_t at = index * kRoomPlacementBytes;
  records.resize(std::max(records.size(), at + kRoomPlacementBytes));
  Put<std::uint8_t>(records, at, 0);
  Put(records, at + 0x02, hidden);
  Put(records, at + 0x04, enemy.enemy_id);
  Put(records, at + 0x05, enemy.slot);
  Put(records, at + 0x06, enemy.flag_test);
  Put(records, at + 0x07, enemy.stat_test);
  Put(records, at + 0x08, enemy.flag_value);
  Put(records, at + 0x0A, enemy.stat_value);
  Put(records, at + 0x0C, enemy.placement);
  Put(records, at + 0x23, enemy.facing);
  Put<std::uint8_t>(records, at + 0x24, enemy.starts_engaged ? 1 : 0);
  Put(records, at + 0x27, enemy.scene_test);
}

// A placement record's bytes (Field_MpdObjectSpawn).
void PutObject(Bytes& records, std::size_t index,
               const ObjectPlacement& object) {
  const std::size_t at = index * kRoomPlacementBytes;
  records.resize(std::max(records.size(), at + kRoomPlacementBytes));
  Put<std::uint8_t>(records, at, 1);
  Put(records, at + 0x04, object.flag);
  Put(records, at + 0x06, object.flag_min);
  Put(records, at + 0x07, object.flag_max);
  Put(records, at + 0x08, object.slot);
  Put(records, at + 0x0A, object.tile_x);
  Put(records, at + 0x0B, object.tile_z);
  Put(records, at + 0x0C, object.height);
  Put(records, at + 0x0D, object.direction);
  Put(records, at + 0x0E, object.model);
}

// A 16-bit TIM without a palette whose pixels count up from `first`.
[[nodiscard]] Bytes Tim(const Render::ScreenRect& area, std::uint8_t first) {
  const auto pixel_bytes = static_cast<std::size_t>(area.width) *
                           static_cast<std::size_t>(area.height) * 2;
  Bytes tim;
  Put<std::uint32_t>(tim, 0, 0x10);
  Put<std::uint32_t>(tim, 4, 2);
  Put<std::uint32_t>(tim, 8, static_cast<std::uint32_t>(12 + pixel_bytes));
  Put(tim, 12, area);
  Append(tim, Counting(pixel_bytes, first));
  return tim;
}

// The texture section: four table sizes (the first table's, then none),
// then the first table: `count` and each sized TIM, padded to a word.
[[nodiscard]] Bytes TextureSection(const std::vector<Bytes>& tims,
                                   std::int32_t count) {
  Bytes table;
  Put<std::int32_t>(table, 0, count);
  for (const Bytes& tim : tims) {
    const std::size_t padded = (tim.size() + 3) / 4 * 4;
    Put<std::int32_t>(table, table.size(), static_cast<std::int32_t>(padded));
    Append(table, tim);
    table.resize(table.size() + (padded - tim.size()));
  }
  Bytes section;
  Put<std::int32_t>(section, 0, static_cast<std::int32_t>(table.size()));
  section.resize(16);
  Append(section, table);
  return section;
}

// A zone file: its header, a two-entry room file table, one enemy (its ZUD
// file and template) and an empty texture table.
constexpr std::size_t kZoneRoomFilesAt = kZoneHeaderBytes;
constexpr std::size_t kZoneEnemiesAt = kZoneRoomFilesAt + 16;
constexpr std::size_t kZoneEnemyTablesBytes = 12 + kZoneEnemyTemplateBytes;
constexpr std::size_t kZoneTexturesAt = kZoneEnemiesAt + kZoneEnemyTablesBytes;

[[nodiscard]] Bytes ZoneFileBytes(std::int32_t music_id) {
  Bytes file(kZoneTexturesAt + 16);
  Put(file, 0x00, static_cast<std::int32_t>(kZoneRoomFilesAt));
  Put<std::int32_t>(file, 0x04, 16);
  Put(file, 0x08, static_cast<std::int32_t>(kZoneEnemiesAt));
  Put(file, 0x10, static_cast<std::int32_t>(kZoneTexturesAt));
  Put<std::int32_t>(file, 0x18, music_id);
  Put<std::int32_t>(file, kZoneRoomFilesAt, 100);
  Put<std::int32_t>(file, kZoneRoomFilesAt + 4, 2048);
  Put<std::int32_t>(file, kZoneRoomFilesAt + 8, 101);
  Put<std::int32_t>(file, kZoneRoomFilesAt + 12, 4096);
  Put<std::int32_t>(file, kZoneEnemiesAt, 1);
  Put<std::int32_t>(file, kZoneEnemiesAt + 4, 200);
  Put<std::int32_t>(file, kZoneEnemiesAt + 8, 8192);
  return file;
}

// A room file's parts; the room sections are placed last.
struct RoomParts {
  Bytes sections{};
  Bytes cleared = Filled(4, 0xC1);
  Bytes script = Filled(8, 0x5C);
  Bytes doors = Filled(4, 0xD0);
  Bytes enemies{};
  Bytes loot = Filled(8, 0x10);
};

[[nodiscard]] Bytes RoomFileBytes(const RoomParts& parts) {
  Bytes file(kRoomFileHeaderBytes);
  const auto place = [&file](std::size_t offset_at,
                             std::span<const std::byte> part) {
    Put<std::int32_t>(file, offset_at, static_cast<std::int32_t>(file.size()));
    Append(file, part);
  };
  place(0x08, parts.cleared);
  Put<std::uint16_t>(file, 0x0C,
                     static_cast<std::uint16_t>(parts.cleared.size()));
  place(0x10, parts.script);
  Put<std::uint16_t>(file, 0x14,
                     static_cast<std::uint16_t>(parts.script.size()));
  place(0x18, parts.doors);
  Put<std::uint16_t>(file, 0x1C,
                     static_cast<std::uint16_t>(parts.doors.size()));
  place(0x20, parts.enemies);
  Put<std::int32_t>(file, 0x24,
                    static_cast<std::int32_t>(parts.enemies.size()));
  place(0x28, parts.loot);
  Put<std::int32_t>(file, 0x2C, static_cast<std::int32_t>(parts.loot.size()));
  place(0x00, parts.sections);
  return file;
}

// A room with scene 17's environment and one enemy for any slot.
[[nodiscard]] RoomParts StandardRoom() {
  RoomParts parts;
  parts.sections =
      SectionBlock({{RoomSectionId::kEnvironment, Environment(17)}});
  PutEnemy(parts.enemies, 0, {});
  return parts;
}

// --- Zone hosts

struct PaletteCopy {
  int row = 0;
  int first_color = 0;
  int colors = 0;
  Bytes bytes{};

  friend bool operator==(const PaletteCopy&, const PaletteCopy&) = default;
};

struct ImageUpload {
  Render::ScreenRect area{};
  Bytes pixels{};

  friend bool operator==(const ImageUpload&, const ImageUpload&) = default;
};

class TextureRecorder : public Recording<ZoneTextureHost> {
 public:
  void CopyPaletteRow(std::span<const std::byte> colors,
                      const ZonePaletteRow& row) override {
    palettes.push_back({.row = row.row,
                        .first_color = row.first_color,
                        .colors = row.colors,
                        .bytes = Bytes(colors.begin(), colors.end())});
  }
  void UploadImage(const Render::ScreenRect& area,
                   std::span<const std::byte> pixels) override {
    uploads.push_back(
        {.area = area, .pixels = Bytes(pixels.begin(), pixels.end())});
  }

  std::vector<PaletteCopy> palettes;
  std::vector<ImageUpload> uploads;
};

class TableRecorder : public Recording<ZoneTableHost> {
 public:
  void StoreRoomFiles(std::span<const Content::DiscFileExtent> files) override {
    room_files.assign(files.begin(), files.end());
    ++room_file_stores;
  }
  void StoreZoneEnemies(std::span<const Content::DiscFileExtent> zud_files,
                        std::span<const std::byte> templates) override {
    zuds.assign(zud_files.begin(), zud_files.end());
    template_bytes = templates.size();
    ++enemy_stores;
  }

  std::vector<Content::DiscFileExtent> room_files;
  std::vector<Content::DiscFileExtent> zuds;
  std::size_t template_bytes = 0;
  int room_file_stores = 0;
  int enemy_stores = 0;
};

class ZoneRecorder : public FileReads<Recording<ZoneLoadHost>> {
 public:
  void StoreRoomFiles(std::span<const Content::DiscFileExtent> files) override {
    Log("StoreRoomFiles");
    room_files.assign(files.begin(), files.end());
  }
  void StoreZoneEnemies(std::span<const Content::DiscFileExtent> zud_files,
                        std::span<const std::byte> templates) override {
    Log("StoreZoneEnemies");
    zuds.assign(zud_files.begin(), zud_files.end());
    template_bytes = templates.size();
  }
  void CopyPaletteRow(std::span<const std::byte> /*colors*/,
                      const ZonePaletteRow& /*row*/) override {
    Log("CopyPaletteRow");
  }
  void UploadImage(const Render::ScreenRect& /*area*/,
                   std::span<const std::byte> /*pixels*/) override {
    Log("UploadImage");
  }
  [[nodiscard]] Content::DiscFileExtent ZoneFile(int zone_id) const override {
    return {.sector = 1000 + zone_id, .bytes = 2048};
  }
  [[nodiscard]] int ZoneMusicId(int /*zone_id*/) const override {
    return zone_music;
  }
  [[nodiscard]] int CurrentMusicId() const override { return current_music; }
  [[nodiscard]] bool MusicFadeEnabled() const override { return fade; }
  void FadeOutCurrentMusic() override { Log("FadeOutCurrentMusic"); }
  [[nodiscard]] bool NativeResumePending() const override { return resume; }
  [[nodiscard]] int GameEntryMode() const override { return entry_mode; }
  void ResetZoneScriptFlags(bool new_game) override {
    Log(new_game ? "ResetZoneScriptFlags new" : "ResetZoneScriptFlags");
  }
  void StopAndFreeCurrentMusic() override { Log("StopAndFreeCurrentMusic"); }
  void FreeMusicSlot(int slot) override {
    Log("FreeMusicSlot " + std::to_string(slot));
  }
  void LoadWaveBank(int music_id) override {
    Log("LoadWaveBank " + std::to_string(music_id));
  }
  [[nodiscard]] int LoadMusicSlot(int music_id, int target_slot) override {
    Log("LoadMusicSlot " + std::to_string(music_id) + " " +
        std::to_string(target_slot));
    return 7;
  }
  void SetMusicSlot(int slot) override {
    Log("SetMusicSlot " + std::to_string(slot));
  }
  void FinishZoneMusic(int music_id) override {
    Log("FinishZoneMusic " + std::to_string(music_id));
  }

  int zone_music = 9;
  int current_music = 3;
  bool fade = true;
  bool resume = false;
  int entry_mode = kNewGameEntryMode;
  std::vector<Content::DiscFileExtent> room_files;
  std::vector<Content::DiscFileExtent> zuds;
  std::size_t template_bytes = 0;
};

[[nodiscard]] bool SameExtents(std::span<const Content::DiscFileExtent> files,
                               std::span<const Content::DiscFileExtent> want) {
  return std::ranges::equal(files, want,
                            [](const Content::DiscFileExtent& left,
                               const Content::DiscFileExtent& right) {
                              return left.sector == right.sector &&
                                     left.bytes == right.bytes;
                            });
}

using Extent = Content::DiscFileExtent;
constexpr Extent kFirstRoomFile{.sector = 100, .bytes = 2048};
constexpr Extent kSecondRoomFile{.sector = 101, .bytes = 4096};
constexpr std::array kZoneRoomFiles{kFirstRoomFile, kSecondRoomFile};
constexpr std::array kZoneZuds{Extent{.sector = 200, .bytes = 8192}};

TEST_CASE("A zone file's sections decode within the file",
          "[field][field-room-loading]") {
  const Bytes file = ZoneFileBytes(9);
  const auto decoded = DecodeZoneFile(file);
  const ZoneFileLayout& layout = Required(decoded);
  CHECK(layout.room_files == ByteRange{.offset = 0x1C, .bytes = 16});
  CHECK(layout.enemies == ByteRange{.offset = kZoneEnemiesAt,
                                    .bytes = file.size() - kZoneEnemiesAt});
  CHECK(layout.textures == ByteRange{.offset = kZoneTexturesAt, .bytes = 16});
  CHECK(layout.music_id == 9);

  Bytes empty_table = file;
  Put<std::int32_t>(empty_table, 0x00, -1);
  Put<std::int32_t>(empty_table, 0x04, 0);
  const auto without_rooms = DecodeZoneFile(empty_table);
  CHECK(Required(without_rooms).room_files == ByteRange{});
}

TEST_CASE("A zone file whose sections leave the file is refused",
          "[field][field-room-loading]") {
  const Bytes file = ZoneFileBytes(9);
  CHECK_FALSE(
      DecodeZoneFile(std::span(file).first(kZoneHeaderBytes - 1)).has_value());
  Bytes past = file;
  Put<std::int32_t>(past, 0x00, static_cast<std::int32_t>(file.size() - 8));
  CHECK_FALSE(DecodeZoneFile(past).has_value());
  Bytes negative = file;
  Put<std::int32_t>(negative, 0x08, -4);
  CHECK_FALSE(DecodeZoneFile(negative).has_value());
  Bytes beyond = file;
  Put(beyond, 0x10, static_cast<std::int32_t>(file.size() + 1));
  CHECK_FALSE(DecodeZoneFile(beyond).has_value());
}

TEST_CASE("A zone's room file and enemy tables decode their entries",
          "[field][field-room-loading]") {
  const Bytes file = ZoneFileBytes(9);
  const auto table = std::span(file).subspan(kZoneRoomFilesAt, 20);
  CHECK(SameExtents(DecodeRoomFileTable(table), kZoneRoomFiles));

  const auto section = std::span(file).subspan(kZoneEnemiesAt);
  const auto decoded = DecodeZoneEnemyTables(section);
  const ZoneEnemyTables& tables = Required(decoded);
  CHECK(SameExtents(tables.zud_files, kZoneZuds));
  CHECK(tables.templates.size() == kZoneEnemyTemplateBytes);
  CHECK(tables.templates.data() == section.data() + 12);
  Bytes none;
  Put<std::int32_t>(none, 0, 0);
  const auto without_enemies = DecodeZoneEnemyTables(none);
  CHECK(Required(without_enemies).zud_files.empty());
}

TEST_CASE("A zone enemy table with a bad count is refused",
          "[field][field-room-loading]") {
  const Bytes file = ZoneFileBytes(9);
  const auto section = std::span(file).subspan(kZoneEnemiesAt);
  CHECK_FALSE(DecodeZoneEnemyTables(section.first(3)).has_value());
  Bytes negative(section.begin(), section.end());
  Put<std::int32_t>(negative, 0, -1);
  CHECK_FALSE(DecodeZoneEnemyTables(negative).has_value());
  Bytes two(section.begin(), section.end());
  Put<std::int32_t>(two, 0, 2);
  CHECK_FALSE(DecodeZoneEnemyTables(two).has_value());
}

TEST_CASE("The zone's room file table is stored, an empty one reported",
          "[field][field-room-loading]") {
  const Bytes file = ZoneFileBytes(9);
  TableRecorder host;
  LoadZoneRoomFileTable(host, std::span(file).subspan(kZoneRoomFilesAt, 16));
  CHECK(SameExtents(host.room_files, kZoneRoomFiles));
  CHECK(host.diagnostics.empty());
  LoadZoneRoomFileTable(host, {});
  CHECK(host.room_files.empty());
  CHECK(host.room_file_stores == 2);
  CHECK(host.diagnostics ==
        std::vector{RoomLoadDiagnostic::kEmptyRoomFileTable});
}

TEST_CASE("The zone's enemy templates are stored, a malformed table refused",
          "[field][field-room-loading]") {
  const Bytes file = ZoneFileBytes(9);
  TableRecorder host;
  LoadZoneEnemyTemplates(host, std::span(file).subspan(kZoneEnemiesAt));
  CHECK(SameExtents(host.zuds, kZoneZuds));
  CHECK(host.template_bytes == kZoneEnemyTemplateBytes);
  CHECK(host.refusals.empty());
  LoadZoneEnemyTemplates(host, std::span(file).subspan(kZoneEnemiesAt, 20));
  CHECK(host.zuds.empty());
  CHECK(host.template_bytes == 0);
  CHECK(host.enemy_stores == 2);
  CHECK(host.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kMalformedZoneEnemies, .value = 0}});
}

TEST_CASE("Zone textures upload shifted and fill the palette rows they cover",
          "[field][field-room-loading]") {
  const Bytes image = Tim({.x = 400, .y = 8, .width = 2, .height = 2}, 0x01);
  const Bytes palette =
      Tim({.x = 784, .y = 223, .width = 4, .height = 3}, 0x40);
  const Bytes last_row =
      Tim({.x = 784, .y = 227, .width = 4, .height = 2}, 0x80);
  TextureRecorder host;
  UploadZoneTextures(host, TextureSection({image, palette, last_row}, 3));

  REQUIRE(host.uploads.size() == 1);
  CHECK(host.uploads[0] ==
        ImageUpload{.area = {.x = 80, .y = 264, .width = 2, .height = 2},
                    .pixels = Counting(8, 0x01)});
  CHECK(host.palettes ==
        std::vector<PaletteCopy>{{.row = 0,
                                  .first_color = 16,
                                  .colors = 4,
                                  .bytes = Counting(8, 0x48)},
                                 {.row = 1,
                                  .first_color = 16,
                                  .colors = 4,
                                  .bytes = Counting(8, 0x50)},
                                 {.row = 3,
                                  .first_color = 16,
                                  .colors = 4,
                                  .bytes = Counting(8, 0x80)}});
  CHECK(host.refusals.empty());
}

TEST_CASE("A zone texture that does not parse is passed over",
          "[field][field-room-loading]") {
  const Bytes wide = Tim({.x = 0, .y = 0, .width = 2000, .height = 1}, 0);
  const Bytes image = Tim({.x = 320, .y = 0, .width = 1, .height = 1}, 0x22);
  TextureRecorder host;
  UploadZoneTextures(host, TextureSection({wide, image}, 2));
  REQUIRE(host.uploads.size() == 1);
  CHECK(host.uploads[0] ==
        ImageUpload{.area = {.x = 0, .y = 256, .width = 1, .height = 1},
                    .pixels = Counting(2, 0x22)});
  CHECK(host.refusals.empty());
}

TEST_CASE("A malformed zone texture section is refused",
          "[field][field-room-loading]") {
  const Bytes image = Tim({.x = 320, .y = 0, .width = 1, .height = 1}, 0x22);
  const auto refused = [](const Bytes& section) {
    TextureRecorder host;
    UploadZoneTextures(host, section);
    return std::pair{host.refusals, host.uploads.size()};
  };
  const auto refusal = [](int value) {
    return std::vector<Refusal>{
        {.refusal = RoomLoadRefusal::kMalformedZoneTextures, .value = value}};
  };

  CHECK(refused(Filled(15, 0)) == std::pair{refusal(0), std::size_t{0}});
  Bytes no_table = Filled(16, 0);
  Put<std::int32_t>(no_table, 0, -8);
  CHECK(refused(no_table) == std::pair{std::vector<Refusal>{}, std::size_t{0}});
  Bytes oversized = TextureSection({image}, 1);
  Put(oversized, 0, static_cast<std::int32_t>(oversized.size()));
  CHECK(refused(oversized) == std::pair{refusal(0), std::size_t{0}});
  CHECK(refused(TextureSection({image}, 2)) ==
        std::pair{refusal(1), std::size_t{1}});
  Bytes negative = TextureSection({image}, 1);
  Put<std::int32_t>(negative, 20, -4);
  CHECK(refused(negative) == std::pair{refusal(0), std::size_t{0}});
}

// --- Loading the zone

TEST_CASE("Queuing a zone load reports a held read and fades the music",
          "[field][field-room-loading]") {
  ZoneRecorder host;
  host.read_pending = true;
  host.data_held = true;
  CHECK(QueueZoneLoad(host, 12));
  CHECK(host.diagnostics == std::vector{RoomLoadDiagnostic::kZoneReadPending,
                                        RoomLoadDiagnostic::kZoneDataHeld});
  REQUIRE(host.reads.size() == 1);
  CHECK(host.reads[0].sector == 1012);
  CHECK(host.reads[0].bytes == 2048);
  CHECK(host.log ==
        std::vector<std::string>{"BeginRead", "FadeOutCurrentMusic"});
}

TEST_CASE("Queuing a zone load keeps the music when it need not fade",
          "[field][field-room-loading]") {
  ZoneRecorder same_music;
  same_music.current_music = same_music.zone_music;
  CHECK(QueueZoneLoad(same_music, 0));
  CHECK(same_music.diagnostics.empty());
  CHECK_FALSE(same_music.Called("FadeOutCurrentMusic"));
  ZoneRecorder no_fade;
  no_fade.fade = false;
  CHECK(QueueZoneLoad(no_fade, 255));
  CHECK_FALSE(no_fade.Called("FadeOutCurrentMusic"));
}

TEST_CASE("Queuing a zone load refuses an out-of-range zone id",
          "[field][field-room-loading]") {
  for (const int zone : {-1, kZoneCount}) {
    ZoneRecorder refused;
    CHECK_FALSE(QueueZoneLoad(refused, zone));
    CHECK(refused.reads.empty());
    CHECK(refused.refusals ==
          std::vector<Refusal>{
              {.refusal = RoomLoadRefusal::kZoneId, .value = zone}});
  }
}

TEST_CASE("Finishing a zone load stores its tables and switches the music",
          "[field][field-room-loading]") {
  ZoneRecorder host;
  host.read_pending = true;
  host.pending_polls = 2;
  host.timer_scope = SceneTimerScope::kZone;
  host.file = ZoneFileBytes(9);
  FinishZoneLoad(host, 4);

  CHECK(host.log == std::vector<std::string>{
                        "ReleaseRead", "ResetSceneTimer", "StoreZoneEnemies",
                        "StoreRoomFiles", "FreeLoadedData",
                        "ResetZoneScriptFlags new", "StopAndFreeCurrentMusic",
                        "FreeMusicSlot 1", "FreeMusicSlot 2", "FreeMusicSlot 3",
                        "FreeMusicSlot 4", "SetMusicSlot 0", "LoadWaveBank 9",
                        "LoadMusicSlot 9 3", "SetMusicSlot 7",
                        "FinishZoneMusic 9"});
  CHECK(host.loading_updates == 2);
  CHECK(host.reads.empty());
  CHECK(SameExtents(host.room_files, kZoneRoomFiles));
  CHECK(SameExtents(host.zuds, kZoneZuds));
  CHECK(host.template_bytes == kZoneEnemyTemplateBytes);
  CHECK(host.refusals.empty());
}

TEST_CASE("A zone load keeps the music it already plays",
          "[field][field-room-loading]") {
  ZoneRecorder host;
  host.file = ZoneFileBytes(3);
  host.zone_music = 3;
  host.resume = true;
  FinishZoneLoad(host, 4);
  CHECK(host.log == std::vector<std::string>{"BeginRead", "ReleaseRead",
                                             "StoreZoneEnemies",
                                             "StoreRoomFiles", "FreeLoadedData",
                                             "FinishZoneMusic 3"});

  ZoneRecorder no_fade;
  no_fade.file = ZoneFileBytes(9);
  no_fade.fade = false;
  FinishZoneLoad(no_fade, 4);
  CHECK_FALSE(no_fade.Called("StopAndFreeCurrentMusic"));
  CHECK(no_fade.Called("FinishZoneMusic 9"));
}

TEST_CASE("A silent zone load resets its flags without loading music",
          "[field][field-room-loading]") {
  ZoneRecorder silent;
  silent.file = ZoneFileBytes(0);
  silent.entry_mode = 0;
  silent.timer_scope = SceneTimerScope::kRoom;
  FinishZoneLoad(silent, 4);
  CHECK(silent.Called("ResetZoneScriptFlags"));
  CHECK_FALSE(silent.Called("ResetSceneTimer"));
  CHECK(silent.Called("SetMusicSlot 0"));
  CHECK_FALSE(silent.Called("LoadWaveBank 0"));
  CHECK(silent.Called("FinishZoneMusic 0"));
  CHECK(silent.Called("FadeOutCurrentMusic"));
}

TEST_CASE("A failed or malformed zone read is refused and freed (#312)",
          "[field][field-room-loading]") {
  ZoneRecorder failed;
  failed.fade = false;
  failed.read_result = DiscReadState::kFailed;
  FinishZoneLoad(failed, 4);
  CHECK(failed.log ==
        std::vector<std::string>{"BeginRead", "ReleaseRead", "FreeLoadedData"});
  CHECK(failed.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kZoneReadFailed, .value = 0}});

  ZoneRecorder malformed;
  malformed.fade = false;
  malformed.file = Filled(kZoneHeaderBytes - 1, 0);
  FinishZoneLoad(malformed, 4);
  CHECK(malformed.log ==
        std::vector<std::string>{"BeginRead", "ReleaseRead", "FreeLoadedData"});
  CHECK(malformed.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kMalformedZoneFile, .value = 4}});
}

TEST_CASE("Finishing a zone load refuses an out-of-range zone id",
          "[field][field-room-loading]") {
  ZoneRecorder refused;
  FinishZoneLoad(refused, kZoneCount);
  CHECK(refused.log.empty());
  CHECK(refused.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kZoneId, .value = kZoneCount}});
}

class ReleaseRecorder : public ZoneReleaseHost {
 public:
  void ReleaseZoneEventResources() override {
    log.emplace_back("ReleaseZoneEventResources");
  }
  void FreeZoneTables() override { log.emplace_back("FreeZoneTables"); }

  std::vector<std::string> log;
};

TEST_CASE("Releasing a zone frees its events and tables",
          "[field][field-room-loading]") {
  ReleaseRecorder host;
  ReleaseZoneData(host);
  CHECK(host.log == std::vector<std::string>{"ReleaseZoneEventResources",
                                             "FreeZoneTables"});
  CHECK(ZoneSoundFileId(42) == 42);

  SavedGlobal saved_zone{Field_zoneContext};
  Field_zoneContext.musicId = 42;
  CHECK(LiveZoneSoundFileId() == 42);
}

// --- Placements

class FlagHost : public PlacementTestHost {
 public:
  [[nodiscard]] std::uint8_t ScriptFlag(int flag) const override {
    const auto found = flags.find(flag);
    return found == flags.end() ? std::uint8_t{0} : found->second;
  }
  [[nodiscard]] int AshleyCurrentHp() const override { return hp; }

  std::map<int, std::uint8_t> flags;
  int hp = 100;
};

[[nodiscard]] RoomPlacement Enemy(const EnemyPlacement& enemy,
                                  std::uint8_t hidden = 0) {
  return {.kind = 0, .hidden = hidden, .enemy = enemy};
}

TEST_CASE("Room placement records decode their enemy and object fields",
          "[field][field-room-loading]") {
  const EnemyPlacement enemy{
      .enemy_id = 3,
      .slot = 4,
      .flag_test = 1,
      .stat_test = 2,
      .flag_value = 0x1234,
      .stat_value = 0x5678,
      .placement = {.tile_x = 9, .direction = 10, .tile_z = 11, .height = 12},
      .facing = 2,
      .starts_engaged = true,
      .scene_test = 3};
  const ObjectPlacement object{.flag = 0x2345,
                               .flag_min = 1,
                               .flag_max = 7,
                               .slot = kAnyObjectSlot,
                               .tile_x = 5,
                               .tile_z = 6,
                               .height = 7,
                               .direction = 8,
                               .model = 0x3A60};
  Bytes records;
  PutEnemy(records, 0, enemy, 1);
  PutObject(records, 1, object);
  records.resize(records.size() + kRoomPlacementBytes - 1);

  const auto placements = DecodeRoomPlacements(records);
  REQUIRE(placements.size() == 2);
  CHECK(placements[0].IsEnemy());
  CHECK(placements[0].hidden == 1);
  CHECK(placements[0].enemy == enemy);
  CHECK(placements[0].NamedSlot() == 4);
  CHECK_FALSE(placements[1].IsEnemy());
  CHECK(placements[1].object == object);
  CHECK(placements[1].NamedSlot() == kAnyObjectSlot);
}

TEST_CASE("A truncated room placement record is not decoded",
          "[field][field-room-loading]") {
  Bytes records;
  PutEnemy(records, 0, {}, 0);
  CHECK(DecodeRoomPlacements(std::span(records).first(kRoomPlacementBytes - 1))
            .empty());
}

TEST_CASE("An enemy placement shows when its scene test passes",
          "[field][field-room-loading]") {
  FlagHost host;
  CHECK(EnemyPlacementVisible(Enemy({}), host));
  CHECK_FALSE(EnemyPlacementVisible(Enemy({}, 1), host));
  CHECK(EnemyPlacementVisible(Enemy({.scene_test = 1}), host));
  CHECK_FALSE(EnemyPlacementVisible(Enemy({.scene_test = 2}), host));
  CHECK_FALSE(EnemyPlacementVisible(Enemy({.scene_test = 3}), host));
  CHECK(EnemyPlacementVisible(Enemy({.scene_test = 9}), host));
  host.flags[1] = 1;
  host.flags[2] = 1;
  CHECK_FALSE(EnemyPlacementVisible(Enemy({.scene_test = 1}), host));
  CHECK(EnemyPlacementVisible(Enemy({.scene_test = 2}), host));
  CHECK(EnemyPlacementVisible(Enemy({.scene_test = 3}), host));
}

TEST_CASE("An enemy placement shows when its flag test passes",
          "[field][field-room-loading]") {
  FlagHost host;
  host.flags[0x82] = 5;
  CHECK(EnemyPlacementVisible(Enemy({.flag_test = 1, .flag_value = 5}), host));
  CHECK_FALSE(
      EnemyPlacementVisible(Enemy({.flag_test = 1, .flag_value = 4}), host));
  CHECK(EnemyPlacementVisible(Enemy({.flag_test = 2, .flag_value = 5}), host));
  CHECK_FALSE(
      EnemyPlacementVisible(Enemy({.flag_test = 2, .flag_value = 4}), host));
  CHECK(EnemyPlacementVisible(Enemy({.flag_test = 3, .flag_value = 5}), host));
  CHECK_FALSE(
      EnemyPlacementVisible(Enemy({.flag_test = 3, .flag_value = 6}), host));
  CHECK(EnemyPlacementVisible(Enemy({.flag_test = 4, .flag_value = 6}), host));
}

TEST_CASE("An enemy placement's stat test reads Ashley's HP or a flag",
          "[field][field-room-loading]") {
  FlagHost host;
  const auto shows = [&host](std::uint8_t test, int value) {
    return EnemyPlacementVisible(
        Enemy({.stat_test = test,
               .stat_value = static_cast<std::uint16_t>(value)}),
        host);
  };
  CHECK(shows(1, 100));
  CHECK_FALSE(shows(1, 99));
  CHECK(shows(2, 100));
  CHECK_FALSE(shows(2, 101));
  CHECK(shows(1, 0xFC00 | 100));
}

TEST_CASE("An enemy placement's stat test can read a script flag",
          "[field][field-room-loading]") {
  FlagHost host;
  const auto shows = [&host](std::uint8_t test, int value) {
    return EnemyPlacementVisible(
        Enemy({.stat_test = test,
               .stat_value = static_cast<std::uint16_t>(value)}),
        host);
  };
  host.flags[0x123] = 3;
  CHECK(shows(3, (3 << 10) | 0x123));
  CHECK_FALSE(shows(3, (2 << 10) | 0x123));
  CHECK(shows(4, (3 << 10) | 0x123));
  CHECK_FALSE(shows(4, (2 << 10) | 0x123));
  CHECK(shows(5, (3 << 10) | 0x123));
  CHECK_FALSE(shows(5, (4 << 10) | 0x123));
  CHECK(shows(6, 0));
}

TEST_CASE("An object placement shows while its flag is in range",
          "[field][field-room-loading]") {
  FlagHost host;
  host.flags[0x45] = 3;
  const auto shows = [&host](int flag, std::uint8_t low, std::uint8_t high) {
    return ObjectPlacementVisible({.flag = static_cast<std::uint16_t>(flag),
                                   .flag_min = low,
                                   .flag_max = high},
                                  host);
  };
  CHECK(shows(0x45, 3, 3));
  CHECK(shows(0x45, 0, 9));
  CHECK_FALSE(shows(0x45, 4, 9));
  CHECK_FALSE(shows(0x45, 0, 2));
  CHECK(shows(0xFC45, 3, 3));
  CHECK(shows(0x46, 0, 0));
}

TEST_CASE("An enemy's spawn flags follow its facing and engagement",
          "[field][field-room-loading]") {
  const std::array<int, 5> expected{6, 0, 1, 6, 6};
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const auto facing = static_cast<std::uint8_t>(index);
    CHECK(EnemySpawnFlags({.facing = facing}) == expected.at(index));
    CHECK(EnemySpawnFlags({.facing = facing, .starts_engaged = true}) ==
          (expected.at(index) | 0x80));
  }
}

// --- Spawning the room's actors

struct SpawnedEnemy {
  int slot = 0;
  std::size_t placement_index = 0;
  int enemy_id = 0;
  int flags = 0;

  friend bool operator==(const SpawnedEnemy&, const SpawnedEnemy&) = default;
};

struct SpawnedActor {
  int slot = 0;
  Actors::SpawnPlacement placement{};
  int flags = 0;
  int model = 0;

  friend bool operator==(const SpawnedActor&, const SpawnedActor&) = default;
};

struct RememberedFlag {
  int spawn_index = 0;
  std::uint8_t hidden = 0;

  friend bool operator==(const RememberedFlag&,
                         const RememberedFlag&) = default;
};

class SpawnRecorder : public Recording<RoomActorSpawnHost> {
 public:
  [[nodiscard]] std::span<const std::byte> PlacementRecords() const override {
    return records;
  }
  [[nodiscard]] bool ActorSlotOccupied(int slot) const override {
    return occupied.at(static_cast<std::size_t>(slot));
  }
  [[nodiscard]] int ZoneEnemyCount() const override { return templates; }
  [[nodiscard]] bool TemplateUsesSecondaryBody(int enemy_id) const override {
    return enemy_id == secondary_template;
  }
  void SpawnEnemy(int slot, std::size_t placement_index, int enemy_id,
                  int flags) override {
    Occupy(slot);
    enemies.push_back({.slot = slot,
                       .placement_index = placement_index,
                       .enemy_id = enemy_id,
                       .flags = flags});
  }
  void SpawnUntemplatedEnemy(int slot, const Actors::SpawnPlacement& placement,
                             int flags) override {
    Occupy(slot);
    plain.push_back({.slot = slot, .placement = placement, .flags = flags});
  }
  void SpawnObject(int slot, int model, const Actors::SpawnPlacement& placement,
                   int material) override {
    Occupy(slot);
    objects.push_back({.slot = slot,
                       .placement = placement,
                       .flags = material,
                       .model = model});
  }
  void RememberHidden(int spawn_index, std::uint8_t hidden) override {
    remembered.push_back({.spawn_index = spawn_index, .hidden = hidden});
  }
  void FreeCachedZud() override { ++zud_frees; }
  [[nodiscard]] std::uint8_t ScriptFlag(int /*flag*/) const override {
    return 0;
  }
  [[nodiscard]] int AshleyCurrentHp() const override { return 1; }

  Bytes records;
  std::array<bool, kRoomActorSlots> occupied{};
  int templates = 1;
  int secondary_template = -1;
  std::vector<SpawnedEnemy> enemies;
  std::vector<SpawnedActor> plain;
  std::vector<SpawnedActor> objects;
  std::vector<RememberedFlag> remembered;
  int zud_frees = 0;

 private:
  void Occupy(int slot) { occupied.at(static_cast<std::size_t>(slot)) = true; }
};

TEST_CASE("Room actors spawn into named and free slots",
          "[field][field-room-loading]") {
  SpawnRecorder host;
  host.templates = 2;
  host.secondary_template = 1;
  host.occupied[2] = true;
  PutEnemy(host.records, 0,
           {.enemy_id = 1, .slot = 0, .facing = 1, .starts_engaged = true});
  PutEnemy(host.records, 1, {.enemy_id = 0, .slot = 3});
  PutObject(host.records, 2,
            {.slot = kAnyObjectSlot,
             .tile_x = 7,
             .tile_z = 8,
             .height = 5,
             .direction = 1,
             .model = 0x3A60});
  PutEnemy(
      host.records, 3,
      {.enemy_id = kUntemplatedEnemyId,
       .placement = {.tile_x = 4, .direction = 2, .tile_z = 6, .height = 2}});
  SpawnRoomActors(host);

  CHECK(host.enemies ==
        std::vector<SpawnedEnemy>{
            {.slot = 4, .placement_index = 0, .enemy_id = 1, .flags = 0x80},
            {.slot = 3, .placement_index = 1, .enemy_id = 0, .flags = 6}});
  CHECK(host.objects == std::vector<SpawnedActor>{{.slot = 5,
                                                   .placement = {.tile_x = 7,
                                                                 .direction = 5,
                                                                 .tile_z = 8,
                                                                 .height = 64},
                                                   .flags = 0x0D,
                                                   .model = 0xA03}});
  CHECK(host.plain == std::vector<SpawnedActor>{{.slot = 6,
                                                 .placement = {.tile_x = 4,
                                                               .direction = 2,
                                                               .tile_z = 6,
                                                               .height = 128},
                                                 .flags = 6}});
  CHECK(host.remembered == std::vector<RememberedFlag>{{.spawn_index = 0},
                                                       {.spawn_index = 1},
                                                       {.spawn_index = 2},
                                                       {.spawn_index = 3}});
  CHECK(host.zud_frees == 1);
  CHECK(host.diagnostics.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE("A second body keeps the slot four on from the free slots",
          "[field][field-room-loading]") {
  SpawnRecorder host;
  host.secondary_template = 0;
  PutEnemy(host.records, 0, {.enemy_id = 0});
  for (std::size_t index = 1; index <= 4; ++index) {
    PutObject(host.records, index, {.slot = kAnyObjectSlot});
  }
  PutEnemy(host.records, 5, {.enemy_id = 0, .slot = 3});
  SpawnRoomActors(host);

  CHECK(host.enemies ==
        std::vector<SpawnedEnemy>{{.slot = 2, .enemy_id = 0, .flags = 6}});
  std::vector<int> object_slots;
  for (const SpawnedActor& object : host.objects) {
    object_slots.push_back(object.slot);
  }
  // Slot 3 is named by placement 5 and slot 6 is the first enemy's body.
  CHECK(object_slots == std::vector{4, 5, 7, 8});
  CHECK(host.diagnostics == std::vector{RoomLoadDiagnostic::kNoFreeActorSlot});
  CHECK(host.zud_frees == 1);
  CHECK(host.remembered.empty());
}

TEST_CASE("Placements naming a missing template or slot are refused",
          "[field][field-room-loading]") {
  SpawnRecorder host;
  host.secondary_template = 0;
  PutEnemy(host.records, 0, {.enemy_id = 3});
  PutEnemy(host.records, 1, {.enemy_id = 0, .slot = 20});
  PutObject(host.records, 2, {.slot = 40});
  PutEnemy(host.records, 3, {.enemy_id = 0, .slot = 13});
  PutEnemy(host.records, 4, {.enemy_id = kUntemplatedEnemyId, .slot = 2});
  SpawnRoomActors(host);

  CHECK(host.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kEnemyTemplate, .value = 0},
            {.refusal = RoomLoadRefusal::kActorSlot, .value = 20},
            {.refusal = RoomLoadRefusal::kActorSlot, .value = 40},
            {.refusal = RoomLoadRefusal::kActorSlot, .value = 13}});
  CHECK(host.enemies.empty());
  CHECK(host.objects.empty());
  CHECK(host.plain == std::vector<SpawnedActor>{{.slot = 2, .flags = 6}});
  CHECK(host.remembered.size() == 5);
  CHECK(host.zud_frees == 1);
}

TEST_CASE("Spawning stops when no actor slot is free",
          "[field][field-room-loading]") {
  SpawnRecorder object_first;
  object_first.occupied.fill(true);
  PutObject(object_first.records, 0, {.slot = kAnyObjectSlot});
  PutEnemy(object_first.records, 1, {.enemy_id = 0});
  SpawnRoomActors(object_first);
  CHECK(object_first.diagnostics ==
        std::vector{RoomLoadDiagnostic::kNoFreeActorSlot});
  CHECK(object_first.zud_frees == 0);
  CHECK(object_first.remembered.empty());

  SpawnRecorder enemy_first;
  enemy_first.occupied.fill(true);
  PutEnemy(enemy_first.records, 0, {.enemy_id = 0});
  PutObject(enemy_first.records, 1, {.slot = kAnyObjectSlot});
  SpawnRoomActors(enemy_first);
  CHECK(enemy_first.diagnostics ==
        std::vector{RoomLoadDiagnostic::kNoFreeActorSlot});
  CHECK(enemy_first.zud_frees == 1);
  CHECK(enemy_first.remembered.empty());
  CHECK(enemy_first.enemies.empty());
}

TEST_CASE("Spawning remembers at most 32 placements' hidden flags",
          "[field][field-room-loading]") {
  SpawnRecorder hidden;
  for (std::size_t index = 0; index < 3; ++index) {
    PutEnemy(hidden.records, index, {.enemy_id = 0}, 1);
  }
  SpawnRoomActors(hidden);
  CHECK(hidden.enemies.empty());
  CHECK(hidden.remembered ==
        std::vector<RememberedFlag>{{.spawn_index = 0, .hidden = 1},
                                    {.spawn_index = 1, .hidden = 1},
                                    {.spawn_index = 2, .hidden = 1}});
  CHECK(hidden.zud_frees == 1);

  SpawnRecorder many;
  for (std::size_t index = 0; index < 40; ++index) {
    PutObject(many.records, index, {.flag_min = 1, .slot = kAnyObjectSlot});
  }
  SpawnRoomActors(many);
  CHECK(many.objects.empty());
  REQUIRE(many.remembered.size() == kRememberedHiddenFlags);
  CHECK(many.remembered.back().spawn_index == 31);

  SpawnRecorder empty;
  SpawnRoomActors(empty);
  CHECK(empty.remembered.empty());
  CHECK(empty.zud_frees == 0);
}

// --- Ashley's entrance and the room enemy cache

class EntranceRecorder : public RoomEntranceHost {
 public:
  [[nodiscard]] RoomEntrance PendingEntrance() const override {
    return entrance;
  }
  void PlaceAshley(const Actors::SpawnPlacement& at) override { placed = at; }
  void StepAshley(const EntryStep& step) override { stepped = step; }
  void SettleAshley() override { ++settles; }

  RoomEntrance entrance;
  std::optional<Actors::SpawnPlacement> placed;
  std::optional<EntryStep> stepped;
  int settles = 0;
};

TEST_CASE("Ashley's entry step follows the door's elevation",
          "[field][field-room-loading]") {
  CHECK(EntranceStep({.elevation = 0}) == EntryStep{.x = 64, .z = 0});
  CHECK(EntranceStep({.elevation = 1}) == EntryStep{.x = 0, .z = 64});
  CHECK(EntranceStep({.elevation = 2}) == EntryStep{.x = 64, .z = 0});
  CHECK(EntranceStep({.elevation = 3}) == EntryStep{.x = 0, .z = 64});
  CHECK(EntranceStep({.elevation = 4}) == EntryStep{});
}

TEST_CASE("Ashley enters at the pending door and steps in",
          "[field][field-room-loading]") {
  EntranceRecorder host;
  host.entrance = {.tile_x = 3,
                   .tile_z = 4,
                   .direction = 2,
                   .elevation = 1,
                   .has_entry_step = true};
  PlaceAshleyAtRoomEntrance(host);
  CHECK(host.placed ==
        Actors::SpawnPlacement{
            .tile_x = 3, .direction = 2, .tile_z = 4, .height = 64});
  CHECK(host.stepped == EntryStep{.x = 0, .z = 64});
  CHECK(host.settles == 1);

  EntranceRecorder no_step;
  no_step.entrance = {.elevation = 2};
  PlaceAshleyAtRoomEntrance(no_step);
  CHECK(no_step.placed == Actors::SpawnPlacement{.height = 128});
  CHECK_FALSE(no_step.stepped.has_value());
  CHECK(no_step.settles == 1);
}

// A remembered enemy record for placement `spawn_tag` - 1.
void Remember(Actors::RememberedEnemy& enemy, std::uint8_t spawn_tag,
              std::uint8_t hidden) {
  enemy.spawn_tag = spawn_tag;
  enemy.hidden = hidden;
}

[[nodiscard]] Actors::RoomEnemyMemory CachedRooms() {
  Actors::RoomEnemyMemory memory;
  const std::array<std::uint8_t, Actors::kRememberedRooms> recency{2, 4, 1, 3};
  for (std::size_t room = 0; room < recency.size(); ++room) {
    memory.rooms.at(room).recency = recency.at(room);
    memory.rooms.at(room).zone = static_cast<std::uint8_t>(room + 1);
  }
  memory.rooms[0].zone = 5;
  memory.rooms[0].map = 7;
  Remember(memory.rooms[0].enemies[0], 2, 1);
  Remember(memory.rooms[0].enemies[1], 3, 0);
  Remember(memory.rooms[2].enemies[0], 1, 1);
  memory.current_room = 3;
  return memory;
}

[[nodiscard]] std::vector<std::uint8_t> Recency(
    const Actors::RoomEnemyMemory& memory) {
  std::vector<std::uint8_t> recency;
  for (const Actors::RememberedRoom& room : memory.rooms) {
    recency.push_back(room.recency);
  }
  return recency;
}

TEST_CASE("A remembered room restores its enemies' hidden flags",
          "[field][field-room-loading]") {
  const std::vector<RoomPlacement> placements{Enemy({}, 1), Enemy({}, 0),
                                              Enemy({}, 1)};
  Actors::RoomEnemyMemory memory = CachedRooms();
  const auto flags =
      RestoreRoomEnemyCache(memory, {.zone_id = 5, .room_id = 7}, placements);
  CHECK(flags == std::vector<HiddenFlag>{{.placement_index = 0, .hidden = 1},
                                         {.placement_index = 1, .hidden = 1},
                                         {.placement_index = 2, .hidden = 0}});
  CHECK(memory.current_room == 0);
  CHECK(Recency(memory) == std::vector<std::uint8_t>{4, 3, 1, 2});
  CHECK(memory.rooms[0].enemies[0].spawn_tag == 2);
}

TEST_CASE("A new room replaces the oldest remembered room",
          "[field][field-room-loading]") {
  const std::vector<RoomPlacement> placements{Enemy({}, 1)};
  Actors::RoomEnemyMemory memory = CachedRooms();
  memory.rooms[1].zone = 9;
  memory.rooms[1].map = 9;
  memory.rooms[1].recency = 0;
  memory.rooms[3].recency = 4;
  const auto flags =
      RestoreRoomEnemyCache(memory, {.zone_id = 9, .room_id = 9}, placements);
  CHECK(flags.empty());
  CHECK(memory.current_room == 1);
  CHECK(Recency(memory) == std::vector<std::uint8_t>{1, 4, 0, 3});
  CHECK(memory.rooms[1].zone == 9);
  CHECK(memory.rooms[1].map == 9);
}

TEST_CASE("A new room evicts the least recent room when the cache is full",
          "[field][field-room-loading]") {
  const std::vector<RoomPlacement> placements{Enemy({}, 1)};
  Actors::RoomEnemyMemory full = CachedRooms();
  CHECK(RestoreRoomEnemyCache(full, {.zone_id = 9, .room_id = 9}, placements)
            .empty());
  CHECK(full.current_room == 2);
  CHECK(Recency(full) == std::vector<std::uint8_t>{1, 3, 4, 2});
  CHECK(full.rooms[2].enemies[0].spawn_tag == 0);
  CHECK(full.rooms[2].zone == 9);
}

class PlacementRecorder : public RoomPlacementHost {
 public:
  [[nodiscard]] std::span<const std::byte> EncounterPlacements(
      std::span<const std::byte> room) override {
    return encounter.empty() ? room : std::span<const std::byte>(encounter);
  }
  void StorePlacements(std::span<const std::byte> records) override {
    stored.assign(records.begin(), records.end());
  }

  Bytes encounter;
  Bytes stored;
};

TEST_CASE("A room's placements are stored, or the encounter's in their place",
          "[field][field-room-loading]") {
  const Bytes room = Filled(kRoomPlacementBytes, 0x11);
  PlacementRecorder host;
  LoadRoomEnemyPlacements(host, room);
  CHECK(host.stored == room);
  host.encounter = Filled(kRoomPlacementBytes * 2, 0x22);
  LoadRoomEnemyPlacements(host, room);
  CHECK(host.stored == host.encounter);
}

// --- The room sections

using SectionRecorder = SectionStore<Recording<RoomSectionHost>>;

[[nodiscard]] Bytes CollisionHeader(std::int16_t width, std::int16_t height) {
  Bytes header(8);
  Put(header, 0, width);
  Put(header, 2, height);
  return header;
}

[[nodiscard]] Bytes SectionE(std::int32_t second_array,
                             std::int32_t has_second) {
  Bytes header(kSectionEHeaderBytes);
  Put(header, 4, second_array);
  Put(header, 8, has_second);
  return header;
}

[[nodiscard]] Bytes LaidOutBlock() {
  return SectionBlock({{RoomSectionId::kGeometry, Filled(4, 1)},
                       {RoomSectionId::kCollisionMap, Filled(8, 2)},
                       {RoomSectionId::kEnvironment, Environment(17)}});
}

TEST_CASE("The room section header lays out each section",
          "[field][field-room-loading]") {
  const Bytes block = LaidOutBlock();
  const auto decoded = DecodeRoomSectionLayout(block);
  const RoomSectionLayout& layout = Required(decoded);
  CHECK(layout.lengths[0] == 4);
  CHECK(layout.lengths[1] == 8);
  CHECK(layout.lengths[10] == kRoomEnvironmentBytes);
  CHECK(layout.offsets[1] == 4);
  CHECK(layout.offsets[2] == 12);
  CHECK(layout.offsets[10] == 12);
  CHECK(layout.offsets[11] == 12 + kRoomEnvironmentBytes);
  CHECK(layout.total_bytes == 12 + kRoomEnvironmentBytes);
}

TEST_CASE("A room section header that leaves the block is refused",
          "[field][field-room-loading]") {
  const Bytes block = LaidOutBlock();
  CHECK_FALSE(DecodeRoomSectionLayout(
                  std::span(block).first(kRoomSectionHeaderBytes - 1))
                  .has_value());
  CHECK_FALSE(DecodeRoomSectionLayout(std::span(block).first(block.size() - 1))
                  .has_value());
  Bytes negative = block;
  Put<std::int32_t>(negative, 12, -1);
  CHECK_FALSE(DecodeRoomSectionLayout(negative).has_value());
}

TEST_CASE("Section E's arrays resolve within the section",
          "[field][field-room-loading]") {
  Bytes section_e = SectionE(0x23, 1);
  CHECK(ResolveSectionEArrays(section_e));
  CHECK(Get<std::uint32_t>(section_e, 0x0C) == 0x14);
  CHECK(Get<std::uint32_t>(section_e, 0x10) == 0x34);
  for (const std::int32_t has_second : {0, -1}) {
    Bytes single = SectionE(0x23, has_second);
    CHECK(ResolveSectionEArrays(single));
    CHECK(Get<std::uint32_t>(single, 0x0C) == 0x14);
    CHECK(Get<std::uint32_t>(single, 0x10) == 0);
  }
  Bytes short_e = Filled(kSectionEHeaderBytes - 1, 0x77);
  CHECK_FALSE(ResolveSectionEArrays(short_e));
  CHECK(short_e == Filled(kSectionEHeaderBytes - 1, 0x77));
}

TEST_CASE("The collision map's cell count resolves from its extent",
          "[field][field-room-loading]") {
  Bytes collision = CollisionHeader(3, 5);
  CHECK(ResolveCollisionCellCount(collision));
  CHECK(Get<std::int16_t>(collision, 6) == 15);
  Bytes wide = CollisionHeader(300, 300);
  CHECK(ResolveCollisionCellCount(wide));
  CHECK(Get<std::int16_t>(wide, 6) == static_cast<std::int16_t>(90000 - 65536));
  Bytes short_map = Filled(7, 0x55);
  CHECK_FALSE(ResolveCollisionCellCount(short_map));
  CHECK(short_map == Filled(7, 0x55));
}

TEST_CASE("Loading the room sections stores and resolves them",
          "[field][field-room-loading]") {
  SectionRecorder host;
  host.pending_door = 3;
  const Bytes block =
      SectionBlock({{RoomSectionId::kGeometry, Filled(4, 1)},
                    {RoomSectionId::kCollisionMap, CollisionHeader(2, 3)},
                    {RoomSectionId::kSectionE, SectionE(8, 1)},
                    {RoomSectionId::kMinimapScene, Filled(4, 3)}});
  CHECK(LoadRoomSections(host, 0, block));
  CHECK(host.log == std::vector<std::string>{
                        "ClearRoomTint", "StoreRoomSections",
                        "OffsetRoomTextureIds", "PrepareMinimapScene",
                        "MarkEntranceDoor 2", "ClearPendingEnteredDoor",
                        "InitializeRoomSystems", "WaitForOverlayReady"});
  CHECK(host.room == RoomDisplay{.loaded = true, .owner_room_index = 0});
  CHECK(host.pending_door == 0);
  CHECK(Get<std::int16_t>(host.RoomSection(RoomSectionId::kCollisionMap), 6) ==
        6);
  const auto section_e = host.RoomSection(RoomSectionId::kSectionE);
  CHECK(Get<std::uint32_t>(section_e, 0x0C) == 0x14);
  CHECK(Get<std::uint32_t>(section_e, 0x10) == 0x1C);
  CHECK(host.refusals.empty());
}

TEST_CASE("Room sections loaded through a door mark the door entered",
          "[field][field-room-loading]") {
  SectionRecorder entered;
  entered.entered_door = 5;
  CHECK(LoadRoomSections(
      entered, 1,
      SectionBlock({{RoomSectionId::kMinimapScene, Filled(4, 3)}})));
  CHECK(entered.log ==
        std::vector<std::string>{"ClearRoomTint", "StoreRoomSections",
                                 "PrepareMinimapScene", "MarkEntranceDoor 5",
                                 "InitializeRoomSystems"});
  CHECK(entered.room.owner_room_index == 1);
}

TEST_CASE("Malformed or empty room sections are refused or skipped",
          "[field][field-room-loading]") {
  SectionRecorder short_headers;
  CHECK(LoadRoomSections(
      short_headers, 0,
      SectionBlock({{RoomSectionId::kCollisionMap, Filled(4, 0)},
                    {RoomSectionId::kSectionE, Filled(0x10, 0)}})));
  CHECK(short_headers.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kMalformedRoomSections, .value = 13},
            {.refusal = RoomLoadRefusal::kMalformedRoomSections, .value = 1}});
  CHECK(short_headers.Called("InitializeRoomSystems"));
}

TEST_CASE("Empty, malformed or unstorable room sections are not loaded",
          "[field][field-room-loading]") {
  SectionRecorder empty;
  CHECK_FALSE(LoadRoomSections(empty, 0, SectionBlock({})));
  CHECK(empty.log == std::vector<std::string>{"ClearRoomTint"});
  CHECK_FALSE(empty.room.loaded);

  SectionRecorder malformed;
  CHECK_FALSE(LoadRoomSections(malformed, 3, Filled(10, 0)));
  CHECK(malformed.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kMalformedRoomSections, .value = 3}});

  SectionRecorder full;
  full.store_succeeds = false;
  CHECK_FALSE(LoadRoomSections(
      full, 0, SectionBlock({{RoomSectionId::kGeometry, Filled(4, 1)}})));
  CHECK(full.log ==
        std::vector<std::string>{"ClearRoomTint", "StoreRoomSections"});
}

TEST_CASE("Only the owning room releases the room sections",
          "[field][field-room-loading]") {
  SectionRecorder host;
  host.room = {.loaded = true, .displayed = true, .owner_room_index = 0};
  CHECK_FALSE(ReleaseOwnedRoomSections(host, 1));
  CHECK(host.log.empty());
  CHECK(ReleaseOwnedRoomSections(host, 0));
  CHECK(host.log ==
        std::vector<std::string>{"ReleaseRoomResources", "FreeRoomSections"});
  CHECK(host.room == RoomDisplay{});
  CHECK_FALSE(ReleaseOwnedRoomSections(host, 0));
}

TEST_CASE("The room's auxiliary and sound sections are bounded",
          "[field][field-room-loading]") {
  Bytes auxiliary = Filled(4, 6);
  Bytes sound = Filled(4, 9);
  RoomSections sections{};
  sections[static_cast<std::size_t>(RoomSectionId::kAuxiliary)] = auxiliary;
  CHECK(RoomAuxiliaryData(sections).data() == auxiliary.data());
  CHECK(RoomAuxiliaryData(sections).size() == 4);
  CHECK(IsRoomAudioSequenceAbsent(sections));
  sections[static_cast<std::size_t>(RoomSectionId::kSoundData)] = sound;
  CHECK_FALSE(IsRoomAudioSequenceAbsent(sections));
}

TEST_CASE("The room's supplemental records are bounded",
          "[field][field-room-loading]") {
  Bytes supplemental = Counting(20, 0);
  RoomSections sections{};
  sections[static_cast<std::size_t>(RoomSectionId::kSupplemental)] =
      supplemental;
  CHECK(RoomSupplementalRecord(sections, 0).data() == supplemental.data());
  CHECK(RoomSupplementalRecord(sections, 1).data() == supplemental.data() + 8);
  CHECK(RoomSupplementalRecord(sections, 1).size() == 8);
  CHECK(RoomSupplementalRecord(sections, 2).empty());
  CHECK(RoomSupplementalRecord(sections, -1).empty());
}

[[nodiscard]] Bytes FilledEnvironment(std::int16_t fall_room) {
  Bytes environment = Environment(17);
  Put<std::uint8_t>(environment, kWorkshopAt, 2);
  Put<std::uint8_t>(environment, kEnvironmentFlagsAt, 0x44);
  Put<std::uint8_t>(environment, kTrapLevelAt, 3);
  Put<std::int16_t>(environment, kFallZoneAt, 9);
  Put(environment, kFallRoomAt, fall_room);
  Put<std::int16_t>(environment, kParameterAt, -5);
  return environment;
}

TEST_CASE("The room environment's fields decode",
          "[field][field-room-loading]") {
  const auto decoded = DecodeRoomEnvironmentProperties(FilledEnvironment(4));
  const RoomEnvironmentProperties& environment = Required(decoded);
  CHECK(environment == RoomEnvironmentProperties{.workshop = 2,
                                                 .flags = 0x44,
                                                 .trap_level = 3,
                                                 .scene_id = 17,
                                                 .fall_zone = 9,
                                                 .fall_room = 4,
                                                 .parameter = -5});
  CHECK(IsCurrentRoomWorkshop(environment) == 2);
  CHECK(RoomEnvironmentFlags(environment) == 0x44);
  CHECK(CurrentSceneId(environment) == 17);
  CHECK(RoomFallDestination(environment) == 0x90004);
  CHECK(RoomEnvironmentParameter(environment) == -5);
  CHECK(RoomTrapLevel(environment) == 3);
}

TEST_CASE("A room's fall destination packs its zone and room",
          "[field][field-room-loading]") {
  CHECK(RoomFallDestination({.fall_zone = 9, .fall_room = -2}) == -2);
  CHECK(RoomFallDestination({.fall_zone = -1, .fall_room = 4}) ==
        static_cast<int>(0xFFFF0004U));
  const Bytes environment_bytes = FilledEnvironment(4);
  CHECK_FALSE(DecodeRoomEnvironmentProperties(
                  std::span(environment_bytes).first(kRoomEnvironmentBytes - 1))
                  .has_value());
}

TEST_CASE("The live environment reads nothing without a loaded section",
          "[field][field-room-loading]") {
  SavedGlobal saved_room{Field_roomData};
  Field_roomData.environment = nullptr;
  Field_roomData.header.environmentLength = 0;
  Field_roomData.soundData = nullptr;
  Field_roomData.header.soundDataLength = 0;
  CHECK(LiveIsCurrentRoomWorkshop() == 0);
  CHECK(LiveCurrentSceneId() == 0);
  CHECK(LiveRoomFallDestination() == 0);
  CHECK(LiveRoomEnvironmentParameter() == 0);
  CHECK(LiveRoomTrapLevel() == 0);
  CHECK(LiveIsRoomAudioSequenceAbsent() == 1);
}

TEST_CASE("The live environment reads the loaded room's whole section",
          "[field][field-room-loading]") {
  SavedGlobal saved_room{Field_roomData};
  Bytes environment = FilledEnvironment(4);
  Field_roomData.environment =
      reinterpret_cast<Field_MpdEnvironment*>(environment.data());
  Field_roomData.header.environmentLength =
      static_cast<u_int>(kRoomEnvironmentBytes);
  Field_roomData.soundData = nullptr;
  Field_roomData.header.soundDataLength = 0;
  CHECK(LiveIsCurrentRoomWorkshop() == 2);
  CHECK(LiveCurrentSceneId() == 17);
  CHECK(LiveRoomFallDestination() == 0x90004);
  CHECK(LiveRoomEnvironmentParameter() == -5);
  CHECK(LiveRoomTrapLevel() == 3);
  Field_roomData.header.environmentLength = 0x40;
  CHECK(LiveCurrentSceneId() == 0);
  CHECK(LiveRoomTrapLevel() == 0);

  std::array<u_int, 1> sound{};
  Field_roomData.soundData = sound.data();
  Field_roomData.header.soundDataLength = static_cast<u_int>(sizeof sound);
  CHECK(LiveIsRoomAudioSequenceAbsent() == 0);
}

// --- Loading and unloading the room

TEST_CASE("A room file's sections decode within the file",
          "[field][field-room-loading]") {
  const Bytes file = RoomFileBytes(StandardRoom());
  const auto decoded = DecodeRoomFile(file);
  CHECK(
      Required(decoded) ==
      RoomFileLayout{.room_sections = {.offset = 0x70,
                                       .bytes = kRoomSectionHeaderBytes +
                                                kRoomEnvironmentBytes},
                     .cleared = {.offset = 0x30, .bytes = 4},
                     .script = {.offset = 0x34, .bytes = 8},
                     .doors = {.offset = 0x3C, .bytes = 4},
                     .enemies = {.offset = 0x40, .bytes = kRoomPlacementBytes},
                     .loot = {.offset = 0x68, .bytes = 8}});

  Bytes sections_at_end = file;
  Put(sections_at_end, 0x00, static_cast<std::int32_t>(file.size()));
  const auto at_end = DecodeRoomFile(sections_at_end);
  CHECK(Required(at_end).room_sections ==
        ByteRange{.offset = file.size(), .bytes = 0});

  RoomParts no_doors = StandardRoom();
  no_doors.doors.clear();
  Bytes bogus_doors = RoomFileBytes(no_doors);
  Put<std::int32_t>(bogus_doors, 0x18, -5);
  const auto empty_doors = DecodeRoomFile(bogus_doors);
  CHECK(Required(empty_doors).doors == ByteRange{});
}

TEST_CASE("A room file whose sections leave the file is refused",
          "[field][field-room-loading]") {
  const Bytes file = RoomFileBytes(StandardRoom());
  CHECK_FALSE(DecodeRoomFile(std::span(file).first(kRoomFileHeaderBytes - 1))
                  .has_value());
  Bytes long_enemies = file;
  Put<std::int32_t>(long_enemies, 0x24, 0x1000);
  CHECK_FALSE(DecodeRoomFile(long_enemies).has_value());
  Bytes negative_sections = file;
  Put<std::int32_t>(negative_sections, 0x00, -1);
  CHECK_FALSE(DecodeRoomFile(negative_sections).has_value());
  Bytes past_end = file;
  Put(past_end, 0x00, static_cast<std::int32_t>(file.size() + 1));
  CHECK_FALSE(DecodeRoomFile(past_end).has_value());
}

TEST_CASE("The zone's tile extent and the entry camera follow the room",
          "[field][field-room-loading]") {
  CHECK(ZoneTileExtentFromDimensions((3 << 20) | 0x40) ==
        ZoneTileExtent{.columns = 4, .rows = 3});
  // The rows divide the signed dimensions, truncating toward zero.
  CHECK(ZoneTileExtentFromDimensions(static_cast<int>(0xFFE00020U)) ==
        ZoneTileExtent{.columns = 2, .rows = -1});
  CHECK(ZoneTileExtentFromDimensions(0xFFFF) ==
        ZoneTileExtent{.columns = 4095, .rows = 0});

  const RoomEntryCamera raised =
      EntryCamera({.zone_id = 27, .room_id = 1}, kNearEntryCameraDistance);
  CHECK(raised.pitch == kRaisedEntryCameraPitch);
  CHECK(raised.mode == kNearEntryCameraMode);
  const RoomEntryCamera far = EntryCamera({.zone_id = 27, .room_id = 2}, 0x500);
  CHECK(far.pitch == kDefaultEntryCameraPitch);
  CHECK(far.mode == kFarEntryCameraMode);
  CHECK(EntryCamera({.zone_id = 26, .room_id = 1}, kNearEntryCameraDistance)
            .pitch == kDefaultEntryCameraPitch);
}

// The room load's host: the room file's read, its sections and placements,
// and a log of the steps the load takes.
class RoomLoadRecorder
    : public FileReads<SectionStore<Recording<RoomLoadHost>>> {
 public:
  RoomLoadRecorder() {
    file = RoomFileBytes(StandardRoom());
    timer_scope = SceneTimerScope::kRoom;
  }

  [[nodiscard]] std::span<const std::byte> PlacementRecords() const override {
    return stored;
  }
  [[nodiscard]] bool ActorSlotOccupied(int slot) const override {
    return occupied.at(static_cast<std::size_t>(slot));
  }
  [[nodiscard]] int ZoneEnemyCount() const override { return 1; }
  [[nodiscard]] bool TemplateUsesSecondaryBody(
      int /*enemy_id*/) const override {
    return false;
  }
  void SpawnEnemy(int slot, std::size_t /*placement_index*/, int /*enemy_id*/,
                  int /*flags*/) override {
    occupied.at(static_cast<std::size_t>(slot)) = true;
    Log("SpawnEnemy " + std::to_string(slot));
  }
  void SpawnUntemplatedEnemy(int slot,
                             const Actors::SpawnPlacement& /*placement*/,
                             int /*flags*/) override {
    Log("SpawnUntemplatedEnemy " + std::to_string(slot));
  }
  void SpawnObject(int slot, int /*model*/,
                   const Actors::SpawnPlacement& /*placement*/,
                   int /*material*/) override {
    Log("SpawnObject " + std::to_string(slot));
  }
  void RememberHidden(int /*spawn_index*/, std::uint8_t hidden) override {
    remembered.push_back(hidden);
  }
  void FreeCachedZud() override { Log("FreeCachedZud"); }
  [[nodiscard]] std::uint8_t ScriptFlag(int /*flag*/) const override {
    return 0;
  }
  [[nodiscard]] int AshleyCurrentHp() const override { return 1; }

  [[nodiscard]] RoomEntrance PendingEntrance() const override { return {}; }
  void PlaceAshley(const Actors::SpawnPlacement& /*placement*/) override {
    Log("PlaceAshley");
  }
  void StepAshley(const EntryStep& /*step*/) override { Log("StepAshley"); }
  void SettleAshley() override { Log("SettleAshley"); }

  [[nodiscard]] std::span<const std::byte> EncounterPlacements(
      std::span<const std::byte> records) override {
    return records;
  }
  void StorePlacements(std::span<const std::byte> records) override {
    Log("StorePlacements");
    stored.assign(records.begin(), records.end());
  }

  void ResetGameplayInspection() override { Log("ResetGameplayInspection"); }
  [[nodiscard]] Content::DiscFileExtent RoomFile(int room_id) const override {
    return {.sector = 2000 + room_id, .bytes = 4096};
  }
  [[nodiscard]] int RoomFileCount() const override { return 4; }
  void AllocatePrimitiveBuffers() override { Log("AllocatePrimitiveBuffers"); }
  [[nodiscard]] CurrentRoom PendingDestination() const override {
    return destination;
  }
  void SetCurrentRoom(const CurrentRoom& entered) override {
    Log("SetCurrentRoom");
    current_room = entered;
  }
  [[nodiscard]] int LoadedSceneId() const override { return loaded_scene; }
  void SetLoadedSceneId(int scene_id) override {
    Log("SetLoadedSceneId " + std::to_string(scene_id));
    loaded_scene = scene_id;
  }
  void ClearRoomEnemyMemory() override { Log("ClearRoomEnemyMemory"); }
  [[nodiscard]] Actors::RoomEnemyMemory LoadRoomEnemyMemory() const override {
    return memory;
  }
  void StoreRoomEnemyMemory(const Actors::RoomEnemyMemory& updated) override {
    Log("StoreRoomEnemyMemory");
    memory = updated;
  }
  void SetPlacementHidden(std::size_t placement_index,
                          std::uint8_t hidden) override {
    Log("SetPlacementHidden " + std::to_string(placement_index));
    stored.at(placement_index * kRoomPlacementBytes + 2) = std::byte{hidden};
  }
  void MarkRoomVisitPending() override { Log("MarkRoomVisitPending"); }
  [[nodiscard]] int CollisionMapDimensions() const override {
    return dimensions;
  }
  [[nodiscard]] std::optional<ZoneBounds> RoomCameraBounds() const override {
    return camera_bounds;
  }
  void SetZoneExtent(const ZoneTileExtent& tiles,
                     const ZoneBounds& zone_bounds) override {
    extent = tiles;
    bounds = zone_bounds;
  }
  void ClearRoomResetWords() override { Log("ClearRoomResetWords"); }
  void CaptureBackgroundColors() override { Log("CaptureBackgroundColors"); }
  void LoadChestLoot(std::span<const std::byte> loot) override {
    Log("LoadChestLoot");
    loot_bytes = loot.size();
  }

  [[nodiscard]] bool EncounterUsesRoomEvents() const override {
    return room_events;
  }
  [[nodiscard]] VS::Event::RoomEventLoad LoadEncounterRoomEvents() override {
    Log("LoadEncounterRoomEvents");
    return event_result;
  }
  [[nodiscard]] VS::Event::RoomEventLoad LoadRoomEvents(
      const RoomEventSections& sections) override {
    Log("LoadRoomEvents");
    event_sizes = {sections.cleared.size(), sections.script.size(),
                   sections.doors.size()};
    first_entry = sections.first_entry;
    return event_result;
  }
  [[nodiscard]] int GameEntryMode() const override { return entry_mode; }
  void SetGameEntryMode(int mode) override { entry_mode = mode; }

  void SnapActorsToSupport() override { Log("SnapActorsToSupport"); }
  void ClearAshleyStatuses(int statuses) override {
    cleared_statuses = statuses;
  }
  void ResetRoomEntryState() override { Log("ResetRoomEntryState"); }
  [[nodiscard]] int ScreenTransitionEffect() const override {
    return transition_effect;
  }
  void SetScreenTransition(int step, bool clear_alpha) override {
    Log("SetScreenTransition " + std::to_string(step) +
        (clear_alpha ? " clear" : ""));
  }
  [[nodiscard]] int InitialCameraDistance() const override {
    return camera_distance;
  }
  void ResetCamera(const RoomEntryCamera& entry_camera) override {
    camera = entry_camera;
  }
  void FrameCameraOnAshley() override { Log("FrameCameraOnAshley"); }

  [[nodiscard]] bool NativeResumePending() const override {
    return resume_pending;
  }
  [[nodiscard]] bool FinishNativeResume() override {
    Log("FinishNativeResume");
    return resume_succeeds;
  }
  void JumpToTitle() override { Log("JumpToTitle"); }
  [[nodiscard]] bool RoomEventTriggered() override { return event_triggered; }
  void RequestEventStart() override { Log("RequestEventStart"); }
  void InitializeWeather(bool event_running) override {
    Log(event_running ? "InitializeWeather event" : "InitializeWeather");
  }
  void PrepareCutsceneActors() override { Log("PrepareCutsceneActors"); }
  void EnterExploration() override { Log("EnterExploration"); }
  [[nodiscard]] int RoomMusicSlot() const override { return music_slot; }
  void PlayRoomMusic(int slot) override {
    Log("PlayRoomMusic " + std::to_string(slot));
  }
  void ShowDisplay() override { Log("ShowDisplay"); }
  void InvalidateControllerConnections() override {
    Log("InvalidateControllerConnections");
  }

  Bytes stored;
  std::array<bool, kRoomActorSlots> occupied{};
  std::vector<std::uint8_t> remembered;
  CurrentRoom destination{.zone_id = 5, .room_id = 2};
  std::optional<CurrentRoom> current_room;
  int loaded_scene = 3;
  Actors::RoomEnemyMemory memory;
  int dimensions = (3 << 20) | 0x40;
  std::optional<ZoneBounds> camera_bounds;
  ZoneTileExtent extent;
  ZoneBounds bounds;
  std::size_t loot_bytes = 0;
  bool room_events = true;
  VS::Event::RoomEventLoad event_result =
      VS::Event::RoomEventLoadResult::kReady;
  std::vector<std::size_t> event_sizes;
  bool first_entry = false;
  int entry_mode = kNewGameEntryMode;
  int cleared_statuses = 0;
  int transition_effect = 3;
  int camera_distance = kNearEntryCameraDistance;
  RoomEntryCamera camera;
  bool resume_pending = false;
  bool resume_succeeds = true;
  bool event_triggered = false;
  int music_slot = 6;
};

// Up to `count` log entries from the first `call`.
[[nodiscard]] std::vector<std::string> LogFrom(
    const std::vector<std::string>& log, std::string_view call,
    std::size_t count) {
  const auto at = std::ranges::find(log, call);
  const auto available = static_cast<std::size_t>(std::distance(at, log.end()));
  return std::vector<std::string>(
      at, at + static_cast<std::ptrdiff_t>(std::min(count, available)));
}

TEST_CASE("A scene change resets the scene timer only when it resets per scene",
          "[field][field-room-loading]") {
  RoomLoadRecorder scene;
  scene.timer_scope = SceneTimerScope::kScene;
  BeginSceneActorTransition(scene);
  CHECK(scene.log ==
        std::vector<std::string>{"ResetSceneTimer", "ClearRoomEnemyMemory"});
  for (const SceneTimerScope scope :
       {SceneTimerScope::kRoom, SceneTimerScope::kZone,
        SceneTimerScope::kNone}) {
    RoomLoadRecorder other;
    other.timer_scope = scope;
    BeginSceneActorTransition(other);
    CHECK(other.log == std::vector<std::string>{"ClearRoomEnemyMemory"});
  }
}

TEST_CASE("Queuing a room load reports a held read",
          "[field][field-room-loading]") {
  RoomLoadRecorder host;
  host.read_pending = true;
  host.data_held = true;
  CHECK(QueueRoomLoad(host, 3));
  CHECK(host.diagnostics == std::vector{RoomLoadDiagnostic::kRoomReadPending,
                                        RoomLoadDiagnostic::kRoomDataHeld});
  constexpr std::array kRoom3{Extent{.sector = 2003, .bytes = 4096}};
  CHECK(SameExtents(host.reads, kRoom3));
}

TEST_CASE("Queuing a room load refuses an unknown room",
          "[field][field-room-loading]") {
  RoomLoadRecorder unknown;
  CHECK_FALSE(QueueRoomLoad(unknown, 4));
  CHECK_FALSE(QueueRoomLoad(unknown, -1));
  CHECK(unknown.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kRoomFileId, .value = 4},
            {.refusal = RoomLoadRefusal::kRoomFileId, .value = -1}});
  CHECK(unknown.reads.empty());
  CHECK(unknown.diagnostics.empty());
}

TEST_CASE("Finishing a room load enters the room and starts its play",
          "[field][field-room-loading]") {
  RoomLoadRecorder host;
  FinishRoomLoad(host, 2);
  CHECK(host.log ==
        std::vector<std::string>{"ResetGameplayInspection",
                                 "BeginRead",
                                 "ReleaseRead",
                                 "AllocatePrimitiveBuffers",
                                 "ClearRoomTint",
                                 "StoreRoomSections",
                                 "InitializeRoomSystems",
                                 "SetCurrentRoom",
                                 "SetLoadedSceneId 17",
                                 "ClearRoomEnemyMemory",
                                 "ResetSceneTimer",
                                 "MarkRoomVisitPending",
                                 "ClearRoomResetWords",
                                 "CaptureBackgroundColors",
                                 "LoadChestLoot",
                                 "LoadRoomEvents",
                                 "StorePlacements",
                                 "FreeLoadedData",
                                 "StoreRoomEnemyMemory",
                                 "PlaceAshley",
                                 "SettleAshley",
                                 "SpawnEnemy 2",
                                 "FreeCachedZud",
                                 "SnapActorsToSupport",
                                 "ResetRoomEntryState",
                                 "SetScreenTransition 0 clear",
                                 "FrameCameraOnAshley",
                                 "EnterExploration",
                                 "PlayRoomMusic 6",
                                 "SetScreenTransition 4",
                                 "ShowDisplay",
                                 "InvalidateControllerConnections"});
}

TEST_CASE("Finishing a room load reads the room and lays out the zone",
          "[field][field-room-loading]") {
  RoomLoadRecorder host;
  FinishRoomLoad(host, 2);
  constexpr std::array kRoom2{Extent{.sector = 2002, .bytes = 4096}};
  CHECK(SameExtents(host.reads, kRoom2));
  CHECK(host.refusals.empty());
  CHECK(host.diagnostics.empty());
  const CurrentRoom& current_room = Required(host.current_room);
  CHECK(current_room.zone_id == 5);
  CHECK(current_room.room_id == 2);
  CHECK(host.loaded_scene == 17);
  CHECK(host.room == RoomDisplay{.loaded = true, .owner_room_index = 0});
  CHECK(host.extent == ZoneTileExtent{.columns = 4, .rows = 3});
  CHECK(host.bounds ==
        ZoneBounds{.min_x = 0, .min_z = 0, .max_x = 4, .max_z = 3});
}

TEST_CASE("Finishing a room load stores its loot, events and enemies",
          "[field][field-room-loading]") {
  RoomLoadRecorder host;
  FinishRoomLoad(host, 2);
  CHECK(host.loot_bytes == 8);
  CHECK(host.event_sizes == std::vector<std::size_t>{4, 8, 4});
  CHECK(host.first_entry);
  CHECK(host.entry_mode == kResumedNewGameEntryMode);
  CHECK(host.stored == StandardRoom().enemies);
  CHECK(host.remembered == std::vector<std::uint8_t>{0});
}

TEST_CASE("Finishing a room load remembers the room and frames the camera",
          "[field][field-room-loading]") {
  RoomLoadRecorder host;
  FinishRoomLoad(host, 2);
  CHECK(host.memory.current_room == 0);
  CHECK(host.memory.rooms[0].zone == 5);
  CHECK(host.memory.rooms[0].map == 2);
  CHECK(host.cleared_statuses == kRoomEntryClearedStatuses);
  CHECK(host.camera.pitch == kDefaultEntryCameraPitch);
  CHECK(host.camera.mode == kNearEntryCameraMode);
}

TEST_CASE("A refused room id loads nothing", "[field][field-room-loading]") {
  for (const int room_id : {4, -1}) {
    RoomLoadRecorder unknown;
    FinishRoomLoad(unknown, room_id);
    CHECK(unknown.refusals ==
          std::vector<Refusal>{
              {.refusal = RoomLoadRefusal::kRoomFileId, .value = room_id}});
    CHECK(unknown.log == std::vector<std::string>{"ResetGameplayInspection",
                                                  "AllocatePrimitiveBuffers"});
  }
}

TEST_CASE("A failed read or malformed room file loads nothing",
          "[field][field-room-loading]") {
  RoomLoadRecorder failed;
  failed.read_result = DiscReadState::kFailed;
  FinishRoomLoad(failed, 2);
  CHECK(failed.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kRoomReadFailed, .value = 0}});
  CHECK(failed.log == std::vector<std::string>{
                          "ResetGameplayInspection", "BeginRead", "ReleaseRead",
                          "FreeLoadedData", "AllocatePrimitiveBuffers"});

  RoomLoadRecorder malformed;
  malformed.file = Filled(16, 0);
  FinishRoomLoad(malformed, 2);
  CHECK(malformed.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kMalformedRoomFile, .value = 2}});
  CHECK(malformed.log == std::vector<std::string>{"ResetGameplayInspection",
                                                  "BeginRead", "ReleaseRead",
                                                  "AllocatePrimitiveBuffers",
                                                  "FreeLoadedData"});
  CHECK_FALSE(malformed.room.loaded);
}

TEST_CASE("A room without an environment section keeps the scene",
          "[field][field-room-loading]") {
  RoomLoadRecorder no_environment;
  RoomParts parts = StandardRoom();
  parts.sections = SectionBlock({});
  no_environment.file = RoomFileBytes(parts);
  FinishRoomLoad(no_environment, 2);
  CHECK(no_environment.refusals ==
        std::vector<Refusal>{
            {.refusal = RoomLoadRefusal::kNoEnvironment, .value = 2}});
  CHECK_FALSE(no_environment.Called("StoreRoomSections"));
  CHECK_FALSE(no_environment.Called("ClearRoomEnemyMemory"));
  CHECK(no_environment.loaded_scene == 3);
  CHECK(no_environment.Called("SetCurrentRoom"));
  CHECK(no_environment.Called("InvalidateControllerConnections"));
}

TEST_CASE("A room's scene decides whether the room enemy memory clears",
          "[field][field-room-loading]") {
  RoomLoadRecorder same_scene;
  same_scene.loaded_scene = 17;
  FinishRoomLoad(same_scene, 2);
  CHECK_FALSE(same_scene.Called("SetLoadedSceneId 17"));
  CHECK_FALSE(same_scene.Called("ClearRoomEnemyMemory"));
  CHECK(same_scene.Called("ResetSceneTimer"));

  RoomLoadRecorder per_scene;
  per_scene.timer_scope = SceneTimerScope::kScene;
  FinishRoomLoad(per_scene, 2);
  CHECK(LogFrom(per_scene.log, "SetLoadedSceneId 17", 4) ==
        std::vector<std::string>{"SetLoadedSceneId 17", "ResetSceneTimer",
                                 "ClearRoomEnemyMemory",
                                 "MarkRoomVisitPending"});
  CHECK(std::ranges::count(per_scene.log, "ResetSceneTimer") == 1);

  RoomLoadRecorder per_zone;
  per_zone.timer_scope = SceneTimerScope::kZone;
  FinishRoomLoad(per_zone, 2);
  CHECK_FALSE(per_zone.Called("ResetSceneTimer"));
}

TEST_CASE("A remembered room hides the enemies it remembers hidden",
          "[field][field-room-loading]") {
  RoomLoadRecorder host;
  host.memory.rooms[1].zone = 5;
  host.memory.rooms[1].map = 2;
  host.memory.rooms[1].recency = 3;
  Remember(host.memory.rooms[1].enemies[0], 1, 1);
  FinishRoomLoad(host, 2);
  CHECK(LogFrom(host.log, "StoreRoomEnemyMemory", 3) ==
        std::vector<std::string>{"StoreRoomEnemyMemory", "SetPlacementHidden 0",
                                 "PlaceAshley"});
  CHECK(Get<std::uint8_t>(host.stored, 2) == 1);
  CHECK_FALSE(host.Called("SpawnEnemy 2"));
  CHECK(host.remembered == std::vector<std::uint8_t>{1});
  CHECK(host.memory.current_room == 1);
  CHECK(host.memory.rooms[1].recency == 4);
}

TEST_CASE("A room's started or triggered event runs in place of exploring",
          "[field][field-room-loading]") {
  RoomLoadRecorder started;
  started.event_result = VS::Event::RoomEventLoadResult::kContinuingEvent;
  FinishRoomLoad(started, 2);
  CHECK(LogFrom(started.log, "FrameCameraOnAshley", 9) ==
        std::vector<std::string>{
            "FrameCameraOnAshley", "InitializeWeather event",
            "PrepareCutsceneActors", "InvalidateControllerConnections"});

  RoomLoadRecorder ready;
  ready.event_result = VS::Event::RoomEventLoadResult::kReady;
  FinishRoomLoad(ready, 2);
  CHECK(ready.Called("EnterExploration"));

  RoomLoadRecorder triggered;
  triggered.event_triggered = true;
  FinishRoomLoad(triggered, 2);
  CHECK(LogFrom(triggered.log, "FrameCameraOnAshley", 9) ==
        std::vector<std::string>{"FrameCameraOnAshley", "RequestEventStart",
                                 "InitializeWeather event",
                                 "PrepareCutsceneActors",
                                 "InvalidateControllerConnections"});

  RoomLoadRecorder silent;
  silent.music_slot = 0;
  FinishRoomLoad(silent, 2);
  CHECK(LogFrom(silent.log, "FrameCameraOnAshley", 9) ==
        std::vector<std::string>{"FrameCameraOnAshley", "EnterExploration",
                                 "SetScreenTransition 4", "ShowDisplay",
                                 "InvalidateControllerConnections"});
}

void CheckNoRoomActorStartup(const RoomLoadRecorder& host) {
  CHECK_FALSE(host.Called("PrepareCutsceneActors"));
  CHECK_FALSE(host.Called("PlaceAshley"));
  CHECK_FALSE(host.Called("StorePlacements"));
}

TEST_CASE("A refused room event load cannot start room play",
          "[field][field-room-loading][room-event-failure]") {
  RoomLoadRecorder host;
  host.event_result = std::unexpected(VS::Event::RoomEventLoadError{
      VS::Event::RoomEventRefusal::kAllocation, 8});
  CHECK_FALSE(FinishRoomLoad(host, 2));
  CheckNoRoomActorStartup(host);
  CHECK_FALSE(host.Called("EnterExploration"));
  CHECK_FALSE(host.Called("FrameCameraOnAshley"));
  CHECK(host.Called("FreeLoadedData"));
  CHECK(host.entry_mode == kNewGameEntryMode);
  CHECK_FALSE(host.room.loaded);
  CHECK(host.refusals ==
        std::vector<Refusal>{{RoomLoadRefusal::kRoomEventLoad, 2}});
}

TEST_CASE("A refused encounter event load cannot start room play",
          "[field][field-room-loading][room-event-failure]") {
  RoomLoadRecorder host;
  host.room_events = false;
  host.event_result = std::unexpected(VS::Event::RoomEventLoadError{
      VS::Event::RoomEventRefusal::kAllocation, 8});
  CHECK_FALSE(FinishRoomLoad(host, 2));
  CHECK(host.Called("LoadEncounterRoomEvents"));
  CheckNoRoomActorStartup(host);
  CHECK(host.Called("FreeLoadedData"));
  CHECK(host.entry_mode == kNewGameEntryMode);
  CHECK_FALSE(host.room.loaded);
}

TEST_CASE("A pending native resume finishes or jumps to the title",
          "[field][field-room-loading]") {
  RoomLoadRecorder resumed;
  resumed.resume_pending = true;
  FinishRoomLoad(resumed, 2);
  CHECK(LogFrom(resumed.log, "FrameCameraOnAshley", 3) ==
        std::vector<std::string>{"FrameCameraOnAshley", "FinishNativeResume",
                                 "EnterExploration"});
  CHECK(resumed.Called("InvalidateControllerConnections"));

  RoomLoadRecorder refused;
  refused.resume_pending = true;
  refused.resume_succeeds = false;
  FinishRoomLoad(refused, 2);
  CHECK(LogFrom(refused.log, "FrameCameraOnAshley", 9) ==
        std::vector<std::string>{"FrameCameraOnAshley", "FinishNativeResume",
                                 "JumpToTitle"});
}

TEST_CASE("A covered room entry fades in under the host's camera bounds",
          "[field][field-room-loading]") {
  RoomLoadRecorder covered;
  covered.transition_effect = 1;
  covered.destination = {.zone_id = 27, .room_id = 1};
  covered.camera_distance = 0x500;
  covered.camera_bounds =
      ZoneBounds{.min_x = 1, .min_z = 2, .max_x = 3, .max_z = 4};
  FinishRoomLoad(covered, 2);
  CHECK(LogFrom(covered.log, "ResetRoomEntryState", 3) ==
        std::vector<std::string>{"ResetRoomEntryState", "SetScreenTransition 3",
                                 "FrameCameraOnAshley"});
  CHECK(covered.camera.pitch == kRaisedEntryCameraPitch);
  CHECK(covered.camera.mode == kFarEntryCameraMode);
  CHECK(covered.extent == ZoneTileExtent{.columns = 4, .rows = 3});
  CHECK(covered.bounds ==
        ZoneBounds{.min_x = 1, .min_z = 2, .max_x = 3, .max_z = 4});
}

TEST_CASE("An encounter room loads the encounter's events",
          "[field][field-room-loading]") {
  RoomLoadRecorder encounter;
  encounter.room_events = false;
  FinishRoomLoad(encounter, 2);
  CHECK(LogFrom(encounter.log, "LoadChestLoot", 3) ==
        std::vector<std::string>{"LoadChestLoot", "LoadEncounterRoomEvents",
                                 "StorePlacements"});
  CHECK(encounter.event_sizes.empty());
  CHECK(encounter.entry_mode == kResumedNewGameEntryMode);
}

TEST_CASE("A continued room entry without loot loads no chest loot",
          "[field][field-room-loading]") {
  RoomLoadRecorder continued;
  continued.entry_mode = 2;
  RoomParts no_loot = StandardRoom();
  no_loot.loot.clear();
  continued.file = RoomFileBytes(no_loot);
  FinishRoomLoad(continued, 2);
  CHECK_FALSE(continued.first_entry);
  CHECK(continued.entry_mode == 2);
  CHECK_FALSE(continued.Called("LoadChestLoot"));
}

TEST_CASE("A room load waits for an already pending read",
          "[field][field-room-loading]") {
  RoomLoadRecorder pending;
  pending.read_pending = true;
  pending.pending_polls = 2;
  FinishRoomLoad(pending, 2);
  CHECK(pending.reads.empty());
  CHECK(pending.loading_updates == 2);
  CHECK(LogFrom(pending.log, "ResetGameplayInspection", 3) ==
        std::vector<std::string>{"ResetGameplayInspection", "ReleaseRead",
                                 "AllocatePrimitiveBuffers"});
}

class UnloadRecorder : public SectionStore<Recording<RoomUnloadHost>> {
 public:
  UnloadRecorder() {
    room = {.loaded = true, .displayed = true, .owner_room_index = 0};
  }

  void ResetGameplayInspection() override { Log("ResetGameplayInspection"); }
  void DrawTransitionFrame() override { Log("DrawTransitionFrame"); }
  void UpdateWhileLoading() override { Log("UpdateWhileLoading"); }
  void FreeRoomEventSections() override { Log("FreeRoomEventSections"); }
  void MarkCurrentRoomVisited() override { Log("MarkCurrentRoomVisited"); }
  void FreePlacements() override { Log("FreePlacements"); }
  void FreeScriptedActorAi() override { Log("FreeScriptedActorAi"); }
  void ResetRoomActorState() override { Log("ResetRoomActorState"); }
  void FreePrimitiveBuffers() override { Log("FreePrimitiveBuffers"); }
  void RemoveActor(int slot) override {
    Log("RemoveActor " + std::to_string(slot));
  }
};

class RemovalRecorder : public RoomActorRemovalHost {
 public:
  void RemoveActor(int slot) override { removed.push_back(slot); }

  std::vector<int> removed;
};

TEST_CASE("Unloading the room frees it and removes the room's actors",
          "[field][field-room-loading]") {
  UnloadRecorder host;
  UnloadCurrentRoom(host);
  std::vector<std::string> expected{
      "ResetGameplayInspection", "DrawTransitionFrame",
      "DrawTransitionFrame",     "UpdateWhileLoading",
      "FreeRoomEventSections",   "MarkCurrentRoomVisited",
      "ReleaseRoomResources",    "FreeRoomSections",
      "FreePlacements",          "FreeScriptedActorAi"};
  for (int slot = kFirstRoomActorSlot; slot < kRoomActorSlots; ++slot) {
    expected.push_back("RemoveActor " + std::to_string(slot));
  }
  expected.emplace_back("ResetRoomActorState");
  expected.emplace_back("FreePrimitiveBuffers");
  CHECK(host.log == expected);
  CHECK(host.room == RoomDisplay{});
}

TEST_CASE("Unloading keeps another room record's sections loaded",
          "[field][field-room-loading]") {
  UnloadRecorder other;
  other.room.owner_room_index = 1;
  UnloadCurrentRoom(other);
  CHECK_FALSE(other.Called("ReleaseRoomResources"));
  CHECK_FALSE(other.Called("FreeRoomSections"));
  CHECK(other.room.loaded);
  CHECK(other.Called("FreePrimitiveBuffers"));
}

TEST_CASE("Removing the room's actors removes every room actor slot",
          "[field][field-room-loading]") {
  RemovalRecorder removal;
  RemoveRoomActors(removal);
  CHECK(removal.removed ==
        std::vector{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
}
}  // namespace
}  // namespace VS::Field
