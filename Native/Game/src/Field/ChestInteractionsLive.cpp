#include "Field/ChestInteractionsLive.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/ChestInteractions.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/DoorMatchesLive.hpp"
#include "Field/DoorRecordLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/MinimapSceneLive.h"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomSwitchesLive.h"
#include "Field/TransitionsLive.hpp"
#include "Field/TrapsLive.hpp"
#include "Input/ControllerLiveState.h"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Location.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/InteractionFocusLive.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
#include "Items/TreasureChests.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/TextBox.h"
#include "State/StateFlags.h"
#include "State/Timers.h"
}

namespace {

constexpr std::int32_t kActorSlots = 17;
constexpr std::int32_t kIdleActorCount = 16;
constexpr std::uint32_t kChestControlMode = 9;
constexpr std::uint32_t kTrapControlMode = 8;
constexpr std::int32_t kFinishedActionFlow = 4;
constexpr std::int32_t kContentsActionFlow = 3;
constexpr std::int32_t kUnlockedNotification = 4;
constexpr std::int32_t kTrapNotification = 0xB;
constexpr std::int32_t kTrapActionOffset = 6;
constexpr std::int32_t kTrapTickSpeed = 4;
constexpr std::size_t kPoseWords = 4;
constexpr std::size_t kWorldLabelCoordinates = 3;
constexpr std::size_t kOriginalLockRecordBytes = 0x14;
constexpr std::size_t kOriginalDoorRecordBytes = sizeof(Field_MpdDoorRecord);
constexpr std::int32_t kRoomObjectSoundGroup = 0x7E;
constexpr std::int32_t kTrapPrimaryTargetPosition = 3;
constexpr std::uint16_t kTrapSourcePointType = 5;
constexpr std::uint16_t kTrapActorTargetType = 4;
constexpr std::int32_t kFacingInteractionMode = 4;
constexpr std::size_t kFacingPositionBytes = 4;
constexpr std::int32_t kFacingTileUnits = 128;
constexpr std::int32_t kFacingTileCenter = 64;
constexpr std::uint32_t kActorEventControlMode = 3;
constexpr std::int32_t kActorFallEventRequest = 0xC;
constexpr std::size_t kTriggerModeByte = 3;

Battle_Actor* TileTriggerActor(std::size_t index) {
  const std::span actors(Battle_actors);
  return index < actors.size() ? actors[index] : nullptr;
}

class LiveAshleyInteractionTileHost final
    : public VS::Field::AshleyInteractionTileHost {
 public:
  bool ReleaseTile(std::int32_t tile_x, std::int32_t tile_z) override {
    constexpr auto kSide = VS::Battle::Ai::kNavigationGridSide;
    if (!Valid() ||
        (flags_ != nullptr &&
         (tile_x < 0 || tile_z < 0 || tile_x >= kSide || tile_z >= kSide))) {
      return false;
    }
    VS::Battle::Ai::ReleaseLiveAiDestinationTile(tile_x, tile_z);
    return Valid();
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           flags_ == Battle_Ai_tileFlagMap;
  }
  std::uint64_t generation_ =
      VS::Field::LiveInteractionTargetDomain().generation;
  decltype(Battle_Ai_tileFlagMap) flags_ = Battle_Ai_tileFlagMap;
};

class LiveSavePointQueryHost final : public VS::Field::SavePointQueryHost {
 public:
  std::optional<VS::Actors::ActorPlacementView> PlayerPlacement() override {
    return Valid() ? VS::Actors::LiveActorPlacementView(0) : std::nullopt;
  }
  std::uint32_t PlayerStateFlags() override {
    return Valid() ? VS::Actors::LiveActorStateFlags(0) : 0;
  }
  bool HasRoomTrigger(const VS::Actors::PackedTile& tile) override {
    if (!Valid()) return false;
    const auto* cell =
        VS::Field::LiveCollisionCellOrAbsent(tile.x, tile.z, tile.layer);
    return Valid() && (cell->hasRoomTrigger & 1) != 0;
  }
  std::optional<VS::Field::SavePointTriggerState> Trigger(
      const VS::Actors::PackedTile& tile) override {
    if (!Valid()) return std::nullopt;
    const auto target =
        VS::Field::FindLiveRoomTriggerAtTile({tile.x, tile.z, tile.layer});
    const auto* record =
        target ? VS::Field::ResolveLiveTrapTarget(*target) : nullptr;
    if (!Valid() || record == nullptr) return std::nullopt;
    return VS::Field::SavePointTriggerState{
        record->kind, std::bit_cast<std::uint8_t>(record->activation)};
  }

 private:
  bool Valid() const {
    return generation_ == VS::Field::LiveInteractionTargetDomain().generation &&
           collision_ == Field_roomData.collisionMap &&
           collision_bytes_ == Field_roomData.header.collisionMapLength &&
           cells_ == Field_roomData.tileCells &&
           cell_bytes_ == Field_roomData.header.tileCellsLength &&
           triggers_ == Field_roomData.floorTriggers &&
           trigger_bytes_ == Field_roomData.header.floorTriggersLength;
  }
  std::uint64_t generation_ =
      VS::Field::LiveInteractionTargetDomain().generation;
  decltype(Field_roomData.collisionMap) collision_ =
      Field_roomData.collisionMap;
  std::uint32_t collision_bytes_ = Field_roomData.header.collisionMapLength;
  decltype(Field_roomData.tileCells) cells_ = Field_roomData.tileCells;
  std::uint32_t cell_bytes_ = Field_roomData.header.tileCellsLength;
  decltype(Field_roomData.floorTriggers) triggers_ =
      Field_roomData.floorTriggers;
  std::uint32_t trigger_bytes_ = Field_roomData.header.floorTriggersLength;
};

class LiveActorTileTriggerHost final : public VS::Field::ActorTileTriggerHost {
 public:
  std::uint32_t StateFlags(std::int32_t actor) override {
    return VS::Actors::LiveActorStateFlags(actor);
  }
  VS::Field::ActorTileTriggerCell Collision(
      const VS::Actors::PackedTile& tile) override {
    const auto* cell =
        VS::Field::LiveCollisionCellOrAbsent(tile.x, tile.z, tile.layer);
    return {.room_trigger = cell->hasRoomTrigger != 0, .pit = cell->isPit != 0};
  }
  bool TransitionInProgress() override {
    return Field_screenTransitionStep != 0;
  }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(
      std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr) return std::nullopt;
    const auto& bytes = record->lastTriggerTile.bytes;
    return VS::Actors::PackedTile{.x = bytes[0],
                                  .layer = bytes[1],
                                  .z = bytes[2],
                                  .spare = bytes[kTriggerModeByte]};
  }
  bool EventOwnsControl() override {
    return Field_controlMode == kActorEventControlMode;
  }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile& tile) override {
    const auto target =
        VS::Field::FindLiveRoomTriggerAtTile({tile.x, tile.z, tile.layer});
    const auto* record =
        target ? VS::Field::ResolveLiveTrapTarget(*target) : nullptr;
    if (!target || record == nullptr) return std::nullopt;
    return VS::Field::ActorTileTrap{.target = *target, .kind = record->kind};
  }
  void BeginTrap(VS::Field::TrapTarget target,
                 const VS::Actors::ActorPlacementView& placement,
                 std::int32_t actor) override {
    static_cast<void>(
        VS::Field::BeginLiveTrapInteraction(target, placement, actor));
  }
  void StoreTriggerTile(std::int32_t actor, const VS::Actors::PackedTile& tile,
                        std::uint8_t mode, bool store_location) override {
    auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr) return;
    auto& bytes = record->lastTriggerTile.bytes;
    if (store_location) {
      bytes[0] = tile.x;
      bytes[1] = tile.layer;
      bytes[2] = tile.z;
    }
    bytes[kTriggerModeByte] = mode;
  }
  bool DefeatForFall(std::int32_t actor) override {
    auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return false;
    record->stats->currentHp = 0;
    VS::Battle::ResolveLiveActorDefeat(0, actor, 0);
    return true;
  }
  bool TriggerFallEvent() override {
    return Event_EvaluateRoomEventTriggersEntry(kActorFallEventRequest, 0,
                                                nullptr, 0) == 1;
  }
};

