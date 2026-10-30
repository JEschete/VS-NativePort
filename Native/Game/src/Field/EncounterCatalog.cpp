#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <format>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Battle/Encounter.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Field/EncounterSpawnProfile.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorLoading.h"
#include "Actors/CharacterInit.h"
#include "Actors/Spawning.h"
#include "Audio/SoundBanks.h"
#include "Field/CollisionSampling.h"
#include "Field/Mpd.h"
#include "Field/Znd.h"
}

namespace VS::Game {
namespace {
using Bytes = std::span<const std::uint8_t>;
constexpr int kFirstEnemySlot = 2;
constexpr int kEnemySlotEnd = 10;
constexpr int kSecondarySlotOffset = 4;
constexpr std::size_t kRoomSectionCount = 24;

struct SpawnProfile {
  std::string asset;
  int index;
  Field_MpdEnemySpawn placement;
};

template <typename Offset, typename Length>
Bytes Section(Bytes bytes, Offset offset, Length length) {
  if (!std::in_range<std::size_t>(offset) ||
      !std::in_range<std::size_t>(length) ||
      static_cast<std::size_t>(offset) > bytes.size() ||
      static_cast<std::size_t>(length) >
          bytes.size() - static_cast<std::size_t>(offset)) {
    throw std::runtime_error("content section is outside its file");
  }
  return bytes.subspan(static_cast<std::size_t>(offset),
                       static_cast<std::size_t>(length));
}

template <typename T>
T Read(Bytes bytes, std::size_t offset = 0) {
  T value{};
  const auto source = Section(bytes, offset, sizeof(T));
  std::memcpy(&value, source.data(), sizeof(T));
  return value;
}

std::string EnemyName(std::span<const char> encoded) {
  constexpr unsigned char kEnd = 0xE7;
  constexpr unsigned char kSpacing = 0xFA;
  constexpr unsigned char kFont = 0xFB;
  constexpr unsigned char kPadding = 0xE5;
  constexpr unsigned char kSpace = 0x8F;
  constexpr unsigned char kUpperStart = 10;
  constexpr unsigned char kLowerStart = 36;
  constexpr unsigned char kAlphabetEnd = 62;
  std::string result;
  for (std::size_t index = 0; index < encoded.size(); ++index) {
    const auto glyph = static_cast<unsigned char>(encoded[index]);
    if (glyph == kEnd) break;
    if (glyph == kSpacing || glyph == kFont) {
      if (++index == encoded.size()) break;
      if (glyph == kSpacing) result += ' ';
      continue;
    }
    if (glyph == kPadding) continue;
    if (glyph < kUpperStart) {
      result += static_cast<char>('0' + glyph);
    } else if (glyph < kLowerStart) {
      result += static_cast<char>('A' + glyph - kUpperStart);
    } else if (glyph < kAlphabetEnd) {
      result += static_cast<char>('a' + glyph - kLowerStart);
    } else if (glyph == kSpace) {
      result += ' ';
    } else {
      result += '?';
    }
  }
  return result;
}

std::vector<EncounterDoor> ReadRoomDoors(
    Bytes bytes, std::uint64_t offset,
    const std::array<std::uint32_t, kRoomSectionCount>& sections) {
  constexpr std::size_t kDoorsSection = 3;
  for (std::size_t i = 0; i < kDoorsSection; ++i) offset += sections[i];
  const auto data = Section(bytes, offset, sections[kDoorsSection]);
  if (data.size() % sizeof(Field_MpdDoorRecord) != 0) {
    throw std::runtime_error("invalid room door table stride");
  }
  std::vector<EncounterDoor> doors;
  for (std::size_t i = 0; i < data.size() / sizeof(Field_MpdDoorRecord); ++i) {
    const auto record =
        Read<Field_MpdDoorRecord>(data, i * sizeof(Field_MpdDoorRecord));
    doors.push_back(
        {static_cast<int>(i),
         record.destinationZoneId,
         record.destinationRoomId,
         record.tileLocation.tileX,
         record.tileLocation.tileZ,
         record.tileLocation.floorLayer,
         {record.entryTileX, record.entryTileZ, record.entryElevation,
          record.tileLocation.entryDirection},
         record.requiredFlagId,
         record.requiredFlagValue,
         record.doorId,
         record.hasEntryStep != 0});
  }
  return doors;
}

EncounterRoom ReadRoom(const Content::DataStore& store, int index,
                       std::uint32_t sector,
                       std::vector<SpawnProfile>& profiles) {
  EncounterRoom room;
  room.index = index;
  const auto* entry = store.FindBySector(sector);
  if (entry == nullptr || entry->lba != sector ||
      !entry->path.ends_with(".MPD")) {
    room.unavailable_reason = "room asset is missing";
    return room;
  }
  room.asset = entry->path;
  const auto bytes = store.ReadAll(*entry);
  if (!bytes) {
    room.unavailable_reason = bytes.error();
    return room;
  }
  try {
    // _loadMpdRoomSection, 146C.c:15663: 24 lengths, then owned sections.
    const auto header = Read<Field_MpdHeader>(*bytes);
    const auto sections = Read<std::array<std::uint32_t, kRoomSectionCount>>(
        *bytes, header.roomSectionOffset);
    std::uint64_t total = sizeof(sections);
    for (const auto length : sections) total += length;
    Section(*bytes, header.roomSectionOffset, static_cast<std::int64_t>(total));
    room.doors = ReadRoomDoors(
        *bytes, header.roomSectionOffset + sizeof(sections), sections);
    if (sections[1] < sizeof(Field_MpdCollisionHeader)) {
      throw std::runtime_error("room has no collision grid");
    }
    const auto collision = Read<Field_MpdCollisionHeader>(
        *bytes, header.roomSectionOffset + sizeof(sections) + sections[0]);
    room.width = collision.width;
    room.depth = collision.height;
    if (room.width <= 0 || room.depth <= 0 ||
        room.width > EncounterLimits::kTiles ||
        room.depth > EncounterLimits::kTiles) {
      throw std::runtime_error("unsupported room grid dimensions");
    }
    room.entrance.x = room.width / 2;
    room.entrance.z = room.depth / 2;
    const auto collision_offset =
        header.roomSectionOffset + sizeof(sections) + sections[0];
    const auto floor = ReadEncounterFloor(
        Section(*bytes, collision_offset, sections[1]),
        Section(*bytes, collision_offset + sections[1], sections[2]));
    if (!floor) throw std::runtime_error(floor.error());
    room.floor = *floor;
    const auto spawns =
        Section(*bytes, header.enemySectionOffset, header.enemySectionSize);
    if (spawns.size() % sizeof(Field_MpdEnemySpawn) != 0) {
      throw std::runtime_error("invalid room enemy table stride");
    }
    for (std::size_t offset = 0; offset < spawns.size();
         offset += sizeof(Field_MpdEnemySpawn)) {
      const auto spawn = Read<Field_MpdEnemySpawn>(spawns, offset);
      if (IsEncounterHostileSpawn(spawn)) {
        profiles.push_back(
            {room.asset, static_cast<int>(offset / sizeof(Field_MpdEnemySpawn)),
             spawn});
      }
    }
  } catch (const std::exception& error) {
    room.unavailable_reason = error.what();
  }
  return room;
}

EncounterZone ReadZone(const Content::DataStore& store,
                       const Content::DataStore::Entry& entry, int id) {
  EncounterZone zone;
  zone.id = id;
  zone.asset = entry.path;
  std::vector<SpawnProfile> profiles;
  const auto bytes = store.ReadAll(entry);
  if (!bytes) throw std::runtime_error(bytes.error());
  const auto header = Read<Field_ZndHeader>(*bytes);
  zone.music = header.musicId;
  const auto rooms = Section(*bytes, header.mpdListOffset, header.mpdListSize);
  constexpr std::size_t kFileRecordBytes = 8;
  if (rooms.size() % kFileRecordBytes != 0) {
    throw std::runtime_error("invalid zone room table stride");
  }
  for (std::size_t offset = 0; offset < rooms.size();
       offset += kFileRecordBytes) {
    zone.rooms.push_back(
        ReadRoom(store, static_cast<int>(offset / kFileRecordBytes),
                 Read<std::uint32_t>(rooms, offset), profiles));
  }
  // Preserve the profile choice for already supported enemies; retained boss
  // records are a fallback when no ordinary hostile placement exists.
  std::stable_partition(profiles.begin(), profiles.end(), [](const auto& p) {
    return p.placement.actorStateByte == 0;
  });
  if (header.enemiesSize == 0) return zone;
  const auto enemies =
      Section(*bytes, header.enemiesOffset, header.enemiesSize);
  const auto count = Read<std::uint32_t>(enemies);
  const std::uint64_t records_offset =
      sizeof(count) + count * std::uint64_t{kFileRecordBytes};
  Section(enemies, records_offset,
          count * std::uint64_t{sizeof(Actors_CharacterInit)});
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto definition = Read<Actors_CharacterInit>(
        enemies, static_cast<std::size_t>(records_offset) +
                     index * sizeof(Actors_CharacterInit));
    EncounterEnemy enemy;
    enemy.index = static_cast<int>(index);
    enemy.name = EnemyName(definition.name);
    enemy.hp = definition.hp;
    enemy.mp = definition.mp;
    const auto kind = static_cast<unsigned char>(definition.characterKind);
    if (index >= EncounterLimits::kEnemyIdEnd || definition.hp <= 0) {
      enemy.unavailable_reason = "not a supported combat actor definition";
    } else {
      enemy.secondary_model = Actors_usesSecondaryBodyModel[kind] != 0;
    }
    const auto* asset = store.FindBySector(
        Read<std::uint32_t>(enemies, sizeof(count) + index * kFileRecordBytes));
    if (asset == nullptr || !asset->path.ends_with(".ZUD")) {
      enemy.unavailable_reason = "enemy model archive is missing";
    }
    if (definition.zudIndex < 0 ||
        static_cast<std::uint32_t>(definition.zudIndex) >= count) {
      enemy.unavailable_reason = "enemy model index is outside its zone";
    }
    const auto profile = std::ranges::find_if(profiles, [index](const auto& p) {
      return p.placement.enemyId == index;
    });
    if (profile == profiles.end()) {
      if (enemy.unavailable_reason.empty()) {
        enemy.unavailable_reason = "no hostile spawn record in this zone";
      }
    } else {
      enemy.spawn_asset = profile->asset;
      enemy.spawn_index = profile->index;
    }
    // func_800E5A9C / func_800E7660: these subtypes require the special arena
    // orbit state, whose initialization is not implemented by the native port.
    constexpr std::uint16_t kFirstOrbitingSubtype = 172;
    constexpr std::uint16_t kLastOrbitingSubtype = 173;
    if (definition.subType >= kFirstOrbitingSubtype &&
        definition.subType <= kLastOrbitingSubtype) {
      enemy.unavailable_reason = "requires unsupported orbiting boss arena";
    }
    zone.enemies.push_back(std::move(enemy));
  }
  return zone;
}

EncounterFloor ClassifyFloor(Bytes collision, const Field_MpdTileCell& tile,
                             int x, int z) {
  constexpr int kSamplesPerTile = 4;
  constexpr int kInvalidHeight = 0x80;
  // func_800A8D64 (40564.c:139) checks passability; func_800A6EE8
  // (3A1A0.c:2175) handles pits. The lab conservatively requires full tiles,
  // excluding diagonal partial cells and dynamic platform/gimmick support.
  if (tile.doorPassability != 0) return EncounterFloor::kBlocked;
  if (tile.isPit != 0) return EncounterFloor::kPit;
  for (int sample_z = 0; sample_z < kSamplesPerTile; ++sample_z) {
    for (int sample_x = 0; sample_x < kSamplesPerTile; ++sample_x) {
      const auto code = Field_SampleCollisionHeightCodeFromData(
          collision.data(), collision.size(), x * kSamplesPerTile + sample_x,
          z * kSamplesPerTile + sample_z, 0);
      if ((code & kInvalidHeight) != 0) return EncounterFloor::kNoFloor;
    }
  }
  return EncounterFloor::kSupported;
}
}  // namespace

