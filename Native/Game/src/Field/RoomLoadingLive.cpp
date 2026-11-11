// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomLoadingLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Battle/Ai/RoomAiMapLive.hpp"
#include "Battle/Encounter.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Runtime.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Event/RoomTriggers.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/MinimapSceneLive.h"
#include "Field/MinimapSceneLive.hpp"
#include "Field/PlatformPlacementLive.h"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomLoading.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Field/RoomStateLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Items/LootLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "State/GameClockLive.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Actors/ActorCommandsLiveState.h"
#include "Actors/CharacterInit.h"
#include "Actors/Location.h"
#include "Actors/Spawning.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/EncounterHooks.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Boot/GameLoop.h"
#include "Core/Scratch.h"
#include "DiscQueue.h"
#include "Effects/Runtime.h"
#include "Effects/Weather.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/Actors.h"
#include "Field/CameraLiveState.h"
#include "Field/Doors.h"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/Gimmicks.h"
#include "Field/Locations.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/RoomStateLiveState.h"
#include "Field/TransitionsLiveState.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Save/NativeResume.h"
#include "State/RoomLoad.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

// decomp: _zoneContext (_zoneContext@BATTLE/BATTLE.PRG)
Field_ZoneContext Field_zoneContext;
// decomp: D_800F1908 (D_800F1908@BATTLE/BATTLE.PRG)
int Field_roomResetWord;
// decomp: D_800F1BBC (D_800F1BBC@BATTLE/BATTLE.PRG)
char Field_reservedResetByte;
}

namespace {
// The ordering table entries the unload's frames clear and present.
constexpr std::size_t kOrderingEntryBytes = 4;
}  // namespace

static_assert(sizeof(Field_MpdEnemySpawn) == VS::Field::kRoomPlacementBytes);
static_assert(sizeof(Field_ZndEnemyRecord) ==
              VS::Field::kZoneEnemyTemplateBytes);
static_assert(sizeof(Actors_CharacterInit) ==
              VS::Field::kZoneEnemyTemplateBytes);
static_assert(sizeof(Content_DiscFile) == VS::Field::kRoomFileEntryBytes);
static_assert(sizeof(Field_roomData.header) ==
              VS::Field::kRoomSectionHeaderBytes);
static_assert(sizeof(Field_MpdEnvironment) == VS::Field::kRoomEnvironmentBytes);
static_assert(sizeof(u_long) == kOrderingEntryBytes);

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
// Each of the two primitive buffers a room takes (146C.c:15296-15297).
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
// Ashley drops onto the entrance from above as a walking actor
// (146C.c:15150).
constexpr int kEntranceDropHeight = -0x800;
constexpr int kEntranceMovementClass = 2;
// The zone template byte naming the enemy's model (Actors_CharacterInit).
constexpr std::size_t kTemplateModelAt = 2;
// The camera looks 90 units above Ashley's feet (146C.c:15440).
constexpr int kCameraTargetLift = 90;
// The field display (146C.c:15449).
constexpr int kFieldFrameWidth = 0x140;
constexpr int kFieldFrameHeight = 0xF0;
// The near clip and projection distance as fractions of ONE
// (146C.c:15414-15415).
constexpr int kNearClipDivisor = 64;
constexpr int kProjectionDistanceDivisor = 8;
constexpr int kRoomMusicVolume = 0x7F;
// The ticks the previous zone's music fades out over (146C.c:14821).
constexpr int kZoneMusicFadeTicks = 0x78;
// The geometry's texture ids are offset past the shared pages
// (146C.c:15712).
constexpr int kRoomTextureIdOffset = 11;
// The unload's frames draw into the ordering table 0x10 bytes into the
// frame's block and present its last entry (146C.c:15496-15500).
constexpr std::ptrdiff_t kOrderingTableOffsetEntries = 4;
constexpr int kOrderingTableEntries = 0x800;
constexpr std::size_t kColorBytes = 2;
// The scene timer's reset scopes (State_sceneTimer.resetScope).
constexpr int kRoomTimerScope = 0;
constexpr int kZoneTimerScope = 1;
constexpr int kSceneTimerScope = 2;
// The minimap scene's first room's visited word.
constexpr std::size_t kMapSceneBytes = sizeof(Field_MapScene);
constexpr std::size_t kMapRoomBytes = sizeof(Field_MapRoom);
constexpr std::size_t kVisitedAt = offsetof(Field_MapRoom, visited);
constexpr std::size_t kFirstMapRoomVisitedAt = kMapSceneBytes + kVisitedAt;
constexpr std::size_t kFirstMapRoomEnd = kMapSceneBytes + kMapRoomBytes;
constexpr std::int32_t kVisited = 1;
// The zone context's reset block (D_800F18C8) clears these words' low
// halves only (146C.c:15338-15344).
constexpr std::uint32_t kHighHalfMask = 0xFFFF0000U;

std::uint64_t refusals = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void RefuseCall(std::string_view entry, std::string_view reason) {
  if (ShouldReport(refusals)) {
    VS_LOG_WARN(kChannel, "{} refused: {} ({} refusals)", entry, reason,
                refusals);
  }
}

[[nodiscard]] std::string_view RefusalName(RoomLoadRefusal refusal) {
  switch (refusal) {
    case RoomLoadRefusal::kZoneId:
      return "zone id outside the zone table";
    case RoomLoadRefusal::kRoomFileId:
      return "room id outside the room file table";
    case RoomLoadRefusal::kZoneReadFailed:
      return "zone file read failed";
    case RoomLoadRefusal::kRoomReadFailed:
      return "room file read failed";
    case RoomLoadRefusal::kMalformedZoneFile:
      return "malformed zone file";
    case RoomLoadRefusal::kMalformedZoneEnemies:
      return "malformed zone enemy section";
    case RoomLoadRefusal::kMalformedZoneTextures:
      return "malformed zone texture section";
    case RoomLoadRefusal::kMalformedRoomFile:
      return "malformed room file";
    case RoomLoadRefusal::kMalformedRoomSections:
      return "malformed room sections";
    case RoomLoadRefusal::kEnemyTemplate:
      return "enemy template outside the zone";
    case RoomLoadRefusal::kActorSlot:
      return "actor slot outside the room's slots";
    case RoomLoadRefusal::kNoEnvironment:
      return "room without an environment section";
    case RoomLoadRefusal::kRoomEventLoad:
      return "room event import failed";
  }
  return "unknown refusal";
}

