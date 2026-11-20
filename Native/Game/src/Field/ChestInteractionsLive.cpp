#include "Field/ChestInteractionsLive.hpp"

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
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Location.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/Actor.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/SecondaryEffectRequestsLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/ChestInteractions.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/DoorMatchesLive.hpp"
#include "Field/DoorRecordLive.hpp"
#include "Field/Doors.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionFocusLive.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/Interactions.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/Mpd.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/RoomSwitchesLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Field/TransitionsLiveState.hpp"
#include "Field/TrapsLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBox.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::SVECTOR;

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
constexpr std::size_t kOriginalDoorRecordBytes = VS::Field::kMpdDoorRecordBytes;
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

VS::Battle::Actor* TileTriggerActor(std::size_t index) {
  const std::span actors(VS::Battle::actors);
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
           flags_ == VS::Battle::Ai::tileFlagMap;
  }
  std::uint64_t generation_ =
      VS::Field::LiveInteractionTargetDomain().generation;
  decltype(VS::Battle::Ai::tileFlagMap) flags_ = VS::Battle::Ai::tileFlagMap;
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
    return Valid() && (cell->has_room_trigger & 1) != 0;
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
           collision_ == VS::Field::roomData.collision_map &&
           collision_bytes_ ==
               VS::Field::roomData.header.collision_map_length &&
           cells_ == VS::Field::roomData.tile_cells &&
           cell_bytes_ == VS::Field::roomData.header.tile_cells_length &&
           triggers_ == VS::Field::roomData.floor_triggers &&
           trigger_bytes_ == VS::Field::roomData.header.floor_triggers_length;
  }
  std::uint64_t generation_ =
      VS::Field::LiveInteractionTargetDomain().generation;
  decltype(VS::Field::roomData.collision_map) collision_ =
      VS::Field::roomData.collision_map;
  std::uint32_t collision_bytes_ =
      VS::Field::roomData.header.collision_map_length;
  decltype(VS::Field::roomData.tile_cells) cells_ =
      VS::Field::roomData.tile_cells;
  std::uint32_t cell_bytes_ = VS::Field::roomData.header.tile_cells_length;
  decltype(VS::Field::roomData.floor_triggers) triggers_ =
      VS::Field::roomData.floor_triggers;
  std::uint32_t trigger_bytes_ =
      VS::Field::roomData.header.floor_triggers_length;
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
    return {.room_trigger = cell->has_room_trigger != 0,
            .pit = cell->is_pit != 0};
  }
  bool TransitionInProgress() override {
    return VS::Field::screenTransitionStep != 0;
  }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(
      std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr) return std::nullopt;
    const auto& bytes = record->last_trigger_tile.bytes;
    return VS::Actors::PackedTile{.x = bytes[0],
                                  .layer = bytes[1],
                                  .z = bytes[2],
                                  .spare = bytes[kTriggerModeByte]};
  }
  bool EventOwnsControl() override {
    return VS::Field::controlMode == kActorEventControlMode;
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
    auto& bytes = record->last_trigger_tile.bytes;
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
    record->stats->current_hp = 0;
    VS::Battle::ResolveLiveActorDefeat(0, actor, 0);
    return true;
  }
  bool TriggerFallEvent() override {
    return VS::Event::EvaluateRoomEventTriggersEntry(kActorFallEventRequest, 0,
                                                     nullptr, 0) == 1;
  }
};