std::expected<std::vector<EncounterFloor>, std::string> ReadEncounterFloor(
    Bytes collision, Bytes tile_cells) {
  constexpr int kCollisionLayers = 2;
  try {
    auto header = Read<Field_MpdCollisionHeader>(collision);
    if (header.width <= 0 || header.height <= 0 ||
        header.width > EncounterLimits::kTiles ||
        header.height > EncounterLimits::kTiles) {
      return std::unexpected("unsupported collision grid dimensions");
    }
    const auto count = std::size_t{static_cast<unsigned>(header.width)} *
                       static_cast<unsigned>(header.height);
    Section(collision, sizeof(header),
            count * kCollisionLayers * sizeof(std::uint16_t));
    Section(tile_cells, 0, count * sizeof(Field_MpdTileCell));
    // _loadMpdRoomSection (146C.c:15663) replaces the on-disk profile count
    // with width*height before the production sampler reads descriptor tables.
    std::vector<std::uint8_t> samples(collision.begin(), collision.end());
    header.cellCount = static_cast<std::int16_t>(count);
    std::memcpy(samples.data(), &header, sizeof(header));
    std::vector<EncounterFloor> floor(count, EncounterFloor::kSupported);
    for (int z = 0; z < header.height; ++z) {
      for (int x = 0; x < header.width; ++x) {
        const auto index = static_cast<std::size_t>(z) * header.width + x;
        const auto tile = Read<Field_MpdTileCell>(
            tile_cells, index * sizeof(Field_MpdTileCell));
        floor[index] = ClassifyFloor(samples, tile, x, z);
      }
    }
    return floor;
  } catch (const std::exception& error) {
    return std::unexpected(error.what());
  }
}