[[nodiscard]] Render_FrameScratch& Scratch() {
  return *static_cast<Render_FrameScratch*>(Core_ScratchBase());
}

[[nodiscard]] RECT ToRect(const Render::ScreenRect& area) {
  return {area.x, area.y, area.width, area.height};
}

[[nodiscard]] Field_ActorPlacement ToActorPlacement(
    const Actors::SpawnPlacement& at) {
  Field_ActorPlacement placement{};
  placement.tileX = at.tile_x;
  placement.direction = at.direction;
  placement.tileZ = at.tile_z;
  placement.height = at.height;
  return placement;
}

[[nodiscard]] Content_DiscFile ToDiscFile(Content::DiscFileExtent extent) {
  return {.lba = extent.sector, .size = extent.bytes};
}

[[nodiscard]] Content::DiscFileExtent ToExtent(const Content_DiscFile& file) {
  return {.sector = file.lba, .bytes = file.size};
}

// --- The file reads (the zone context's request and block per file)

// The block each read took and the file's size, which bounds its bytes.
struct ReadRecord {
  const void* data = nullptr;
  std::size_t bytes = 0;
};
std::array<ReadRecord, 2> read_records{};

[[nodiscard]] ReadRecord& RecordOf(LoadedFile file) {
  return read_records.at(file == LoadedFile::kZone ? 0 : 1);
}

[[nodiscard]] Content_DiscQueueSlot*& RequestOf(LoadedFile file) {
  return file == LoadedFile::kZone ? Field_zoneContext.zndDiscRequest
                                   : Field_zoneContext.mpdDiscRequest;
}

[[nodiscard]] void*& DataOf(LoadedFile file) {
  return file == LoadedFile::kZone ? Field_zoneContext.zndData
                                   : Field_zoneContext.mpdData;
}

// decomp: BATTLE.PRG _loadZnd and _loadMpd (146C.c:14803-14818 and
// 15254-15269). A block that cannot be taken leaves no request, which the
// wait refuses; the original read into a null block.
void BeginLiveRead(LoadedFile file, Content::DiscFileExtent extent) {
  const Content_DiscFile disc_file = ToDiscFile(extent);
  Content_DiscQueueSlot* request = Content::AllocateLiveDiscRequest(&disc_file);
  const auto bytes = static_cast<std::size_t>(std::max(extent.bytes, 0));
  void* data = Core::Memory::AllocateLiveFromHighEnd(bytes);
  RecordOf(file) = {};
  DataOf(file) = data;
  if (data == nullptr) {
    if (request != nullptr) {
      Content::ReleaseLiveDiscRequest(request);
    }
    RequestOf(file) = nullptr;
    return;
  }
  RecordOf(file) = {.data = data, .bytes = bytes};
  if (request != nullptr) {
    Content::EnqueueLivePriorityDiscRequest(request, data);
  } else {
    request = Content::CompleteLiveRefusedDiscRequest(&disc_file, data);
  }
  RequestOf(file) = request;
}

[[nodiscard]] std::span<const std::byte> LiveLoadedData(LoadedFile file) {
  const void* data = DataOf(file);
  if (data == nullptr) {
    return {};
  }
  const ReadRecord& record = RecordOf(file);
  std::size_t bytes = 0;
  if (record.data == data) {
    bytes = record.bytes;
  } else if (const auto capacity = Core::Memory::LivePayloadCapacity(data)) {
    bytes = *capacity;
  }
  return {static_cast<const std::byte*>(data), bytes};
}

// --- The room record's sections (Field_roomData)

template <typename Visit>
void VisitSectionPointer(RoomSectionId section, Visit&& visit) {
  Field_RoomRecord& room = Field_roomData;
  switch (section) {
    case RoomSectionId::kGeometry:
      return visit(room.geometry);
    case RoomSectionId::kCollisionMap:
      return visit(room.collisionMap);
    case RoomSectionId::kTileCells:
      return visit(room.tileCells);
    case RoomSectionId::kDoors:
      return visit(room.doors);
    case RoomSectionId::kLighting:
      return visit(room.lighting);
    case RoomSectionId::kAuxiliary:
      return visit(room.section6);
    case RoomSectionId::kSupplemental:
      return visit(room.section7);
    case RoomSectionId::kSection8:
      return visit(room.section8);
    case RoomSectionId::kFloorTriggers:
      return visit(room.floorTriggers);
    case RoomSectionId::kLockTriggers:
      return visit(room.lockTriggers);
    case RoomSectionId::kEnvironment:
      return visit(room.environment);
    case RoomSectionId::kTextureEffects:
      return visit(room.textureEffects);
    case RoomSectionId::kEffectScripts:
      return visit(room.effectScripts);
    case RoomSectionId::kSectionE:
      return visit(room.sectionE);
    case RoomSectionId::kMinimapScene:
      return visit(room.minimapScene);
    case RoomSectionId::kTiledBackground:
      return visit(room.tiledBackground);
    case RoomSectionId::kViewTriggers:
      return visit(room.viewTriggers);
    case RoomSectionId::kMovingPlatforms:
      return visit(room.movingPlatforms);
    case RoomSectionId::kContainers:
      return visit(room.containers);
    case RoomSectionId::kSoundData:
      return visit(room.soundData);
    case RoomSectionId::kSection15:
      return visit(room.section15);
    case RoomSectionId::kSection16:
      return visit(room.section16);
    case RoomSectionId::kSection17:
      return visit(room.section17);
    case RoomSectionId::kZoneBounds:
      return visit(room.zoneBounds);
  }
}

[[nodiscard]] void* SectionData(RoomSectionId section) {
  void* data = nullptr;
  VisitSectionPointer(section, [&data](auto*& field) { data = field; });
  return data;
}

void SetSectionData(RoomSectionId section, std::byte* data) {
  VisitSectionPointer(section, [data](auto*& field) {
    field = reinterpret_cast<std::remove_reference_t<decltype(field)>>(data);
  });
}