class LiveEntranceDoorHost final : public VS::Field::EntranceDoorHost {
 public:
  std::optional<std::uint8_t> SceneDoorId(std::uint8_t door_id) override {
    const auto scene_id = VS::Field::SceneDoorIdEntry(door_id);
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
        static_cast<std::uint16_t>(record->stats->current_hp));
  }
  void ShowFallDamage(std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return;
    constexpr std::uint32_t kFallValueLabel = 0x80000014;
    VS::Battle::LiveQueueActorValueLabel(record->stats->actor_id, 0,
                                         kFallValueLabel, 0);
  }
  void SetHp(std::int32_t actor, std::int16_t hp) override {
    auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return;
    record->stats->current_hp = hp;
  }
  void BeginDefeat(std::int32_t actor) override {
    const auto* record = TileTriggerActor(static_cast<std::size_t>(actor));
    if (record == nullptr || record->stats == nullptr) return;
    VS::Battle::BeginLiveActorDefeatAnimation(record->stats->actor_id);
  }
  VS::Field::DoorRecord StoreEntranceForReload() override {
    VS::Field::StoreEntranceDoorForReloadEntry();
    return VS::Field::LoadLiveRoomTransition().pending_door;
  }
  std::uint8_t CurrentZoneId() const override {
    return static_cast<std::uint8_t>(VS::Field::zoneContext.zone_id);
  }
  void BeginTransition(const VS::Field::DoorRecord& door,
                       VS::Field::AshleyTransitionRoute route) override {
    VS::Field::MpdDoorRecord record{};
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
    return {.actor = {.room_trigger = cell->has_room_trigger != 0,
                      .pit = cell->is_pit != 0},
            .event_trigger = cell->has_event_trigger != 0,
            .docked_door = cell->is_platform_docked != 0};
  }
  bool TransitionInProgress() override { return actor_.TransitionInProgress(); }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(
      std::int32_t actor) override {
    return actor_.LastTriggerTile(actor);
  }
  bool EventOwnsControl() override { return actor_.EventOwnsControl(); }
  bool TriggerPlayerTileEvent() override {
    constexpr std::int32_t kPlayerTileEvent = 2;
    return VS::Event::EvaluateRoomEventTriggersEntry(kPlayerTileEvent, 0,
                                                     nullptr, 0) == 1;
  }
  void BeginQueuedCutscene() override { VS::Event::BeginLiveQueuedCutscene(); }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile& tile) override {
    return actor_.Trap(tile);
  }
  void SpawnTrapTileEffect(
      const VS::Actors::ActorPlacementView& placement) override {
    VS::Actors::ActorLocation location{};
    location.packed_tile.target.tile_x = placement.tile.x;
    location.packed_tile.target.tile_layer = placement.tile.layer;
    location.packed_tile.target.tile_z = placement.tile.z;
    location.packed_tile.target.facing = placement.tile.spare;
    location.position.vx = placement.position.x;
    location.position.vy = placement.position.y;
    location.position.vz = placement.position.z;
    location.position.pad = placement.position.w;
    VS::Effects::LiveSpawnTrapTileEffect(&location);
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
    if (count > VS::Field::roomData.header.doors_length /
                    sizeof(VS::Field::MpdDoorRecord)) {
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
    return VS::Event::EvaluateRoomEventTriggersEntry(kFacingInteractionMode, 0,
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
                                       .opened_flag = record->opened_flag};
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
    const auto& point =
        VS::Field::interactionFocus.actor_query.location.position;
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
    return VS::Menu::IsLiveSceneNotificationPending() != 0;
  }
  bool AshleyTransitionPose() override {
    return VS::Actors::IsLiveAshleyTransitionPose() != 0;
  }
};