class LiveEntranceDoorHost final : public VS::Field::EntranceDoorHost {
 public:
  std::optional<std::uint8_t> SceneDoorId(std::uint8_t door_id) override {
    const auto scene_id = Field_SceneDoorIdEntry(door_id);
    if (scene_id < 0) return std::nullopt;
    return static_cast<std::uint8_t>(scene_id);
  }
};

class LiveAshleyFallHost final : public VS::Field::AshleyFallHost {
 public:
  std::optional<VS::Field::ScriptedFallDestination> ScriptedDestination()
      override {
    const auto destination = VS::Field::LiveRoomFallDestination();
    if (destination == 0 || destination == -1) return std::nullopt;
    constexpr unsigned kZoneShift = 16;
    const auto packed = static_cast<std::uint32_t>(destination);
    return VS::Field::ScriptedFallDestination{
        .zone_id = static_cast<std::uint8_t>(packed >> kZoneShift),
        .room_id = static_cast<std::uint8_t>(packed)};
  }
  std::optional<std::int16_t> CurrentHp(std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return std::nullopt;
    return std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(record->stats->currentHp));
  }
  void ShowFallDamage(std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return;
    constexpr std::uint32_t kFallValueLabel = 0x80000014;
    Battle_QueueActorValueLabel(record->stats->actorId, 0, kFallValueLabel, 0);
  }
  void SetHp(std::int32_t actor, std::int16_t hp) override {
    auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return;
    record->stats->currentHp = hp;
  }
  void BeginDefeat(std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return;
    Battle_BeginActorDefeatAnimation(record->stats->actorId);
  }
  VS::Field::DoorRecord StoreEntranceForReload() override {
    Field_StoreEntranceDoorForReloadEntry();
    return VS::Field::LoadLiveRoomTransition().pending_door;
  }
  std::uint8_t CurrentZoneId() const override {
    return static_cast<std::uint8_t>(Field_zoneContext.zndId);
  }
  void BeginTransition(const VS::Field::DoorRecord& door,
                       VS::Field::AshleyTransitionRoute route) override {
    Field_MpdDoorRecord record{};
    VS::Field::StoreDoorRecord(door, record);
    constexpr std::int32_t kFallTransitionSpeed = 8;
    if (route == VS::Field::AshleyTransitionRoute::kRoom) {
      VS::Field::LiveBeginRoomTransition(&record, 0, kFallTransitionSpeed);
    } else {
      VS::Field::LiveBeginZoneTransition(&record, 0, kFallTransitionSpeed);
    }
  }
  void ResetRunMomentum(std::int32_t actor) override {
    VS::Actors::ResetLiveActorRunMomentum(actor);
  }
  void SetEntrance(const std::optional<VS::Field::DoorRecord>& door) override {
    VS::Field::SetLiveEntranceDoor(door);
  }
};

class LiveAshleyTileHost final : public VS::Field::AshleyTileTriggerHost {
 public:
  std::uint32_t StateFlags(std::int32_t actor) override {
    return actor_.StateFlags(actor);
  }
  VS::Field::AshleyTileTriggerCell Collision(
      const VS::Actors::PackedTile& tile) override {
    const auto* cell =
        VS::Field::LiveCollisionCellOrAbsent(tile.x, tile.z, tile.layer);
    return {.actor = {.room_trigger = cell->hasRoomTrigger != 0,
                      .pit = cell->isPit != 0},
            .event_trigger = cell->hasEventTrigger != 0,
            .docked_door = cell->isPlatformDocked != 0};
  }
  bool TransitionInProgress() override { return actor_.TransitionInProgress(); }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(
      std::int32_t actor) override {
    return actor_.LastTriggerTile(actor);
  }
  bool EventOwnsControl() override { return actor_.EventOwnsControl(); }
  bool TriggerPlayerTileEvent() override {
    constexpr std::int32_t kPlayerTileEvent = 2;
    return Event_EvaluateRoomEventTriggersEntry(kPlayerTileEvent, 0, nullptr,
                                                0) == 1;
  }
  void BeginQueuedCutscene() override { VS::Event::BeginLiveQueuedCutscene(); }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile& tile) override {
    return actor_.Trap(tile);
  }
  void SpawnTrapTileEffect(
      const VS::Actors::ActorPlacementView& placement) override {
    Actors_ActorLocation location{};
    location.packedTile.target.tileX = placement.tile.x;
    location.packedTile.target.tileLayer = placement.tile.layer;
    location.packedTile.target.tileZ = placement.tile.z;
    location.packedTile.target.facing = placement.tile.spare;
    location.position.vx = placement.position.x;
    location.position.vy = placement.position.y;
    location.position.vz = placement.position.z;
    location.position.pad = placement.position.w;
    Effects_SpawnTrapTileEffect(&location);
  }
  void BeginTrap(VS::Field::TrapTarget target,
                 const VS::Actors::ActorPlacementView& placement,
                 std::int32_t actor) override {
    actor_.BeginTrap(target, placement, actor);
  }
  void StoreTriggerTile(std::int32_t actor, const VS::Actors::PackedTile& tile,
                        std::uint8_t mode, bool store_location) override {
    actor_.StoreTriggerTile(actor, tile, mode, store_location);
  }
  std::vector<VS::Field::DoorRecord> DoorsAt(
      const VS::Actors::PackedTile& tile) override {
    const auto* matches =
        VS::Field::TransitionLiveDoorsAtTile(tile.x, tile.z, tile.layer);
    std::vector<VS::Field::DoorRecord> doors;
    if (matches == nullptr || matches->records == nullptr ||
        matches->count <= 0) {
      return doors;
    }
    const auto count = static_cast<std::size_t>(matches->count);
    if (count >
        Field_roomData.header.doorsLength / sizeof(Field_MpdDoorRecord)) {
      return doors;
    }
    for (const auto* record : std::span(matches->records, count)) {
      if (record != nullptr) doors.push_back(VS::Field::DoorRecordOf(*record));
    }
    return doors;
  }
  std::uint8_t ScriptFlag(std::uint16_t flag) override {
    return VS::State::LiveScriptFlag(flag);
  }
  VS::Field::AshleyFallHost& FallHost() override { return fall_; }

 private:
  LiveActorTileTriggerHost actor_;
  LiveAshleyFallHost fall_;
};