[[nodiscard]] std::array<std::uint32_t, kRoomSectionCount> SectionLengths() {
  std::array<std::uint32_t, kRoomSectionCount> lengths{};
  std::memcpy(lengths.data(), &Field_roomData.header, sizeof lengths);
  return lengths;
}

// The section, bounded by its length; empty when absent or its length is
// not positive.
[[nodiscard]] std::span<std::byte> LiveRoomSection(RoomSectionId section) {
  const auto length = static_cast<std::int32_t>(
      SectionLengths().at(static_cast<std::size_t>(section)));
  auto* data = static_cast<std::byte*>(SectionData(section));
  if (data == nullptr || length <= 0) {
    return {};
  }
  return {data, static_cast<std::size_t>(length)};
}

[[nodiscard]] RoomSections LiveRoomSections() {
  RoomSections sections{};
  for (std::size_t index = 0; index < kRoomSectionCount; ++index) {
    sections.at(index) = LiveRoomSection(static_cast<RoomSectionId>(index));
  }
  return sections;
}

[[nodiscard]] RoomEnvironmentProperties LiveEnvironment() {
  return DecodeRoomEnvironmentProperties(
             LiveRoomSection(RoomSectionId::kEnvironment))
      .value_or(RoomEnvironmentProperties{});
}

// --- The zone's tables (low-end blocks the zone context points at)

// The room file table the zone load stored and its entry count.
struct RoomFileTable {
  const Content_DiscFile* table = nullptr;
  std::size_t count = 0;
};
RoomFileTable room_files{};

void FreeLowEnd(auto*& block) {
  if (block != nullptr) {
    Core::Memory::FreeLiveFromLowEnd(block);
  }
  block = nullptr;
}

void ClearPlacements() {
  Field_zoneContext.mpdEnemies = nullptr;
  Field_zoneContext.mpdEnemyCount = 0;
}

// --- The host services, as linear mixins over each host interface

template <typename Host>
class LiveReporting : public Host {
 public:
  void Diagnostic(RoomLoadDiagnostic code) override {
    Core::IgnoreDiagnostic(static_cast<int>(code), 0);
  }

  void Refuse(RoomLoadRefusal refusal, int value) override {
    if (ShouldReport(refusals)) {
      VS_LOG_WARN(kChannel, "room load refused: {} ({}; {} refusals)",
                  RefusalName(refusal), value, refusals);
    }
  }
};

template <typename Host>
class LiveFileReads : public Host {
 public:
  [[nodiscard]] bool ReadPending(LoadedFile file) const override {
    return RequestOf(file) != nullptr;
  }

  [[nodiscard]] bool DataHeld(LoadedFile file) const override {
    return DataOf(file) != nullptr;
  }

  void BeginRead(LoadedFile file, Content::DiscFileExtent extent) override {
    BeginLiveRead(file, extent);
  }

  [[nodiscard]] DiscReadState PollRead(LoadedFile file) const override {
    const Content_DiscQueueSlot* request = RequestOf(file);
    if (request == nullptr) {
      return DiscReadState::kFailed;
    }
    switch (request->state) {
      case Content_DiscRequestState_Loaded:
        return DiscReadState::kLoaded;
      case Content_DiscRequestState_Error:
        return DiscReadState::kFailed;
      default:
        return DiscReadState::kPending;
    }
  }

  void UpdateWhileLoading() override {
    static_cast<void>(State::UpdateLivePlayTimeAndLoading(0));
  }

  void ReleaseRead(LoadedFile file) override {
    Content_DiscQueueSlot*& request = RequestOf(file);
    if (request != nullptr) {
      Content::ReleaseLiveDiscRequest(request);
    }
    request = nullptr;
  }

  [[nodiscard]] std::span<const std::byte> LoadedData(
      LoadedFile file) const override {
    return LiveLoadedData(file);
  }

  void FreeLoadedData(LoadedFile file) override {
    void*& data = DataOf(file);
    if (data != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(data);
    }
    data = nullptr;
    RecordOf(file) = {};
  }

  [[nodiscard]] SceneTimerScope SceneTimerResetScope() const override {
    switch (State_sceneTimer.resetScope) {
      case kRoomTimerScope:
        return SceneTimerScope::kRoom;
      case kZoneTimerScope:
        return SceneTimerScope::kZone;
      case kSceneTimerScope:
        return SceneTimerScope::kScene;
      default:
        return SceneTimerScope::kNone;
    }
  }

  void ResetSceneTimer() override { State::ResetLiveSceneTimer(); }
};

template <typename Host>
class LiveSections : public Host {
 public:
  void ClearRoomTint() override { LiveClearRoomTint(); }

  // decomp: BATTLE.PRG _loadMpdRoomSection (146C.c:15681-15703)
  bool StoreRoomSections(int owner_room_index, const RoomSectionLayout& layout,
                         std::span<const std::byte> sections) override {
    auto* block = static_cast<std::byte*>(
        Core::Memory::AllocateLiveFromLowEnd(layout.total_bytes));
    if (block == nullptr) {
      return false;
    }
    InvalidateLiveInteractionTargets();
    std::memcpy(&Field_roomData.header, layout.lengths.data(),
                sizeof layout.lengths);
    std::ranges::copy(
        sections.first(std::min(sections.size(), layout.total_bytes)), block);
    for (std::size_t index = 0; index < kRoomSectionCount; ++index) {
      SetSectionData(static_cast<RoomSectionId>(index),
                     layout.lengths.at(index) > 0
                         ? block + layout.offsets.at(index)
                         : nullptr);
    }
    Field_roomData.isLoaded = 1;
    Field_roomData.displayFlag = 0;
    Field_roomData.roomIndex = static_cast<std::int16_t>(owner_room_index);
    return true;
  }

  [[nodiscard]] std::span<std::byte> RoomSection(
      RoomSectionId section) override {
    return LiveRoomSection(section);
  }

  void OffsetRoomTextureIds() override {
    LiveOffsetRoomTextureIds(kRoomTextureIdOffset, Field_roomData.geometry);
  }