bool IsEncounterHostileSpawn(const Field_MpdEnemySpawn& source) {
  // Field_SpawnRoomActors (146C.c:14901): facing 2 selects hostile AI;
  // actorStateByte instead controls retention after defeat, not activation.
  return source.kind == 0 && source.facing == 2;
}

bool UsesConfiguredEncounterPlacements(const EncounterSetup& setup, int zone,
                                       int room, bool external_event_started) {
  return setup.room_policy == EncounterRoomPolicy::kIsolatedCombat &&
         setup.zone == zone && setup.room == room &&
         !(setup.external_event && external_event_started);
}

bool UsesEncounterRoomEvents(const EncounterSetup& setup,
                             bool external_event_started) {
  return setup.room_policy == EncounterRoomPolicy::kOriginalRoom ||
         (setup.external_event && external_event_started);
}

std::expected<Field_MpdEnemySpawn, std::string> ReadEncounterSpawnProfile(
    const EncounterEnemy& enemy) {
  const auto* store = Content::DataStore::Get();
  const auto* entry = store ? store->Find(enemy.spawn_asset) : nullptr;
  if (entry == nullptr || enemy.spawn_index < 0) {
    return std::unexpected("source spawn profile is missing");
  }
  const auto bytes = store->ReadAll(*entry);
  if (!bytes) return std::unexpected(bytes.error());
  try {
    const auto header = Read<Field_MpdHeader>(*bytes);
    const auto spawns =
        Section(*bytes, header.enemySectionOffset, header.enemySectionSize);
    if (spawns.size() % sizeof(Field_MpdEnemySpawn) != 0) {
      return std::unexpected("invalid room enemy table stride");
    }
    auto spawn = Read<Field_MpdEnemySpawn>(
        spawns, static_cast<std::size_t>(enemy.spawn_index) *
                    sizeof(Field_MpdEnemySpawn));
    if (!IsEncounterHostileSpawn(spawn) || spawn.enemyId != enemy.index) {
      return std::unexpected("source spawn record changed or is not hostile");
    }
    return spawn;
  } catch (const std::exception& error) {
    return std::unexpected(error.what());
  }
}