class LiveTrapActionHost final : public VS::Field::TrapActionHost {
 public:
  bool HasActionState(const VS::Field::TrapActionInput& input) const override {
    return VS::Battle::actionState != nullptr && input.actor >= 0 &&
           input.actor < kActorSlots &&
           VS::Battle::actors[input.actor] != nullptr &&
           VS::Battle::actors[input.actor]->stats != nullptr;
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
    auto& action = *VS::Battle::actionState;
    action.selection.primary_target.targets_position =
        kTrapPrimaryTargetPosition;
    action.selection.target_count = 1;
    action.selection.targets[0].targets_position = 0;
    action.selection.targets[0].target.target_actor =
        static_cast<std::uint8_t>(input.actor);
    action.selection.targets[0].target.target_limb =
        std::bit_cast<std::int8_t>(input.default_limb);
    action.chain_step = 1;
    action.queue_index = 0;
    action.range_origin.vx = input.position.x;
    action.range_origin.vy = input.position.y;
    action.range_origin.vz = input.position.z;
    return true;
  }
  bool ResolveAction(std::uint16_t action_id) override {
    if (VS::Battle::actionState == nullptr) {
      return false;
    }
    auto& action = *VS::Battle::actionState;
    VS::Battle::ResolveLiveActionTargets(action_id, &action.queue[0],
                                         &action.selection, 1);
    return true;
  }
  bool BeginEffect(const VS::Field::TrapEffectRequest& effect) override {
    if (!HasActionState({.actor = effect.actor})) {
      return false;
    }
    auto request = VS::Field::LiveTrapEffectRecord(effect);
    static_cast<void>(VS::Effects::LiveBeginSecondaryEffects(&request, 0));
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
                              record->trigger.lock_state.lock.state)}}
                    : std::nullopt;
    }
    const auto* record = VS::Field::ResolveLiveChestTarget(
        std::get<VS::Field::ChestTarget>(target));
    return record ? std::optional{VS::Field::UnlockEffectState{
                        .lock_level = record->lock_level,
                        .opened_flag =
                            std::bit_cast<std::uint16_t>(record->opened_flag)}}
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
      record->trigger.lock_state.lock.state = 0;
      return true;
    }
    auto* record = VS::Field::ResolveLiveChestTarget(
        std::get<VS::Field::ChestTarget>(target));
    if (record == nullptr) return false;
    record->lock_level = 0;
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
    static_cast<void>(VS::Battle::QueueLiveWorldLockedLabel(words));
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
        .tile_x = record->trigger.tile_x,
        .tile_z = record->trigger.tile_z,
        .required_item = record->key_or_sigil,
        .lock_state =
            static_cast<std::uint8_t>(record->trigger.lock_state.lock.state),
        .is_open = record->is_open != 0,
        .is_triggered = record->is_triggered != 0};
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
    record->trigger.lock_state.lock.state = 0;
    return true;
  }
  void ShowNotification(std::int32_t kind, std::int16_t item) override {
    VS::Menu::ShowLiveSceneNotification(kind, item, 1);
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
      static_cast<void>(VS::Field::BeginDoorToggleEntry(record));
    }
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kRoomObjectSoundGroup, sound);
  }
  void RestoreIdle(std::int32_t actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }

 private:
  std::span<VS::Field::MpdLockTrigger> Records() const {
    if (generation_ != VS::Field::LiveInteractionTargetDomain().generation ||
        VS::Field::roomData.lock_triggers == nullptr) {
      return {};
    }
    return {VS::Field::roomData.lock_triggers,
            VS::Field::roomData.header.lock_triggers_length /
                kOriginalLockRecordBytes};
  }
  VS::Field::MpdLockTrigger* Record() const {
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
    return VS::Field::ChestLockState{chest->tile_x, chest->tile_z,
                                     chest->lock_level, chest->required_item,
                                     chest->opened_flag};
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
    chest->lock_level = 0;
    return true;
  }
  void ShowUnlocked(std::int16_t item) override {
    VS::Menu::ShowLiveSceneNotification(kUnlockedNotification, item, 1);
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
    VS::Actors::LiveActorPlacement(actor,
                                   &VS::Field::interactionFocus.actor_query);
    VS::Field::interactionFocus.actor_query.actor_index =
        static_cast<std::int16_t>(actor);
    return true;
  }
  std::optional<VS::Field::ChestTarget> SelectedChest() const override {
    const auto* target = VS::Field::LiveInteractionTargetState().Chest(
        VS::Field::LiveInteractionTargetDomain());
    return target ? std::optional{*target} : std::nullopt;
  }
  void BeginChestFlow() override {
    VS::Field::controlMode = kChestControlMode;
    VS::Battle::actionFlowMode = 1;
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void FinishChestFlow() override {
    VS::Battle::actionFlowMode = kFinishedActionFlow;
    VS::State::tickSpeed = 2;
  }
  void RestoreIdlePoses() override {
    for (int actor = 0; actor < kIdleActorCount; ++actor) {
      VS::Actors::RestoreLiveActorIdlePose(actor);
    }
  }
  void ResetActionSelection() override {
    if (VS::Battle::actionState != nullptr) {
      VS::Battle::actionState->chain_step = 0;
      VS::Battle::actionState->selection.action_id = 0;
    }
  }
  void ReleaseNotification() override {
    VS::Menu::ReleaseLiveSceneNotification();
  }
  void ShowLocked(std::int32_t notification, std::int16_t item) override {
    VS::Battle::actionFlowMode = kContentsActionFlow;
    VS::Menu::ShowLiveSceneNotification(notification, item, 1);
  }
  void OpenChestLoot(VS::Field::ChestTarget target) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target);
    if (chest != nullptr) {
      VS::Battle::actionFlowMode = kContentsActionFlow;
      VS::Menu::Framework::OpenLiveLootEntryMenu(&chest->loot);
    }
  }
  void OpenChestStorage(std::int32_t storage) override {
    VS::Battle::actionFlowMode = kContentsActionFlow;
    VS::Menu::Framework::OpenLiveChestStorageMenu(storage);
  }
  void PauseForDialog() override {
    VS::Field::LivePauseFieldForInteractionDialog();
  }
  void PauseForMenu() override { VS::Field::LivePauseFieldForMenu(); }
  bool NotificationPending() override {
    return VS::Menu::IsLiveSceneNotificationPending() != 0;
  }
  VS::Field::ChestSelectionResult UpdateContentsSelection() override {
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    const auto* selection = VS::Menu::Framework::LiveActionSelectionState();
    if (selection == nullptr || selection->menu_status == 1) {
      return VS::Field::ChestSelectionResult::kPending;
    }
    return selection->selection_kind != 0
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
    return VS::Field::controlMode == kTrapControlMode;
  }
  void BeginDispatch() override { VS::Battle::actionFlowMode = 2; }
  bool DispatchChest(VS::Field::ChestTarget target) override {
    return VS::Field::TryOpenLiveChest(target);
  }
  bool DispatchTrap(VS::Field::TrapTarget target) override {
    const auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    const int actor = VS::Field::interactionFocus.actor_query.actor_index;
    if (trap == nullptr || actor < 0 || actor >= kActorSlots ||
        VS::Battle::actionState == nullptr ||
        VS::Battle::actors[actor] == nullptr ||
        VS::Battle::actors[actor]->stats == nullptr) {
      return false;
    }
    const auto& point =
        VS::Field::interactionFocus.actor_query.location.position;
    VS::State::tickSpeed = kTrapTickSpeed;
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
    VS::Battle::Ai::MarkLiveAiTileDynamicObstacle(trap->tile_x, trap->tile_z);
    return true;
  }
  void FinishFlow() override {
    VS::Battle::actionFlowMode = kFinishedActionFlow;
    VS::State::tickSpeed = 2;
  }
  void ReleaseNotification() override {
    VS::Menu::ReleaseLiveSceneNotification();
  }
  void ClearTarget() override {
    VS::Field::LiveInteractionTargetState().Clear();
  }
};