  // decomp: BATTLE.PRG _loadMpdRoomSection (146C.c:15726-15729)
  void PrepareMinimapScene() override {
    const auto scene = LiveRoomSection(RoomSectionId::kMinimapScene);
    ResolveLiveSceneRoomData(scene.data(), scene.size());
    RestoreLiveSceneRoomVisits(scene.data(), scene.size());
    if (scene.size() >= kFirstMapRoomEnd) {
      std::memcpy(scene.subspan(kFirstMapRoomVisitedAt).data(), &kVisited,
                  sizeof kVisited);
    }
  }

  [[nodiscard]] int PendingEnteredDoor() const override {
    return static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.doorEntered);
  }

  void ClearPendingEnteredDoor() override {
    Audio_Seq_unusedStateFlags.doorEntered = 0;
  }

  [[nodiscard]] int EnteredDoorId() const override {
    return Field_enteredDoorId;
  }

  void MarkEntranceDoor(int door) override {
    Field_MarkEntranceDoorOnSceneMapEntry(door);
  }

  // decomp: BATTLE.PRG _loadMpdRoomSection (146C.c:15735-15743)
  void InitializeRoomSystems() override {
    Field_ResetGimmickAnimationQueueEntry();
    LiveInitializeRoomEffects();
    Items::RestoreLiveRoomChestStates();
    Field_InitializeMovingPlatformsEntry();
    Battle_ResetFloatingLabels();
    Effects_AllocateWeatherParticles();
    LiveInitializeRoomBackgroundColors();
    LiveUploadRoomPalettes();
    Field_reservedResetByte = 0;
  }

  void WaitForOverlayReady() override { Boot::WaitForOverlayReadyLive(); }

  [[nodiscard]] RoomDisplay LoadedRoom() const override {
    return {.loaded = Field_roomData.isLoaded == 1,
            .displayed = Field_roomData.displayFlag != 0,
            .owner_room_index = Field_roomData.roomIndex};
  }

  void ReleaseRoomResources() override {
    static_cast<void>(LiveReleaseTiledBackground());
    LiveReleaseWeatherParticles();
    static_cast<void>(LiveReleaseRoomEffects());
  }

  // decomp: BATTLE.PRG func_8008ABF0 (146C.c:15780-15784)
  // #311: the block starts at the first present section; the original freed
  // the geometry and leaked the block of a room without one. A section the
  // heap did not give (a test's own record) is not freed.
  void FreeRoomSections() override {
    InvalidateLiveInteractionTargets();
    for (std::size_t index = 0; index < kRoomSectionCount; ++index) {
      void* data = SectionData(static_cast<RoomSectionId>(index));
      if (data == nullptr) {
        continue;
      }
      if (Core::Memory::LivePayloadCapacity(data).has_value()) {
        Core::Memory::FreeLiveFromLowEnd(data);
      }
      break;
    }
    Field_roomData = Field_RoomRecord{};
  }
};

template <typename Host>
class LiveSpawning : public Host {
 public:
  [[nodiscard]] std::uint8_t ScriptFlag(int flag) const override {
    return State::LiveScriptFlag(flag);
  }

  [[nodiscard]] int AshleyCurrentHp() const override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      return 0;
    }
    return Battle_characterState->stats->currentHp;
  }

  [[nodiscard]] std::span<const std::byte> PlacementRecords() const override {
    if (Field_zoneContext.mpdEnemies == nullptr) {
      return {};
    }
    return std::as_bytes(std::span<const Field_MpdEnemySpawn>(
        Field_zoneContext.mpdEnemies, Field_zoneContext.mpdEnemyCount));
  }

  [[nodiscard]] bool ActorSlotOccupied(int slot) const override {
    return slot >= 0 &&
           static_cast<std::size_t>(slot) < std::size(Battle_actors) &&
           Battle_actors[slot] != nullptr;
  }

  [[nodiscard]] int ZoneEnemyCount() const override {
    return Field_zoneContext.zndEnemies != nullptr
               ? Field_zoneContext.zndEnemyCount
               : 0;
  }

  [[nodiscard]] bool TemplateUsesSecondaryBody(int enemy_id) const override {
    if (enemy_id < 0 || enemy_id >= ZoneEnemyCount()) {
      return false;
    }
    const Field_ZndEnemyRecord& record = Field_zoneContext.zndEnemies[enemy_id];
    return Actors_usesSecondaryBodyModel[record
                                             .recordBytes[kTemplateModelAt]] !=
           0;
  }

  // The template is copied: the spawn decodes it and keeps no pointer.
  void SpawnEnemy(int slot, std::size_t placement_index, int enemy_id,
                  int flags) override {
    if (placement_index >= Field_zoneContext.mpdEnemyCount || enemy_id < 0 ||
        enemy_id >= ZoneEnemyCount()) {
      this->Refuse(RoomLoadRefusal::kEnemyTemplate, enemy_id);
      return;
    }
    Actors_CharacterInit init{};
    std::memcpy(&init, &Field_zoneContext.zndEnemies[enemy_id], sizeof init);
    static_cast<void>(Actors::SpawnLiveRoomEnemy(
        slot, &Field_zoneContext.mpdEnemies[placement_index], &init, flags));
  }

  void SpawnUntemplatedEnemy(int slot, const Actors::SpawnPlacement& placement,
                             int flags) override {
    Field_ActorPlacement at = ToActorPlacement(placement);
    static_cast<void>(Actors::CreateLiveActorWithEquipment(
        slot, kUntemplatedEnemyId, 0, 0, 0, 0, &at, flags));
  }

  void SpawnObject(int slot, int model, const Actors::SpawnPlacement& placement,
                   int material) override {
    Field_ActorPlacement at = ToActorPlacement(placement);
    static_cast<void>(
        Actors::CreateLiveAuxiliaryActor(slot, model, &at, material, 0));
  }

  void RememberHidden(int spawn_index, std::uint8_t hidden) override {
    Actors::SetLiveRememberedEnemyHidden(spawn_index, hidden);
  }

  void FreeCachedZud() override {
    if (Field_zoneContext.zudData != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(Field_zoneContext.zudData);
    }
    Field_zoneContext.zudData = nullptr;
    Field_zoneContext.loadedZudIndexPlusOne = 0;
  }
};