class LiveFacingEventHost final : public VS::Field::FacingEventHost {
 public:
  std::optional<VS::Actors::ActorPlacementView> Placement(
      std::int32_t actor) const override {
    return VS::Actors::LiveActorPlacementView(actor);
  }
  bool EvaluateInteraction(const VS::Field::FacingTile& tile) override {
    std::array<std::uint8_t, kFacingPositionBytes> position{tile.x, tile.layer,
                                                            tile.z, 0};
    return Event_EvaluateRoomEventTriggersEntry(kFacingInteractionMode, 0,
                                                position.data(), 1) == 1;
  }
};

class LiveFacingResourceHost final : public VS::Field::FacingResourceHost {
 public:
  std::optional<VS::Actors::ActorPlacementView> Placement(
      std::int32_t actor) const override {
    return VS::Actors::LiveActorPlacementView(actor);
  }
  std::optional<VS::Field::RoomObjectTarget> RoomObject(
      const VS::Field::FacingTile& tile) override {
    return VS::Field::FindLiveInteractiveDoorAtTile(
        {.tile_x = tile.x, .tile_z = tile.z, .floor_layer = tile.layer});
  }
  std::optional<VS::Field::FacingChestState> Chest(
      const VS::Field::FacingTile& tile) override {
    const auto* record =
        VS::Items::FindLiveTreasureChestAtTile(tile.x, tile.z, tile.layer);
    const auto target = VS::Field::FindLiveChestTarget(record);
    if (!target || record == nullptr) {
      return std::nullopt;
    }
    return VS::Field::FacingChestState{.target = *target,
                                       .facing = record->facing,
                                       .opened_flag = record->openedFlag};
  }
  std::int32_t FloorHeight(const VS::Field::FacingTile& tile) override {
    return VS::Field::LiveFloorHeightAt(
        static_cast<std::int32_t>(tile.x) * kFacingTileUnits +
            kFacingTileCenter,
        static_cast<std::int32_t>(tile.z) * kFacingTileUnits +
            kFacingTileCenter);
  }
  bool ScriptFlag(std::int32_t flag) override {
    return VS::State::LiveScriptFlag(flag) != 0;
  }
};

class LiveInteractionCameraHost final
    : public VS::Field::InteractionCameraHost {
 public:
  std::optional<VS::Field::TrapPosition> Focus() const override {
    const auto domain = VS::Field::LiveInteractionTargetDomain();
    const auto& target = VS::Field::LiveInteractionTargetState();
    if (target.Trap(domain) == nullptr && target.Chest(domain) == nullptr) {
      return std::nullopt;
    }
    const auto& point = Field_interactionFocus.actor_query.location.position;
    return VS::Field::TrapPosition{point.vx, point.vy, point.vz};
  }
  std::optional<VS::Field::TrapPosition> AshleyPosition() const override {
    const auto actor = VS::Actors::LiveActorPlacementView(0);
    if (!actor) {
      return std::nullopt;
    }
    return VS::Field::TrapPosition{actor->position.x, actor->position.y,
                                   actor->position.z};
  }
  std::int32_t StepLookAt(const VS::Field::TrapPosition& point,
                          std::int32_t mode, std::int32_t speed) override {
    SVECTOR target{point.x, point.y, point.z, 0};
    return VS::Field::LiveStepCameraLookAtTowardTarget(&target, mode, speed);
  }
  std::uint32_t AshleyStateFlags() const override {
    return VS::Actors::LiveActorStateFlags(0);
  }
  bool NotificationPending() override {
    return Menu_IsSceneNotificationPending() != 0;
  }
  bool AshleyTransitionPose() override {
    return VS::Actors::IsLiveAshleyTransitionPose() != 0;
  }
};

class LiveTrapActionHost final : public VS::Field::TrapActionHost {
 public:
  bool HasActionState(const VS::Field::TrapActionInput& input) const override {
    return Battle_actionState != nullptr && input.actor >= 0 &&
           input.actor < kActorSlots && Battle_actors[input.actor] != nullptr &&
           Battle_actors[input.actor]->stats != nullptr;
  }
  std::optional<std::int16_t> Kind(
      VS::Field::TrapTarget target) const override {
    const auto* record = VS::Field::ResolveLiveTrapTarget(target);
    return record ? std::optional<std::int16_t>{record->kind} : std::nullopt;
  }
  bool StageVictim(const VS::Field::TrapActionInput& input) override {
    if (!HasActionState(input)) {
      return false;
    }
    auto& action = *Battle_actionState;
    action.selection.primaryTarget.targetsPosition = kTrapPrimaryTargetPosition;
    action.selection.targetCount = 1;
    action.selection.targets[0].targetsPosition = 0;
    action.selection.targets[0].target.targetActor =
        static_cast<std::uint8_t>(input.actor);
    action.selection.targets[0].target.targetLimb =
        std::bit_cast<std::int8_t>(input.default_limb);
    action.chainStep = 1;
    action.queueIndex = 0;
    action.rangeOrigin.vx = input.position.x;
    action.rangeOrigin.vy = input.position.y;
    action.rangeOrigin.vz = input.position.z;
    return true;
  }
  bool ResolveAction(std::uint16_t action_id) override {
    if (Battle_actionState == nullptr) {
      return false;
    }
    auto& action = *Battle_actionState;
    Battle_ResolveActionTargets(action_id, &action.queue[0], &action.selection,
                                1);
    return true;
  }
  bool BeginEffect(const VS::Field::TrapEffectRequest& effect) override {
    if (!HasActionState({.actor = effect.actor})) {
      return false;
    }
    auto request = VS::Field::LiveTrapEffectRecord(effect);
    static_cast<void>(Effects_BeginSecondaryEffects(&request, 0));
    return true;
  }
};