std::expected<Field_MpdEnemySpawn, std::string> PrepareEncounterSpawn(
    const Field_MpdEnemySpawn& source, const EncounterSpawn& instance,
    std::size_t index) {
  constexpr std::size_t kPrimaryCapacity = kEnemySlotEnd - kFirstEnemySlot;
  constexpr int kTileLimit = 32;
  constexpr int kElevationLimit = 4;
  if (!IsEncounterHostileSpawn(source) || source.enemyId != instance.enemy ||
      instance.enemy < 0 || instance.enemy >= EncounterLimits::kEnemyIdEnd ||
      index >= kPrimaryCapacity) {
    return std::unexpected("unsupported hostile spawn record");
  }
  const auto& position = instance.position;
  if (position.x < 0 || position.x >= kTileLimit || position.z < 0 ||
      position.z >= kTileLimit || position.elevation < 0 ||
      position.elevation >= kElevationLimit || position.direction < 0 ||
      position.direction > 1) {
    return std::unexpected("placement exceeds the production encoding");
  }
  auto placement = source;
  placement.index = static_cast<u_char>(index);
  placement.tileX = static_cast<u_char>(position.x);
  placement.tileZ = static_cast<u_char>(position.z);
  placement.height = static_cast<u_char>(position.elevation);
  placement.direction = static_cast<u_char>(position.direction);
  // Field_SpawnRoomActors (146C.c:14901) allocates slots when slot is zero.
  // Membership is explicit, not story-gated. AI, hostility, loot and variant
  // remain from the source record; no progression flags are modified.
  placement.slot = 0;
  placement.isHidden = 0;
  placement.flagTest = 0;
  placement.statTest = 0;
  placement.sceneTest = 0;
  // func_80076F24 (146C.c:6367) maps this byte to retainAfterDefeat.
  // Isolated encounters have no story death event: use ordinary production
  // defeat/removal instead, without changing the original room or its actor.
  placement.actorStateByte = 0;
  return placement;
}