template <typename Host>
class LiveEntrance : public Host {
 public:
  [[nodiscard]] RoomEntrance PendingEntrance() const override {
    const Field_MpdDoorRecord& door = Field_pendingDestinationDoor;
    return {.tile_x = static_cast<std::uint8_t>(door.entryTileX),
            .tile_z = static_cast<std::uint8_t>(door.entryTileZ),
            .direction =
                static_cast<std::uint8_t>(door.tileLocation.entryDirection),
            .elevation = static_cast<std::uint8_t>(door.entryElevation),
            .has_entry_step = door.hasEntryStep != 0};
  }

  void PlaceAshley(const Actors::SpawnPlacement& placement) override {
    const Field_ActorPlacement at = ToActorPlacement(placement);
    Actors::PlaceLiveActorInRoom(0, &at, kEntranceDropHeight,
                                 kEntranceMovementClass);
  }

  void StepAshley(const EntryStep& step) override {
    SVECTOR offset{};
    offset.vx = static_cast<std::int16_t>(step.x);
    offset.vz = static_cast<std::int16_t>(step.z);
    Actors::TranslateLiveActorByAtRate(0, &offset, 0);
  }

  // decomp: BATTLE.PRG func_80089888 (146C.c:15184-15188)
  void SettleAshley() override {
    Actors_ActorLocationQuery location{};
    static_cast<void>(Actors::LiveActorPlacement(0, &location));
    Actors::RestoreLiveActorIdlePose(0);
    Actors::BuildLiveActorPoseAtMatrix(0, &Scratch().viewMatrix);
    SetLiveActorCollisionBypass(0, 0);
    Battle::Ai::InitializeLiveRoomNavigation();
  }
};

template <typename Host>
class LivePlacements : public Host {
 public:
  [[nodiscard]] std::span<const std::byte> EncounterPlacements(
      std::span<const std::byte> room) override {
    // The hook replaces the pointer and size; it does not write the records.
    void* data = const_cast<std::byte*>(room.data());
    auto bytes = static_cast<unsigned int>(room.size());
    Boot_EncounterRoomPlacements(&data, &bytes);
    if (data == nullptr) {
      return {};
    }
    return {static_cast<const std::byte*>(data), bytes};
  }

  // decomp: BATTLE.PRG _loadMpdEnemySection (146C.c:15241-15250)
  void StorePlacements(std::span<const std::byte> records) override {
    if (records.empty()) {
      ClearPlacements();
      return;
    }
    auto* block = static_cast<std::byte*>(
        Core::Memory::AllocateLiveFromLowEnd(records.size()));
    if (block == nullptr) {
      ClearPlacements();
      return;
    }
    std::ranges::copy(records, block);
    Field_zoneContext.mpdEnemies =
        reinterpret_cast<Field_MpdEnemySpawn*>(block);
    Field_zoneContext.mpdEnemyCount =
        static_cast<u_int>(records.size() / kRoomPlacementBytes);
  }
};

// --- The hosts

class LiveZoneLoadHost final
    : public LiveFileReads<LiveReporting<ZoneLoadHost>> {
 public:
  // decomp: BATTLE.PRG _loadMpdLbas (146C.c:14774)
  void StoreRoomFiles(std::span<const Content::DiscFileExtent> files) override {
    Field_zoneContext.mpdFiles = nullptr;
    room_files = {};
    if (files.empty()) {
      return;
    }
    auto* table =
        static_cast<Content_DiscFile*>(Core::Memory::AllocateLiveFromLowEnd(
            files.size_bytes() / sizeof(Content::DiscFileExtent) *
            sizeof(Content_DiscFile)));
    if (table == nullptr) {
      return;
    }
    std::ranges::transform(files, table, ToDiscFile);
    Field_zoneContext.mpdFiles = table;
    room_files = {.table = table, .count = files.size()};
  }

  // decomp: BATTLE.PRG _loadZndEnemies (146C.c:14783)
  void StoreZoneEnemies(std::span<const Content::DiscFileExtent> zud_files,
                        std::span<const std::byte> templates) override {
    Field_zoneContext.zndEnemyCount = static_cast<int>(zud_files.size());
    Field_zoneContext.zudFiles = nullptr;
    Field_zoneContext.zndEnemies = nullptr;
    if (zud_files.empty()) {
      return;
    }
    auto* files =
        static_cast<Content_DiscFile*>(Core::Memory::AllocateLiveFromLowEnd(
            zud_files.size() * sizeof(Content_DiscFile)));
    auto* records = static_cast<std::byte*>(
        Core::Memory::AllocateLiveFromLowEnd(templates.size()));
    if (files == nullptr || records == nullptr) {
      FreeLowEnd(records);
      FreeLowEnd(files);
      Field_zoneContext.zndEnemyCount = 0;
      return;
    }
    std::ranges::transform(zud_files, files, ToDiscFile);
    std::ranges::copy(templates, records);
    Field_zoneContext.zudFiles = files;
    Field_zoneContext.zndEnemies =
        reinterpret_cast<Field_ZndEnemyRecord*>(records);
  }

  void CopyPaletteRow(std::span<const std::byte> colors,
                      const ZonePaletteRow& row) override {
    const auto needed =
        static_cast<std::size_t>(std::max(row.colors, 0)) * kColorBytes;
    if (colors.size() < needed) {
      Refuse(RoomLoadRefusal::kMalformedZoneTextures, row.row);
      return;
    }
    LiveCopyZonePaletteRow(colors.data(), row.row, row.first_color, row.colors);
  }

  void UploadImage(const Render::ScreenRect& area,
                   std::span<const std::byte> pixels) override {
    const std::size_t bytes = area.width < 0 || area.height < 0
                                  ? 0
                                  : static_cast<std::size_t>(area.width) *
                                        static_cast<std::size_t>(area.height) *
                                        kColorBytes;
    if (area.width < 0 || area.height < 0 || pixels.size() < bytes) {
      Refuse(RoomLoadRefusal::kMalformedZoneTextures, area.x);
      return;
    }
    RECT rect = ToRect(area);
    // libgpu's LoadImage only reads the pixels.
    ::LoadImage(&rect, reinterpret_cast<u_long*>(
                           const_cast<std::byte*>(pixels.data())));
  }

  [[nodiscard]] Content::DiscFileExtent ZoneFile(int zone_id) const override {
    return ToExtent(Field_legacyZoneDiscFiles[zone_id]);
  }

  [[nodiscard]] int ZoneMusicId(int zone_id) const override {
    return static_cast<std::uint8_t>(Field_zoneMusicIds[zone_id]);
  }

  [[nodiscard]] int CurrentMusicId() const override {
    return Audio::LiveCurrentMusicId();
  }

  [[nodiscard]] bool MusicFadeEnabled() const override {
    return Field_zoneContext.isMusicFadeEnabled != 0;
  }

  void FadeOutCurrentMusic() override {
    static_cast<void>(Audio::LiveFadeMusicSlotVolume(
        Audio_soundState.currentMusicSlot, 0, kZoneMusicFadeTicks));
  }

  [[nodiscard]] bool NativeResumePending() const override {
    return Save_NativeResumePending() != 0;
  }

  [[nodiscard]] int GameEntryMode() const override {
    return Boot_gameEntryMode;
  }

  void ResetZoneScriptFlags(bool new_game) override {
    State::ResetLiveZoneScriptFlags(new_game ? 1 : 0);
  }

  void StopAndFreeCurrentMusic() override {
    Audio::LiveStopAndFreeCurrentMusic();
  }

  void FreeMusicSlot(int slot) override {
    static_cast<void>(Audio::LiveFreeMusicSlot(slot));
  }

  void LoadWaveBank(int music_id) override {
    Audio::LiveLoadWaveBankBlocking(music_id);
  }

  [[nodiscard]] int LoadMusicSlot(int music_id, int target_slot) override {
    return Audio::LiveLoadMusicSlotBlocking(music_id, target_slot);
  }

  void SetMusicSlot(int slot) override { Field_zoneContext.musicSlot = slot; }

  // decomp: BATTLE.PRG _finishLoadZnd (146C.c:14876-14878)
  void FinishZoneMusic(int music_id) override {
    Field_zoneContext.musicId = music_id;
    Field_zoneContext.isMusicFadeEnabled = 1;
    Field_zoneContext.reserved38 = 0;
  }
};