class LiveUnlockEffectHost final : public VS::Field::UnlockEffectHost {
 public:
  std::optional<VS::Field::UnlockEffectState> State(
      const VS::Field::UnlockEffectTarget& target) const override {
    if (const auto* door = std::get_if<VS::Field::RoomObjectTarget>(&target)) {
      const auto* record = VS::Field::ResolveLiveRoomObjectTarget(*door);
      return record ? std::optional{VS::Field::UnlockEffectState{
                          .lock_level = std::bit_cast<std::uint8_t>(
                              record->trigger.lockState.lock.state)}}
                    : std::nullopt;
    }
    const auto* record = VS::Field::ResolveLiveChestTarget(
        std::get<VS::Field::ChestTarget>(target));
    return record ? std::optional{VS::Field::UnlockEffectState{
                        .lock_level = record->lockLevel,
                        .opened_flag =
                            std::bit_cast<std::uint16_t>(record->openedFlag)}}
                  : std::nullopt;
  }
  bool ChestOpened(std::int16_t flag) const override {
    return VS::State::LiveScriptFlag(flag);
  }
  bool RollLockBreak() override {
    return VS::Field::RollLiveRiskLockBreak() != 0;
  }
  bool ClearLock(const VS::Field::UnlockEffectTarget& target) override {
    if (const auto* door = std::get_if<VS::Field::RoomObjectTarget>(&target)) {
      auto* record = VS::Field::ResolveLiveRoomObjectTarget(*door);
      if (record == nullptr) return false;
      record->trigger.lockState.lock.state = 0;
      return true;
    }
    auto* record = VS::Field::ResolveLiveChestTarget(
        std::get<VS::Field::ChestTarget>(target));
    if (record == nullptr) return false;
    record->lockLevel = 0;
    return true;
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kRoomObjectSoundGroup, sound);
  }
  void ShowLockedLabel(const VS::Field::TrapPosition& point) override {
    std::array<std::uint16_t, kWorldLabelCoordinates> words{
        std::bit_cast<std::uint16_t>(point.x),
        std::bit_cast<std::uint16_t>(point.y),
        std::bit_cast<std::uint16_t>(point.z)};
    static_cast<void>(Battle_QueueWorldLockedLabel(words.data()));
  }
};

class LiveRoomObjectInteractionHost final
    : public VS::Field::RoomObjectInteractionHost {
 public:
  explicit LiveRoomObjectInteractionHost(const void* address)
      : generation_(VS::Field::LiveInteractionTargetDomain().generation) {
    const auto records = Records();
    for (std::size_t index = 0; index < records.size(); ++index) {
      if (static_cast<const void*>(&records[index]) == address) {
        index_ = index;
        break;
      }
    }
  }
  std::optional<VS::Field::RoomObjectLockState> State() const override {
    const auto* record = Record();
    if (record == nullptr) {
      return std::nullopt;
    }
    return VS::Field::RoomObjectLockState{
        .tile_x = record->trigger.tileX,
        .tile_z = record->trigger.tileZ,
        .required_item = record->keyOrSigil,
        .lock_state =
            static_cast<std::uint8_t>(record->trigger.lockState.lock.state),
        .is_open = record->isOpen != 0,
        .is_triggered = record->isTriggered != 0};
  }
  bool HasItem(std::int16_t item) override {
    return VS::Items::CountLiveInventoryItem(item) != 0;
  }
  bool PreviousUnlockFlag(std::int32_t flag) override {
    return VS::State::LiveScriptFlag(flag) != 0;
  }
  void SetUnlockFlag(std::int32_t flag, std::int16_t value) override {
    VS::State::SetLiveScriptFlag(flag, value);
  }
  void ConsumeItem(std::int16_t item) override {
    VS::Items::ConsumeLiveMiscItem(item);
  }
  bool Unlock() override {
    auto* record = Record();
    if (record == nullptr) {
      return false;
    }
    record->trigger.lockState.lock.state = 0;
    return true;
  }
  void ShowNotification(std::int32_t kind, std::int16_t item) override {
    Menu_ShowSceneNotification(kind, item, 1);
  }
  void PlayPose(const VS::Field::RoomObjectLockState& state,
                std::uint8_t pose) override {
    const auto x = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(state.tile_x) * 128 + 64);
    const auto z = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(state.tile_z) * 128 + 64);
    std::array<std::int16_t, kPoseWords> position{
        std::bit_cast<std::int16_t>(x), 0, std::bit_cast<std::int16_t>(z), 0};
    VS::Actors::PlayLiveActorActionPose(0, position.data(), pose);
  }
  void BeginToggle() override {
    if (auto* record = Record()) {
      static_cast<void>(Field_BeginDoorToggleEntry(record));
    }
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kRoomObjectSoundGroup, sound);
  }
  void RestoreIdle(std::int32_t actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }

 private:
  std::span<Field_MpdLockTrigger> Records() const {
    if (generation_ != VS::Field::LiveInteractionTargetDomain().generation ||
        Field_roomData.lockTriggers == nullptr) {
      return {};
    }
    return {
        Field_roomData.lockTriggers,
        Field_roomData.header.lockTriggersLength / kOriginalLockRecordBytes};
  }
  Field_MpdLockTrigger* Record() const {
    const auto records = Records();
    return index_ && *index_ < records.size() ? &records[*index_] : nullptr;
  }

  std::uint64_t generation_ = 0;
  std::optional<std::size_t> index_;
};