class LiveTrapSelectionHost final : public VS::Field::TrapSelectionHost {
 public:
  explicit LiveTrapSelectionHost(VS::Actors::ActorLocation* location)
      : location_(location) {}
  bool Valid(VS::Field::TrapTarget target, std::int32_t actor) const override {
    return location_ != nullptr && actor >= 0 && actor < kActorSlots &&
           VS::Field::ResolveLiveTrapTarget(target) != nullptr;
  }
  void BeginFlow() override {
    VS::Field::controlMode = kTrapControlMode;
    VS::Battle::actionFlowMode = 1;
    VS::Field::LiveStorePreferredActionCameraDistance();
  }
  void Select(VS::Field::TrapTarget target, std::int32_t actor) override {
    VS::Field::LiveInteractionTargetState().Select(target);
    VS::Field::interactionFocus.actor_query.location = *location_;
    VS::Field::interactionFocus.actor_query.actor_index =
        static_cast<std::int16_t>(actor);
  }
  void RestoreIdle(std::int32_t actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }
  void ShowInformation(VS::Field::TrapTarget target) override {
    const auto* trap = VS::Field::ResolveLiveTrapTarget(target);
    if (trap != nullptr && VS::Input::settings.information != 0) {
      VS::Menu::ShowLiveSceneNotification(kTrapNotification,
                                          trap->kind + kTrapActionOffset, 1);
    }
  }
  void ResetActionSelection() override {
    if (VS::Battle::actionState != nullptr) {
      VS::Battle::actionState->chain_step = 0;
      VS::Battle::actionState->selection.action_id = 0;
    }
  }