class LiveZoneReleaseHost final : public ZoneReleaseHost {
 public:
  void ReleaseZoneEventResources() override {
    VS::Event::ReleaseZoneEventResources();
  }

  // decomp: BATTLE.PRG func_80089098 (146C.c:14889-14896); the original
  // left the freed tables set.
  void FreeZoneTables() override {
    FreeLowEnd(Field_zoneContext.zndEnemies);
    FreeLowEnd(Field_zoneContext.zudFiles);
    FreeLowEnd(Field_zoneContext.mpdFiles);
    Field_zoneContext.zndEnemyCount = 0;
    room_files = {};
  }
};

using LiveActorSpawnBase = LiveSpawning<LiveReporting<RoomActorSpawnHost>>;
class LiveActorSpawnHost final : public LiveActorSpawnBase {};

class LiveRoomLoadHost final
    : public LivePlacements<LiveEntrance<LiveSpawning<
          LiveSections<LiveFileReads<LiveReporting<RoomLoadHost>>>>>> {
 public:
  void ResetGameplayInspection() override { Boot_ResetGameplayInspection(); }

  [[nodiscard]] Content::DiscFileExtent RoomFile(int room_id) const override {
    return ToExtent(room_files.table[room_id]);
  }

  // Only the table the zone load stored is read.
  [[nodiscard]] int RoomFileCount() const override {
    return room_files.table != nullptr &&
                   room_files.table == Field_zoneContext.mpdFiles
               ? static_cast<int>(room_files.count)
               : 0;
  }

  // decomp: BATTLE.PRG func_80089DC0 (146C.c:15296-15299)
  void AllocatePrimitiveBuffers() override {
    Render_legacyPrimitiveBuffers[0] =
        Core::Memory::AllocateLiveFromLowEnd(kPrimitiveBufferBytes);
    Render_legacyPrimitiveBuffers[1] =
        Core::Memory::AllocateLiveFromLowEnd(kPrimitiveBufferBytes);
    Render::InitializeLiveClutAnimation();
  }

  [[nodiscard]] CurrentRoom PendingDestination() const override {
    return {.zone_id = Field_pendingDestinationDoor.destinationZoneId,
            .room_id = Field_pendingDestinationDoor.destinationRoomId};
  }

  void SetCurrentRoom(const CurrentRoom& room) override {
    Field_zoneContext.zndId = room.zone_id;
    Field_zoneContext.mapId = room.room_id;
  }

  [[nodiscard]] int LoadedSceneId() const override {
    return Field_zoneContext.currentSceneId;
  }

  void SetLoadedSceneId(int scene_id) override {
    Field_zoneContext.currentSceneId = scene_id;
  }

  void ClearRoomEnemyMemory() override { Actors::ClearLiveRoomEnemyMemory(); }

  [[nodiscard]] Actors::RoomEnemyMemory LoadRoomEnemyMemory() const override {
    return Actors::LoadLiveRoomEnemyMemory();
  }

  void StoreRoomEnemyMemory(const Actors::RoomEnemyMemory& memory) override {
    Actors::StoreLiveRoomEnemyMemory(memory);
  }

  void SetPlacementHidden(std::size_t placement_index,
                          std::uint8_t hidden) override {
    if (Field_zoneContext.mpdEnemies != nullptr &&
        placement_index < Field_zoneContext.mpdEnemyCount) {
      Field_zoneContext.mpdEnemies[placement_index].isHidden = hidden;
    }
  }

  void MarkRoomVisitPending() override { Field_isRoomVisitPending = 1; }

  [[nodiscard]] int CollisionMapDimensions() const override {
    return LiveCollisionMapDimensions(0);
  }

  [[nodiscard]] std::optional<ZoneBounds> RoomCameraBounds() const override {
    const Field_ZoneBounds* bounds = LiveRoomCameraBounds(0);
    if (bounds == nullptr) {
      return std::nullopt;
    }
    return ZoneBounds{.min_x = bounds->minX,
                      .min_z = bounds->minZ,
                      .max_x = bounds->maxX,
                      .max_z = bounds->maxZ};
  }

  void SetZoneExtent(const ZoneTileExtent& extent,
                     const ZoneBounds& bounds) override {
    Field_zoneContext.tileColumns = extent.columns;
    Field_zoneContext.tileRows = extent.rows;
    Field_zoneContext.bounds.minX = bounds.min_x;
    Field_zoneContext.bounds.minZ = bounds.min_z;
    Field_zoneContext.bounds.maxX = bounds.max_x;
    Field_zoneContext.bounds.maxZ = bounds.max_z;
  }

  // decomp: BATTLE.PRG func_80089DC0 (146C.c:15338-15344): the halfwords at
  // 0x48, 0x4A, 0x4C, 0x50, 0x52 and 0x54.
  void ClearRoomResetWords() override {
    Field_zoneContext.reserved48 = 0;
    Field_zoneContext.reserved4C = static_cast<int>(
        static_cast<std::uint32_t>(Field_zoneContext.reserved4C) &
        kHighHalfMask);
    Field_zoneContext.reserved50 = 0;
    Field_zoneContext.reserved54 = static_cast<int>(
        static_cast<std::uint32_t>(Field_zoneContext.reserved54) &
        kHighHalfMask);
  }

  void CaptureBackgroundColors() override {
    Render::CaptureLiveRoomBackgroundColors();
  }

  void LoadChestLoot(std::span<const std::byte> loot) override {
    Items::LoadLiveRoomChestLoot(loot.data());
  }

  [[nodiscard]] bool EncounterUsesRoomEvents() const override {
    return Boot_EncounterUsesRoomEvents() != 0;
  }

  [[nodiscard]] Event::RoomEventLoad LoadEncounterRoomEvents() override {
    return Game::LoadEncounterRoomEvents();
  }

  [[nodiscard]] Event::RoomEventLoad LoadRoomEvents(
      const RoomEventSections& sections) override {
    const auto data = [](std::span<const std::byte> section) {
      return Event::ScriptCommand{
          reinterpret_cast<const std::uint8_t*>(section.data()),
          section.size()};
    };
    return Event::LoadLiveRoomEventSections(
        {.triggers = data(sections.cleared),
         .entry_script = data(sections.script),
         .doors = data(sections.doors),
         .first_entry = sections.first_entry});
  }

  [[nodiscard]] int GameEntryMode() const override {
    return Boot_gameEntryMode;
  }

  void SetGameEntryMode(int mode) override { Boot_gameEntryMode = mode; }

  void SnapActorsToSupport() override { SnapLiveRoomActorsToSupport(); }

  void ClearAshleyStatuses(int statuses) override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      RefuseCall("ClearAshleyStatuses", "Ashley has no stats");
      return;
    }
    Battle_ActorStats* stats = Battle_characterState->stats;
    static_cast<void>(
        Battle_QueueActorStatusLabels(0, 0, stats->statuses & statuses, 1));
    Battle_RemoveActorStatuses(statuses, stats);
  }

  // decomp: BATTLE.PRG func_80089DC0 (146C.c:15392-15405)
  void ResetRoomEntryState() override {
    Field_roomGeometryDrawMode = 0;
    Field_isFirstPersonViewEnabled = 0;
    Actors_ashleyMoveInput[0] = 1;
    Effects_effectBusyFlags = 0;
    static_cast<void>(Actors::SetOtherLiveActorLogicEnabled(1));
    State_roomLoadResetWordTwo = 0;
    State_roomLoadResetWordThree = 0;
    State_roomLoadResetWordOne = 0;
    Input_buttonRepeat = 0;
    Input_releasedButtons = 0;
    Input_buttonsPressed.allButtons = 0;
    Input_previousButtons = 0;
    Input_buttonsState = 0;
    Scratch().camera.farClip = ONE;
  }

  [[nodiscard]] int ScreenTransitionEffect() const override {
    return Field_screenTransitionEffect;
  }

  void SetScreenTransition(int step, bool clear_alpha) override {
    Field_screenTransitionStep = static_cast<unsigned int>(step);
    if (clear_alpha) {
      Field_screenTransitionAlpha = 0;
    }
  }

  [[nodiscard]] int InitialCameraDistance() const override {
    return Field_sphericalCamera.initialDistance;
  }

  // decomp: BATTLE.PRG func_80089DC0 (146C.c:15414-15435)
  void ResetCamera(const RoomEntryCamera& camera) override {
    Render::SetLiveCameraNearClip(ONE / kNearClipDivisor);
    Render::SetLiveCameraProjectionDistance(ONE / kProjectionDistanceDivisor);
    Field_SphericalCamera& orbit = Field_sphericalCamera;
    orbit.delta.yaw = 0;
    orbit.delta.distance = 0;
    orbit.values.yaw = orbit.delta.pitch;
    orbit.values.distance = orbit.initialDistance;
    orbit.delta.mode = camera.mode;
    orbit.values.pitch = camera.pitch;
    orbit.zoomVelocity = 0;
    Field_roomResetWord = 0;
  }

  // decomp: BATTLE.PRG func_80089DC0 (146C.c:15437-15449)
  void FrameCameraOnAshley() override {
    Actors_ActorLocationQuery ashley{};
    static_cast<void>(Actors::LiveActorPlacement(0, &ashley));
    const SVECTOR& feet = ashley.location.position;
    const VECTOR target{.vx = feet.vx * ONE,
                        .vy = (feet.vy * ONE) - (kCameraTargetLift * ONE),
                        .vz = feet.vz * ONE,
                        .pad = 0};
    VECTOR look_at{};
    LiveClampCameraLookAtToZone(&target, &look_at);
    Render_FrameScratch& scratch = Scratch();
    scratch.camera.lookAt = look_at;
    scratch.camera.angles.vy = Field_sphericalCamera.values.yaw;
    LiveSetCameraPositionFromAngles(&Field_sphericalCamera.values);
    static_cast<void>(Actors::LiveActorPlacement(0, &ashley));
    LiveUpdateCameraTileConstraint(0, &ashley.location.packedTile.target);
    Render::ResetLiveFramebufferEffect();
    Render::CommitLiveClutRows();
    Render::ConfigureLiveFieldDisplay(kFieldFrameWidth, kFieldFrameHeight,
                                      Render_projectionDistance, 0, 0, 0);
  }

  [[nodiscard]] bool NativeResumePending() const override {
    return Save_NativeResumePending() != 0;
  }

  [[nodiscard]] bool FinishNativeResume() override {
    return Save_NativeFinishResume() != 0;
  }

  void JumpToTitle() override { Boot::JumpToLiveTitle(); }

  [[nodiscard]] bool RoomEventTriggered() override {
    return Event_EvaluateRoomEventTriggersEntry(1, 0, nullptr, 0) == 1;
  }

  void RequestEventStart() override {
    static_cast<void>(VS::Event::RequestEventStart(VS::Event::LiveScriptVm()));
  }

  void InitializeWeather(bool event_running) override {
    static_cast<void>(Effects_InitializeRoomWeather(event_running ? 1 : 0));
  }

  void PrepareCutsceneActors() override { Event::PrepareLiveCutsceneActors(); }

  // decomp: BATTLE.PRG func_80089DC0 (146C.c:15466-15471)
  void EnterExploration() override {
    LiveShowPrimaryRoomGeometry();
    Actors::EnableAllLiveActorModels();
    static_cast<void>(Effects_InitializeRoomWeather(0));
    LiveRestoreRoomEffectSoundVolume();
    LiveEnterFieldExplorationMode();
    Battle_SetHudHidden(0);
  }

  [[nodiscard]] int RoomMusicSlot() const override {
    return Field_zoneContext.musicSlot;
  }

  void PlayRoomMusic(int slot) override {
    static_cast<void>(Audio::LivePlayMusicSlot(slot, kRoomMusicVolume, 0));
  }

  void ShowDisplay() override { Render::ShowLiveDisplay(); }

  void InvalidateControllerConnections() override {
    Input::InvalidateLiveControllerConnections();
  }
};

