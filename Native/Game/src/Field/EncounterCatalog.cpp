#include <algorithm>
#include <array>
#include <charconv>
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
    const auto spawns =
        Section(*bytes, header.enemySectionOffset, header.enemySectionSize);
    if (spawns.size() % sizeof(Field_MpdEnemySpawn) != 0) {
      throw std::runtime_error("invalid room enemy table stride");
    }
    for (std::size_t offset = 0; offset < spawns.size();
         offset += sizeof(Field_MpdEnemySpawn)) {
      const auto spawn = Read<Field_MpdEnemySpawn>(spawns, offset);
      if (spawn.kind == 0 && spawn.facing == 2 && spawn.actorStateByte == 0) {
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
      enemy.unavailable_reason = "no independent hostile spawn profile";
    } else {
      enemy.spawn_asset = profile->asset;
      enemy.spawn_index = profile->index;
    }
    zone.enemies.push_back(std::move(enemy));
  }
  return zone;
}
}  // namespace

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
    if (spawn.kind != 0 || spawn.enemyId != enemy.index || spawn.facing != 2 ||
        spawn.actorStateByte != 0) {
      return std::unexpected("source spawn profile changed or needs a script");
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
  if (source.kind != 0 || source.facing != 2 || source.actorStateByte != 0 ||
      source.enemyId != instance.enemy || instance.enemy < 0 ||
      instance.enemy >= EncounterLimits::kEnemyIdEnd ||
      index >= kPrimaryCapacity) {
    return std::unexpected("unsupported independent hostile spawn profile");
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
  return placement;
}

std::expected<std::vector<EncounterZone>, std::string> ReadEncounterCatalog() {
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
      VS_LOG_WARN("encounter.catalog", "{}: {}", entry.path, error.what());
    }
  }
  std::ranges::sort(zones, {}, &EncounterZone::id);
  if (zones.empty()) return std::unexpected("no readable room catalogs found");
  return zones;
}

std::vector<std::string> ValidateEncounter(const EncounterSetup& setup,
                                           const EncounterZone& zone) {
  std::vector<std::string> errors;
  if (setup.zone != zone.id) errors.emplace_back("zone does not match catalog");
  const auto room =
      std::ranges::find(zone.rooms, setup.room, &EncounterRoom::index);
  if (room == zone.rooms.end()) return {"room is not in the selected zone"};
  if (!room->unavailable_reason.empty()) {
    errors.push_back(room->unavailable_reason);
  }
  std::set<std::tuple<int, int, int>> positions;
  const auto check_position = [&](const EncounterPosition& position) {
    if (position.x < 0 || position.x >= room->width || position.z < 0 ||
        position.z >= room->depth || position.elevation < 0 ||
        position.elevation >= EncounterLimits::kElevations ||
        position.direction < 0 || position.direction > 1) {
      errors.emplace_back("placement is outside supported room coordinates");
    }
    if (!positions.emplace(position.x, position.z, position.elevation).second) {
      errors.emplace_back("two actors occupy the same starting tile");
    }
  };
  check_position(setup.player);
  std::array<bool, EncounterLimits::kActorSlots> slots{};
  if (setup.enemies.empty()) errors.emplace_back("add at least one enemy");
  for (const auto& spawn : setup.enemies) {
    check_position(spawn.position);
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