 private:
  VS::Actors::ActorLocation* location_ = nullptr;
};

class LiveObjectInteractionWaitHost final
    : public VS::Field::ObjectInteractionWaitHost {
 public:
  void SetControlMode(VS::Field::FieldControlMode mode) override {
    VS::Field::controlMode = static_cast<std::uint32_t>(mode);
  }
  void PauseField() override {
    VS::Field::LivePauseFieldForObjectInteraction();
  }
  void ReleaseNotification() override {
    VS::Menu::ReleaseLiveSceneNotification();
  }
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

bool RefreshLiveAshleyInteractionTile(const VS::Actors::ActorRecord* record) {
  constexpr std::size_t kRequiredBytes =
      offsetof(VS::Actors::ActorRecord, animation_id) +
      sizeof(decltype(VS::Actors::ActorRecord::animation_id));
  if (record == nullptr || reinterpret_cast<std::uintptr_t>(record) %
                                   alignof(VS::Actors::ActorRecord) !=
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
      .animation_id = static_cast<std::uint8_t>(record->animation_id),
      .facing = record->instance.facing,
      .tile_x = static_cast<std::uint8_t>(record->instance.current_tile_x),
      .tile_z = static_cast<std::uint8_t>(record->instance.current_tile_z)};
  LiveAshleyInteractionTileHost host;
  return RefreshAshleyInteractionTile(state, host);
}

void SetLiveEntranceDoor(const std::optional<DoorRecord>& door) {
  EntranceDoorState state;
  LiveEntranceDoorHost host;
  SetEntranceDoorRecord(state, door, host);
  StoreDoorRecord(state.door, VS::Field::entranceDoorRecord);
  VS::Field::enteredDoorId = state.entered_door_id;
}

void FindLiveEntranceDoorNearTile(std::int32_t tile_x, std::int32_t tile_z,
                                  std::int32_t layer) {
  std::vector<DoorRecord> doors;
  if (VS::Field::roomData.doors != nullptr) {
    const auto bytes = VS::Field::roomData.header.doors_length;
    if (bytes % kOriginalDoorRecordBytes != 0) {
      SetLiveEntranceDoor(std::nullopt);
      return;
    }
    const auto count =
        static_cast<std::size_t>(bytes / kOriginalDoorRecordBytes);
    for (const auto& record : std::span(VS::Field::roomData.doors, count)) {
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

VS::Field::MpdLockTrigger* ResolveLiveRoomObjectTarget(
    RoomObjectTarget target) {
  if (target.generation != LiveInteractionTargetDomain().generation ||
      VS::Field::roomData.lock_triggers == nullptr ||
      VS::Field::roomData.header.lock_triggers_length %
              kOriginalLockRecordBytes !=
          0 ||
      reinterpret_cast<std::uintptr_t>(VS::Field::roomData.lock_triggers) %
              alignof(VS::Field::MpdLockTrigger) !=
          0 ||
      target.index >= VS::Field::roomData.header.lock_triggers_length /
                          kOriginalLockRecordBytes) {
    return nullptr;
  }
  return &VS::Field::roomData.lock_triggers[target.index];
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

VS::Effects::LiveActionEffectRequest LiveTrapEffectRecord(
    const TrapEffectRequest& effect) {
  VS::Effects::LiveActionEffectRequest request{};
  request.effect_id = effect.effect_id;
  request.count = 1;
  request.source.type = kTrapSourcePointType;
  if (effect.source_position) {
    request.source.position.vx = effect.source_position->x;
    request.source.position.vy = effect.source_position->y;
    request.source.position.vz = effect.source_position->z;
  }
  request.targets[0].type = kTrapActorTargetType;
  request.targets[0].SetActor({.actor = static_cast<std::uint8_t>(effect.actor),
                               .event_actor_slot = 0,
                               .part = effect.ground_anchor,
                               .reserved3 = 0});
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
  VS::Actors::ActorLocation location{};
  location.packed_tile.target.tile_x = placement.tile.x;
  location.packed_tile.target.tile_layer = placement.tile.layer;
  location.packed_tile.target.tile_z = placement.tile.z;
  location.packed_tile.target.facing = placement.tile.spare;
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
  input.default_limb = static_cast<std::uint8_t>(
      VS::Battle::actors[actor]->stats->default_target_limb);
  return TriggerActorTrap(target, input, host);
}
}  // namespace VS::Field

// decomp: BATTLE.PRG func_80073E30 (src/BATTLE/BATTLE.PRG/146C.c:4935).
void VS::Field::TryOpenChestEntry(VS::Field::MpdLockTrigger* address, int) {
  const auto target = VS::Field::FindLiveChestTargetAddress(address);
  if (target) {
    static_cast<void>(VS::Field::TryOpenLiveChest(*target));
  }
}

// decomp: BATTLE.PRG func_80074050 (src/BATTLE/BATTLE.PRG/146C.c:4993).
void VS::Field::BeginChestInteractionEntry(VS::Field::MpdLockTrigger* address,
                                           int actor) {
  const auto target = VS::Field::FindLiveChestTargetAddress(address);
  if (target) {
    static_cast<void>(VS::Field::BeginLiveChestInteraction(*target, actor));
  }
}

// decomp: BATTLE.PRG func_800742A4 (src/BATTLE/BATTLE.PRG/146C.c:5066).
void VS::Field::FinishChestInteractionEntry() {
  LiveChestInteractionHost host;
  static_cast<void>(VS::Field::FinishChestInteraction(host));
}

// decomp: BATTLE.PRG func_800743E0 (src/BATTLE/BATTLE.PRG/146C.c:5107).
void VS::Field::OpenChestContentsEntry() {
  LiveChestInteractionHost host;
  static_cast<void>(VS::Field::OpenChestContents(host));
}

// decomp: BATTLE.PRG func_800744B8 (src/BATTLE/BATTLE.PRG/146C.c:5139).
int VS::Field::UpdateChestContentsSelectionEntry() {
  LiveChestInteractionHost host;
  return VS::Field::UpdateChestContentsSelection(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_800741D4 (src/BATTLE/BATTLE.PRG/146C.c:5044).
void VS::Field::DispatchTrapOrChestInteractionEntry() {
  LiveInteractionDispatchHost host;
  static_cast<void>(VS::Field::DispatchTrapOrChestInteraction(host));
}

// decomp: BATTLE.PRG func_80073F7C (src/BATTLE/BATTLE.PRG/146C.c:4973).
void VS::Field::BeginTrapInteractionEntry(
    VS::Field::MpdLockTrigger* address,
    VS::Actors::ActorLocation* victim_location, int actor) {
  const auto target = VS::Field::FindLiveTrapTargetAddress(address);
  if (target) {
    LiveTrapSelectionHost host(victim_location);
    static_cast<void>(VS::Field::BeginTrapInteraction(*target, actor, host));
  }
}

// decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:5080).
void VS::Field::FinishTrapInteractionEntry() {
  LiveTrapFinishHost host;
  static_cast<void>(VS::Field::FinishTrapInteraction(host));
}

// decomp: BATTLE.PRG func_80073AA4 (src/BATTLE/BATTLE.PRG/146C.c:4823).
void VS::Field::BeginObjectInteractionWaitEntry() {
  LiveObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
}

// decomp: BATTLE.PRG func_80073ACC (src/BATTLE/BATTLE.PRG/146C.c:4829).
void VS::Field::EndObjectInteractionWaitEntry() {
  LiveObjectInteractionWaitHost host;
  VS::Field::EndObjectInteractionWait(host);
}

// decomp: BATTLE.PRG func_80073AFC (src/BATTLE/BATTLE.PRG/146C.c:4836).
int VS::Field::InteractWithLockedRoomObjectEntry(
    VS::Field::MpdLockTrigger* address) {
  LiveRoomObjectInteractionHost host(address);
  return VS::Field::InteractWithLockedRoomObject(host) ==
                 VS::Field::RoomObjectInteractionResult::kWait
             ? 0
             : 1;
}

// decomp: BATTLE.PRG func_80073D30 (src/BATTLE/BATTLE.PRG/146C.c:4909).
void VS::Field::TriggerActorTrapEntry(VS::Field::MpdLockTrigger* address,
                                      VS::Actors::ActorLocationQuery* location,
                                      int actor) {
  const auto target = VS::Field::FindLiveTrapTargetAddress(address);
  if (target && location != nullptr) {
    const auto& point = location->location.position;
    static_cast<void>(VS::Field::TriggerLiveActorTrap(
        *target, {.x = point.vx, .y = point.vy, .z = point.vz}, actor));
  }
}

// decomp: BATTLE.PRG func_80074120 (src/BATTLE/BATTLE.PRG/146C.c:5019).
int VS::Field::WaitForChestCameraFocusEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::WaitForChestCameraFocus(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_8007418C (src/BATTLE/BATTLE.PRG/146C.c:5034).
int VS::Field::StepTrapCameraFocusEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::StepTrapCameraFocus(host).value_or(0);
}

// decomp: BATTLE.PRG func_8007424C (src/BATTLE/BATTLE.PRG/146C.c:5055).
int VS::Field::IsChestAnimationReadyEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::IsChestAnimationReady(host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_80074374 (src/BATTLE/BATTLE.PRG/146C.c:5089).
int VS::Field::TrackInteractionCameraToAshleyEntry() {
  LiveInteractionCameraHost host;
  return VS::Field::TrackInteractionCameraToAshley(host).value_or(0);
}

// decomp: BATTLE.PRG func_80074798 (src/BATTLE/BATTLE.PRG/146C.c:5232).
int VS::Field::CurrentSavePointIdEntry(void) {
  return VS::Field::LiveCurrentSavePointId();
}

void VS::Field::RefreshAshleyInteractionTileEntry(
    VS::Actors::ActorRecord* record) {
  static_cast<void>(VS::Field::RefreshLiveAshleyInteractionTile(record));
}

int VS::Field::TileInFrontOfActorEntry(VS::Actors::ActorLocationQuery* actor,
                                       unsigned char* output) {
  if (actor == nullptr || output == nullptr) {
    return -1;
  }
  VS::Actors::ActorPlacementView view{};
  view.tile.x =
      static_cast<std::uint8_t>(actor->location.packed_tile.target.tile_x);
  view.tile.z =
      static_cast<std::uint8_t>(actor->location.packed_tile.target.tile_z);
  view.tile.layer =
      static_cast<std::uint8_t>(actor->location.packed_tile.target.tile_layer);
  view.position.w = actor->location.position.pad;
  const auto tile = VS::Field::TileInFrontOfActor(view);
  output[0] = tile.x;
  output[1] = tile.layer;
  output[2] = tile.z;
  return static_cast<int>(tile.direction);
}

// decomp: BATTLE.PRG func_80074860 (src/BATTLE/BATTLE.PRG/146C.c:5260).
// decomp: BATTLE.PRG func_800751B8 (src/BATTLE/BATTLE.PRG/146C.c:5514).
void VS::Field::ApplyUnlockEffectToRoomObjectEntry(SVECTOR* target_position) {
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

void VS::Field::StoreEntranceDoorForReloadEntry(void) {
  const auto encoded =
      VS::Field::EnteredDoorReloadByte(VS::Field::enteredDoorId);
  VS::State::stateFlags.door_entered = std::bit_cast<char>(encoded);
}

void VS::Field::SetEntranceDoorRecordEntry(
    char (*input)[VS::Field::kEntranceRecordBytes]) {
  if (input == nullptr) {
    VS::Field::SetLiveEntranceDoor(std::nullopt);
    return;
  }
  VS::Field::MpdDoorRecord record{};
  std::memcpy(&record, input, sizeof record);
  VS::Field::SetLiveEntranceDoor(VS::Field::DoorRecordOf(record));
}

void VS::Field::FindEntranceDoorNearTileEntry(int tile_x, int tile_z,
                                              int layer) {
  VS::Field::FindLiveEntranceDoorNearTile(tile_x, tile_z, layer);
}

int VS::Field::CheckAshleyTileTriggersEntry(int actor, unsigned char* input) {
  if (input == nullptr) return 0;
  VS::Actors::ActorLocation location{};
  std::memcpy(&location, input, sizeof location);
  const VS::Actors::ActorPlacementView placement{
      .tile = {.x = location.packed_tile.target.tile_x,
               .layer = location.packed_tile.target.tile_layer,
               .z = location.packed_tile.target.tile_z,
               .spare = location.packed_tile.target.facing},
      .position = {.x = location.position.vx,
                   .y = location.position.vy,
                   .z = location.position.vz,
                   .w = location.position.pad}};
  return VS::Field::CheckLiveAshleyTileTriggers(actor, placement) ? 1 : 0;
}

int VS::Field::CheckActorTrapAndFallTriggersEntry(
    int actor, VS::Actors::ActorLocation* location) {
  if (location == nullptr) return 0;
  const VS::Actors::ActorPlacementView placement{
      .tile = {.x = location->packed_tile.target.tile_x,
               .layer = location->packed_tile.target.tile_layer,
               .z = location->packed_tile.target.tile_z,
               .spare = location->packed_tile.target.facing},
      .position = {.x = location->position.vx,
                   .y = location->position.vy,
                   .z = location->position.vz,
                   .w = location->position.pad}};
  return static_cast<int>(
      VS::Field::CheckLiveActorTrapAndFallTriggers(actor, placement));
}

int VS::Field::TryTriggerFacingTileEventEntry(int actor) {
  LiveFacingEventHost host;
  return VS::Field::TryTriggerFacingTileEvent(actor, host) ? 1 : 0;
}

// decomp: BATTLE.PRG func_800748B8 (src/BATTLE/BATTLE.PRG/146C.c:5271).
VS::Field::MpdLockTrigger* VS::Field::FindFacingRoomObjectEntry(int actor) {
  const auto target = VS::Field::FindLiveFacingRoomObject(actor);
  return target ? VS::Field::ResolveLiveRoomObjectTarget(*target) : nullptr;
}

// decomp: BATTLE.PRG func_80074950 (src/BATTLE/BATTLE.PRG/146C.c:5290).
VS::Items::TreasureChest* VS::Field::FindFacingUnlootedChestEntry(int actor) {
  const auto target = VS::Field::FindLiveFacingUnlootedChest(actor);
  return target ? VS::Field::ResolveLiveChestTarget(*target) : nullptr;
}