class LiveRoomUnloadHost final
    : public LiveSections<LiveReporting<RoomUnloadHost>> {
 public:
  void ResetGameplayInspection() override { Boot_ResetGameplayInspection(); }

  // decomp: BATTLE.PRG func_8008A3A0 (146C.c:15495-15501)
  void DrawTransitionFrame() override {
    const int buffer = Render_frameBuf;
    if (buffer < 0 ||
        static_cast<std::size_t>(buffer) >=
            std::size(Render_legacyOrderingTables) ||
        Render_legacyOrderingTables[buffer] == nullptr) {
      RefuseCall("DrawTransitionFrame", "no ordering table for the frame");
      return;
    }
    u_long* ordering_table =
        static_cast<u_long*>(Render_legacyOrderingTables[buffer]) +
        kOrderingTableOffsetEntries;
    Scratch().orderingTable = ordering_table;
    ClearOTagR(ordering_table, kOrderingTableEntries);
    Render::DrawLiveScreenTransition(ordering_table);
    static_cast<void>(Render::PresentLiveFieldFrame(
        ordering_table + (kOrderingTableEntries - 1)));
  }

  void UpdateWhileLoading() override {
    static_cast<void>(State::UpdateLivePlayTimeAndLoading(0));
  }

  void FreeRoomEventSections() override { Event::FreeLiveRoomScripts(); }

  void MarkCurrentRoomVisited() override { LiveMarkCurrentRoomVisited(); }

  void FreePlacements() override {
    FreeLowEnd(Field_zoneContext.mpdEnemies);
    Field_zoneContext.mpdEnemyCount = 0;
  }

  void FreeScriptedActorAi() override {
    if (Battle_actors[1] != nullptr) {
      Battle::Ai::FreeLiveActorAiState(Battle_actors[1]);
    }
  }

  void RemoveActor(int slot) override {
    static_cast<void>(
        Actors::RemoveLiveActorAtSlot(static_cast<unsigned int>(slot), 0));
  }

  void ResetRoomActorState() override {
    Battle::Ai::FreeRoomAiState(Battle::Ai::BattleRoomAiRuntime());
    Battle_SetActorMarker(0, 0);
    Actors::ResetAllLiveActorMovement();
    Items::ClearLiveRoomLootList();
  }

  // The original freed both and left them set.
  void FreePrimitiveBuffers() override {
    FreeLowEnd(Render_legacyPrimitiveBuffers[1]);
    FreeLowEnd(Render_legacyPrimitiveBuffers[0]);
  }
};
}  // namespace