class LiveChestInteractionHost final : public VS::Field::ChestInteractionHost {
 public:
  std::optional<VS::Field::ChestLockState> Chest(
      VS::Field::ChestTarget target) const override {
    const auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest == nullptr) {
      return std::nullopt;
    }
    return VS::Field::ChestLockState{chest->tileX, chest->tileZ,
                                     chest->lockLevel, chest->reservedE,
                                     chest->openedFlag};
  }
  bool ScriptFlag(std::int32_t flag) override {
    return VS::State::LiveScriptFlag(flag);
  }
  bool HasItem(std::int16_t item) override {
    return VS::Items::CountLiveInventoryItem(item) != 0;
  }
  void SetScriptFlag(std::int32_t flag) override {
    VS::State::SetLiveScriptFlag(flag, 1);
  }
  void ConsumeItem(std::int16_t item) override {
    VS::Items::ConsumeLiveMiscItem(item);
  }
  bool Unlock(VS::Field::ChestTarget target) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest == nullptr) {
      return false;
    }
    chest->lockLevel = 0;
    return true;
  }
  void ShowUnlocked(std::int16_t item) override {
    Menu_ShowSceneNotification(kUnlockedNotification, item, 1);
  }
  void PlayPose(const VS::Field::ChestLockState& chest,
                std::uint8_t pose) override {
    const auto x = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(chest.tile_x) * 128 + 64);
    const auto z = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(chest.tile_z) * 128 + 64);
    std::array<std::int16_t, kPoseWords> position{
        std::bit_cast<std::int16_t>(x), 0, std::bit_cast<std::int16_t>(z), 0};
    VS::Actors::PlayLiveActorActionPose(0, position.data(), pose);
  }
  bool StartAnimation(VS::Field::ChestTarget target) override {
    return VS::Field::BeginLiveChestSwitch(target);
  }
  bool SelectChest(VS::Field::ChestTarget target, std::int32_t actor) override {
    if (VS::Field::ResolveLiveChestTarget(target) == nullptr || actor < 0 ||
        actor >= kActorSlots) {
      return false;
    }
    VS::Field::LiveInteractionTargetState().Select(target);
    VS::Actors::LiveActorPlacement(actor, &Field_interactionFocus.actor_query);
    Field_interactionFocus.actor_query.actorIndex =
        static_cast<std::int16_t>(actor);
    return true;
  }
  std::optional<VS::Field::ChestTarget> SelectedChest() const override {
    const auto* target = VS::Field::LiveInteractionTargetState().Chest(
        VS::Field::LiveInteractionTargetDomain());
    return target ? std::optional{*target} : std::nullopt;
  }
  void BeginChestFlow() override {
    Field_controlMode = kChestControlMode;
    Battle_actionFlowMode = 1;
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void FinishChestFlow() override {
    Battle_actionFlowMode = kFinishedActionFlow;
    State_tickSpeed = 2;
  }
  void RestoreIdlePoses() override {
    for (int actor = 0; actor < kIdleActorCount; ++actor) {
      VS::Actors::RestoreLiveActorIdlePose(actor);
    }
  }
  void ResetActionSelection() override {
    if (Battle_actionState != nullptr) {
      Battle_actionState->chainStep = 0;
      Battle_actionState->selection.actionId = 0;
    }
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ShowLocked(std::int32_t notification, std::int16_t item) override {
    Battle_actionFlowMode = kContentsActionFlow;
    Menu_ShowSceneNotification(notification, item, 1);
  }
  void OpenChestLoot(VS::Field::ChestTarget target) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest != nullptr) {
      Battle_actionFlowMode = kContentsActionFlow;
      VS::Menu::Framework::OpenLiveLootEntryMenu(&chest->loot);
    }
  }
  void OpenChestStorage(std::int32_t storage) override {
    Battle_actionFlowMode = kContentsActionFlow;
    VS::Menu::Framework::OpenLiveChestStorageMenu(storage);
  }
  void PauseForDialog() override {
    VS::Field::LivePauseFieldForInteractionDialog();
  }
  void PauseForMenu() override { VS::Field::LivePauseFieldForMenu(); }
  bool NotificationPending() override {
    return Menu_IsSceneNotificationPending() != 0;
  }
  VS::Field::ChestSelectionResult UpdateContentsSelection() override {
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    const auto* selection = VS::Menu::Framework::LiveActionSelectionState();
    if (selection == nullptr || selection->menuStatus == 1) {
      return VS::Field::ChestSelectionResult::kPending;
    }
    return selection->selectionKind != 0
               ? VS::Field::ChestSelectionResult::kAccepted
               : VS::Field::ChestSelectionResult::kCancelled;
  }
  void ReleaseContentsRows() override {
    VS::Menu::Framework::ReleaseLiveMenuRows();
  }
  void ResumeField() override { VS::Field::LiveResumeFieldAfterMenu(); }
};

class LiveInteractionDispatchHost final
    : public VS::Field::InteractionDispatchHost {
 public:
  VS::Field::InteractionTarget Target() const override {
    return VS::Field::LiveInteractionTargetState().Target();
  }
  bool TrapMode() const override {
    return Field_controlMode == kTrapControlMode;
  }
  void BeginDispatch() override { Battle_actionFlowMode = 2; }
  bool DispatchChest(VS::Field::ChestTarget target) override {
    return VS::Field::TryOpenLiveChest(target);
  }
  bool DispatchTrap(VS::Field::TrapTarget target) override {
    const auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    const int actor = Field_interactionFocus.actor_query.actorIndex;
    if (trap == nullptr || actor < 0 || actor >= kActorSlots ||
        Battle_actionState == nullptr || Battle_actors[actor] == nullptr ||
        Battle_actors[actor]->stats == nullptr) {
      return false;
    }
    const auto& point = Field_interactionFocus.actor_query.location.position;
    State_tickSpeed = kTrapTickSpeed;
    return VS::Field::TriggerLiveActorTrap(
        target, {.x = point.vx, .y = point.vy, .z = point.vz}, actor);
  }
};