std::expected<std::vector<EncounterZone>, std::string> ReadEncounterCatalog(
    bool strict) {
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) return std::unexpected("extracted data is not open");
  std::vector<EncounterZone> zones;
  for (const auto& entry : store->Entries()) {
    constexpr std::string_view kPrefix = "MAP/ZONE";
    const std::string_view path = entry.path;
    if (!path.starts_with(kPrefix) || !path.ends_with(".ZND")) continue;
    const auto number =
        path.substr(kPrefix.size(), path.size() - kPrefix.size() - 4);
    int id = 0;
    const auto parsed =
        std::from_chars(number.data(), number.data() + number.size(), id);
    if (parsed.ec != std::errc() ||
        parsed.ptr != number.data() + number.size() || id < 1 ||
        id > EncounterLimits::kLastZone) {
      continue;
    }
    try {
      zones.push_back(ReadZone(*store, entry, id));
    } catch (const std::exception& error) {
      if (strict) return std::unexpected(entry.path + ": " + error.what());
      VS_LOG_WARN("encounter.catalog", "{}: {}", entry.path, error.what());
    }
  }
  std::ranges::sort(zones, {}, &EncounterZone::id);
  if (zones.empty()) return std::unexpected("no readable room catalogs found");
  return zones;
}

bool EncounterStatOverrides::Valid() const {
  for (const auto value : {max_hp, max_mp, strength, intelligence, agility}) {
    if (value && (*value < EncounterLimits::kMinPlayerStat ||
                  *value > EncounterLimits::kMaxPlayerStat)) {
      return false;
    }
  }
  return true;
}

