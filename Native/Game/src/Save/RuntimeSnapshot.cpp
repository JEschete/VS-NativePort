#include "Save/RuntimeSnapshot.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Actors/ActorRecord.h"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.h"
#include "Battle/Damage.h"
#include "Boot/GameLoop.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/RandomLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/GameModeLiveState.hpp"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/Znd.h"
#include "Input/ControllerLiveState.hpp"
#include "Items/Categories.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/Loadout.h"
#include "Items/Materials.hpp"
#include "Render/Camera.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Location.hpp"
#include "State/PlayTime.hpp"
#include "State/Progress.hpp"
#include "State/RoomMemory.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

namespace VS::Game::Save {
namespace {
using Result = std::expected<void, std::string>;
using Bytes = std::span<const std::uint8_t>;
constexpr auto kCategoryCount = std::size(VS::Items::itemCategoryUpperIds);
using CategoryBounds = std::array<unsigned, kCategoryCount>;
constexpr int kClockRadix = 60;
constexpr int kMaximumHours = 100;
constexpr std::uint64_t kTicksPerHour = 60ull * 60 * 60;
constexpr std::uint64_t kMaximumTicks = 101 * kTicksPerHour - 1;
constexpr std::size_t kSectionCount = 24;
constexpr unsigned kShieldParent = VS::Items::kShieldGemFlag;
constexpr int kLastLocation = 255;
constexpr int kLastScene = 1023;
constexpr int kLastAngle = ONE - 1;
constexpr unsigned kItemIdEnd = 512;
constexpr int kTileUnits = 128;
constexpr int kTileLimit = 32;
constexpr unsigned kNoRoomSpawn = 0x80;
constexpr unsigned kAccessoryLimb = 7;
constexpr unsigned kLearnedBitsPerByte = 8;
constexpr unsigned kLearnedFirstBit = 0x80;
constexpr int kEquipmentGems = 3;
constexpr int kGripCategoryCount = 5;
constexpr int kArmorCategoryCount = 8;
constexpr int kQuarterTurn = ONE / 4;
constexpr unsigned kActiveAnimationEnd = 6;
constexpr int kCameraLookAtHeight = 90;
constexpr int kNearCameraDistance = 3 * ONE / 8;
constexpr int kFarCameraDistance = 9 * ONE / 16;
constexpr int kFarZoomMode = 3;
constexpr int kMaximumOrbitStep = 40;
constexpr int kMaximumZoomVelocity = 64;
constexpr int kMaximumTrackingSpeed = 40;
constexpr int kWorldExtent = -std::numeric_limits<std::int16_t>::min();
constexpr int kCameraWorldExtent = kWorldExtent + kCameraLookAtHeight;
constexpr int kCameraCoordinateLimit = kCameraWorldExtent * ONE;
constexpr int kFieldNearClip = ONE / 64;
constexpr int kFieldProjection = ONE / 8;
constexpr unsigned kStereoCommand = 0x80;
constexpr unsigned kMonoCommand = 0x81;
constexpr unsigned kMonoOutput = 2;
constexpr unsigned kOutputModeMask = 3;
constexpr unsigned kClearMenuFlag = 0x20;
constexpr unsigned kPuzzleSettingShift = 3;
enum CategoryBoundary {
  kBladeEnd,
  kGripEnd,
  kArmorShieldEnd,
  kArmorAccessoryEnd,
  kGemEnd,
  kConsumableEnd,
  kKeyItemEnd,
  kMiscEnd
};

Result ItemError(std::string_view scope, std::string_view category,
                 std::size_t index, std::string_view reason) {
  return std::unexpected(
      std::format("{}.{}/slot {}: {}", scope, category, index + 1, reason));
}

struct PersistentInputs {
  Snapshot state;
  VS::State::AshleySnapshot ashley;
  Field_MpdDoorRecord entry;
  std::array<char, std::size(VS::State::learnedActionFlags)> learned;
  decltype(Snapshot::learned_actions) unlocked;
  std::uint32_t mix_behavior = 0;
  decltype(VS::State::playTime) play_time{};
};

struct PendingResume {
  Snapshot state;
  PersistentInputs previous;
  bool new_game = false;
};

std::optional<PendingResume> pending;

bool AudioConsumerReady() {
  return VS::Audio::Seq::activeMusicConfig != nullptr &&
         VS::Audio::Mixer::kCommandHandlers[kStereoCommand] != nullptr &&
         VS::Audio::Mixer::kCommandHandlers[kMonoCommand] != nullptr;
}

Result ValidateAudioConsumer() {
  if (!AudioConsumerReady()) {
    return std::unexpected("mono/stereo audio consumer is not initialized");
  }
  return {};
}

void SyncMonoOutput(bool mono) {
  const auto before = VS::Audio::Driver::controlLatches.mix_behavior;
  if (((before & kMonoOutput) != 0) == mono) return;
  // Original loaded-save apply ends with vs_main_setMonoSound. The production
  // 0x80/0x81 handlers invalidate voice volumes and update the very same latch
  // consumed by native sequencer/XA/stream output. Preserve unrelated live
  // music-stack bits; only the output mode belongs to saved settings.
  VS::Audio::LiveSetMonoOutput(mono ? 1 : 0);
  VS::Audio::Driver::controlLatches.mix_behavior |= before & ~kOutputModeMask;
}

template <typename T>
std::expected<T, std::string> Read(Bytes bytes, std::size_t offset = 0) {
  if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
    return std::unexpected("content record is outside its file");
  }
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

std::expected<Bytes, std::string> Section(Bytes bytes, std::uint64_t offset,
                                          std::uint64_t size) {
  if (offset > bytes.size() || size > bytes.size() - offset) {
    return std::unexpected("content section is outside its file");
  }
  return bytes.subspan(static_cast<std::size_t>(offset),
                       static_cast<std::size_t>(size));
}

struct RoomContent {
  std::vector<std::uint8_t> bytes;
  std::array<std::uint32_t, kSectionCount> lengths{};
  std::array<std::size_t, kSectionCount> offsets{};
  Field_MpdCollisionHeader collision{};
  Field_MpdEnvironment environment{};
  std::size_t spawn_count = 0;
};

Result ValidateCollision(Bytes bytes,
                         const Field_MpdCollisionHeader& collision) {
  // _loadMpdRoomSection (146C.c:15663) replaces cellCount with width*height.
  // section2 is a compact region/direction table, NOT width*height cells
  // (MAP212 and MAP214 deliberately have fewer region bytes).
  if (collision.width <= 0 || collision.width > kTileLimit ||
      collision.height <= 0 || collision.height > kTileLimit) {
    return std::unexpected("unsupported room collision dimensions");
  }
  const auto cells = static_cast<std::size_t>(collision.width) *
                     static_cast<std::size_t>(collision.height);
  const auto descriptor_size = cells * 2 * sizeof(std::uint16_t);
  const auto descriptors = Section(bytes, sizeof(collision), descriptor_size);
  if (!descriptors) return std::unexpected(descriptors.error());
  constexpr unsigned kPatternIndexMask = 0x7FF;
  constexpr unsigned kPatternBytes = 16;
  for (std::size_t index = 0; index < cells * 2; ++index) {
    const auto descriptor =
        Read<std::uint16_t>(*descriptors, index * sizeof(std::uint16_t));
    if (!descriptor) return std::unexpected(descriptor.error());
    const auto pattern_offset =
        sizeof(collision) + descriptor_size +
        static_cast<std::size_t>(*descriptor & kPatternIndexMask) *
            kPatternBytes;
    if (!Section(bytes, pattern_offset, kPatternBytes)) {
      return std::unexpected(
          "collision descriptor references a missing pattern");
    }
  }
  return {};
}

std::expected<const Content::DataStore::Entry*, std::string> ResolveRoomEntry(
    const Content::DataStore& store, unsigned zone, unsigned room) {
  const auto* zone_entry = store.Find(std::format("MAP/ZONE{:03}.ZND", zone));
  if (zone_entry == nullptr) return std::unexpected("zone asset is missing");
  const auto zone_bytes = store.ReadAll(*zone_entry);
  if (!zone_bytes) return std::unexpected(zone_bytes.error());
  const auto zone_header = Read<Field_ZndHeader>(*zone_bytes);
  if (!zone_header) return std::unexpected(zone_header.error());
  const auto rooms = Section(*zone_bytes, zone_header->mpdListOffset,
                             zone_header->mpdListSize);
  if (!rooms) return std::unexpected(rooms.error());
  constexpr std::size_t kFileRecordSize = 8;
  if (rooms->size() % kFileRecordSize != 0 ||
      room >= rooms->size() / kFileRecordSize) {
    return std::unexpected("room is outside the zone room table");
  }
  const auto sector = Read<std::uint32_t>(*rooms, room * kFileRecordSize);
  const auto size = Read<std::uint32_t>(
      *rooms, room * kFileRecordSize + sizeof(std::uint32_t));
  if (!sector || !size) return std::unexpected("invalid zone room record");
  const auto* entry = store.FindBySector(*sector);
  if (entry == nullptr || entry->lba != *sector ||
      !entry->path.ends_with(".MPD")) {
    return std::unexpected(
        std::format("zone {}/room {}: sector {} does not identify an MPD start",
                    zone, room, *sector));
  }
  // _loadMpd (146C.c:15254) submits the ZND's allocation-sized byte request.
  // MAP014 is 65904 file bytes but ZONE009 room5 requests 33 sectors/67584.
  // VS::Content::ReadDiscBytes zero-fills the last sector; that allocation
  // padding is not part of the MPD's defined sections, which remain
  // file-bounded below.
  const auto allocation = static_cast<std::uint64_t>(entry->SectorCount()) *
                          Content::DataStore::kDataSectorBytes;
  if (*size == 0 || entry->size == 0 ||
      (*size != entry->size && *size != allocation)) {
    return std::unexpected(std::format(
        "zone {}/room {}: {} request {} bytes does not match file {} or "
        "sector allocation {}",
        zone, room, entry->path, *size, entry->size, allocation));
  }
  return entry;
}

std::expected<RoomContent, std::string> ReadRoom(
    const Content::DataStore& store, unsigned zone, unsigned room) {
  const auto entry = ResolveRoomEntry(store, zone, room);
  if (!entry) return std::unexpected(entry.error());
  auto bytes = store.ReadAll(**entry);
  if (!bytes) return std::unexpected(bytes.error());
  const auto header = Read<Field_MpdHeader>(*bytes);
  if (!header) return std::unexpected(header.error());
  const auto lengths = Read<std::array<std::uint32_t, kSectionCount>>(
      *bytes, static_cast<std::size_t>(header->roomSectionOffset));
  if (!lengths) return std::unexpected(lengths.error());
  RoomContent result;
  result.bytes = std::move(*bytes);
  result.lengths = *lengths;
  std::uint64_t offset =
      static_cast<std::uint64_t>(header->roomSectionOffset) + sizeof(*lengths);
  for (std::size_t index = 0; index < kSectionCount; ++index) {
    const auto section = Section(result.bytes, offset, (*lengths)[index]);
    if (!section) return std::unexpected(section.error());
    result.offsets[index] = static_cast<std::size_t>(offset);
    offset += (*lengths)[index];
  }
  const auto collision_section =
      Section(result.bytes, result.offsets[1], result.lengths[1]);
  const auto environment_section =
      Section(result.bytes, result.offsets[10], result.lengths[10]);
  if (!collision_section || !environment_section) {
    return std::unexpected("room collision or environment section is missing");
  }
  const auto collision = Read<Field_MpdCollisionHeader>(*collision_section);
  const auto environment = Read<Field_MpdEnvironment>(*environment_section);
  if (!collision || !environment) {
    return std::unexpected("room collision or environment header is truncated");
  }
  result.collision = *collision;
  result.environment = *environment;
  const auto valid_collision =
      ValidateCollision(*collision_section, *collision);
  if (!valid_collision) return std::unexpected(valid_collision.error());
  const auto spawns = Section(result.bytes, header->enemySectionOffset,
                              header->enemySectionSize);
  if (!spawns || spawns->size() % sizeof(Field_MpdEnemySpawn) != 0) {
    return std::unexpected("invalid room actor placement table");
  }
  result.spawn_count = spawns->size() / sizeof(Field_MpdEnemySpawn);
  return result;
}

Result ValidateLocation(const Location& location, const RoomContent& room) {
  if (location.scene != room.environment.sceneId) {
    return std::unexpected("location.scene does not match the MPD actor area");
  }
  const auto& position = location.position;
  // Dynamic support, lifts and airborne placements need not equal static
  // ground height. Preserve Y exactly; only enforce the native SVECTOR domain.
  if (position[0] < 0 || position[0] >= room.collision.width * kTileUnits ||
      position[2] < 0 || position[2] >= room.collision.height * kTileUnits ||
      !std::in_range<std::int16_t>(position[1])) {
    return std::unexpected(
        "location.position is outside the room/native domain");
  }
  const auto valid_tile = [&](const TileAddress& tile) {
    return tile.layer <= 1 && tile.x < room.collision.width &&
           tile.z < room.collision.height;
  };
  if (location.floor_layer > 1 || !valid_tile(location.previous_tile) ||
      !valid_tile(location.saved_placement.tile)) {
    return std::unexpected(
        "static-layer placement history is outside the room");
  }
  return {};
}

Result ValidateOrbitCamera(const Snapshot& state) {
  const auto& camera = state.camera;
  const int zoom_mode =
      state.ashley.camera_distance == kNearCameraDistance ? 1 : kFarZoomMode;
  if (camera.pitch < -kLastAngle || camera.pitch > kLastAngle ||
      VS::Core::Math::Cosine(camera.pitch) == 0 ||
      camera.distance != state.ashley.camera_distance ||
      camera.zoom_mode != zoom_mode || camera.rotation_step < 0 ||
      camera.rotation_step > kMaximumOrbitStep ||
      camera.zoom_velocity < -kMaximumZoomVelocity ||
      camera.zoom_velocity > kMaximumZoomVelocity ||
      camera.tracking_speed < 0 ||
      camera.tracking_speed > kMaximumTrackingSpeed ||
      camera.view_angles[0] < -kQuarterTurn ||
      camera.view_angles[0] > kQuarterTurn || camera.view_angles[1] < 0 ||
      camera.view_angles[1] > kLastAngle || camera.view_angles[2] != 0) {
    return std::unexpected("unsupported or unfinished orbit camera state");
  }
  for (const auto value : camera.look_at) {
    if (value < -kCameraCoordinateLimit || value > kCameraCoordinateLimit) {
      return std::unexpected("camera look-at exceeds native arithmetic bounds");
    }
  }
  return {};
}

VECTOR OrbitPosition(const Snapshot& state) {
  // _computeSphericalOffset, 146C.c:8092. This helper and its scalar GTE math
  // (Core/Gte.cpp VectorNormal/Sin/Cos) use only their arguments, not
  // registers. ValidateOrbitCamera first excludes a zero cosine and unsafe
  // arithmetic.
  VECTOR look_at{state.camera.look_at[0], state.camera.look_at[1],
                 state.camera.look_at[2], 0};
  Field_SphericalCameraValues values{state.location.camera_yaw,
                                     state.camera.pitch, state.camera.distance,
                                     state.camera.mode};
  VECTOR position{};
  VS::Field::LiveComputeSphericalCameraPosition(&position, &look_at, &values);
  return position;
}

void CaptureCamera(Snapshot& state) {
  const auto& source = VS::Field::sphericalCamera;
  auto& target = state.camera;
  target.pitch = source.values.pitch;
  target.distance = source.values.distance;
  target.mode = source.values.mode;
  // Orbit input (146C.c:2641) retains the step and zoom velocity even after
  // settling; the next user input consumes them. Look-at also retains history
  // through the dead-zone tracker (146C.c:2597), so do not recenter it on load.
  target.rotation_step = source.delta.distance;
  target.zoom_mode = source.delta.mode;
  target.zoom_velocity = source.zoomVelocity;
  target.tracking_speed = source.speed;
  const auto& camera = VS::Render::FrameScratchpad().camera;
  target.look_at = {camera.look_at.vx, camera.look_at.vy, camera.look_at.vz};
  target.view_angles = {camera.angles.vx, camera.angles.vy, camera.angles.vz};
}

void RestoreCamera(const Snapshot& state) {
  auto& target = VS::Field::sphericalCamera;
  const auto& camera = state.camera;
  target.values = {state.location.camera_yaw, camera.pitch, camera.distance,
                   camera.mode};
  target.delta.yaw = 0;
  target.delta.pitch = state.ashley.camera_pitch;
  target.delta.distance = camera.rotation_step;
  target.delta.mode = camera.zoom_mode;
  target.initialDistance = state.ashley.camera_distance;
  target.zoomVelocity = camera.zoom_velocity;
  target.speed = camera.tracking_speed;
  VS::Render::FrameScratchpad().camera.look_at =
      VECTOR{camera.look_at[0], camera.look_at[1], camera.look_at[2], 0};
  VS::Render::FrameScratchpad().camera.position = OrbitPosition(state);
  VS::Render::FrameScratchpad().camera.angles.vx = camera.view_angles[0];
  VS::Render::FrameScratchpad().camera.angles.vy = camera.view_angles[1];
  VS::Render::FrameScratchpad().camera.angles.vz = camera.view_angles[2];
}

std::expected<CategoryBounds, std::string> ReadCategoryBounds(
    const Content::DataStore& store) {
  // G00027 _itemCategorySizes: resolve its registered on-disc slice, rather
  // than depending on whether the BATTLE overlay has been initialized yet.
  const VS::Content::ModuleDataEntry* slice = nullptr;
  for (const auto& entry : VS::Content::ModuleDataEntries()) {
    if (entry.target == &VS::Items::itemCategoryUpperIds) {
      slice = &entry;
      break;
    }
  }
  CategoryBounds bounds{};
  std::array<std::uint8_t, sizeof(bounds)> bytes{};
  const auto* entry = slice ? store.Find(slice->module) : nullptr;
  if (slice == nullptr || slice->size != bytes.size() || entry == nullptr ||
      !store.Read(*entry, slice->offset, bytes)) {
    return std::unexpected("BATTLE item category data is unavailable");
  }
  std::memcpy(bounds.data(), bytes.data(), bytes.size());
  unsigned previous = 0;
  for (const auto upper : bounds) {
    if (upper <= previous || upper > kItemIdEnd) {
      return std::unexpected("invalid BATTLE item category table");
    }
    previous = upper;
  }
  if (previous != kItemIdEnd) {
    return std::unexpected("unsupported BATTLE item category table");
  }
  if (std::ranges::any_of(VS::Items::itemCategoryUpperIds,
                          [](auto value) { return value != 0; }) &&
      !std::equal(bounds.begin(), bounds.end(),
                  VS::Items::itemCategoryUpperIds)) {
    return std::unexpected("live BATTLE item categories disagree with content");
  }
  return bounds;
}

bool InCategory(unsigned id, unsigned first, unsigned last) {
  return id >= first && id < last;
}

template <typename Collection>
Result ValidateItemIds(const Collection& items, const CategoryBounds& bounds,
                       std::string_view scope) {
  const auto valid_durability = [](const auto& item) {
    return item.current_dp <= item.max_dp && item.current_pp <= item.max_pp;
  };
  for (const auto& blade : items.blades) {
    if (blade.id != 0 &&
        (!InCategory(blade.id, 1, bounds[kBladeEnd]) ||
         !valid_durability(blade) ||
         blade.material >= std::size(VS::Items::materialModifiers) ||
         static_cast<unsigned char>(blade.category) >=
             std::size(VS::State::ArtsStatus{}.kills.weapon_category_kills))) {
      return ItemError(scope, "blades",
                       static_cast<std::size_t>(&blade - items.blades.data()),
                       "invalid ID, category, material or DP/PP");
    }
  }
  for (const auto& grip : items.grips) {
    if (grip.id != 0 &&
        (!InCategory(grip.id, bounds[kBladeEnd], bounds[kGripEnd]) ||
         grip.gem_slots < 0 || grip.gem_slots > kEquipmentGems ||
         grip.category >= kGripCategoryCount)) {
      return ItemError(scope, "grips",
                       static_cast<std::size_t>(&grip - items.grips.data()),
                       "invalid ID, category or gem capacity");
    }
  }
  const auto valid_armor = [&](const auto& armor) {
    return InCategory(armor.id, bounds[kGripEnd], bounds[kArmorAccessoryEnd]) &&
           valid_durability(armor) && armor.gem_slots >= 0 &&
           armor.gem_slots <= kEquipmentGems && armor.limb <= kAccessoryLimb &&
           armor.material < std::size(VS::Items::materialModifiers) &&
           static_cast<unsigned char>(armor.category) < kArmorCategoryCount;
  };
  for (const auto& armor : items.armor) {
    if (armor.id != 0 && !valid_armor(armor)) {
      return ItemError(scope, "armor",
                       static_cast<std::size_t>(&armor - items.armor.data()),
                       "invalid ID, limb, material or DP/PP");
    }
  }
  for (const auto& shield : items.shields) {
    if (shield.base.id != 0 && !valid_armor(shield.base)) {
      return ItemError(scope, "shields",
                       static_cast<std::size_t>(&shield - items.shields.data()),
                       "invalid ID, gem capacity, material or DP/PP");
    }
  }
  for (const auto& gem : items.gems) {
    if (gem.id != 0 &&
        !InCategory(gem.id, bounds[kArmorAccessoryEnd], bounds[kGemEnd])) {
      return ItemError(scope, "gems",
                       static_cast<std::size_t>(&gem - items.gems.data()),
                       "invalid definition ID");
    }
  }
  for (const auto& misc : items.misc) {
    if (misc.id != 0 &&
        (!InCategory(misc.id, bounds[kGemEnd], bounds[kMiscEnd]) ||
         static_cast<unsigned char>(misc.count) == 0)) {
      return ItemError(scope, "misc",
                       static_cast<std::size_t>(&misc - items.misc.data()),
                       "invalid ID or count");
    }
  }

  return {};
}

template <typename Collection>
Result ValidateItems(const Collection& items, const CategoryBounds& bounds,
                     std::string_view scope) {
  const auto ids = ValidateItemIds(items, bounds, scope);
  if (!ids) return ids;
  std::array<unsigned, std::size(items.blades)> blade_parents{};
  std::array<unsigned, std::size(items.grips)> grip_parents{};
  std::array<unsigned, std::size(items.gems)> gem_parents{};
  const auto gems = [&](const auto& refs, unsigned capacity, unsigned parent,
                        bool shield) -> Result {
    for (unsigned slot = 0; slot < kEquipmentGems; ++slot) {
      const unsigned raw = refs[slot];
      const unsigned reference = shield ? raw & 0x7Fu : raw;
      if (raw == 0) continue;
      if (slot >= capacity || reference == 0 ||
          reference > std::size(items.gems) ||
          items.gems[reference - 1].id == 0 ||
          gem_parents[reference - 1] != 0) {
        return ItemError(
            scope, shield ? "shields" : "weapons",
            (parent & ~kShieldParent) - 1,
            std::format("gems[{}]={}: dangling, duplicate or excess reference",
                        slot, raw));
      }
      gem_parents[reference - 1] = parent;
    }
    return {};
  };
  for (std::size_t index = 0; index < std::size(items.weapons); ++index) {
    const auto& weapon = items.weapons[index];
    if (weapon.blade == 0) {
      if (weapon.is_equipped != 0) {
        return ItemError(scope, "weapons", index, "empty slot is equipped");
      }
      continue;
    }
    if (weapon.blade > std::size(items.blades) || weapon.grip == 0 ||
        weapon.grip > std::size(items.grips) ||
        items.blades[weapon.blade - 1].id == 0 ||
        items.grips[weapon.grip - 1].id == 0 ||
        blade_parents[weapon.blade - 1] != 0 ||
        grip_parents[weapon.grip - 1] != 0) {
      return ItemError(scope, "weapons", index,
                       std::format("blade={}, grip={}: dangling or shared part",
                                   weapon.blade, weapon.grip));
    }
    const auto parent = static_cast<unsigned>(index + 1);
    blade_parents[weapon.blade - 1] = parent;
    grip_parents[weapon.grip - 1] = parent;
    const auto result =
        gems(weapon.gems,
             static_cast<unsigned>(items.grips[weapon.grip - 1].gem_slots),
             parent, false);
    if (!result) return result;
  }
  for (std::size_t index = 0; index < std::size(items.shields); ++index) {
    const auto& shield = items.shields[index];
    if (shield.base.id == 0) {
      if (shield.is_equipped != 0) {
        return ItemError(scope, "shields", index, "empty slot is equipped");
      }
      continue;
    }
    const auto result =
        gems(shield.gems, static_cast<unsigned>(shield.base.gem_slots),
             static_cast<unsigned>(index + 1) | kShieldParent, true);
    if (!result) return result;
  }
  for (std::size_t index = 0; index < std::size(items.blades); ++index) {
    if (items.blades[index].id != 0 &&
        static_cast<unsigned char>(
            items.blades[index].assembled_weapon_index) !=
            blade_parents[index]) {
      return ItemError(scope, "blades", index,
                       "parent disagrees with weapon references");
    }
  }
  for (std::size_t index = 0; index < std::size(items.grips); ++index) {
    if (items.grips[index].id != 0 &&
        items.grips[index].assembled_weapon_index != grip_parents[index]) {
      return ItemError(scope, "grips", index,
                       "parent disagrees with weapon references");
    }
  }
  for (std::size_t index = 0; index < std::size(items.gems); ++index) {
    if (items.gems[index].id != 0 &&
        items.gems[index].set_item_index != gem_parents[index]) {
      return ItemError(scope, "gems", index,
                       "parent disagrees with equipment references");
    }
  }
  return {};
}

template <typename T>
bool Occupied(const T& item) {
  return item.id != 0;
}
bool Occupied(const VS::Items::InventoryWeapon& item) {
  return item.blade != 0;
}
bool Occupied(const VS::Items::InventoryShield& item) {
  return item.base.id != 0;
}

template <typename T, std::size_t N>
Result CaptureOrder(const std::array<T, N>& items,
                    std::array<std::uint8_t, N>& order,
                    std::string_view category) {
  std::array<bool, N> seen{};
  std::size_t count = 0;
  // Original consumption (5BF94.c:4454) leaves stale display rows until
  // 2D10.c:1254 rebuilds them. Project its ordered survivors on the copy.
  for (std::size_t row = 0; row < N; ++row) {
    const auto reference = order[row];
    if (reference == 0) continue;
    if (reference > N) {
      return ItemError(
          "inventory_order", category, row,
          std::format("reference {} exceeds capacity {}", reference, N));
    }
    if (seen[reference - 1]) {
      return ItemError("inventory_order", category, row,
                       std::format("duplicate reference {}", reference));
    }
    seen[reference - 1] = true;
    if (Occupied(items[reference - 1])) order[count++] = reference;
  }
  std::ranges::fill(std::span(order).subspan(count), 0);
  return {};
}

Result CaptureInventoryOrder(Snapshot& state) {
#define VS_SAVE_CAPTURE_ORDER(category)                                     \
  if (auto valid = CaptureOrder(state.inventory.category,                   \
                                state.inventory_order.category, #category); \
      !valid)                                                               \
  return valid
  VS_SAVE_CAPTURE_ORDER(weapons);
  VS_SAVE_CAPTURE_ORDER(blades);
  VS_SAVE_CAPTURE_ORDER(grips);
  VS_SAVE_CAPTURE_ORDER(shields);
  VS_SAVE_CAPTURE_ORDER(armor);
  VS_SAVE_CAPTURE_ORDER(gems);
  VS_SAVE_CAPTURE_ORDER(misc);
#undef VS_SAVE_CAPTURE_ORDER
  return {};
}

template <typename T, std::size_t N>
Result ValidateOrder(const std::array<T, N>& items,
                     const std::array<std::uint8_t, N>& order,
                     std::string_view category) {
  std::array<bool, N> seen{};
  bool empty_seen = false;
  for (std::size_t row = 0; row < N; ++row) {
    const auto reference = order[row];
    if (reference == 0) {
      empty_seen = true;
      continue;
    }
    if (reference > N) {
      return ItemError(
          "inventory_order", category, row,
          std::format("reference {} exceeds capacity {}", reference, N));
    }
    if (!Occupied(items[reference - 1])) {
      return ItemError(
          "inventory_order", category, row,
          std::format("reference {} points to an empty slot", reference));
    }
    if (seen[reference - 1]) {
      return ItemError("inventory_order", category, row,
                       std::format("duplicate reference {}", reference));
    }
    if (empty_seen) {
      return ItemError(
          "inventory_order", category, row,
          std::format("reference {} follows an empty row", reference));
    }
    seen[reference - 1] = true;
  }
  for (std::size_t index = 0; index < N; ++index) {
    if (Occupied(items[index]) &&
        static_cast<unsigned char>(items[index].index) != index + 1) {
      return ItemError(
          "inventory", category, index,
          std::format("stored index {}, expected {}",
                      static_cast<unsigned char>(items[index].index),
                      index + 1));
    }
  }
  // Title _initEnvironment (22C.c:4463) clears these caches; _initGameData
  // (4698) copies occupied slots without populating them. Items_CopyItem also
  // leaves display lists unchanged. F02508 rebuilds missing entries on opening
  // the inventory, so coverage is deliberately partial, not a slot permutation.
  return {};
}

Result ValidateInventory(const Snapshot& state, const CategoryBounds& bounds) {
  auto result = ValidateItems(state.inventory, bounds, "inventory");
  if (!result) return result;
  result = ValidateItems(state.storage, bounds, "storage");
  if (!result) return result;
#define VS_SAVE_CHECK_ORDER(category)                                \
  result = ValidateOrder(state.inventory.category,                   \
                         state.inventory_order.category, #category); \
  if (!result) return result
  VS_SAVE_CHECK_ORDER(weapons);
  VS_SAVE_CHECK_ORDER(blades);
  VS_SAVE_CHECK_ORDER(grips);
  VS_SAVE_CHECK_ORDER(shields);
  VS_SAVE_CHECK_ORDER(armor);
  VS_SAVE_CHECK_ORDER(gems);
  VS_SAVE_CHECK_ORDER(misc);
#undef VS_SAVE_CHECK_ORDER
  unsigned weapons = 0;
  unsigned shields = 0;
  std::array<bool, kAccessoryLimb + 1> limbs{};
  for (const auto& weapon : state.inventory.weapons) {
    if (weapon.is_equipped != 0 && ++weapons > 1) {
      return ItemError(
          "inventory", "weapons",
          static_cast<std::size_t>(&weapon - state.inventory.weapons.data()),
          "multiple equipped weapons");
    }
  }
  for (const auto& shield : state.inventory.shields) {
    if (shield.is_equipped != 0 && ++shields > 1) {
      return ItemError(
          "inventory", "shields",
          static_cast<std::size_t>(&shield - state.inventory.shields.data()),
          "multiple equipped shields");
    }
  }
  for (const auto& armor : state.inventory.armor) {
    if (armor.limb == 0) continue;
    if (armor.id == 0 || armor.limb >= limbs.size() || limbs[armor.limb]) {
      return ItemError(
          "inventory", "armor",
          static_cast<std::size_t>(&armor - state.inventory.armor.data()),
          std::format("duplicate or invalid equipped limb {}", armor.limb));
    }
    limbs[armor.limb] = true;
  }
  return {};
}

Result ValidateModels(const Snapshot& state, const Content::DataStore& store) {
  std::set<unsigned> models;
  const auto collect = [&](const auto& items) {
    for (const auto& blade : items.blades) {
      if (blade.id != 0) {
        models.insert(static_cast<unsigned char>(blade.weapon_model_id));
      }
    }
    for (const auto& shield : items.shields) {
      if (shield.base.id != 0) {
        models.insert(static_cast<unsigned char>(shield.base.weapon_model_id));
      }
    }
  };
  collect(state.inventory);
  collect(state.storage);
  for (const auto model : models) {
    const auto path = std::format("OBJ/{:02X}.WEP", model);
    const auto* entry = store.Find(path);
    std::array<std::uint8_t, 1> byte{};
    if (model == 0 || entry == nullptr || entry->size == 0 ||
        !store.Read(*entry, 0, byte)) {
      return std::unexpected("required equipment model is unavailable: " +
                             path);
    }
  }
  return {};
}

Result ValidateRuntimeValues(const Snapshot& state) {
  if (state.location.zone == 0 || state.location.zone > kLastLocation ||
      state.location.room > kLastLocation ||
      state.location.scene > kLastScene || state.location.facing < 0 ||
      state.location.facing > kLastAngle || state.location.camera_yaw < 0 ||
      state.location.camera_yaw > kLastAngle ||
      state.room_cache_slot >= state.room_cache.size() ||
      state.play_ticks > kMaximumTicks) {
    return std::unexpected("invalid location, angle, room-cache slot or clock");
  }
  if (state.random.shuffle_index >= state.random.shuffle.size() ||
      std::ranges::any_of(state.random.shuffle, [](auto value) {
        return value < 0 || value > std::numeric_limits<std::int16_t>::max();
      })) {
    return std::unexpected("invalid random shuffle state");
  }
  const auto& timer = state.scene_timer;
  if (timer.time.frames < 0 || timer.time.frames >= kClockRadix ||
      timer.time.seconds < 0 || timer.time.seconds >= kClockRadix ||
      timer.time.minutes < 0 || timer.time.minutes >= kClockRadix ||
      timer.time.hours < 0) {
    return std::unexpected("invalid scene timer");
  }
  const auto& stats = state.ashley;
  if (stats.camera_distance != kNearCameraDistance &&
      stats.camera_distance != kFarCameraDistance) {
    return std::unexpected(
        "saved camera distance is not a field camera preset");
  }
  if (stats.max_hp <= 0 || stats.current_hp <= 0 ||
      stats.current_hp > stats.max_hp || stats.max_mp < 0 ||
      stats.current_mp < 0 || stats.current_mp > stats.max_mp) {
    return std::unexpected("invalid Ashley HP/MP");
  }
  for (const auto& limb : stats.limbs) {
    if (limb.hp > limb.max_hp ||
        limb.max_hp >
            static_cast<unsigned>(std::numeric_limits<std::int16_t>::max())) {
      return std::unexpected("invalid Ashley limb HP");
    }
  }
  return {};
}

void CopyCachesToSnapshot(Snapshot& target) {
  target.room_cache_slot =
      static_cast<std::uint32_t>(VS::State::currentRoomStateSlot);
  for (std::size_t index = 0; index < target.room_cache.size(); ++index) {
    const auto& source = VS::State::recentRoomEnemyStates[index];
    auto& cache = target.room_cache[index];
    cache.recency = source.recency;
    cache.zone = source.zone_id;
    cache.map = source.map_id;
    cache.next_slot = source.next_slot;
    for (std::size_t slot = 0; slot < cache.actors.size(); ++slot) {
      std::memcpy(&cache.actors[slot], source.actor_states[slot],
                  sizeof(cache.actors[slot]));
    }
  }
}

void ApplyCaches(const Snapshot& source) {
  VS::State::currentRoomStateSlot = static_cast<int>(source.room_cache_slot);
  for (std::size_t index = 0; index < source.room_cache.size(); ++index) {
    const auto& cache = source.room_cache[index];
    auto& target = VS::State::recentRoomEnemyStates[index];
    target.recency = cache.recency;
    target.zone_id = cache.zone;
    target.map_id = cache.map;
    target.next_slot = cache.next_slot;
    for (std::size_t slot = 0; slot < cache.actors.size(); ++slot) {
      std::memcpy(target.actor_states[slot], &cache.actors[slot],
                  sizeof(cache.actors[slot]));
    }
  }
}

void CapturePersistent(Snapshot& state, bool packed_actions = false) {
  for (std::size_t index = 0; index < state.script_flags.size(); ++index) {
    state.script_flags[index] = static_cast<unsigned char>(
        VS::State::LiveScriptFlag(static_cast<std::int16_t>(index)));
  }
  for (std::size_t index = 0; index < state.learned_actions.size(); ++index) {
    state.learned_actions[index] =
        packed_actions
            ? (static_cast<unsigned char>(
                   VS::State::learnedActionFlags[index / kLearnedBitsPerByte]) &
               (kLearnedFirstBit >> (index % kLearnedBitsPerByte))) != 0
            : Battle_actionDefinitions[index].unlocked != 0;
  }
  state.inventory = VS::Items::inventory;
  state.inventory_order = VS::Items::inventoryIndices;
  state.settings = VS::Input::settings;
  state.visits = VS::State::roomVisitProgress;
  state.score = VS::State::scoreData;
  state.arts = VS::State::artsStatus;
  CopyCachesToSnapshot(state);
}

void ApplyPersistent(const Snapshot& state) {
  for (std::size_t index = 0; index < state.script_flags.size(); ++index) {
    VS::State::SetLiveScriptFlag(static_cast<std::int16_t>(index),
                                 static_cast<char>(state.script_flags[index]));
  }
  std::fill_n(VS::State::learnedActionFlags,
              std::size(VS::State::learnedActionFlags), 0);
  for (std::size_t index = 0; index < state.learned_actions.size(); ++index) {
    Battle_actionDefinitions[index].unlocked = state.learned_actions[index];
    if (state.learned_actions[index]) {
      const auto value =
          static_cast<unsigned char>(
              VS::State::learnedActionFlags[index / kLearnedBitsPerByte]) |
          (kLearnedFirstBit >> (index % kLearnedBitsPerByte));
      VS::State::learnedActionFlags[index / kLearnedBitsPerByte] =
          static_cast<char>(value);
    }
  }
  VS::Items::inventory = state.inventory;
  VS::Items::inventoryIndices = state.inventory_order;
  VS::Input::settings = state.settings;
  VS::State::roomVisitProgress = state.visits;
  VS::State::scoreData = state.score;
  VS::State::artsStatus = state.arts;
  ApplyCaches(state);
}

Result OverlayHandEquipment(Snapshot& state, const Battle_ActorStats& stats) {
  // vs_battle_copyUi{Weapon,Shield,Armor}Stats, 146C.c:1545-1600.
  // The original stores into canonical inventory and edits the loadouts.
  // Here only copied loadouts and the staged inventory may be written.
  const auto has_weapon = std::ranges::any_of(
      state.inventory.weapons,
      [](const auto& weapon) { return weapon.is_equipped != 0; });
  const auto has_shield = std::ranges::any_of(
      state.inventory.shields,
      [](const auto& shield) { return shield.is_equipped != 0; });
  if (has_weapon != (stats.weapon.index != 0) ||
      has_shield != (stats.shield.index != 0)) {
    return std::unexpected("live equipment presence disagrees with inventory");
  }
  if (stats.weapon.index != 0) {
    if (stats.weapon.index > std::size(state.inventory.weapons)) {
      return std::unexpected("live weapon index is invalid");
    }
    auto weapon = stats.weapon;
    auto& target = state.inventory.weapons[weapon.index - 1];
    if (!target.is_equipped || weapon.blade.id == 0 || target.blade == 0 ||
        target.blade > std::size(state.inventory.blades) || target.grip == 0 ||
        target.grip > std::size(state.inventory.grips)) {
      return std::unexpected("live weapon does not match its inventory slot");
    }
    if (state.inventory.blades[target.blade - 1].id != weapon.blade.id ||
        state.inventory.grips[target.grip - 1].id != weapon.grip.id) {
      return std::unexpected(
          "live weapon part identity disagrees with inventory");
    }
    weapon.blade.current_dp = weapon.currentDp;
    weapon.blade.current_pp = weapon.currentPp;
    VS::Items::WriteLiveInventoryBladeStats(
        &state.inventory.blades[target.blade - 1], &weapon.blade);
    VS::Items::WriteLiveInventoryGripStats(
        &state.inventory.grips[target.grip - 1], &weapon.grip);
    for (std::size_t index = 0; index < kEquipmentGems; ++index) {
      const auto reference = target.gems[index];
      if (weapon.gems[index].id == 0) {
        if (reference != 0) {
          return std::unexpected("live weapon gem is missing");
        }
        continue;
      }
      if (reference == 0 || reference > std::size(state.inventory.gems) ||
          state.inventory.gems[reference - 1].id != weapon.gems[index].id) {
        return std::unexpected("live weapon gem reference is invalid");
      }
      VS::Items::WriteLiveInventoryGemStats(
          &state.inventory.gems[reference - 1], &weapon.gems[index]);
    }
    std::copy_n(weapon.name, target.name.size(), target.name.begin());
  }
  if (stats.shield.index != 0) {
    if (stats.shield.index > std::size(state.inventory.shields)) {
      return std::unexpected("live shield index is invalid");
    }
    auto shield = stats.shield;
    auto& target = state.inventory.shields[shield.index - 1];
    if (!target.is_equipped || shield.base.id == 0 ||
        shield.base.id != target.base.id) {
      return std::unexpected("live shield does not match its inventory slot");
    }
    shield.base.current_dp = shield.currentDp;
    shield.base.current_pp = shield.currentPp;
    VS::Items::WriteLiveInventoryArmorStats(&target.base, &shield.base);
    for (std::size_t index = 0; index < kEquipmentGems; ++index) {
      const auto reference = target.gems[index] & 0x7Fu;
      if (shield.gems[index].id == 0) {
        if (reference != 0) {
          return std::unexpected("live shield gem is missing");
        }
        continue;
      }
      if (reference == 0 || reference > std::size(state.inventory.gems) ||
          state.inventory.gems[reference - 1].id != shield.gems[index].id) {
        return std::unexpected("live shield gem reference is invalid");
      }
      VS::Items::WriteLiveInventoryGemStats(
          &state.inventory.gems[reference - 1], &shield.gems[index]);
    }
  }
  return {};
}

Result OverlayEquipped(Snapshot& state, const Battle_ActorStats& stats) {
  const auto hands = OverlayHandEquipment(state, stats);
  if (!hands) return hands;
  for (std::size_t limb = 0; limb < std::size(stats.limbs); ++limb) {
    auto armor = stats.limbs[limb].armor;
    const auto equipped = std::ranges::find(state.inventory.armor, limb + 1,
                                            &VS::Items::InventoryArmor::limb);
    if (armor.index == 0) {
      if (equipped != std::end(state.inventory.armor)) {
        return std::unexpected("live limb armor is missing");
      }
      continue;
    }
    if (armor.index > std::size(state.inventory.armor) || armor.armor.id == 0 ||
        state.inventory.armor[armor.index - 1].limb != limb + 1 ||
        state.inventory.armor[armor.index - 1].id != armor.armor.id) {
      return std::unexpected("live armor does not match its inventory limb");
    }
    armor.armor.current_dp = armor.currentDp;
    VS::Items::WriteLiveInventoryArmorStats(
        &state.inventory.armor[armor.index - 1], &armor.armor);
  }
  auto accessory = stats.accessory;
  if (accessory.index == 0 &&
      std::ranges::find(state.inventory.armor, kAccessoryLimb,
                        &VS::Items::InventoryArmor::limb) !=
          std::end(state.inventory.armor)) {
    return std::unexpected("live accessory is missing");
  }
  if (accessory.index != 0) {
    if (accessory.index > std::size(state.inventory.armor) ||
        accessory.accessory.id == 0 ||
        state.inventory.armor[accessory.index - 1].limb != kAccessoryLimb ||
        state.inventory.armor[accessory.index - 1].id !=
            accessory.accessory.id) {
      return std::unexpected(
          "live accessory does not match its inventory limb");
    }
    VS::Items::WriteLiveInventoryArmorStats(
        &state.inventory.armor[accessory.index - 1], &accessory.accessory);
  }
  return {};
}

void CaptureAshley(Snapshot& state, const Battle_Actor& character) {
  const auto& source = *character.stats;
  auto& target = state.ashley;
  target.is_pending = 1;
  target.current_hp = source.currentHp;
  target.max_hp = source.maxHp;
  target.current_mp = source.currentMp;
  target.max_mp = source.maxMp;
  target.strength = std::bit_cast<std::int16_t>(source.strength);
  target.total_strength = std::bit_cast<std::int16_t>(source.totalStrength);
  target.intelligence = std::bit_cast<std::int16_t>(source.intelligence);
  target.total_intelligence =
      std::bit_cast<std::int16_t>(source.totalIntelligence);
  target.agility = std::bit_cast<std::int16_t>(source.agility);
  target.total_agility = std::bit_cast<std::int16_t>(source.totalAgility);
  target.risk = std::bit_cast<std::int16_t>(source.risk);
  target.action_recovery_timer = source.actionRecoveryTimer;
  target.statuses = source.statuses;
  for (std::size_t index = 0; index < std::size(source.statusTimers); ++index) {
    target.status_timers[index] = source.statusTimers[index];
  }
  for (std::size_t index = 0; index < std::size(source.limbs); ++index) {
    const auto& limb = source.limbs[index];
    auto& saved = target.limbs[index];
    saved.hp = static_cast<std::uint16_t>(limb.hp);
    saved.max_hp = static_cast<std::uint16_t>(limb.maxHp);
    for (std::size_t type = 0; type < std::size(limb.types); ++type) {
      saved.types[type] = static_cast<std::uint16_t>(limb.types[type]);
    }
    for (std::size_t affinity = 0; affinity < std::size(limb.affinities);
         ++affinity) {
      saved.affinities[affinity] =
          static_cast<std::uint16_t>(limb.affinities[affinity]);
    }
  }
  target.weapon_drawn = character.weaponDrawn;
  target.camera_pitch = VS::Field::sphericalCamera.delta.pitch;
  target.camera_distance = VS::Field::sphericalCamera.initialDistance;
}

void RestoreAshleyValues(const VS::State::AshleyStatsSnapshot& source,
                         Battle_Actor& character) {
  // func_800F9AD8, INITBTL/18.c:310. Spawn has already built equipment and
  // status effects. Reassert saved scalars after room-entry status cleanup.
  auto& target = *character.stats;
  target.currentHp = source.current_hp;
  target.maxHp = source.max_hp;
  target.currentMp = source.current_mp;
  target.maxMp = source.max_mp;
  target.strength = static_cast<std::uint16_t>(source.strength);
  target.totalStrength = static_cast<std::uint16_t>(source.total_strength);
  target.intelligence = static_cast<std::uint16_t>(source.intelligence);
  target.totalIntelligence =
      static_cast<std::uint16_t>(source.total_intelligence);
  target.agility = static_cast<std::uint16_t>(source.agility);
  target.totalAgility = static_cast<std::uint16_t>(source.total_agility);
  target.risk = static_cast<std::uint16_t>(source.risk);
  target.actionRecoveryTimer = source.action_recovery_timer;
  target.statuses = source.statuses;
  for (std::size_t index = 0; index < std::size(source.status_timers);
       ++index) {
    target.statusTimers[index] = source.status_timers[index];
  }
  for (std::size_t index = 0; index < std::size(source.limbs); ++index) {
    auto& limb = target.limbs[index];
    const auto& saved = source.limbs[index];
    limb.hp = static_cast<std::int16_t>(saved.hp);
    limb.maxHp = static_cast<std::int16_t>(saved.max_hp);
    for (std::size_t type = 0; type < std::size(saved.types); ++type) {
      limb.types[type] = std::bit_cast<std::int16_t>(saved.types[type]);
    }
    for (std::size_t affinity = 0; affinity < std::size(saved.affinities);
         ++affinity) {
      limb.affinities[affinity] =
          std::bit_cast<std::int16_t>(saved.affinities[affinity]);
    }
  }
  character.weaponDrawn = source.weapon_drawn;
}

Result CaptureActors(Snapshot& state) {
  if (state.room_cache_slot >= state.room_cache.size()) {
    return std::unexpected("live room-cache slot is invalid");
  }
  auto& cache = state.room_cache[state.room_cache_slot];
  if (cache.next_slot >= cache.actors.size() ||
      cache.zone != state.location.zone || cache.map != state.location.room ||
      cache.recency == 0) {
    return std::unexpected(
        "live room cache does not identify the current room");
  }
  // func_80069FC4, 146C.c:840. Use the original pure placement and AI
  // projections, but acquire records ONLY in the owned staging cache.
  for (int slot = 0; std::cmp_less(slot, VS::Battle::Ai::kFrameActorSlots);
       ++slot) {
    const auto* actor = Battle_actors[slot];
    if (actor == nullptr || actor->roomSpawnIndex == kNoRoomSpawn) continue;
    if (actor->stats == nullptr || Actors_actorRecords[slot] == nullptr ||
        actor->roomSpawnIndex == kLastLocation) {
      return std::unexpected("live room actor is not ready");
    }
    const auto spawn = static_cast<unsigned>(actor->roomSpawnIndex) + 1;
    auto found = std::ranges::find(cache.actors, spawn,
                                   &Actors_RoomActorRecord::spawnSlot);
    if (found == cache.actors.end()) {
      found = cache.actors.begin() + cache.next_slot;
      cache.next_slot = (cache.next_slot + 1) % cache.actors.size();
    }
    const auto& stats = *actor->stats;
    found->spawnSlot = static_cast<unsigned char>(spawn);
    found->isHidden = 0;
    found->aiFlags = VS::Battle::Ai::PackLiveActorAiPersistenceFlags(slot);
    // The action animation test of StateFlag::kActionAnimating
    // (ActorFlags.cpp), read directly so the snapshot does not compose the
    // weapon and model state.
    const auto animation =
        Actors_actorRecords[slot]->instance.actionAnimationState;
    found->currentHp =
        animation != 0 && animation < kActiveAnimationEnd ? 1 : stats.currentHp;
    found->maxHp = stats.maxHp;
    found->currentMp = stats.currentMp;
    found->maxMp = stats.maxMp;
    found->strength = std::bit_cast<std::int16_t>(stats.totalStrength);
    found->intelligence = std::bit_cast<std::int16_t>(stats.totalIntelligence);
    found->agility = std::bit_cast<std::int16_t>(stats.totalAgility);
    for (std::size_t limb = 0; limb < std::size(stats.limbs); ++limb) {
      found->limbHp[limb] = stats.limbs[limb].hp;
    }
    found->statuses = stats.statuses;
    Actors_ActorLocationQuery placement{};
    VS::Actors::LiveActorSavedPlacement(slot, stats.movementClass, &placement);
    found->tileX =
        static_cast<unsigned char>(placement.location.packedTile.target.tileX);
    found->direction = static_cast<unsigned char>(
        placement.location.packedTile.target.tileLayer);
    found->tileZ =
        static_cast<unsigned char>(placement.location.packedTile.target.tileZ);
    found->height =
        static_cast<unsigned char>(placement.location.packedTile.target.facing);
  }
  return {};
}

void ApplyClock(std::uint64_t ticks) {
  VS::State::playTime.frames = static_cast<std::int8_t>(ticks % kClockRadix);
  ticks /= kClockRadix;
  VS::State::playTime.seconds = static_cast<std::int8_t>(ticks % kClockRadix);
  ticks /= kClockRadix;
  VS::State::playTime.minutes = static_cast<std::int8_t>(ticks % kClockRadix);
  VS::State::playTime.hours = static_cast<std::int8_t>(ticks / kClockRadix);
}

Result ValidateCaptureBoundary(const Actors_ActorRecord& actor) {
  const auto& model = actor.instance;
  // func_8006C39C (146C.c:1893) freezes ordinary walk/turn intent in menus.
  // Resume rebuilds an idle actor at this position, not that transient intent.
  const bool menu_paused = VS::Field::isInteractionPaused != 0 &&
                           (Field_controlMode == 5 || Field_controlMode == 6);
  const bool horizontal_motion =
      model.turnFrames != 0 || model.rootMotion.vx != 0 ||
      model.rootMotion.vz != 0 || actor.momentumDistance != 0;
  // supportLayer/placementLayer are platform bindings, not collision floor1.
  // VS::Actors::PlaceActorInRoom (3A1A0.c:274) uses ids>=2 for moving
  // supports.
  if (model.currentTileLayer > 1 || model.supportLayer != 0 ||
      model.placementLayer != 0 || model.isAttached || model.movementPending ||
      model.movementMode != 0 || model.locomotionState != 0 ||
      model.motionLocked || model.isJumping || model.jumpCountdown != 0 ||
      (!menu_paused && horizontal_motion) || model.currentAnimation != 0 ||
      model.rootMotion.vy != 0 || actor.supportGimmick != 0 ||
      actor.routePhase != 0 || actor.destinationMode != 0 ||
      actor.jumpFrames != 0 || actor.transitionFrames != 0) {
    return std::unexpected(std::format(
        "schema3 requires a static floor and quiescent unattached actor: "
        "tileLayer={} supportLayer={} placementLayer={} attached={} "
        "movementPending={} movementMode={} locomotionState={} "
        "motionLocked={} jumping={} jumpCountdown={} turnFrames={} "
        "animation={} rootMotion={},{},{} supportGimmick={} routePhase={} "
        "destinationMode={} jumpFrames={} momentumDistance={} "
        "transitionFrames={}",
        model.currentTileLayer, model.supportLayer, model.placementLayer,
        model.isAttached, model.movementPending, model.movementMode,
        model.locomotionState, model.motionLocked, model.isJumping,
        model.jumpCountdown, model.turnFrames, model.currentAnimation,
        model.rootMotion.vx, model.rootMotion.vy, model.rootMotion.vz,
        actor.supportGimmick, actor.routePhase, actor.destinationMode,
        actor.jumpFrames, actor.momentumDistance, actor.transitionFrames));
  }
  if (model.currentTileX != model.position.vx / kTileUnits ||
      model.currentTileZ != model.position.vz / kTileUnits) {
    return std::unexpected(
        "current tile does not match the static world position");
  }
  if (VS::Field::cameraTransition != nullptr ||
      VS::Field::isFirstPersonViewEnabled ||
      VS::Field::sphericalCamera.delta.yaw != 0 ||
      VS::Render::nearClip != kFieldNearClip ||
      VS::Render::projectionDistance != kFieldProjection ||
      VS::Render::FrameScratchpad().camera.far_clip != ONE) {
    return std::unexpected(
        "schema3 requires a settled orbit camera, not a transition/custom "
        "view");
  }
  return {};
}
}  // namespace

namespace {
Result ValidateContentImpl(const Snapshot& state,
                           const Field_MpdDoorRecord* new_game = nullptr) {
  auto valid = ValidateRuntimeValues(state);
  if (!valid) return valid;
  if (!new_game) {
    valid = ValidateOrbitCamera(state);
    if (!valid) return valid;
  }
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) {
    return std::unexpected("extracted content is not open");
  }
  const auto bounds = ReadCategoryBounds(*store);
  if (!bounds) return std::unexpected(bounds.error());
  valid = ValidateInventory(state, *bounds);
  if (!valid) return valid;
  valid = ValidateModels(state, *store);
  if (!valid) return valid;
  const auto room = ReadRoom(*store, state.location.zone, state.location.room);
  if (!room) return std::unexpected(room.error());
  if (new_game && (new_game->entryTileX >= room->collision.width ||
                   new_game->entryTileZ >= room->collision.height ||
                   new_game->tileLocation.entryDirection > 1)) {
    return std::unexpected(
        "clear-game entry requires an in-bounds static starting-room tile");
  }
  if (!new_game) {
    valid = ValidateLocation(state.location, *room);
    if (!valid) return valid;
    const auto& current_cache = state.room_cache[state.room_cache_slot];
    if (current_cache.recency == 0 ||
        current_cache.zone != state.location.zone ||
        current_cache.map != state.location.room) {
      return std::unexpected("current cache does not identify the saved room");
    }
  }
  std::set<std::pair<unsigned, unsigned>> cached_rooms;
  for (const auto& cache : state.room_cache) {
    if (cache.next_slot >= cache.actors.size() ||
        cache.recency > state.room_cache.size()) {
      return std::unexpected("invalid room-cache cursor or recency");
    }
    if (cache.recency == 0) {
      if (std::ranges::any_of(cache.actors, [](const auto& actor) {
            return actor.spawnSlot != 0;
          })) {
        return std::unexpected("inactive room cache contains actor references");
      }
      continue;
    }
    if (!cached_rooms.emplace(cache.zone, cache.map).second) {
      return std::unexpected("duplicate active room-cache identity");
    }
    const auto cached_room = ReadRoom(*store, cache.zone, cache.map);
    if (!cached_room) {
      return std::unexpected("room cache: " + cached_room.error());
    }
    std::set<unsigned> spawns;
    for (const auto& actor : cache.actors) {
      if (actor.spawnSlot == 0) continue;
      if (actor.spawnSlot > cached_room->spawn_count ||
          !spawns.insert(actor.spawnSlot).second ||
          actor.tileX >= cached_room->collision.width ||
          actor.tileZ >= cached_room->collision.height) {
        return std::unexpected(
            "room cache contains an invalid actor reference");
      }
    }
  }
  return {};
}
}  // namespace

std::expected<void, std::string> ValidateContent(const Snapshot& state) {
  return ValidateContentImpl(state);
}

std::expected<void, std::string> ValidateContent(const Document& document) {
  if (!document.new_game_entry) return ValidateContent(document.state);
  const auto& state = document.state;
  const auto& entry = *document.new_game_entry;
  constexpr auto kPuzzleMode = offsetof(VS::State::StateFlags, puzzle_mode);
  constexpr auto kCampaignFlags = 0x10;
  constexpr auto kMaximumClears = 99;
  // ENDING resets 0x10..0x43f. TITLE sets only puzzleMode after that reset.
  if (state.script_flags[0] == 0 || state.script_flags[0] > kMaximumClears ||
      state.script_flags[1] != 1 || state.script_flags[2] > 1 ||
      state.play_ticks != 0 || state.settings.current_game_save_count != 0 ||
      (state.settings.menu_flags & kClearMenuFlag) != 0 ||
      state.script_flags[kPuzzleMode] !=
          ((~(state.settings.mode_flags >> kPuzzleSettingShift)) & 1) ||
      entry.destinationZoneId != 1 || entry.destinationRoomId != 0 ||
      state.location.zone != 1 || state.location.room != 0) {
    return std::unexpected("invalid original clear-game restart state");
  }
  for (std::size_t index = kCampaignFlags; index < state.script_flags.size();
       ++index) {
    if (index != kPuzzleMode && state.script_flags[index] != 0) {
      return std::unexpected("clear game retains per-campaign script flags");
    }
  }
  const auto has_misc = [](const auto& item) { return item.id != 0; };
  if (std::ranges::any_of(state.inventory.misc, has_misc) ||
      std::ranges::any_of(state.inventory_order.misc,
                          [](auto value) { return value != 0; }) ||
      std::ranges::any_of(
          std::span(state.storage.misc).first(kClearContainerResetStacks),
          has_misc) ||
      std::ranges::any_of(state.visits.room_flags,
                          [](auto value) { return value != 0; }) ||
      std::ranges::any_of(state.visits.area_flags,
                          [](auto value) { return value != 0; })) {
    return std::unexpected("clear game retains reset inventory or map visits");
  }
  return ValidateContentImpl(state, &entry);
}

std::expected<Document, std::string> CaptureClearGame() {
  if (pending || VS::Boot::isGameplayActive || !VS::Boot::isClearSavePending ||
      VS::State::savedEntryDoor.destinationZoneId != 1 ||
      VS::State::savedEntryDoor.destinationRoomId != 0 ||
      VS::State::stateFlags.clear_count <= 0) {
    return std::unexpected("no original ending clear-save inputs are pending");
  }
  Document document;
  document.generation = 1;
  document.new_game_entry = VS::State::savedEntryDoor;
  auto& state = document.state;
  CapturePersistent(state, true);
  // The battle overlay has gone away: TITLE copies the already captured
  // Ashley/learned-action values, not the current actor or action-table views.
  state.ashley = VS::State::ashleySnapshot.stats;
  // F03601 TITLE _packageGameSaveData, 22C.c:632-700; original MIPS
  // 800698dc/800698e8, 80069c34, 80069c94..80069cc0. Container clearing is
  // exactly 0x100 bytes (64 stacks), NOT the entire 256-stack container.
  state.settings.menu_flags &= static_cast<u_short>(~kClearMenuFlag);
  state.settings.current_game_save_count = 0;
  state.script_flags[offsetof(VS::State::StateFlags, puzzle_mode)] =
      static_cast<std::uint8_t>(
          (~(state.settings.mode_flags >> kPuzzleSettingShift)) & 1);
  std::fill(std::begin(state.inventory.misc), std::end(state.inventory.misc),
            VS::Items::InventoryMisc{});
  std::fill(std::begin(state.inventory_order.misc),
            std::end(state.inventory_order.misc), 0);
  state.location.zone = 1;
  const auto valid = ValidateContent(document);
  if (!valid) return std::unexpected(valid.error());
  return document;
}

std::expected<Snapshot, std::string> Capture(
    const VS::Items::ContainerData& storage) {
  if (pending || !VS::Boot::isGameplayActive ||
      Battle_characterState == nullptr ||
      Battle_characterState != Battle_actors[0] ||
      Battle_characterState->stats == nullptr ||
      Actors_actorRecords[0] == nullptr || !Field_roomData.isLoaded ||
      Field_roomData.environment == nullptr ||
      VS::Field::zoneContext.mpdDiscRequest != nullptr ||
      VS::Field::zoneContext.zndDiscRequest != nullptr) {
    return std::unexpected("gameplay is not ready for a semantic capture");
  }
  if (VS::Field::zoneContext.zndId <= 0 ||
      VS::Field::zoneContext.zndId > kLastLocation ||
      VS::Field::zoneContext.mapId < 0 ||
      VS::Field::zoneContext.mapId > kLastLocation ||
      VS::Field::zoneContext.currentSceneId < 0 ||
      VS::Field::zoneContext.currentSceneId > kLastScene) {
    return std::unexpected("live scene identity is invalid");
  }
  const auto phase = ValidateCaptureBoundary(*Actors_actorRecords[0]);
  if (!phase) return std::unexpected(phase.error());
  Snapshot state;
  CapturePersistent(state);
  state.storage = storage;
  CaptureAshley(state, *Battle_characterState);
  auto valid = OverlayEquipped(state, *Battle_characterState->stats);
  if (!valid) return std::unexpected(valid.error());
  valid = CaptureInventoryOrder(state);
  if (!valid) return std::unexpected(valid.error());
  const auto& model = Actors_actorRecords[0]->instance;
  state.location.zone =
      static_cast<std::uint16_t>(VS::Field::zoneContext.zndId);
  state.location.room =
      static_cast<std::uint16_t>(VS::Field::zoneContext.mapId);
  // _zoneContext+0x68 (G00254/D800F18E8), not the detached door alias.
  state.location.scene =
      static_cast<std::uint16_t>(VS::Field::zoneContext.currentSceneId);
  state.location.position = {model.position.vx, model.position.vy,
                             model.position.vz};
  state.location.facing = model.facing;
  state.location.camera_yaw = VS::Field::sphericalCamera.values.yaw;
  state.location.floor_layer = model.currentTileLayer;
  state.location.previous_tile = {model.previousTileX, model.previousTileZ,
                                  model.previousTileLayer};
  state.location.previous_facing = model.previousFacing;
  const auto saved =
      std::bit_cast<Actors_TileTarget>(Actors_actorRecords[0]->savedPlacement);
  state.location.saved_placement = {
      {static_cast<std::uint8_t>(saved.tileX),
       static_cast<std::uint8_t>(saved.tileZ),
       static_cast<std::uint8_t>(saved.tileLayer)},
      static_cast<std::uint8_t>(saved.facing)};
  CaptureCamera(state);
  valid = ValidateOrbitCamera(state);
  if (!valid) return std::unexpected(valid.error());
  const auto orbit_position = OrbitPosition(state);
  const auto& actual = VS::Render::FrameScratchpad().camera.position;
  if (actual.vx != orbit_position.vx || actual.vy != orbit_position.vy ||
      actual.vz != orbit_position.vz) {
    return std::unexpected("current camera position is not the captured orbit");
  }
  const auto& time = VS::State::playTime;
  if (time.hours < 0 || time.hours > kMaximumHours || time.minutes < 0 ||
      time.minutes >= kClockRadix || time.seconds < 0 ||
      time.seconds >= kClockRadix || time.frames < 0 ||
      time.frames >= kClockRadix) {
    return std::unexpected("live play clock is invalid");
  }
  state.play_ticks =
      ((static_cast<std::uint64_t>(time.hours) * kClockRadix + time.minutes) *
           kClockRadix +
       time.seconds) *
          kClockRadix +
      time.frames;
  state.scene_timer = VS::State::sceneTimer;
  state.random.bios_state = VS::Core::BiosRand().State();
  state.random.interface_state = VS::Core::interfaceRandomState;
  std::copy_n(VS::Core::randomShuffleTable, state.random.shuffle.size(),
              state.random.shuffle.begin());
  state.random.shuffle_index =
      static_cast<std::uint32_t>(VS::Core::randomShuffleIndex);
  state.random.movement_state =
      std::bit_cast<std::uint32_t>(Battle_Ai_savedMovementRandomState);
  valid = CaptureActors(state);
  if (!valid) return std::unexpected(valid.error());
  valid = ValidateContent(state);
  if (!valid) return std::unexpected(valid.error());
  return state;
}

std::expected<void, std::string> PrepareResume(const Document& document) {
  if (pending) return std::unexpected("a resume is already pending");
  const auto valid = ValidateContent(document);
  if (!valid) return valid;
  const auto audio = ValidateAudioConsumer();
  if (!audio) return audio;
  PendingResume staged;
  staged.state = document.state;
  staged.new_game = document.new_game_entry.has_value();
  if (staged.new_game) {
    const auto* store = Content::DataStore::Get();
    if (store == nullptr) {
      return std::unexpected("extracted content is not open");
    }
    const auto room = ReadRoom(*store, staged.state.location.zone,
                               staged.state.location.room);
    if (!room) return std::unexpected(room.error());
    staged.state.location.scene =
        static_cast<std::uint16_t>(room->environment.sceneId);
  }
  CapturePersistent(staged.previous.state);
  staged.previous.ashley = VS::State::ashleySnapshot;
  staged.previous.entry = VS::State::savedEntryDoor;
  std::copy_n(VS::State::learnedActionFlags, staged.previous.learned.size(),
              staged.previous.learned.begin());
  staged.previous.unlocked = staged.previous.state.learned_actions;
  staged.previous.mix_behavior = VS::Audio::Driver::controlLatches.mix_behavior;
  staged.previous.play_time = VS::State::playTime;
  // All fallible work, including owning the staging copy, precedes mutation.
  pending.emplace(std::move(staged));
  ApplyPersistent(pending->state);
  VS::State::ashleySnapshot.stats = pending->state.ashley;
  VS::State::ashleySnapshot.stats.is_pending = 1;
  if (document.new_game_entry) {
    VS::State::savedEntryDoor = *document.new_game_entry;
    ApplyClock(0);
    SyncMonoOutput(pending->state.settings.mono_sound == 1);
    return {};
  }
  VS::State::savedEntryDoor = {};
  VS::State::savedEntryDoor.destinationZoneId =
      static_cast<unsigned char>(pending->state.location.zone);
  VS::State::savedEntryDoor.destinationRoomId =
      static_cast<unsigned char>(pending->state.location.room);
  VS::State::savedEntryDoor.requiredFlagId = pending->state.location.scene;
  VS::State::savedEntryDoor.tileLocation.entryDirection =
      pending->state.location.floor_layer;
  VS::State::savedEntryDoor.entryTileX =
      static_cast<unsigned>(pending->state.location.position[0] / kTileUnits);
  VS::State::savedEntryDoor.entryTileZ =
      static_cast<unsigned>(pending->state.location.position[2] / kTileUnits);
  VS::State::savedEntryDoor.entryElevation =
      static_cast<unsigned>(pending->state.location.facing / kQuarterTurn);
  SyncMonoOutput(pending->state.settings.mono_sound == 1);
  return {};
}

bool ResumePending() { return pending.has_value(); }

void CancelResume() {
  if (!pending) return;
  const auto& previous = pending->previous;
  ApplyPersistent(previous.state);
  VS::State::ashleySnapshot = previous.ashley;
  VS::State::savedEntryDoor = previous.entry;
  std::copy(previous.learned.begin(), previous.learned.end(),
            VS::State::learnedActionFlags);
  for (std::size_t index = 0; index < previous.unlocked.size(); ++index) {
    Battle_actionDefinitions[index].unlocked = previous.unlocked[index];
  }
  if (AudioConsumerReady()) {
    SyncMonoOutput((previous.mix_behavior & kMonoOutput) != 0);
  } else {
    VS::Core::LogWarn(
        "save",
        "CancelResume: audio consumer unavailable; restoring output latch");
  }
  VS::Audio::Driver::controlLatches.mix_behavior = previous.mix_behavior;
  if (pending->new_game) VS::State::playTime = previous.play_time;
  pending.reset();
}

std::expected<void, std::string> FinishResume() {
  if (!pending) return std::unexpected("no resume is pending");
  const auto audio = ValidateAudioConsumer();
  if (!audio) return audio;
  const auto& state = pending->state;
  if (!VS::Boot::isGameplayActive || Battle_characterState == nullptr ||
      Battle_characterState != Battle_actors[0] ||
      Battle_characterState->stats == nullptr ||
      Actors_actorRecords[0] == nullptr || !Field_roomData.isLoaded ||
      Field_roomData.environment == nullptr ||
      VS::Field::zoneContext.zndId != state.location.zone ||
      VS::Field::zoneContext.mapId != state.location.room ||
      VS::Field::zoneContext.currentSceneId != state.location.scene ||
      Field_roomData.environment->sceneId != state.location.scene ||
      VS::Field::zoneContext.mpdDiscRequest != nullptr ||
      VS::Field::zoneContext.zndDiscRequest != nullptr ||
      VS::State::ashleySnapshot.stats.is_pending != 0) {
    return std::unexpected("the staged resume room/Ashley spawn is not ready");
  }
  if (pending->new_game) {
    // The original loaded-save path lets the starting room allocate its cache,
    // camera and RNG naturally. Never apply the old campaign's field placement.
    pending.reset();
    return {};
  }
  auto& actor = *Actors_actorRecords[0];
  auto& model = actor.instance;
  RestoreAshleyValues(state.ashley, *Battle_characterState);
  model.position.vx = static_cast<std::int16_t>(state.location.position[0]);
  model.position.vy = static_cast<std::int16_t>(state.location.position[1]);
  model.position.vz = static_cast<std::int16_t>(state.location.position[2]);
  model.facing = static_cast<std::int16_t>(state.location.facing);
  model.previousFacing = state.location.previous_facing;
  model.currentTileX =
      static_cast<unsigned char>(state.location.position[0] / kTileUnits);
  model.currentTileZ =
      static_cast<unsigned char>(state.location.position[2] / kTileUnits);
  model.currentTileLayer = state.location.floor_layer;
  model.previousTileX = state.location.previous_tile.x;
  model.previousTileZ = state.location.previous_tile.z;
  model.previousTileLayer = state.location.previous_tile.layer;
  model.supportLayer = 0;
  model.placementLayer = 0;
  Actors_TileTarget saved_placement{};
  const auto& saved = state.location.saved_placement;
  saved_placement.tileX = saved.tile.x;
  saved_placement.tileLayer = saved.tile.layer;
  saved_placement.tileZ = saved.tile.z;
  saved_placement.facing = saved.facing;
  actor.savedPlacement = std::bit_cast<int>(saved_placement);
  RestoreCamera(state);
  // Room load consumes time/random numbers and may clear zone flags. Restore
  // their exact saved values before the first room event/frame can run.
  for (std::size_t index = 0; index < state.script_flags.size(); ++index) {
    VS::State::SetLiveScriptFlag(static_cast<std::int16_t>(index),
                                 static_cast<char>(state.script_flags[index]));
  }
  ApplyClock(state.play_ticks);
  VS::State::sceneTimer = state.scene_timer;
  ApplyCaches(state);
  SyncMonoOutput(state.settings.mono_sound == 1);
  VS::Core::SeedBiosRand(state.random.bios_state);
  VS::Core::interfaceRandomState = state.random.interface_state;
  std::copy(state.random.shuffle.begin(), state.random.shuffle.end(),
            VS::Core::randomShuffleTable);
  VS::Core::randomShuffleIndex = static_cast<int>(state.random.shuffle_index);
  Battle_Ai_savedMovementRandomState =
      std::bit_cast<int>(state.random.movement_state);
  VS::Battle::Ai::LiveFrameContext().randomState = state.random.movement_state;
  pending.reset();
  return {};
}

}  // namespace VS::Game::Save