class LiveTrapFinishHost final : public VS::Field::TrapFinishHost {
 public:
  std::optional<VS::Field::TrapTarget> Target() const override {
    const auto* target = VS::Field::LiveInteractionTargetState().Trap(
        VS::Field::LiveInteractionTargetDomain());
    return target ? std::optional{*target} : std::nullopt;
  }
  bool DisableTrap(VS::Field::TrapTarget target) override {
    if (!VS::Field::SetLiveRoomTriggerEnabled(target, 1)) return false;
    auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    if (trap == nullptr) {
      return false;
    }
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(trap->tileX, trap->tileZ);
    return true;
  }
  void FinishFlow() override {
    Battle_actionFlowMode = kFinishedActionFlow;
    State_tickSpeed = 2;
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ClearTarget() override {
    VS::Field::LiveInteractionTargetState().Clear();
  }
};

class LiveTrapSelectionHost final : public VS::Field::TrapSelectionHost {
 public:
  explicit LiveTrapSelectionHost(Actors_ActorLocation* location)
      : location_(location) {}
  bool Valid(VS::Field::TrapTarget target, std::int32_t actor) const override {
    return location_ != nullptr && actor >= 0 && actor < kActorSlots &&
           VS::Field::ResolveLiveTrapTarget(target) != nullptr;
  }
  void BeginFlow() override {
    Field_controlMode = kTrapControlMode;
    Battle_actionFlowMode = 1;
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void Select(VS::Field::TrapTarget target, std::int32_t actor) override {
    VS::Field::LiveInteractionTargetState().Select(target);
    Field_interactionFocus.actor_query.location = *location_;
    Field_interactionFocus.actor_query.actorIndex =
        static_cast<std::int16_t>(actor);
  }
  void RestoreIdle(std::int32_t actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }
  void ShowInformation(VS::Field::TrapTarget target) override {
    const auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    if (trap != nullptr && Input_settings.information != 0) {
      Menu_ShowSceneNotification(kTrapNotification,
                                 trap->kind + kTrapActionOffset, 1);
    }
  }
  void ResetActionSelection() override {
    if (Battle_actionState != nullptr) {
      Battle_actionState->chainStep = 0;
      Battle_actionState->selection.actionId = 0;
    }
  }

 private:
  Actors_ActorLocation* location_ = nullptr;
};

class LiveObjectInteractionWaitHost final
    : public VS::Field::ObjectInteractionWaitHost {
 public:
  void SetControlMode(VS::Field::FieldControlMode mode) override {
    Field_controlMode = static_cast<std::uint32_t>(mode);
  }
  void PauseField() override {
    VS::Field::LivePauseFieldForObjectInteraction();
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ResumeField() override { VS::Field::LiveResumeFieldAfterMenu(); }
  void EnterExploration() override {
    VS::Field::LiveEnterFieldExplorationMode();
  }
};

}  // namespace

namespace VS::Field {
std::uint8_t LiveCurrentSavePointId() {
  LiveSavePointQueryHost host;
  return CurrentSavePointId(host);
}

bool RefreshLiveAshleyInteractionTile(const Actors_ActorRecord* record) {
  constexpr std::size_t kRequiredBytes =
      offsetof(Actors_ActorRecord, animationId) +
      sizeof(decltype(Actors_ActorRecord::animationId));
  if (record == nullptr ||
      reinterpret_cast<std::uintptr_t>(record) % alignof(Actors_ActorRecord) !=
          0) {
    return false;
  }
  const auto allocation = Core::Memory::LivePayloadContaining(record);
  if (allocation) {
    const auto offset = reinterpret_cast<std::uintptr_t>(record) -
                        reinterpret_cast<std::uintptr_t>(allocation->data());
    if (offset > allocation->size() ||
        kRequiredBytes > allocation->size() - offset) {
      return false;
    }
  } else if (allocation.error() != Core::Memory::HeapError::kOutsideArena) {
    return false;
  }
  const AshleyInteractionTileState state{
      .actor_slot = static_cast<std::uint8_t>(record->instance.slot),
      .animation_id = static_cast<std::uint8_t>(record->animationId),
      .facing = record->instance.facing,
      .tile_x = static_cast<std::uint8_t>(record->instance.currentTileX),
      .tile_z = static_cast<std::uint8_t>(record->instance.currentTileZ)};
  LiveAshleyInteractionTileHost host;
  return RefreshAshleyInteractionTile(state, host);
}

void SetLiveEntranceDoor(const std::optional<DoorRecord>& door) {
  EntranceDoorState state;
  LiveEntranceDoorHost host;
  SetEntranceDoorRecord(state, door, host);
  StoreDoorRecord(state.door, Field_entranceDoorRecord);
  Field_enteredDoorId = state.entered_door_id;
}

void FindLiveEntranceDoorNearTile(std::int32_t tile_x, std::int32_t tile_z,
                                  std::int32_t layer) {
  std::vector<DoorRecord> doors;
  if (Field_roomData.doors != nullptr) {
    const auto bytes = Field_roomData.header.doorsLength;
    if (bytes % kOriginalDoorRecordBytes != 0) {
      SetLiveEntranceDoor(std::nullopt);
      return;
    }
    const auto count =
        static_cast<std::size_t>(bytes / kOriginalDoorRecordBytes);
    for (const auto& record : std::span(Field_roomData.doors, count)) {
      doors.push_back(DoorRecordOf(record));
    }
  }
  SetLiveEntranceDoor(FindEntranceDoorNearTile(doors, tile_x, tile_z, layer));
}

bool CheckLiveAshleyTileTriggers(std::int32_t actor,
                                 const Actors::ActorPlacementView& placement) {
  LiveAshleyTileHost host;
  return CheckAshleyTileTriggers(actor, placement, host);
}

ActorTileTriggerResult CheckLiveActorTrapAndFallTriggers(
    std::int32_t actor, const Actors::ActorPlacementView& placement) {
  LiveActorTileTriggerHost host;
  return CheckActorTrapAndFallTriggers(actor, placement, host);
}

Field_MpdLockTrigger* ResolveLiveRoomObjectTarget(RoomObjectTarget target) {
  if (target.generation != LiveInteractionTargetDomain().generation ||
      Field_roomData.lockTriggers == nullptr ||
      Field_roomData.header.lockTriggersLength % kOriginalLockRecordBytes !=
          0 ||
      reinterpret_cast<std::uintptr_t>(Field_roomData.lockTriggers) %
              alignof(Field_MpdLockTrigger) !=
          0 ||
      target.index >=
          Field_roomData.header.lockTriggersLength / kOriginalLockRecordBytes) {
    return nullptr;
  }
  return &Field_roomData.lockTriggers[target.index];
}

std::optional<RoomObjectTarget> FindLiveFacingRoomObject(std::int32_t actor) {
  LiveFacingResourceHost host;
  return FindFacingRoomObject(actor, host);
}

std::optional<ChestTarget> FindLiveFacingUnlootedChest(std::int32_t actor) {
  LiveFacingResourceHost host;
  return FindFacingUnlootedChest(actor, host);
}

RoomObjectInteractionResult InteractLiveRoomObject(RoomObjectTarget target) {
  LiveRoomObjectInteractionHost host(ResolveLiveRoomObjectTarget(target));
  return InteractWithLockedRoomObject(host);
}

Effects_ActionEffectRequest LiveTrapEffectRecord(
    const TrapEffectRequest& effect) {
  Effects_ActionEffectRequest request{};
  request.effectId = effect.effect_id;
  request.count = 1;
  request.source.type = kTrapSourcePointType;
  if (effect.source_position) {
    request.source.value.position.vx = effect.source_position->x;
    request.source.value.position.vy = effect.source_position->y;
    request.source.value.position.vz = effect.source_position->z;
  }
  request.targets[0].type = kTrapActorTargetType;
  request.targets[0].value.actor.actor =
      static_cast<std::uint8_t>(effect.actor);
  request.targets[0].value.actor.part = effect.ground_anchor;
  return request;
}

bool TryOpenLiveChest(ChestTarget target) {
  LiveChestInteractionHost host;
  return TryOpenChest(target, host);
}
bool BeginLiveChestInteraction(ChestTarget target, std::int32_t actor) {
  LiveChestInteractionHost host;
  return BeginChestInteraction(target, actor, host);
}
bool BeginLiveTrapInteraction(TrapTarget target,
                              const Actors::ActorPlacementView& placement,
                              std::int32_t actor) {
  Actors_ActorLocation location{};
  location.packedTile.target.tileX = placement.tile.x;
  location.packedTile.target.tileLayer = placement.tile.layer;
  location.packedTile.target.tileZ = placement.tile.z;
  location.packedTile.target.facing = placement.tile.spare;
  location.position = {.vx = placement.position.x,
                       .vy = placement.position.y,
                       .vz = placement.position.z,
                       .pad = placement.position.w};
  LiveTrapSelectionHost host(&location);
  return BeginTrapInteraction(target, actor, host);
}
bool TriggerLiveActorTrap(TrapTarget target, const TrapPosition& point,
                          std::int32_t actor) {
  LiveTrapActionHost host;
  TrapActionInput input{.actor = actor, .position = point};
  if (!host.HasActionState(input)) {
    return false;
  }
  input.default_limb =
      static_cast<std::uint8_t>(Battle_actors[actor]->stats->defaultTargetLimb);
  return TriggerActorTrap(target, input, host);
}
}  // namespace VS::Field

// decomp: BATTLE.PRG func_80073E30 (src/BATTLE/BATTLE.PRG/146C.c:4935).
void Field_TryOpenChestEntry(Field_MpdLockTrigger* address, int) {
  const auto target = VS::Field::FindLiveChestTargetAddress(address);
  if (target) {
    static_cast<void>(VS::Field::TryOpenLiveChest(*target));
  }
}

// decomp: BATTLE.PRG func_80074050 (src/BATTLE/BATTLE.PRG/146C.c:4993).
void Field_BeginChestInteractionEntry(Field_MpdLockTrigger* address,
                                      int actor) {
  const auto target = VS::Field::FindLiveChestTargetAddress(address);
  if (target) {
    static_cast<void>(VS::Field::BeginLiveChestInteraction(*target, actor));
  }
}

// decomp: BATTLE.PRG func_800742A4 (src/BATTLE/BATTLE.PRG/146C.c:5066).
void Field_FinishChestInteractionEntry() {
  LiveChestInteractionHost host;
  static_cast<void>(VS::Field::FinishChestInteraction(host));
}

// decomp: BATTLE.PRG func_800743E0 (src/BATTLE/BATTLE.PRG/146C.c:5107).
void Field_OpenChestContentsEntry() {
  LiveChestInteractionHost host;
  static_cast<void>(VS::Field::OpenChestContents(host));
}

// decomp: BATTLE.PRG func_800744B8 (src/BATTLE/BATTLE.PRG/146C.c:5139).
int Field_UpdateChestContentsSelectionEntry() {
  LiveChestInteractionHost host;
  return VS::Field::UpdateChestContentsSelection(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_800741D4 (src/BATTLE/BATTLE.PRG/146C.c:5044).
void Field_DispatchTrapOrChestInteractionEntry() {
  LiveInteractionDispatchHost host;
  static_cast<void>(VS::Field::DispatchTrapOrChestInteraction(host));
}

// decomp: BATTLE.PRG func_80073F7C (src/BATTLE/BATTLE.PRG/146C.c:4973).
void Field_BeginTrapInteractionEntry(Field_MpdLockTrigger* address,
                                     Actors_ActorLocation* victim_location,
                                     int actor) {
  const auto target = VS::Field::FindLiveTrapTargetAddress(address);
  if (target) {
    LiveTrapSelectionHost host(victim_location);
    static_cast<void>(VS::Field::BeginTrapInteraction(*target, actor, host));
  }
}

// decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:5080).
void Field_FinishTrapInteractionEntry() {
  LiveTrapFinishHost host;
  static_cast<void>(VS::Field::FinishTrapInteraction(host));
}

// decomp: BATTLE.PRG func_80073AA4 (src/BATTLE/BATTLE.PRG/146C.c:4823).
void Field_BeginObjectInteractionWaitEntry() {
  LiveObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
}

// decomp: BATTLE.PRG func_80073ACC (src/BATTLE/BATTLE.PRG/146C.c:4829).
void Field_EndObjectInteractionWaitEntry() {
  LiveObjectInteractionWaitHost host;
  VS::Field::EndObjectInteractionWait(host);
}

// decomp: BATTLE.PRG func_80073AFC (src/BATTLE/BATTLE.PRG/146C.c:4836).
int Field_InteractWithLockedRoomObjectEntry(Field_MpdLockTrigger* address) {
  LiveRoomObjectInteractionHost host(address);
  return VS::Field::InteractWithLockedRoomObject(host) ==
                 VS::Field::RoomObjectInteractionResult::kWait
             ? 0
             : 1;
}

// decomp: BATTLE.PRG func_80073D30 (src/BATTLE/BATTLE.PRG/146C.c:4909).
void Field_TriggerActorTrapEntry(Field_MpdLockTrigger* address,
                                 Actors_ActorLocationQuery* location,
                                 int actor) {
  const auto target = VS::Field::FindLiveTrapTargetAddress(address);
  if (target && location != nullptr) {
    const auto& point = location->location.position;
    static_cast<void>(VS::Field::TriggerLiveActorTrap(
        *target, {.x = point.vx, .y = point.vy, .z = point.vz}, actor));
  }
}

// decomp: BATTLE.PRG func_80074120 (src/BATTLE/BATTLE.PRG/146C.c:5019).
int Field_WaitForChestCameraFocusEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::WaitForChestCameraFocus(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_8007418C (src/BATTLE/BATTLE.PRG/146C.c:5034).
int Field_StepTrapCameraFocusEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::StepTrapCameraFocus(host).value_or(0);
}

// decomp: BATTLE.PRG func_8007424C (src/BATTLE/BATTLE.PRG/146C.c:5055).
int Field_IsChestAnimationReadyEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::IsChestAnimationReady(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_80074374 (src/BATTLE/BATTLE.PRG/146C.c:5089).
int Field_TrackInteractionCameraToAshleyEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::TrackInteractionCameraToAshley(host).value_or(0);
}

// decomp: BATTLE.PRG func_80074798 (src/BATTLE/BATTLE.PRG/146C.c:5232).
int Field_CurrentSavePointIdEntry(void) {
  return VS::Field::LiveCurrentSavePointId();
}

void Field_RefreshAshleyInteractionTileEntry(Actors_ActorRecord* record) {
  static_cast<void>(VS::Field::RefreshLiveAshleyInteractionTile(record));
}

int Field_TileInFrontOfActorEntry(Actors_ActorLocationQuery* actor,
                                  unsigned char* output) {
  if (actor == nullptr || output == nullptr) {
    return -1;
  }
  VS::Actors::ActorPlacementView view{};
  view.tile.x =
      static_cast<std::uint8_t>(actor->location.packedTile.target.tileX);
  view.tile.z =
      static_cast<std::uint8_t>(actor->location.packedTile.target.tileZ);
  view.tile.layer =
      static_cast<std::uint8_t>(actor->location.packedTile.target.tileLayer);
  view.position.w = actor->location.position.pad;
  const auto tile = VS::Field::TileInFrontOfActor(view);
  output[0] = tile.x;
  output[1] = tile.layer;
  output[2] = tile.z;
  return static_cast<int>(tile.direction);
}

// decomp: BATTLE.PRG func_80074860 (src/BATTLE/BATTLE.PRG/146C.c:5260).
// decomp: BATTLE.PRG func_800751B8 (src/BATTLE/BATTLE.PRG/146C.c:5514).
void Field_ApplyUnlockEffectToRoomObjectEntry(SVECTOR* target_position) {
  if (target_position == nullptr) return;
  SVECTOR position{};
  std::memcpy(&position, target_position, sizeof(position));
  const auto packed = std::bit_cast<std::uint16_t>(position.pad);
  constexpr std::uint16_t kTargetKindMask = 0x1F0;
  constexpr std::uint16_t kTargetIndexMask = 0xF;
  constexpr std::uint16_t kDoorKind = 0x10;
  constexpr std::uint16_t kChestKind = 0x20;
  constexpr std::uint16_t kOtherChestKind = 0x100;
  const auto index = static_cast<std::uint32_t>(packed & kTargetIndexMask);
  const auto generation = VS::Field::LiveInteractionTargetDomain().generation;
  std::optional<VS::Field::UnlockEffectTarget> target;
  switch (packed & kTargetKindMask) {
    case kDoorKind:
      target = VS::Field::RoomObjectTarget{index, generation};
      break;
    case kChestKind:
    case kOtherChestKind:
      target = VS::Field::ChestTarget{index, generation};
      break;
    default:
      return;
  }
  LiveUnlockEffectHost host;
  static_cast<void>(VS::Field::ApplyUnlockEffectToRoomObject(
      target, {position.vx, position.vy, position.vz}, host));
}

void Field_StoreEntranceDoorForReloadEntry(void) {
  const auto encoded = VS::Field::EnteredDoorReloadByte(Field_enteredDoorId);
  Audio_Seq_unusedStateFlags.doorEntered = std::bit_cast<char>(encoded);
}

void Field_SetEntranceDoorRecordEntry(
    char (*input)[kFieldEntranceRecordBytes]) {
  if (input == nullptr) {
    VS::Field::SetLiveEntranceDoor(std::nullopt);
    return;
  }
  Field_MpdDoorRecord record{};
  std::memcpy(&record, input, sizeof record);
  VS::Field::SetLiveEntranceDoor(VS::Field::DoorRecordOf(record));
}

void Field_FindEntranceDoorNearTileEntry(int tile_x, int tile_z, int layer) {
  VS::Field::FindLiveEntranceDoorNearTile(tile_x, tile_z, layer);
}

int Field_CheckAshleyTileTriggersEntry(int actor, unsigned char* input) {
  if (input == nullptr) return 0;
  Actors_ActorLocation location{};
  std::memcpy(&location, input, sizeof location);
  const VS::Actors::ActorPlacementView placement{
      .tile = {.x = location.packedTile.target.tileX,
               .layer = location.packedTile.target.tileLayer,
               .z = location.packedTile.target.tileZ,
               .spare = location.packedTile.target.facing},
      .position = {.x = location.position.vx,
                   .y = location.position.vy,
                   .z = location.position.vz,
                   .w = location.position.pad}};
  return VS::Field::CheckLiveAshleyTileTriggers(actor, placement) ? 1 : 0;
}

int Field_CheckActorTrapAndFallTriggersEntry(int actor,
                                             Actors_ActorLocation* location) {
  if (location == nullptr) return 0;
  const VS::Actors::ActorPlacementView placement{
      .tile = {.x = location->packedTile.target.tileX,
               .layer = location->packedTile.target.tileLayer,
               .z = location->packedTile.target.tileZ,
               .spare = location->packedTile.target.facing},
      .position = {.x = location->position.vx,
                   .y = location->position.vy,
                   .z = location->position.vz,
                   .w = location->position.pad}};
  return static_cast<int>(
      VS::Field::CheckLiveActorTrapAndFallTriggers(actor, placement));
}

int Field_TryTriggerFacingTileEventEntry(int actor) {
  LiveFacingEventHost host;
  return VS::Field::TryTriggerFacingTileEvent(actor, host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_800748B8 (src/BATTLE/BATTLE.PRG/146C.c:5271).
Field_MpdLockTrigger* Field_FindFacingRoomObjectEntry(int actor) {
  const auto target = VS::Field::FindLiveFacingRoomObject(actor);
  return target ? VS::Field::ResolveLiveRoomObjectTarget(*target) : nullptr;
}

// decomp: BATTLE.PRG func_80074950 (src/BATTLE/BATTLE.PRG/146C.c:5290).
Items_TreasureChest* Field_FindFacingUnlootedChestEntry(int actor) {
  const auto target = VS::Field::FindLiveFacingUnlootedChest(actor);
  return target ? VS::Field::ResolveLiveChestTarget(*target) : nullptr;
}