std::expected<Items_WeaponSpawnLoadout, std::string> ReadEncounterWeapon(
    EncounterWeapon weapon) {
  struct Source {
    int zone, actor, blade, grip, model, grip_category;
  };
  // Original ZND CharacterInit weapon records, not manufactured item stats.
  constexpr auto kSources = std::to_array<Source>({
      {9, 10, 1, 96, 1, 1},
      {1, 0, 15, 96, 15, 1},
      {13, 18, 25, 96, 28, 1},
      {9, 1, 35, 105, 35, 2},
      {15, 8, 51, 106, 55, 2},
      {24, 2, 60, 111, 60, 2},
      {12, 0, 70, 105, 68, 2},
      {13, 11, 71, 115, 71, 3},
      {9, 9, 83, 121, 83, 4},
  });
  if (weapon < EncounterWeapon::kDagger ||
      weapon > EncounterWeapon::kCrossbow) {
    return std::unexpected("weapon choice has no source equipment record");
  }
  const auto& source = kSources[static_cast<std::size_t>(weapon) - 1];
  const auto path = std::format("MAP/ZONE{:03}.ZND", source.zone);
  const auto* store = Content::DataStore::Get();
  const auto* entry = store ? store->Find(path) : nullptr;
  if (entry == nullptr) return std::unexpected("missing weapon source " + path);
  const auto bytes = store->ReadAll(*entry);
  if (!bytes) return std::unexpected(bytes.error());
  try {
    const auto header = Read<Field_ZndHeader>(*bytes);
    const auto enemies =
        Section(*bytes, header.enemiesOffset, header.enemiesSize);
    const auto count = Read<std::uint32_t>(enemies);
    constexpr std::uint64_t kFileRecordBytes = 8;
    const auto offset = sizeof(count) + count * kFileRecordBytes;
    const auto records = Section(
        enemies, offset, count * std::uint64_t{sizeof(Actors_CharacterInit)});
    auto result = Read<Actors_CharacterInit>(
                      records, source.actor * sizeof(Actors_CharacterInit))
                      .weapon;
    constexpr unsigned kDamageTypes = 4, kCostTypes = 8, kMaterials = 8;
    if (result.blade.category != static_cast<int>(weapon) ||
        result.blade.id != source.blade || result.grip.id != source.grip ||
        result.blade.weaponModelId != source.model ||
        result.grip.category != source.grip_category ||
        static_cast<unsigned char>(result.blade.damageType) >= kDamageTypes ||
        static_cast<unsigned char>(result.blade.costType) >= kCostTypes ||
        static_cast<unsigned char>(result.material) >= kMaterials ||
        std::ranges::any_of(result.gems,
                            [](const auto& gem) { return gem.id != 0; })) {
      return std::unexpected("weapon source record changed: " + path);
    }
    VS_LOG_INFO("encounter.weapon",
                "choice={} source={} actor={} blade={} grip={} model={}",
                EncounterWeaponId(weapon), path, source.actor, source.blade,
                source.grip, source.model);
    return result;
  } catch (const std::runtime_error& error) {
    return std::unexpected(path + ": " + error.what());
  }
}

namespace {
std::vector<std::string> ValidateEncounterOptions(const EncounterSetup& setup) {
  std::vector<std::string> errors;
  if (setup.room_policy != EncounterRoomPolicy::kIsolatedCombat &&
      setup.room_policy != EncounterRoomPolicy::kOriginalRoom) {
    errors.emplace_back("unsupported encounter room policy");
  }
  if (setup.room_policy == EncounterRoomPolicy::kOriginalRoom &&
      !setup.enemies.empty()) {
    errors.emplace_back("production rooms cannot override original actors");
  }
  if (setup.external_event) {
    if (*setup.external_event < 0 ||
        *setup.external_event > EncounterLimits::kLastExternalEvent) {
      errors.emplace_back("external event must be a four-digit asset number");
    }
    if (setup.room_policy != EncounterRoomPolicy::kIsolatedCombat ||
        !setup.enemies.empty()) {
      errors.emplace_back(
          "external event components require isolated entry and no enemy "
          "overrides");
    }
  }
  if (setup.room_local_event) {
    if (*setup.room_local_event < 0 ||
        *setup.room_local_event > EncounterLimits::kLastRoomLocalEvent) {
      errors.emplace_back("room-local event selector must be in 0..15");
    }
    if (setup.room_policy != EncounterRoomPolicy::kOriginalRoom ||
        setup.external_event) {
      errors.emplace_back(
          "room-local events require production room events and no external "
          "event");
    }
  }
  if (setup.inherited_music &&
      (*setup.inherited_music < 1 ||
       *setup.inherited_music > EncounterLimits::kLastMusic ||
       Audio_waveBankFileMap[*setup.inherited_music] == 0)) {
    errors.emplace_back("inherited music has no mapped wave bank");
  }
  if (!setup.stat_overrides.Valid()) {
    errors.emplace_back("ASHLEY STAT OVERRIDES MUST BE IN 1..999");
  }
  if (setup.profile < EncounterProfile::kPrologue ||
      setup.profile >= EncounterProfile::kCount) {
    errors.emplace_back("unsupported Ashley profile");
  }
  if (setup.weapon < EncounterWeapon::kDefault ||
      setup.weapon >= EncounterWeapon::kCount) {
    errors.emplace_back("unsupported Ashley weapon");
  }
  return errors;
}
}  // namespace