void LiveQueueZoneLoad(int zone_id) {
  LiveZoneLoadHost host;
  static_cast<void>(QueueZoneLoad(host, zone_id));
}

void LiveFinishZoneLoad(int zone_id) {
  LiveZoneLoadHost host;
  FinishZoneLoad(host, zone_id);
}

void LiveReleaseZoneData() {
  InvalidateLiveInteractionTargets();
  LiveZoneReleaseHost host;
  ReleaseZoneData(host);
}

int LiveZoneSoundFileId() { return ZoneSoundFileId(Field_zoneContext.musicId); }

void LiveSpawnRoomActors() {
  LiveActorSpawnHost host;
  SpawnRoomActors(host);
}

void LiveQueueRoomLoad(int room_id) {
  LiveRoomLoadHost host;
  static_cast<void>(QueueRoomLoad(host, room_id));
}

bool LiveFinishRoomLoad(int room_id) {
  LiveRoomLoadHost host;
  return FinishRoomLoad(host, room_id);
}

void LiveUnloadCurrentRoom() {
  InvalidateLiveInteractionTargets();
  LiveRoomUnloadHost host;
  UnloadCurrentRoom(host);
}

int LiveIsCurrentRoomWorkshop() {
  return IsCurrentRoomWorkshop(LiveEnvironment());
}

int LiveCurrentSceneId() { return CurrentSceneId(LiveEnvironment()); }

int LiveRoomFallDestination() { return RoomFallDestination(LiveEnvironment()); }

int LiveIsRoomAudioSequenceAbsent() {
  return IsRoomAudioSequenceAbsent(LiveRoomSections()) ? 1 : 0;
}

std::int16_t LiveRoomEnvironmentParameter() {
  return static_cast<std::int16_t>(RoomEnvironmentParameter(LiveEnvironment()));
}

int LiveRoomTrapLevel() { return RoomTrapLevel(LiveEnvironment()); }
}  // namespace VS::Field