std::vector<std::string> ValidateEncounter(const EncounterSetup& setup,
                                           const EncounterZone& zone) {
  auto errors = ValidateEncounterOptions(setup);
  if (setup.zone != zone.id) errors.emplace_back("zone does not match catalog");
  if (setup.inherited_music && zone.music != 0) {
    errors.emplace_back(
        "inherited music requires a zone that keeps the previous wave bank");
  }
  const auto room =
      std::ranges::find(zone.rooms, setup.room, &EncounterRoom::index);
  if (room == zone.rooms.end()) return {"room is not in the selected zone"};
  if (!room->unavailable_reason.empty()) {
    errors.push_back(room->unavailable_reason);
  }
  std::set<std::tuple<int, int, int>> positions;
  const auto check_position = [&](const EncounterPosition& position,
                                  std::string_view actor) {
    if (position.x < 0 || position.x >= room->width || position.z < 0 ||
        position.z >= room->depth || position.elevation < 0 ||
        position.elevation >= EncounterLimits::kElevations ||
        position.direction < 0 || position.direction > 1) {
      errors.emplace_back("placement is outside supported room coordinates");
      return;
    }
    if (!positions.emplace(position.x, position.z, position.elevation).second) {
      errors.emplace_back("two actors occupy the same starting tile");
    }
    const auto index =
        static_cast<std::size_t>(position.z) * room->width + position.x;
    std::string_view reason;
    if (index >= room->floor.size()) {
      reason = "FLOOR DATA UNAVAILABLE";
    } else {
      switch (room->floor[index]) {
        case EncounterFloor::kSupported:
          break;
        case EncounterFloor::kBlocked:
          reason = "BLOCKED OR PARTIAL TILE";
          break;
        case EncounterFloor::kPit:
          reason = "PIT TILE";
          break;
        case EncounterFloor::kNoFloor:
          reason = "NO COMPLETE FLOOR SUPPORT";
          break;
      }
    }
    if (!reason.empty()) {
      errors.push_back(std::format("{} TILE ({},{}): {}", actor, position.x,
                                   position.z, reason));
    }
  };
  check_position(setup.player, "ASHLEY");
  std::array<bool, EncounterLimits::kActorSlots> slots{};
  for (std::size_t index = 0; index < setup.enemies.size(); ++index) {
    const auto& spawn = setup.enemies[index];
    check_position(spawn.position, std::format("ENEMY {}", index + 1));
    const auto enemy =
        std::ranges::find(zone.enemies, spawn.enemy, &EncounterEnemy::index);
    if (enemy == zone.enemies.end()) {
      errors.emplace_back("enemy is not in the selected zone");
      continue;
    }
    if (!enemy->unavailable_reason.empty()) {
      errors.push_back(enemy->unavailable_reason);
    }
    // Mirrors Field_SpawnRoomActors' primary and secondary slot reservations.
    int slot = kFirstEnemySlot;
    for (; slot < kEnemySlotEnd; ++slot) {
      if (!slots[slot] &&
          (!enemy->secondary_model || !slots[slot + kSecondarySlotOffset])) {
        slots[slot] = true;
        if (enemy->secondary_model) slots[slot + kSecondarySlotOffset] = true;
        break;
      }
    }
    if (slot == kEnemySlotEnd) {
      errors.emplace_back("enemy actor slots exhausted");
    }
  }
  return errors;
}

}  // namespace VS::Game
