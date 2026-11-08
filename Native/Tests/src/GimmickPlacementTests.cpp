#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDriveLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/ChestInteractions.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickAnimationQueue.hpp"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/GimmickPlacement.hpp"
#include "Field/InteractionTarget.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomSwitches.hpp"
#include "Field/RoomSwitchesLive.h"
#include "Field/TransitionsLive.hpp"
#include "Field/Traps.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/Runtime.h"
#include "Event/Triggers.h"
#include "Field/Doors.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
}

namespace {

struct RecordingEntranceDoorHost final : VS::Field::EntranceDoorHost {
  std::optional<std::uint8_t> scene_id;
  std::optional<std::uint8_t> observed_id;
  std::optional<std::uint8_t> SceneDoorId(std::uint8_t door_id) override {
    observed_id = door_id;
    return scene_id;
  }
};

struct RecordingAshleyFallHost final : VS::Field::AshleyFallHost {
  std::optional<VS::Field::ScriptedFallDestination> destination;
  std::optional<std::int16_t> hp = 21;
  VS::Field::DoorRecord reload{.destination_zone_id = 9,
                               .destination_room_id = 4};
  std::optional<VS::Field::DoorRecord> transition;
  std::optional<VS::Field::DoorRecord> entrance;
  VS::Field::AshleyTransitionRoute route =
      VS::Field::AshleyTransitionRoute::kRoom;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::ScriptedFallDestination> ScriptedDestination()
      override {
    calls.push_back(0);
    return destination;
  }
  std::optional<std::int16_t> CurrentHp(std::int32_t) override {
    calls.push_back(1);
    return hp;
  }
  void ShowFallDamage(std::int32_t) override { calls.push_back(2); }
  void SetHp(std::int32_t, std::int16_t value) override {
    calls.push_back(3);
    hp = value;
  }
  void BeginDefeat(std::int32_t) override { calls.push_back(4); }
  VS::Field::DoorRecord StoreEntranceForReload() override {
    calls.push_back(5);
    return reload;
  }
  std::uint8_t CurrentZoneId() const override { return 9; }
  void BeginTransition(const VS::Field::DoorRecord& door,
                       VS::Field::AshleyTransitionRoute value) override {
    calls.push_back(6);
    transition = door;
    route = value;
  }
  void ResetRunMomentum(std::int32_t) override { calls.push_back(7); }
  void SetEntrance(const std::optional<VS::Field::DoorRecord>& door) override {
    calls.push_back(8);
    entrance = door;
  }
};

struct RecordingAshleyTileHost final : VS::Field::AshleyTileTriggerHost {
  RecordingAshleyFallHost fall;
  std::uint32_t flags = 0;
  VS::Field::AshleyTileTriggerCell cell{};
  std::optional<VS::Actors::PackedTile> last{VS::Actors::PackedTile{}};
  VS::Actors::PackedTile collision_tile{};
  VS::Actors::PackedTile doors_tile{};
  bool transitioning = false;
  bool event_control = false;
  bool event_started = false;
  bool stored_location = false;
  std::optional<VS::Field::ActorTileTrap> trap;
  std::vector<VS::Field::DoorRecord> doors;
  std::uint8_t flag_value = 0;

  std::uint32_t StateFlags(std::int32_t) override {
    fall.calls.push_back(10);
    return flags;
  }
  VS::Field::AshleyTileTriggerCell Collision(
      const VS::Actors::PackedTile& tile) override {
    fall.calls.push_back(11);
    collision_tile = tile;
    return cell;
  }
  bool TransitionInProgress() override {
    fall.calls.push_back(12);
    return transitioning;
  }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(std::int32_t) override {
    fall.calls.push_back(13);
    return last;
  }
  bool EventOwnsControl() override {
    fall.calls.push_back(14);
    return event_control;
  }
  bool TriggerPlayerTileEvent() override {
    fall.calls.push_back(15);
    return event_started;
  }
  void BeginQueuedCutscene() override { fall.calls.push_back(16); }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile&) override {
    fall.calls.push_back(17);
    return trap;
  }
  void SpawnTrapTileEffect(const VS::Actors::ActorPlacementView&) override {
    fall.calls.push_back(18);
  }
  void BeginTrap(VS::Field::TrapTarget, const VS::Actors::ActorPlacementView&,
                 std::int32_t) override {
    fall.calls.push_back(19);
  }
  void StoreTriggerTile(std::int32_t, const VS::Actors::PackedTile& tile,
                        std::uint8_t mode, bool store_location) override {
    fall.calls.push_back(20);
    stored_location = store_location;
    auto next = store_location ? tile : last.value_or(VS::Actors::PackedTile{});
    next.spare = mode;
    last = next;
  }
  std::vector<VS::Field::DoorRecord> DoorsAt(
      const VS::Actors::PackedTile& tile) override {
    fall.calls.push_back(21);
    doors_tile = tile;
    return doors;
  }
  std::uint8_t ScriptFlag(std::uint16_t) override {
    fall.calls.push_back(22);
    return flag_value;
  }
  VS::Field::AshleyFallHost& FallHost() override { return fall; }
};

struct RecordingActorTileTriggerHost final : VS::Field::ActorTileTriggerHost {
  std::uint32_t flags = 0;
  VS::Field::ActorTileTriggerCell cell{};
  bool transition = false;
  bool event_control = false;
  bool defeat = true;
  bool fall_event = false;
  std::optional<VS::Actors::PackedTile> last{VS::Actors::PackedTile{}};
  std::optional<VS::Field::ActorTileTrap> trap;
  VS::Actors::PackedTile collision_tile{};
  std::vector<std::int32_t> calls;

  std::uint32_t StateFlags(std::int32_t) override {
    calls.push_back(0);
    return flags;
  }
  VS::Field::ActorTileTriggerCell Collision(
      const VS::Actors::PackedTile& tile) override {
    calls.push_back(1);
    collision_tile = tile;
    return cell;
  }
  bool TransitionInProgress() override {
    calls.push_back(2);
    return transition;
  }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(std::int32_t) override {
    calls.push_back(3);
    return last;
  }
  bool EventOwnsControl() override {
    calls.push_back(4);
    return event_control;
  }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile&) override {
    calls.push_back(5);
    return trap;
  }
  void BeginTrap(VS::Field::TrapTarget, const VS::Actors::ActorPlacementView&,
                 std::int32_t) override {
    calls.push_back(6);
  }
  void StoreTriggerTile(std::int32_t, const VS::Actors::PackedTile& tile,
                        std::uint8_t mode, bool store_location) override {
    calls.push_back(7);
    if (!last) return;
    if (store_location) *last = tile;
    last->spare = mode;
  }
  bool DefeatForFall(std::int32_t) override {
    calls.push_back(8);
    return defeat;
  }
  bool TriggerFallEvent() override {
    calls.push_back(9);
    return fall_event;
  }
};

struct RecordingChestInteractionHost final : VS::Field::ChestInteractionHost {
  std::optional<VS::Field::ChestLockState> chest{VS::Field::ChestLockState{}};
  bool flag = false;
  bool item = false;
  std::vector<std::int32_t> calls;
  std::optional<VS::Field::ChestTarget> selected;
  bool notification_pending = false;
  VS::Field::ChestSelectionResult contents_result =
      VS::Field::ChestSelectionResult::kPending;

  std::optional<VS::Field::ChestLockState> Chest(
      VS::Field::ChestTarget) const override {
    return chest;
  }
  bool ScriptFlag(std::int32_t) override { return flag; }
  bool HasItem(std::int16_t) override { return item; }
  void SetScriptFlag(std::int32_t value) override {
    calls.push_back(value);
    flag = true;
  }
  void ConsumeItem(std::int16_t value) override { calls.push_back(value); }
  bool Unlock(VS::Field::ChestTarget) override {
    if (!chest) {
      return false;
    }
    chest->lock_level = 0;
    calls.push_back(1000);
    return true;
  }
  void ShowUnlocked(std::int16_t) override { calls.push_back(1001); }
  void PlayPose(const VS::Field::ChestLockState&, std::uint8_t pose) override {
    calls.push_back(pose);
  }
  bool StartAnimation(VS::Field::ChestTarget) override {
    calls.push_back(1002);
    return true;
  }
  bool SelectChest(VS::Field::ChestTarget target, std::int32_t) override {
    selected = target;
    return true;
  }
  std::optional<VS::Field::ChestTarget> SelectedChest() const override {
    return selected;
  }
  void BeginChestFlow() override { calls.push_back(1010); }
  void FinishChestFlow() override { calls.push_back(1011); }
  void RestoreIdlePoses() override { calls.push_back(1012); }
  void ResetActionSelection() override { calls.push_back(1013); }
  void ReleaseNotification() override { calls.push_back(1014); }
  void ShowLocked(std::int32_t kind, std::int16_t) override {
    calls.push_back(kind);
  }
  void OpenChestLoot(VS::Field::ChestTarget) override { calls.push_back(1015); }
  void OpenChestStorage(std::int32_t storage) override {
    calls.push_back(storage);
  }
  void PauseForDialog() override { calls.push_back(1016); }
  void PauseForMenu() override { calls.push_back(1017); }
  bool NotificationPending() override { return notification_pending; }
  VS::Field::ChestSelectionResult UpdateContentsSelection() override {
    return contents_result;
  }
  void ReleaseContentsRows() override { calls.push_back(1018); }
  void ResumeField() override { calls.push_back(1019); }
};

struct RecordingInteractionDispatchHost final
    : VS::Field::InteractionDispatchHost {
  VS::Field::InteractionTarget target;
  bool trap_mode = false;
  std::vector<int> calls;
  VS::Field::InteractionTarget Target() const override { return target; }
  bool TrapMode() const override { return trap_mode; }
  void BeginDispatch() override { calls.push_back(0); }
  bool DispatchTrap(VS::Field::TrapTarget) override {
    calls.push_back(1);
    return true;
  }
  bool DispatchChest(VS::Field::ChestTarget) override {
    calls.push_back(2);
    return true;
  }
};

struct RecordingTrapInteractionHost final : VS::Field::TrapSelectionHost,
                                            VS::Field::TrapFinishHost {
  bool valid = true;
  bool disable_succeeds = true;
  std::optional<VS::Field::TrapTarget> selected;
  std::optional<VS::Field::TrapTarget> disabled;
  std::int32_t selected_actor = -1;
  std::vector<std::int32_t> calls;

  bool Valid(VS::Field::TrapTarget, std::int32_t) const override {
    return valid;
  }
  void BeginFlow() override { calls.push_back(1000); }
  void Select(VS::Field::TrapTarget target, std::int32_t actor) override {
    selected = target;
    selected_actor = actor;
    calls.push_back(1001);
  }
  void RestoreIdle(std::int32_t actor) override { calls.push_back(actor); }
  void ShowInformation(VS::Field::TrapTarget) override {
    calls.push_back(1002);
  }
  void ResetActionSelection() override { calls.push_back(1003); }
  std::optional<VS::Field::TrapTarget> Target() const override {
    return selected;
  }
  void FinishFlow() override { calls.push_back(1004); }
  void ReleaseNotification() override { calls.push_back(1005); }
  bool DisableTrap(VS::Field::TrapTarget target) override {
    disabled = target;
    calls.push_back(1006);
    return disable_succeeds;
  }
  void ClearTarget() override {
    selected.reset();
    calls.push_back(1007);
  }
};

using SnapshotBytes = std::array<std::byte, sizeof(Field_GimmickSnapshot)>;

struct RecordingTrapActionHost final : VS::Field::TrapActionHost {
  bool action_state = true;
  bool stage_succeeds = true;
  bool resolve_succeeds = true;
  bool remove_during_resolve = false;
  bool change_during_resolve = false;
  mutable std::optional<std::int16_t> kind = 0;
  std::optional<VS::Field::TrapActionInput> staged;
  std::optional<VS::Field::TrapEffectRequest> effect;
  std::vector<std::int32_t> calls;

  bool HasActionState(const VS::Field::TrapActionInput&) const override {
    return action_state;
  }
  std::optional<std::int16_t> Kind(VS::Field::TrapTarget) const override {
    return kind;
  }
  bool StageVictim(const VS::Field::TrapActionInput& input) override {
    calls.push_back(1000);
    staged = input;
    return stage_succeeds;
  }
  bool ResolveAction(std::uint16_t action_id) override {
    calls.push_back(action_id);
    if (remove_during_resolve) {
      kind.reset();
    } else if (change_during_resolve) {
      kind = 1;
    }
    return resolve_succeeds;
  }
  bool BeginEffect(const VS::Field::TrapEffectRequest& request) override {
    calls.push_back(1001);
    effect = request;
    return true;
  }
};

struct RecordingInteractionCameraHost final : VS::Field::InteractionCameraHost {
  std::optional<VS::Field::TrapPosition> focus{
      VS::Field::TrapPosition{1, 2, 3}};
  std::optional<VS::Field::TrapPosition> ashley{
      VS::Field::TrapPosition{4, 5, 6}};
  std::int32_t speed = 0;
  std::uint32_t flags = 0;
  bool pending = false;
  bool transition = false;
  std::vector<std::int32_t> calls;
  mutable std::size_t flag_reads = 0;
  VS::Field::TrapPosition stepped{};
  std::int32_t mode = -1;
  std::int32_t requested_speed = 0;

  std::optional<VS::Field::TrapPosition> Focus() const override {
    return focus;
  }
  std::optional<VS::Field::TrapPosition> AshleyPosition() const override {
    return ashley;
  }
  std::int32_t StepLookAt(const VS::Field::TrapPosition& point,
                          std::int32_t step_mode,
                          std::int32_t step_speed) override {
    stepped = point;
    mode = step_mode;
    requested_speed = step_speed;
    calls.push_back(0);
    return speed;
  }
  std::uint32_t AshleyStateFlags() const override {
    ++flag_reads;
    return flags;
  }
  bool NotificationPending() override {
    calls.push_back(1);
    return pending;
  }
  bool AshleyTransitionPose() override {
    calls.push_back(2);
    return transition;
  }
};

struct RecordingFacingEventHost final : VS::Field::FacingEventHost {
  std::optional<VS::Actors::ActorPlacementView> placement{
      VS::Actors::ActorPlacementView{}};
  mutable std::int32_t observed_actor = -1;
  mutable std::size_t reads = 0;
  bool result = false;
  std::optional<VS::Field::FacingTile> evaluated;

  std::optional<VS::Actors::ActorPlacementView> Placement(
      std::int32_t actor) const override {
    ++reads;
    observed_actor = actor;
    return placement;
  }
  bool EvaluateInteraction(const VS::Field::FacingTile& tile) override {
    evaluated = tile;
    return result;
  }
};

struct RecordingFacingResourceHost final : VS::Field::FacingResourceHost {
  std::optional<VS::Actors::ActorPlacementView> placement{
      VS::Actors::ActorPlacementView{}};
  std::optional<VS::Field::RoomObjectTarget> object{
      VS::Field::RoomObjectTarget{2, 7}};
  std::optional<VS::Field::FacingChestState> chest{
      VS::Field::FacingChestState{.target = {3, 7}, .opened_flag = 9}};
  std::int32_t floor = 0;
  bool collected = false;
  std::vector<std::int32_t> calls;
  std::int32_t observed_flag = -1;

  std::optional<VS::Actors::ActorPlacementView> Placement(
      std::int32_t) const override {
    return placement;
  }
  std::optional<VS::Field::RoomObjectTarget> RoomObject(
      const VS::Field::FacingTile&) override {
    calls.push_back(0);
    return object;
  }
  std::optional<VS::Field::FacingChestState> Chest(
      const VS::Field::FacingTile&) override {
    calls.push_back(1);
    return chest;
  }
  std::int32_t FloorHeight(const VS::Field::FacingTile&) override {
    calls.push_back(2);
    return floor;
  }
  bool ScriptFlag(std::int32_t flag) override {
    calls.push_back(3);
    observed_flag = flag;
    return collected;
  }
};

struct RecordingObjectInteractionWaitHost final
    : VS::Field::ObjectInteractionWaitHost {
  VS::Field::FieldControlMode mode = VS::Field::FieldControlMode::kNone;
  std::vector<int> calls;

  void SetControlMode(VS::Field::FieldControlMode value) override {
    mode = value;
    calls.push_back(0);
  }
  void PauseField() override { calls.push_back(1); }
  void ReleaseNotification() override { calls.push_back(2); }
  void ResumeField() override { calls.push_back(3); }
  void EnterExploration() override {
    mode = VS::Field::FieldControlMode::kExploration;
    calls.push_back(4);
  }
};

struct RecordingRoomObjectInteractionHost final
    : VS::Field::RoomObjectInteractionHost {
  std::optional<VS::Field::RoomObjectLockState> state{
      VS::Field::RoomObjectLockState{}};
  bool has_item = false;
  bool previous_flag = false;
  bool unlock_succeeds = true;
  std::int16_t flag_value = 0;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::RoomObjectLockState> State() const override {
    return state;
  }
  bool HasItem(std::int16_t) override {
    calls.push_back(0);
    return has_item;
  }
  bool PreviousUnlockFlag(std::int32_t) override {
    calls.push_back(1);
    return previous_flag;
  }
  void SetUnlockFlag(std::int32_t flag, std::int16_t value) override {
    calls.push_back(flag);
    flag_value = value;
  }
  void ConsumeItem(std::int16_t item) override { calls.push_back(item); }
  bool Unlock() override {
    calls.push_back(1000);
    if (unlock_succeeds && state) {
      state->lock_state = 0;
    }
    return unlock_succeeds;
  }
  void ShowNotification(std::int32_t kind, std::int16_t) override {
    calls.push_back(kind);
  }
  void PlayPose(const VS::Field::RoomObjectLockState&,
                std::uint8_t pose) override {
    calls.push_back(pose);
  }
  void BeginToggle() override { calls.push_back(1001); }
  void PlaySound(std::int32_t sound) override { calls.push_back(sound); }
  void RestoreIdle(std::int32_t actor) override {
    CHECK(actor == 0);
    calls.push_back(1002);
  }
};

struct RoomObjectRecordFixture {
  Field_RoomRecord saved = Field_roomData;
  std::array<Field_MpdLockTrigger, 2> locks{};

  RoomObjectRecordFixture() {
    Field_roomData.lockTriggers = locks.data();
    Field_roomData.header.lockTriggersLength = 0x28;
  }
  ~RoomObjectRecordFixture() {
    Field_roomData = saved;
    VS::Field::InvalidateLiveInteractionTargets();
  }
};

struct SnapshotFixture {
  std::array<Field_GimmickInstance*, 16> saved{};
  std::unique_ptr<Field_GimmickInstance> object =
      std::make_unique<Field_GimmickInstance>();

  SnapshotFixture() {
    std::ranges::copy(Field_gimmickInstances, saved.begin());
    std::ranges::fill(Field_gimmickInstances, nullptr);
    object->parent = 0xFF;
  }
  ~SnapshotFixture() { std::ranges::copy(saved, Field_gimmickInstances); }
};

struct ActorTileTriggerLiveFixture {
  Battle_Actor* saved_actor = Battle_actors[2];
  Actors_ActorRecord* saved_record = Actors_actorRecords[2];
  decltype(Field_roomData) saved_room = Field_roomData;
  decltype(Field_controlMode) saved_control = Field_controlMode;
  std::unique_ptr<Battle_Actor> actor = std::make_unique<Battle_Actor>();
  std::unique_ptr<Actors_ActorRecord> record =
      std::make_unique<Actors_ActorRecord>();

  ActorTileTriggerLiveFixture() {
    Battle_actors[2] = actor.get();
    Actors_actorRecords[2] = record.get();
    Field_roomData = {};
    Field_controlMode = 3;
  }
  ~ActorTileTriggerLiveFixture() {
    Battle_actors[2] = saved_actor;
    Actors_actorRecords[2] = saved_record;
    Field_roomData = saved_room;
    Field_controlMode = saved_control;
  }
};

struct SceneDoorIdFixture {
  decltype(Field_roomData) saved_room = Field_roomData;
  Field_MpdDoorRecord saved_entrance = Field_entranceDoorRecord;
  decltype(Field_enteredDoorId) saved_id = Field_enteredDoorId;
  std::array<std::uint32_t, 136> scene{};

  SceneDoorIdFixture() {
    scene[6] = 129;
    scene[135] = 0xFE00;
    Field_roomData = {};
    Field_roomData.minimapScene =
        reinterpret_cast<decltype(Field_roomData.minimapScene)>(scene.data());
  }
  ~SceneDoorIdFixture() {
    Field_roomData = saved_room;
    Field_entranceDoorRecord = saved_entrance;
    Field_enteredDoorId = saved_id;
  }
};

struct ScriptedFallDoorFixture {
  ActorTileTriggerLiveFixture actors;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{256} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<Battle_Actor*, std::size(Battle_actors)> saved_actors{};
  std::array<Actors_ActorRecord*, std::size(Actors_actorRecords)>
      saved_records{};
  VS::Field::RoomTransitionState saved_transition =
      VS::Field::LoadLiveRoomTransition();
  Field_MpdDoorRecord saved_entrance = Field_entranceDoorRecord;
  decltype(Field_zoneContext) saved_zone = Field_zoneContext;
  Field_MpdCollisionHeader collision{
      .width = 2, .height = 2, .heightOrigin = 0, .cellCount = 4};
  std::array<Field_MpdTileCell, 4> cells{};
  Field_MpdEnvironment environment{};

  ScriptedFallDoorFixture() {
    REQUIRE(saved_zone.zndData == nullptr);
    REQUIRE(saved_zone.zndDiscRequest == nullptr);
    std::ranges::copy(Battle_actors, saved_actors.begin());
    std::ranges::copy(Actors_actorRecords, saved_records.begin());
    std::ranges::fill(Battle_actors, nullptr);
    std::ranges::fill(Actors_actorRecords, nullptr);
    Battle_actors[2] = actors.actor.get();
    Actors_actorRecords[2] = actors.record.get();
    VS::Field::StoreLiveRoomTransition({});
    Field_zoneContext = {};
    Field_zoneContext.zndId = 9;
    cells[3].isPit = 1;
    environment.fallDestinationZone = 9;
    environment.fallDestinationRoom = 3;
    Field_roomData.collisionMap = &collision;
    Field_roomData.tileCells = cells.data();
    Field_roomData.environment = &environment;
    Field_roomData.header.collisionMapLength = sizeof collision;
    Field_roomData.header.tileCellsLength = sizeof cells;
    Field_roomData.header.environmentLength = sizeof environment;
  }
  ~ScriptedFallDoorFixture() {
    if (Field_zoneContext.zndDiscRequest != nullptr) {
      VS::Content::ReleaseLiveDiscRequest(Field_zoneContext.zndDiscRequest);
    }
    if (Field_zoneContext.zndData != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(Field_zoneContext.zndData);
    }
    VS::Field::StoreLiveRoomTransition(saved_transition);
    Field_entranceDoorRecord = saved_entrance;
    Field_zoneContext = saved_zone;
    Field_roomData = actors.saved_room;
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_records, Actors_actorRecords);
  }
};

struct TriggerFixture {
  SnapshotFixture snapshots;
  VS::Event::RoomEventLease room = VS::Event::LiveRoomScriptSections();
  VS::Event::PlaybackPhase phase = VS::Event::LiveScriptVm().Phase();
  VS::Event::EventSource source = VS::Event::LiveScriptVm().Source();
  std::int16_t event_id = VS::Event::LiveScriptVm().EventId();
  std::int16_t entry = Event_hasRoomEntryScript;
  char direct = Event_isDirectEventRequest;

  TriggerFixture() {
    VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
    Event_isDirectEventRequest = 0;
    Event_hasRoomEntryScript = 0;
  }
  ~TriggerFixture() {
    VS::Event::PublishLiveRoomScriptSections(room);
    VS::Event::LiveScriptVm().SetPhase(phase);
    VS::Event::LiveScriptVm().SetSource(source);
    VS::Event::LiveScriptVm().SetEventId(event_id);
    Event_hasRoomEntryScript = entry;
    Event_isDirectEventRequest = direct;
  }
};

void PublishTrigger(std::initializer_list<std::uint8_t> bytes) {
  const std::vector<std::uint8_t> data(bytes);
  const auto room = VS::Event::RoomEventSections::Copy({.triggers = data});
  REQUIRE(room);
  VS::Event::PublishLiveRoomScriptSections(*room);
}

Field_GimmickSnapshot SentinelSnapshot() {
  return {0xAA, 0xBB, 0xCC, 0xDD, 0x12345678, 0x7F7F7F7F};
}

std::uint8_t ExpectedHeight(const Field_GimmickInstance& object) {
  const auto support = VS::Field::LiveSampleSupportHeightAt(
      object.positionX, object.positionY, object.positionZ, 0, 0, 0);
  const auto difference = static_cast<std::int64_t>(support) - object.positionY;
  const auto wrapped =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
  return static_cast<std::uint8_t>(wrapped / 128);
}

void CheckValidSnapshot(int slot, const Field_GimmickInstance& object) {
  const auto expected_height = ExpectedHeight(object);
  auto output = SentinelSnapshot();
  REQUIRE(Field_GimmickPlacementEntry(slot, &output) == 0);
  CHECK(output.placementByte0 == 5);
  CHECK(output.placementByte2 == 9);
  CHECK(output.orientation == 3);
  CHECK(output.heightAboveGround == expected_height);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) & 0xFFFF) == 0xFB2E);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) >> 16) ==
        static_cast<std::uint16_t>(object.positionY));
  CHECK(std::bit_cast<std::uint32_t>(output.positionZ) == 0x13570929);
}

struct RecordingGimmickHost final : VS::Field::GimmickPlacementHost {
  static constexpr std::size_t kSlots = 16;
  std::array<std::optional<VS::Field::GimmickPlacementState>, kSlots> states{};
  mutable std::size_t reads = 0;
  std::size_t support_calls = 0;
  std::int32_t support_height = 0;

  std::size_t GimmickSlots() const override { return states.size(); }
  std::optional<VS::Field::GimmickPlacementState> Gimmick(
      std::size_t slot) const override {
    ++reads;
    return states.at(slot);
  }
  std::int32_t SupportHeight(const VS::Field::GimmickPlacementState&) override {
    ++support_calls;
    return support_height;
  }
};

std::uint8_t OriginalGimmickHeight(std::int32_t support_height,
                                   std::int16_t position_y) {
  // CPU 800A00BC subu, 800A00C0 bgez, 800A00C8 addiu127, 800A00CC sra7.
  const auto difference =
      static_cast<std::int64_t>(support_height) - position_y;
  const auto wrapped =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
  const auto rounded =
      wrapped < 0 ? static_cast<std::int64_t>(wrapped) + 127 : wrapped;
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(rounded) >> 7);
}

void CheckTypedGimmickPlacement(RecordingGimmickHost& host,
                                const VS::Field::GimmickPlacementState& state) {
  const auto snapshot = VS::Field::GimmickPlacement(0, host);
  if (!snapshot) {
    FAIL("Stationary gimmick placement must produce a snapshot");
    return;
  }
  REQUIRE(snapshot);
  CHECK(snapshot->tile_x == 0xFF);
  CHECK(snapshot->tile_z == 9);
  CHECK(snapshot->orientation == 0x81);
  CHECK(snapshot->height_above_ground ==
        OriginalGimmickHeight(host.support_height, state.position_y));
  CHECK(
      std::bit_cast<std::uint32_t>(snapshot->position_xy) ==
      (0x0000FB2E |
       (static_cast<std::uint32_t>(static_cast<std::uint16_t>(state.position_y))
        << 16)));
  CHECK(std::bit_cast<std::uint32_t>(snapshot->position_z_tail) == 0xFEDC0929);
}

}  // namespace

namespace {

struct RecordingAnimationQueueHost final
    : VS::Field::GimmickAnimationQueueHost {
  bool target = true;
  bool refuse_store = false;
  VS::Field::GimmickAnimationOccupancy slots{};
  mutable std::size_t reads = 0;
  std::size_t stores = 0;
  std::size_t count_writes = 0;
  std::size_t pending = 0;
  std::optional<std::size_t> stored_slot;

  bool HasRequestedTarget() const override { return target; }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    ++reads;
    return slots;
  }
  bool StoreRequestedJob(std::size_t slot,
                         VS::Field::GimmickAnimationKind) override {
    if (refuse_store) {
      return false;
    }
    ++stores;
    stored_slot = slot;
    slots.at(slot) = true;
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    ++count_writes;
    pending = count;
  }
};

struct RecordingAnimationDispatchHost final
    : VS::Field::GimmickAnimationDispatchHost {
  std::array<std::optional<VS::Field::GimmickAnimationJobSnapshot>,
             VS::Field::kGimmickAnimationSlots>
      jobs{};
  std::vector<std::int32_t> calls;
  bool finish_ready = true;
  bool replace_during_finish = false;
  bool reset_during_finish = false;

  std::int32_t PendingCount() const override {
    return static_cast<std::int32_t>(std::ranges::count_if(
        jobs, [](const auto& job) { return job.has_value(); }));
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    VS::Field::GimmickAnimationOccupancy occupied{};
    for (std::size_t slot = 0; slot < jobs.size(); ++slot) {
      occupied[slot] = jobs[slot].has_value();
    }
    return occupied;
  }
  std::optional<VS::Field::GimmickAnimationJobSnapshot> Job(
      std::size_t slot) const override {
    return jobs.at(slot);
  }
  bool Matches(std::size_t slot,
               const VS::Field::GimmickAnimationJobSnapshot& expected) const {
    const auto& job = jobs.at(slot);
    return job && job->serial == expected.serial;
  }
  bool StoreCountdown(std::size_t slot,
                      const VS::Field::GimmickAnimationJobSnapshot& expected,
                      std::uint8_t countdown) override {
    if (!Matches(slot, expected)) {
      return false;
    }
    if (auto& job = jobs.at(slot)) {
      job->countdown = countdown;
    }
    return true;
  }
  bool FinishDoor(std::size_t slot,
                  const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(1);
    if (reset_during_finish) {
      for (auto& job : jobs) {
        job.reset();
      }
    }
    if (replace_during_finish) {
      if (auto& job = jobs.at(slot)) {
        ++job->serial;
      }
    }
    return finish_ready;
  }
  void ToggleSwitch(std::size_t,
                    const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(2);
  }
  void StartDoor(std::size_t,
                 const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(3);
  }
  bool RetireJob(
      std::size_t slot,
      const VS::Field::GimmickAnimationJobSnapshot& expected) override {
    if (!Matches(slot, expected)) {
      return false;
    }
    calls.push_back(4);
    jobs.at(slot).reset();
    return true;
  }
};

struct RecordingAnimationResetHost final
    : VS::Field::GimmickAnimationResetHost {
  std::vector<std::size_t> calls;
  void ClearPendingCount() override { calls.push_back(8); }
  void ClearMarkerTicks() override { calls.push_back(9); }
  void ClearJob(std::size_t slot) override { calls.push_back(slot); }
};

struct AnimationQueueFixture {
  static constexpr std::size_t kSlots = 8;
  std::array<Field_GimmickAnimationJob, kSlots> saved{};
  std::int16_t pending = Field_pendingGimmickAnimationCount;
  std::uint16_t marker_ticks = Field_triggerMarkerFrameTicks;

  AnimationQueueFixture() {
    std::ranges::copy(Field_gimmickAnimationQueue, saved.begin());
    Field_ResetGimmickAnimationQueueEntry();
  }
  ~AnimationQueueFixture() {
    std::ranges::copy(saved, Field_gimmickAnimationQueue);
    Field_pendingGimmickAnimationCount = pending;
    Field_triggerMarkerFrameTicks = marker_ticks;
  }
  AnimationQueueFixture(const AnimationQueueFixture&) = delete;
  AnimationQueueFixture& operator=(const AnimationQueueFixture&) = delete;
};

struct ChestTargetFixture {
  Field_RoomRecord saved = Field_roomData;
  std::uint32_t control_mode = Field_controlMode;
  decltype(Battle_actionFlowMode) action_flow = Battle_actionFlowMode;
  Actors_ActorLocationQuery actor_query = Field_interactionFocus.actor_query;
  AnimationQueueFixture queue;
  std::array<Items_TreasureChest, 1> chests{};

  ChestTargetFixture() {
    Field_roomData.containers = chests.data();
    Field_roomData.header.containersLength = 0x234;
    VS::Field::InvalidateLiveInteractionTargets();
    chests[0].tileX = -1;
    chests[0].tileZ = -1;
    chests[0].layer = -1;
    chests[0].lidRotationId = -1;
  }
  ~ChestTargetFixture() {
    Field_roomData = saved;
    VS::Field::InvalidateLiveInteractionTargets();
    Field_controlMode = control_mode;
    Battle_actionFlowMode = action_flow;
    Field_interactionFocus.actor_query = actor_query;
  }
};

void ConfigureOneFrameDelayedDoor(Field_MpdLockTrigger& target) {
  target.trigger.tileX = -1;
  target.trigger.tileZ = -1;
  target.trigger.floorLayer = -1;
  target.trigger.lockState.lock.roomEffect = -1;
  target.linkedLockIndex = -1;
  target.isTriggered = 1;
  target.delayFrames = 1;
}

void QueueOneFrameSwitchJobs(std::span<Field_MpdLockTrigger> targets) {
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    auto& target = targets[slot];
    target.trigger.tileX = -1;
    target.trigger.tileZ = -1;
    target.trigger.floorLayer = -1;
    target.trigger.kind = -1;
    target.trigger.activation = 1;
    Field_QueueGimmickAnimationEntry(2, &target);
  }
  Field_QueueGimmickAnimationEntry(2, &targets.back());
}

void CheckDrainedSwitchJobs(std::span<const Field_MpdLockTrigger> targets) {
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    CHECK(targets[slot].trigger.activation == 0);
    CHECK(targets[slot].trigger.lockState.switchValue == 1);
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 0);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == nullptr);
  }
}

void CheckAnimationKindResets(Field_MpdLockTrigger& target) {
  for (const auto kind : {1, 2, 3}) {
    Field_QueueGimmickAnimationEntry(kind, &target);
    CHECK(Field_pendingGimmickAnimationCount == 1);
    Field_ResetGimmickAnimationQueueEntry();
    CHECK(Field_pendingGimmickAnimationCount == 0);
  }
}

enum class SwitchCallKind { kSound, kEffect, kStore, kCell };

struct SwitchCall {
  SwitchCallKind kind = SwitchCallKind::kSound;
  std::int32_t value = 0;
  friend bool operator==(const SwitchCall&, const SwitchCall&) = default;
};

struct RecordingSwitchHost final : VS::Field::RoomSwitchHost {
  std::optional<VS::Field::RoomSwitchState> state;
  std::optional<VS::Field::RoomSwitchState> after_effect;
  bool remove_after_sound = false;
  std::vector<SwitchCall> calls;
  std::array<std::int32_t, 3> cell_query{};

  std::optional<VS::Field::RoomSwitchState> State() const override {
    return state;
  }
  void PlaySound(std::int32_t sound) override {
    calls.push_back({SwitchCallKind::kSound, sound});
    if (remove_after_sound) {
      state.reset();
    }
  }
  void StartEffect(std::int32_t kind) override {
    calls.push_back({SwitchCallKind::kEffect, kind});
    if (after_effect) {
      state = after_effect;
    }
  }
  void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) override {
    calls.push_back({SwitchCallKind::kStore, value});
    if (state) {
      state->switch_value = value;
      state->marker_fade = marker_fade;
    }
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& query,
                          std::uint8_t passability) override {
    calls.push_back({SwitchCallKind::kCell, passability});
    cell_query = {query.tile_x, query.tile_z, query.floor_layer};
  }
};

struct RecordingSwitchStartHost final : VS::Field::RoomSwitchStartHost {
  std::optional<VS::Field::RoomSwitchState> state{VS::Field::RoomSwitchState{}};
  bool refuse = false;
  bool refuse_store = false;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::RoomSwitchState> State() const override {
    return state;
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    if (refuse_store || !state) {
      return false;
    }
    state->marker_fade = marker;
    state->activation = activation;
    calls.push_back(marker);
    calls.push_back(activation);
    return true;
  }
  bool QueueToggle() override {
    calls.push_back(2);
    return !refuse;
  }
  bool ToggleImmediately() override {
    calls.push_back(3);
    return !refuse;
  }
};

void CheckOriginalCountdownByte(VS::Field::GimmickAnimationKind kind,
                                int byte) {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      kind, static_cast<std::uint8_t>(byte), 1, true};
  const int original_decrement = byte - 1;
  const auto original_byte = static_cast<std::uint8_t>(original_decrement);
  const bool complete = kind == VS::Field::GimmickAnimationKind::kToggleSwitch
                            ? std::bit_cast<std::int8_t>(original_byte) <= 0
                            : original_byte == 0;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) ==
        static_cast<std::size_t>(complete));
  CHECK(host.PendingCount() == static_cast<std::int32_t>(!complete));
  if (const auto& job = host.jobs[0]) {
    CHECK(job->countdown == original_byte);
  }
}

void CheckChestOpenAndBlockedRules(VS::Field::ChestTarget target) {
  RecordingChestInteractionHost open;
  CHECK(VS::Field::TryOpenChest(target, open));
  CHECK(open.calls == std::vector<std::int32_t>{2, 1002});
  RecordingChestInteractionHost blocked;
  blocked.chest = VS::Field::ChestLockState{.lock_level = 2};
  CHECK_FALSE(VS::Field::TryOpenChest(target, blocked));
  CHECK(blocked.calls == std::vector<std::int32_t>{3});
  blocked.chest.reset();
  CHECK_FALSE(VS::Field::TryOpenChest(target, blocked));
}

void CheckChestLockedItemRules(VS::Field::ChestTarget target) {
  RecordingChestInteractionHost locked;
  locked.chest =
      VS::Field::ChestLockState{.lock_level = 1, .required_item = 0x1D2};
  CHECK_FALSE(VS::Field::TryOpenChest(target, locked));
  CHECK(locked.calls == std::vector<std::int32_t>{3});
  locked.item = true;
  locked.calls.clear();
  CHECK(VS::Field::TryOpenChest(target, locked));
  CHECK(locked.calls ==
        std::vector<std::int32_t>{0x388, 0x1D2, 1000, 1001, 2, 1002});
}

void CheckChestBeginFinishRules(RecordingChestInteractionHost& host,
                                VS::Field::ChestTarget target) {
  CHECK(VS::Field::BeginChestInteraction(target, 0, host));
  CHECK(host.selected == target);
  CHECK(host.calls == std::vector<std::int32_t>{1010, 1012, 1, 1013});
  host.calls.clear();
  CHECK(VS::Field::FinishChestInteraction(host));
  CHECK(host.calls == std::vector<std::int32_t>{1011, 4, 1014});
  host.calls.clear();
}

void CheckChestContentsRules(RecordingChestInteractionHost& host) {
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{1015, 1017});
  host.calls.clear();
  host.chest = VS::Field::ChestLockState{.lock_level = 3, .required_item = 9};
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{3, 1016});
  host.calls.clear();
  host.chest = VS::Field::ChestLockState{.opened_flag = 67};
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{3, 1017});
  host.selected.reset();
  CHECK_FALSE(VS::Field::OpenChestContents(host));
}

void CheckTargetKindSelection(VS::Field::InteractionTargetState& state) {
  state.Select(VS::Field::ChestTarget{3, 7});
  CHECK(state.Trap() == nullptr);
  REQUIRE(state.Chest());
  CHECK(*state.Chest() == VS::Field::ChestTarget{3, 7});
  state.Select(VS::Field::TrapTarget{4, 8});
  CHECK(state.Chest() == nullptr);
  REQUIRE(state.Trap());
  CHECK(*state.Trap() == VS::Field::TrapTarget{4, 8});
}

void CheckTargetDomainBounds(VS::Field::InteractionTargetDomain domain) {
  CHECK(VS::Field::ResolveTrapTarget({1, 7}, domain) == 1);
  CHECK_FALSE(VS::Field::ResolveTrapTarget({2, 7}, domain));
  CHECK_FALSE(VS::Field::ResolveTrapTarget({0, 6}, domain));
  CHECK(VS::Field::ResolveChestTarget({2, 7}, domain) == 2);
  CHECK_FALSE(VS::Field::ResolveChestTarget({3, 7}, domain));
  CHECK_FALSE(VS::Field::ResolveChestTarget({0, 8}, domain));
}

void CheckSelectedTargetDomain(VS::Field::InteractionTargetDomain domain) {
  VS::Field::InteractionTargetState state;
  state.Select(VS::Field::ChestTarget{2, 7});
  CHECK(state.Chest(domain) != nullptr);
  CHECK(state.Trap(domain) == nullptr);
  CHECK(state.Chest({8, 2, 3}) == nullptr);
  CHECK(state.Chest({7, 2, 2}) == nullptr);
}

}  // namespace

TEST_CASE("Typed chest interaction keeps lock item and pose rules separate",
          "[chest-interaction-core][field]") {
  const VS::Field::ChestTarget target{0, 1};
  CheckChestOpenAndBlockedRules(target);
  CheckChestLockedItemRules(target);
}

namespace {

void CheckStagedTrapVictim(const RecordingTrapActionHost& host,
                           const VS::Field::TrapActionInput& input) {
  if (!host.staged) {
    FAIL("Trap action must retain staged victim values");
    return;
  }
  REQUIRE(host.staged);
  CHECK(host.staged->actor == 3);
  CHECK(host.staged->default_limb == 7);
  CHECK(host.staged->position == input.position);
}

void CheckStagedTrapEffect(const RecordingTrapActionHost& host) {
  if (!host.effect) {
    FAIL("Trap action must retain initialized effect values");
    return;
  }
  REQUIRE(host.effect);
  CHECK(host.effect->effect_id == 6);
  CHECK(host.effect->actor == 3);
  CHECK(host.effect->ground_anchor == 0xFD);
  CHECK_FALSE(host.effect->source_position);
}

void CheckTrapCompatibilitySource(const Effects_ActionEffectRequest& request) {
  CHECK(request.source.type == 5);
  CHECK(request.source.colorPreset == 0);
  CHECK(request.source.value.position.vx == 0);
  CHECK(request.source.value.position.vy == 0);
  CHECK(request.source.value.position.vz == 0);
  CHECK(request.source.value.position.pad == 0);
}

void CheckTrapCompatibilityVictim(const Effects_ActionEffectRequest& request) {
  CHECK(request.targets[0].type == 4);
  CHECK(request.targets[0].colorPreset == 0);
  CHECK(request.targets[0].value.actor.actor == 3);
  CHECK(request.targets[0].value.actor.part == 0xFD);
  CHECK(request.targets[0].value.actor.eventActorSlot == 0);
  CHECK(request.targets[0].value.actor.reserved3 == 0);
}

void CheckInvalidTrapActors(RecordingTrapActionHost& host) {
  for (const auto actor : {-1, 17}) {
    CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = actor}, host));
    CHECK(host.calls.empty());
  }
}

void CheckInvalidTrapKinds(RecordingTrapActionHost& host) {
  for (const auto kind : {-1, 16}) {
    host.kind = static_cast<std::int16_t>(kind);
    CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
    CHECK(host.calls.empty());
  }
}

}  // namespace

TEST_CASE("Trap action stages victim then resolves before initialized effect",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  const VS::Field::TrapActionInput input{
      .actor = 3, .default_limb = 7, .position = {123, -234, 345}};
  REQUIRE(VS::Field::TriggerActorTrap({0, 1}, input, host));
  CHECK(host.calls == std::vector<std::int32_t>{1000, 6, 1001});
  CheckStagedTrapVictim(host, input);
  CheckStagedTrapEffect(host);
}

TEST_CASE("Trap effect compatibility record initializes every supplied field",
          "[trap-action-live][field]") {
  const auto request = VS::Field::LiveTrapEffectRecord(
      {.effect_id = 6, .actor = 3, .source_position = std::nullopt});
  CHECK(request.effectId == 6);
  CHECK(request.count == 1);
  CHECK(request.otherCount == 0);
  CheckTrapCompatibilitySource(request);
  CheckTrapCompatibilityVictim(request);
  CHECK(request.targets[1].type == 0);
  CHECK(request.others[0] == 0);
  CHECK(request.position.vx == 0);
  CHECK(request.position.vy == 0);
  CHECK(request.position.vz == 0);
}

TEST_CASE("Trap effect compatibility record retains explicitly supplied source",
          "[trap-action-live][field]") {
  const auto request = VS::Field::LiveTrapEffectRecord(
      {.effect_id = 9, .actor = 0, .source_position = {{123, -234, 345}}});
  CHECK(request.source.value.position.vx == 123);
  CHECK(request.source.value.position.vy == -234);
  CHECK(request.source.value.position.vz == 345);
}

TEST_CASE("Trap action refuses invalid actor state or kind before mutation",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  CheckInvalidTrapActors(host);
  CheckInvalidTrapKinds(host);
  host.kind = 0;
  host.action_state = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
  CHECK(host.calls.empty());
}

TEST_CASE("Trap action reobserves target after resolver callback",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  host.change_during_resolve = true;
  REQUIRE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
  if (!host.effect) {
    FAIL("Resolver must publish the reobserved trap effect");
    return;
  }
  REQUIRE(host.effect);
  CHECK(host.effect->effect_id == 7);
  RecordingTrapActionHost removed;
  removed.remove_during_resolve = true;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, removed));
  CHECK(removed.calls == std::vector<std::int32_t>{1000, 6});
  CHECK_FALSE(removed.effect);
}

TEST_CASE("Trap action stops on refused staging or resolution",
          "[trap-action-core][field]") {
  RecordingTrapActionHost staged;
  staged.stage_succeeds = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, staged));
  CHECK(staged.calls == std::vector<std::int32_t>{1000});
  RecordingTrapActionHost resolved;
  resolved.resolve_succeeds = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, resolved));
  CHECK(resolved.calls == std::vector<std::int32_t>{1000, 6});
  CHECK_FALSE(resolved.effect);
}

TEST_CASE("Native trap action preserves its original module binding",
          "[trap-action-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073D30) ==
        reinterpret_cast<const void*>(&Field_TriggerActorTrapEntry));
}

TEST_CASE("Native trap action refuses invalid actor or stale target",
          "[trap-action-live][field]") {
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, -1));
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, 17));
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, 0));
  Field_TriggerActorTrapEntry(nullptr, nullptr, -1);
}

TEST_CASE("Interaction camera focus wait preserves actor gate short circuit",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  host.flags = 0x08000000;
  CHECK_FALSE(VS::Field::WaitForChestCameraFocus(host));
  host.speed = 7;
  const auto reads = host.flag_reads;
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  CHECK(host.flag_reads == reads);
  CHECK(host.mode == 0);
  CHECK(host.requested_speed == 0x28);
}

TEST_CASE("Interaction camera missing focus performs no camera step",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  host.focus.reset();
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  CHECK_FALSE(VS::Field::StepTrapCameraFocus(host));
  CHECK(host.calls.empty());
  CHECK(host.flag_reads == 0);
}

TEST_CASE("Chest animation readiness observes notification only after actor",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  CHECK_FALSE(VS::Field::IsChestAnimationReady(host));
  CHECK(host.calls.empty());
  host.flags = 0x08000000;
  CHECK(VS::Field::IsChestAnimationReady(host));
  host.pending = true;
  CHECK_FALSE(VS::Field::IsChestAnimationReady(host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 1});
}

TEST_CASE("Interaction camera returns from Ashley with original halfword wrap",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  host.ashley = VS::Field::TrapPosition{123, -32768, -234};
  host.speed = 9;
  CHECK(VS::Field::TrackInteractionCameraToAshley(host) == 9);
  CHECK(host.stepped == VS::Field::TrapPosition{123, 32678, -234});
  CHECK(host.mode == 1);
  CHECK(host.calls == std::vector<std::int32_t>{0, 2});
  host.transition = true;
  CHECK(VS::Field::TrackInteractionCameraToAshley(host) == 0);
  host.ashley.reset();
  host.calls.clear();
  CHECK_FALSE(VS::Field::TrackInteractionCameraToAshley(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Interaction camera entries retain all original module addresses",
          "[interaction-camera-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074120) ==
        reinterpret_cast<const void*>(&Field_WaitForChestCameraFocusEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007418C) ==
        reinterpret_cast<const void*>(&Field_StepTrapCameraFocusEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007424C) ==
        reinterpret_cast<const void*>(&Field_IsChestAnimationReadyEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074374) ==
        reinterpret_cast<const void*>(
            &Field_TrackInteractionCameraToAshleyEntry));
}

TEST_CASE("Interaction camera entries refuse an absent target focus",
          "[interaction-camera-live][field]") {
  const auto saved = VS::Field::LiveInteractionTargetState();
  VS::Field::LiveInteractionTargetState().Clear();
  CHECK(Field_WaitForChestCameraFocusEntry() == 1);
  CHECK(Field_StepTrapCameraFocusEntry() == 0);
  VS::Field::LiveInteractionTargetState() = saved;
}

TEST_CASE("Facing tile rounds every heading into original cardinal quadrants",
          "[facing-tile-core][field]") {
  VS::Actors::ActorPlacementView actor{};
  actor.tile.x = 17;
  actor.tile.z = 23;
  actor.tile.layer = 2;
  for (std::uint32_t heading = 0; heading < 65536; ++heading) {
    actor.position.w =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(heading));
    const auto tile = VS::Field::TileInFrontOfActor(actor);
    const auto expected = ((heading + 0x200) & 0xFFF) >> 10;
    CHECK(static_cast<std::uint8_t>(tile.direction) == expected);
    CHECK(tile.layer == 2);
    CHECK(tile.x == (expected == 1 ? 16 : expected == 3 ? 18 : 17));
    CHECK(tile.z == (expected == 0 ? 22 : expected == 2 ? 24 : 23));
  }
}

TEST_CASE("Facing tile preserves original byte wrapping at room boundaries",
          "[facing-tile-core][field]") {
  VS::Actors::ActorPlacementView actor{};
  CHECK(VS::Field::TileInFrontOfActor(actor).z == 255);
  actor.position.w = 1024;
  CHECK(VS::Field::TileInFrontOfActor(actor).x == 255);
  actor.tile.x = 255;
  actor.tile.z = 255;
  actor.position.w = 2048;
  CHECK(VS::Field::TileInFrontOfActor(actor).z == 0);
  actor.position.w = 3072;
  CHECK(VS::Field::TileInFrontOfActor(actor).x == 0);
}

TEST_CASE("Facing tile entry preserves X layer Z and untouched fourth byte",
          "[facing-tile-live][field]") {
  Actors_ActorLocationQuery actor{};
  actor.location.packedTile.target.tileX = 17;
  actor.location.packedTile.target.tileZ = 23;
  actor.location.packedTile.target.tileLayer = 2;
  actor.location.position.pad = 1024;
  std::array<std::uint8_t, 4> output{0xA1, 0xA2, 0xA3, 0xA4};
  CHECK(Field_TileInFrontOfActorEntry(&actor, output.data()) == 1);
  CHECK(output == std::array<std::uint8_t, 4>{16, 2, 23, 0xA4});
  const auto saved = output;
  CHECK(Field_TileInFrontOfActorEntry(nullptr, output.data()) == -1);
  CHECK(output == saved);
  CHECK(Field_TileInFrontOfActorEntry(&actor, nullptr) == -1);
}

TEST_CASE("Facing tile entry retains original module address",
          "[facing-tile-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074798) ==
        reinterpret_cast<const void*>(&Field_TileInFrontOfActorEntry));
}

TEST_CASE("Facing event uses requested actor and computed adjacent tile",
          "[facing-event-core][field]") {
  RecordingFacingEventHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.x = 17;
  placement.tile.z = 23;
  placement.tile.layer = 2;
  placement.position.w = 1024;
  host.placement = placement;
  host.result = true;
  CHECK(VS::Field::TryTriggerFacingTileEvent(3, host));
  CHECK(host.observed_actor == 3);
  if (!host.evaluated) {
    FAIL("Facing interaction must receive the adjacent tile");
    return;
  }
  REQUIRE(host.evaluated);
  CHECK(host.evaluated->x == 16);
  CHECK(host.evaluated->z == 23);
  CHECK(host.evaluated->layer == 2);
  host.result = false;
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(3, host));
}

TEST_CASE("Entrance state copies complete door and unsigned scene identifier",
          "[entrance-door-core][field]") {
  RecordingEntranceDoorHost host;
  host.scene_id = 254;
  VS::Field::EntranceDoorState state;
  const VS::Field::DoorRecord door{.destination_zone_id = 9,
                                   .destination_room_id = 3,
                                   .required_flag_value = 7,
                                   .required_flag_id = 321,
                                   .door_id = 128,
                                   .reserved = {1, 2, 3}};
  VS::Field::SetEntranceDoorRecord(state, door, host);
  CHECK(state.door == door);
  CHECK(state.entered_door_id == 254);
  CHECK(host.observed_id == 128);
}

TEST_CASE("Entrance state clears absent records without scene lookup",
          "[entrance-door-core][field]") {
  RecordingEntranceDoorHost host;
  VS::Field::EntranceDoorState state;
  state.door.door_id = 128;
  state.entered_door_id = 254;
  VS::Field::SetEntranceDoorRecord(state, std::nullopt, host);
  CHECK(state.door == VS::Field::DoorRecord{});
  CHECK(state.entered_door_id == -1);
  CHECK_FALSE(host.observed_id);
}

TEST_CASE("Entrance state preserves record when scene mapping is absent",
          "[entrance-door-core][field]") {
  RecordingEntranceDoorHost host;
  VS::Field::EntranceDoorState state;
  const VS::Field::DoorRecord door{.door_id = 128};
  VS::Field::SetEntranceDoorRecord(state, door, host);
  CHECK(state.door == door);
  CHECK(state.entered_door_id == -1);
  CHECK(host.observed_id == 128);
}

TEST_CASE("Scene door native resolver keeps original unsigned map byte",
          "[scene-door-byte][field]") {
  SceneDoorIdFixture fixture;
  CHECK(Field_SceneDoorId(128) == 254);
  CHECK(Field_SceneDoorId(-1) == -1);
  CHECK(Field_SceneDoorId(129) == -1);
}

TEST_CASE("Entrance native setter keeps original unsigned door index byte",
          "[scene-door-byte][field]") {
  SceneDoorIdFixture fixture;
  Field_MpdDoorRecord door{};
  door.doorId = 128;
  Field_SetEntranceDoorRecordEntry(reinterpret_cast<char (*)[12]>(&door));
  CHECK(Field_enteredDoorId == 254);
  CHECK(Field_entranceDoorRecord.doorId == 128);
  Field_SetEntranceDoorRecordEntry(nullptr);
  CHECK(Field_enteredDoorId == -1);
}

TEST_CASE("Typed entrance native setter preserves complete record and clear",
          "[entrance-door-live][field]") {
  SceneDoorIdFixture fixture;
  Field_MpdDoorRecord door{};
  door.doorId = 128;
  door.requiredFlagId = 321;
  door.requiredFlagValue = 7;
  Field_SetEntranceDoorRecordEntry(reinterpret_cast<char (*)[12]>(&door));
  CHECK(Field_enteredDoorId == 254);
  CHECK(Field_entranceDoorRecord.doorId == 128);
  CHECK(Field_entranceDoorRecord.requiredFlagId == 321);
  CHECK(Field_entranceDoorRecord.requiredFlagValue == 7);
  Field_SetEntranceDoorRecordEntry(nullptr);
  CHECK(Field_enteredDoorId == -1);
  CHECK(Field_entranceDoorRecord.doorId == 0);
}

TEST_CASE("Typed entrance live lookup keeps order upper layers and byte bounds",
          "[entrance-door-live][field]") {
  SceneDoorIdFixture fixture;
  std::array<Field_MpdDoorRecord, 2> doors{};
  doors[0].tileLocation.tileX = 2;
  doors[0].tileLocation.tileZ = 3;
  doors[0].doorId = 128;
  doors[1].tileLocation.tileX = 7;
  doors[1].tileLocation.tileZ = 7;
  doors[1].tileLocation.floorLayer = 2;
  Field_roomData.doors = doors.data();
  Field_roomData.header.doorsLength = sizeof doors;
  Field_FindEntranceDoorNearTileEntry(3, 3, 0);
  CHECK(Field_enteredDoorId == 254);
  CHECK(Field_entranceDoorRecord.tileLocation.tileX == 2);
  Field_FindEntranceDoorNearTileEntry(50, 50, 2);
  CHECK(Field_enteredDoorId == 0);
  CHECK(Field_entranceDoorRecord.tileLocation.tileX == 7);
  Field_roomData.header.doorsLength = sizeof doors - 1;
  Field_FindEntranceDoorNearTileEntry(3, 3, 0);
  CHECK(Field_enteredDoorId == -1);
  CHECK(Field_entranceDoorRecord.doorId == 0);
}

TEST_CASE("Entrance native entries retain original module addresses",
          "[entrance-door-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B8F8) ==
        reinterpret_cast<const void*>(&Field_SetEntranceDoorRecordEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B960) ==
        reinterpret_cast<const void*>(&Field_FindEntranceDoorNearTileEntry));
}

TEST_CASE("Entrance lookup preserves file order and one-tile boundaries",
          "[entrance-door-core][field]") {
  const std::array<VS::Field::DoorRecord, 3> doors{
      VS::Field::DoorRecord{.tile_x = 3, .tile_z = 3, .floor_layer = 1},
      VS::Field::DoorRecord{.tile_x = 2, .tile_z = 3},
      VS::Field::DoorRecord{.tile_x = 3, .tile_z = 3}};
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, 3, 3, 0) == doors[1]);
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(doors, 5, 3, 0));
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, 4, 4, 0) == doors[2]);
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile({}, 3, 3, 0));
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(doors, 3, 3, -1));
}

TEST_CASE("Entrance lookup upper layers ignore coordinates but keep layer",
          "[entrance-door-core][field]") {
  const std::array<VS::Field::DoorRecord, 2> doors{
      VS::Field::DoorRecord{.tile_x = 3, .tile_z = 3, .floor_layer = 3},
      VS::Field::DoorRecord{.tile_x = 7, .tile_z = 7, .floor_layer = 2}};
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, 100, 100, 2) == doors[1]);
  CHECK(VS::Field::FindEntranceDoorNearTile(doors, -100, -100, 3) == doors[0]);
  CHECK_FALSE(VS::Field::FindEntranceDoorNearTile(doors, 3, 3, 4));
}

TEST_CASE("Ashley tile event wins before trap door and fatal pit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.cell = {.actor = {.room_trigger = true, .pit = true},
               .event_trigger = true,
               .docked_door = true};
  host.event_started = true;
  host.trap = VS::Field::ActorTileTrap{.target = {1, 2}, .kind = 16};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1, .spare = 192};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 15, 16, 20, 17, 18, 7});
  CHECK(host.last == VS::Actors::PackedTile{.x = 1, .z = 1});
}

TEST_CASE("Ashley trigger collision precedes transition and disabled refusal",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  VS::Actors::ActorPlacementView placement{};
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(-1, placement, host));
  CHECK(host.fall.calls.empty());
  host.flags = 0x30000;
  host.transitioning = true;
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12});
  CHECK(host.collision_tile == VS::Actors::PackedTile{.layer = 3});
  host.fall.calls.clear();
  host.transitioning = false;
  host.flags = 0x01000000;
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12});
}

TEST_CASE("Ashley ordinary trap wins before door and fatal pit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.cell = {.actor = {.room_trigger = true, .pit = true},
               .event_trigger = true,
               .docked_door = true};
  host.trap = VS::Field::ActorTileTrap{.target = {1, 2}, .kind = 3};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 15, 17, 19, 20});
  CHECK(host.last == placement.tile);
  CHECK(host.fall.hp == 21);
}

TEST_CASE("Ashley effect-only trap continues to physical-layer zone door",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.flags = 0x20000000;
  host.cell = {.actor = {.room_trigger = true}, .docked_door = true};
  host.trap = VS::Field::ActorTileTrap{.target = {1, 2}, .kind = 16};
  host.doors = {{.destination_zone_id = 10,
                 .destination_room_id = 4,
                 .required_flag_value = 1}};
  host.flag_value = 1;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .layer = 2, .z = 1};
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 17, 18, 21, 22, 6, 7, 8});
  CHECK(host.doors_tile == placement.tile);
  CHECK(host.fall.route == VS::Field::AshleyTransitionRoute::kZone);
  CHECK(host.fall.entrance == host.doors.front());
}

TEST_CASE("Ashley forced-layer door lookup obeys its separate suppression bit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.flags = 0x20030000;
  host.cell.docked_door = true;
  host.doors = {{.destination_zone_id = 9, .destination_room_id = 4}};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .layer = 2, .z = 1};
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13, 14, 20});
  CHECK(host.collision_tile == VS::Actors::PackedTile{.layer = 3});
  CHECK(host.last == placement.tile);
  host.flags = 0x30000;
  host.fall.calls.clear();
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.doors_tile == VS::Actors::PackedTile{.layer = 3});
  CHECK(host.fall.route == VS::Field::AshleyTransitionRoute::kRoom);
}

TEST_CASE("Ashley door chooses the first flag-accepted record",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.cell.docked_door = true;
  host.doors = {{.destination_room_id = 1, .required_flag_value = 2},
                {.destination_zone_id = 9,
                 .destination_room_id = 2,
                 .required_flag_value = 1}};
  host.flag_value = 1;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1};
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 21, 22, 22, 6, 7, 8});
  CHECK(host.fall.transition == host.doors[1]);
  CHECK(host.fall.entrance == host.doors[1]);
}

TEST_CASE("Ashley event-owned control caches before still handling a pit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.event_control = true;
  host.cell = {.actor = {.room_trigger = true, .pit = true},
               .event_trigger = true,
               .docked_door = true};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13, 14, 20, 0,
                                                     1, 3, 5, 6, 2, 7, 8});
  CHECK(host.fall.hp == 1);
  CHECK(host.last == placement.tile);
}

TEST_CASE("Ashley cached tile ignores input spare without refreshing cache",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.last = VS::Actors::PackedTile{.x = 1, .z = 1};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1, .spare = 192};
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13});
  CHECK(host.last == VS::Actors::PackedTile{.x = 1, .z = 1});
}

TEST_CASE("Ashley trap suppression retains original held-location mode writes",
          "[ashley-tile-core][field]") {
  constexpr std::array<std::uint32_t, 3> kSuppression{0x4000, 0x100000,
                                                      0x400000};
  for (const auto flags : kSuppression) {
    RecordingAshleyTileHost host;
    host.flags = flags;
    host.cell.actor.room_trigger = true;
    VS::Actors::ActorPlacementView placement{};
    placement.tile = {.x = 1, .z = 1, .spare = 192};
    CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
    CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13, 14, 20});
    auto expected =
        (flags & 0x104000) == 0 ? placement.tile : VS::Actors::PackedTile{};
    expected.spare = static_cast<std::uint8_t>((flags >> 14) & 1);
    CHECK(host.last == expected);
  }
}

TEST_CASE("Ashley fall refuses invalid actors nonpits and height sixteen",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(-1, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(VS::Field::CheckAshleyFall(17, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(VS::Field::CheckAshleyFall(2, placement, false, host) ==
        VS::Field::AshleyFallResult::kNone);
  placement.position.y = 16;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(host.calls.empty());
}

TEST_CASE("Ashley scripted fall transitions without reading HP",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.destination = VS::Field::ScriptedFallDestination{9, 3};
  host.hp.reset();
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  placement.tile = {.x = 1, .z = 1, .spare = 192};
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kScriptedTransition);
  CHECK(host.calls == std::vector<std::int32_t>{0, 6, 7, 8});
  CHECK(host.transition ==
        VS::Field::BuildScriptedFallDoor({9, 3}, placement.tile));
  CHECK_FALSE(host.entrance);
}

TEST_CASE("Ashley fatal fall keeps signed halfword HP and label ordering",
          "[ashley-fall-core][field]") {
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  constexpr std::array<std::int16_t, 4> kFatalHp{-32768, -1, 1, 20};
  for (const auto hp : kFatalHp) {
    RecordingAshleyFallHost host;
    host.hp = hp;
    CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
          VS::Field::AshleyFallResult::kHandled);
    CHECK(host.hp == 0);
    CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4});
  }
}

TEST_CASE("Ashley zero HP fall defeats without another value label",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.hp = 0;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kHandled);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 3, 4});
}

TEST_CASE("Ashley HP twenty-one loses twenty before reload transition",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.reload.destination_zone_id = 10;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kHandled);
  CHECK(host.hp == 1);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 3, 5, 6, 2, 7, 8});
  CHECK(host.transition == host.reload);
  CHECK(host.entrance == host.reload);
  CHECK(host.route == VS::Field::AshleyTransitionRoute::kRoom);
}

TEST_CASE("Ashley fall refuses absent HP before damage or transition",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.hp.reset();
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1});
}

TEST_CASE("Scripted fall door values preserve original packed field widths",
          "[ashley-fall-core][field]") {
  const auto door = VS::Field::BuildScriptedFallDoor(
      {.zone_id = 9, .room_id = 3},
      {.x = 255, .layer = 255, .z = 254, .spare = 255});
  CHECK(door == VS::Field::DoorRecord{.destination_zone_id = 9,
                                      .destination_room_id = 3,
                                      .tile_x = 31,
                                      .tile_z = 30,
                                      .floor_layer = 7,
                                      .entry_direction = 7,
                                      .entry_tile_x = 31,
                                      .entry_tile_z = 30,
                                      .entry_elevation = 3});
}

TEST_CASE("Scripted fall producer initializes the complete transition door",
          "[fall-door-initialization][field]") {
  ScriptedFallDoorFixture fixture;
  std::uint8_t destination_zone = 9;
  SECTION("Same-zone fall retains original room transition") {
    destination_zone = 9;
  }
  SECTION("Cross-zone fall retains original zone transition") {
    destination_zone = 10;
  }
  fixture.environment.fallDestinationZone = destination_zone;
  REQUIRE(VS::Field::LiveRoomFallDestination() ==
          static_cast<int>(destination_zone) * 65536 + 3);
  REQUIRE(VS::Field::LiveCollisionCellOrAbsent(1, 1, 0)->isPit == 1);
  Actors_ActorLocation location{};
  location.packedTile.word =
      std::bit_cast<std::int32_t>(std::uint32_t{0xC0010001});
  location.position.vy = 17;
  CHECK(Field_CheckAshleyTileTriggersEntry(
            2, reinterpret_cast<unsigned char*>(&location)) == 0);
  CHECK(Field_screenTransitionStep == (destination_zone == 9 ? 1 : 2));
  const auto transition = VS::Field::LoadLiveRoomTransition();
  CHECK(transition.pending_door ==
        VS::Field::DoorRecord{.destination_zone_id = destination_zone,
                              .destination_room_id = 3,
                              .tile_x = 1,
                              .tile_z = 1,
                              .entry_direction = 7,
                              .entry_tile_x = 1,
                              .entry_tile_z = 1,
                              .entry_elevation = 3});
}

TEST_CASE("Actor tile triggers preserve collision before transition refusal",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  host.flags = 0x50000;
  host.transition = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2});
  CHECK(host.collision_tile.x == 0);
  CHECK(host.collision_tile.z == 0);
  CHECK(host.collision_tile.layer == 5);
  host.calls.clear();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(-1, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls.empty());
}

TEST_CASE("Actor new tile starts trap before checking a fatal pit",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 3, .layer = 2, .z = 4};
  placement.position.y = 100;
  host.cell = {.room_trigger = true, .pit = true};
  host.trap = VS::Field::ActorTileTrap{.target = {1, 7}, .kind = 3};
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kTrap);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 5, 6, 7});
  CHECK(host.last == placement.tile);
}

TEST_CASE("Actor fatal pit defeats before requesting its event",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  host.cell.pit = true;
  placement.position.y = 16;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3});
  placement.position.y = 17;
  host.calls.clear();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kDefeated);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 8, 9});
  host.fall_event = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kFallEvent);
}

TEST_CASE("Actor cached tile ignores placement spare and skips trap reads",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.spare = 255;
  host.cell.room_trigger = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3});
  host.last.reset();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
}

TEST_CASE("Actor trigger location held flags still replace cached mode",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 3, .layer = 2, .z = 4};
  host.last = VS::Actors::PackedTile{.x = 9, .layer = 8, .z = 7};
  host.flags = 0x4000;
  host.cell.room_trigger = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.last ==
        VS::Actors::PackedTile{.x = 9, .layer = 8, .z = 7, .spare = 1});
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 7});
}

TEST_CASE("Actor effect-only trap falls through to original pit result",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.x = 3;
  placement.position.y = 17;
  host.cell = {.room_trigger = true, .pit = true};
  host.trap = VS::Field::ActorTileTrap{.kind = 16};
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kDefeated);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 5, 7, 8, 9});
}

TEST_CASE("Actor tile trap suppression bits never read the trap record",
          "[actor-tile-trigger-core][field]") {
  for (const auto flags : {0x4000U, 0x100000U, 0x200000U, 0x400000U, 0x2000000U,
                           0x4000000U, 0x10000000U}) {
    RecordingActorTileTriggerHost host;
    VS::Actors::ActorPlacementView placement{};
    placement.tile.x = 3;
    host.flags = flags;
    host.cell.room_trigger = true;
    CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
          VS::Field::ActorTileTriggerResult::kNone);
    CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 7});
  }
}

TEST_CASE("Actor event control holds traps but still remembers the tile",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.x = 3;
  host.cell.room_trigger = true;
  host.event_control = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 7});
  CHECK(host.last == placement.tile);
}

TEST_CASE("Actor disabled tile trigger refuses before actor cache access",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  host.flags = 0x01000000;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2});
  host.flags = 0;
  host.cell.pit = true;
  host.defeat = false;
  placement.position.y = 17;
  host.calls.clear();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 8});
}

TEST_CASE("Ashley native entry preserves unaligned input and cached tile bytes",
          "[ashley-tile-live][field]") {
  ActorTileTriggerLiveFixture fixture;
  fixture.actor->lastTriggerTile.word = 0x01070809;
  Actors_ActorLocation location{};
  location.packedTile.target = {
      .tileX = 3, .tileLayer = 2, .tileZ = 4, .facing = 255};
  std::array<unsigned char, sizeof(Actors_ActorLocation) + 1> bytes{};
  std::memcpy(bytes.data() + 1, &location, sizeof location);
  const auto before = bytes;
  CHECK(Field_CheckAshleyTileTriggersEntry(2, bytes.data() + 1) == 0);
  CHECK(fixture.actor->lastTriggerTile.word == 0x00040203);
  CHECK(bytes == before);
  CHECK(Field_CheckAshleyTileTriggersEntry(2, bytes.data() + 1) == 0);
  CHECK(fixture.actor->lastTriggerTile.word == 0x00040203);
  Battle_actors[2] = nullptr;
  CHECK(Field_CheckAshleyTileTriggersEntry(2, bytes.data() + 1) == 0);
  CHECK(bytes == before);
}

TEST_CASE("Ashley native entry refuses null and out of range actors",
          "[ashley-tile-live][field]") {
  CHECK(Field_CheckAshleyTileTriggersEntry(2, nullptr) == 0);
  Actors_ActorLocation location{};
  auto* input = reinterpret_cast<unsigned char*>(&location);
  CHECK(Field_CheckAshleyTileTriggersEntry(-1, input) == 0);
  CHECK(Field_CheckAshleyTileTriggersEntry(17, input) == 0);
}

TEST_CASE("Ashley native entry retains original module address",
          "[ashley-tile-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074BAC) ==
        reinterpret_cast<const void*>(&Field_CheckAshleyTileTriggersEntry));
}

TEST_CASE("Actor tile trigger C boundary retains module and refuses bad input",
          "[actor-tile-trigger-live][field]") {
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, nullptr) == 0);
  Actors_ActorLocation location{};
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(-1, &location) == 0);
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(17, &location) == 0);
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800751B8) ==
      reinterpret_cast<const void*>(&Field_CheckActorTrapAndFallTriggersEntry));
}

TEST_CASE("Actor native tile trigger stores packed location and computed mode",
          "[actor-tile-trigger-live][field]") {
  ActorTileTriggerLiveFixture fixture;
  fixture.actor->lastTriggerTile.word = 0x01070809;
  Actors_ActorLocation location{};
  location.packedTile.target = {
      .tileX = 3, .tileLayer = 2, .tileZ = 4, .facing = 255};
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, &location) == 0);
  CHECK(fixture.actor->lastTriggerTile.word == 0x00040203);
  CHECK(std::bit_cast<std::uint32_t>(location.packedTile.word) == 0xFF040203);
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, &location) == 0);
  CHECK(fixture.actor->lastTriggerTile.word == 0x00040203);
  Battle_actors[2] = nullptr;
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, &location) == 0);
}

TEST_CASE("Facing event refuses invalid actor and absent placement",
          "[facing-event-core][field]") {
  RecordingFacingEventHost host;
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(-1, host));
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(17, host));
  CHECK(host.reads == 0);
  CHECK_FALSE(host.evaluated);
  host.placement.reset();
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(0, host));
  CHECK(host.reads == 1);
  CHECK_FALSE(host.evaluated);
}

TEST_CASE("Facing room query keeps unsigned height window boundaries",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  for (const auto height : {-64, -63, 0, 63, 64}) {
    host.floor = height;
    host.calls.clear();
    CHECK(VS::Field::FindFacingRoomObject(0, host).has_value() ==
          (height >= -63 && height <= 63));
    CHECK(host.calls == std::vector<std::int32_t>{0, 2});
  }
  host.floor = 0x800000;
  CHECK(VS::Field::FindFacingRoomObject(0, host) == host.object);
}

TEST_CASE("Facing chest query gates facing flag and exact raised floor",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 96;
  host.placement = placement;
  CHECK(VS::Field::FindFacingUnlootedChest(0, host) ==
        VS::Field::ChestTarget{3, 7});
  CHECK(host.calls == std::vector<std::int32_t>{1, 3, 2});
  CHECK(host.observed_flag == 0x3C9);
  host.calls.clear();
  host.chest = VS::Field::FacingChestState{
      .target = {3, 7}, .facing = 1, .opened_flag = 9};
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{1});
  host.chest = VS::Field::FacingChestState{.target = {3, 7}, .opened_flag = 9};
  host.collected = true;
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 3});
}

TEST_CASE("Facing resource query refuses missing actor or object state",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  host.placement.reset();
  CHECK_FALSE(VS::Field::FindFacingRoomObject(0, host));
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls.empty());
  CHECK_FALSE(VS::Field::FindFacingRoomObject(-1, host));
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(17, host));
}

TEST_CASE(
    "Facing chest query rejects absent record without flag or floor reads",
    "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  host.chest.reset();
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{1});
  host.object.reset();
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindFacingRoomObject(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 2});
}

TEST_CASE("Facing resource floor uses original low fifteen bit sign extension",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = -16384;
  host.placement = placement;
  host.floor = 0x4000;
  CHECK(VS::Field::FindFacingRoomObject(0, host) == host.object);
  placement.position.y = -16288;
  host.placement = placement;
  CHECK(VS::Field::FindFacingUnlootedChest(0, host) ==
        VS::Field::ChestTarget{3, 7});
  placement.position.y = -16289;
  host.placement = placement;
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
}

TEST_CASE("Facing chest collection flag keeps original halfword argument wrap",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  host.chest =
      VS::Field::FacingChestState{.target = {3, 7}, .opened_flag = 32767};
  host.collected = true;
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.observed_flag == -31809);
}

TEST_CASE("Facing query C entries refuse invalid actor indices",
          "[facing-query-live][field]") {
  CHECK(Field_TryTriggerFacingTileEventEntry(-1) == 0);
  CHECK(Field_TryTriggerFacingTileEventEntry(17) == 0);
  CHECK(Field_FindFacingRoomObjectEntry(-1) == nullptr);
  CHECK(Field_FindFacingRoomObjectEntry(17) == nullptr);
  CHECK(Field_FindFacingUnlootedChestEntry(-1) == nullptr);
  CHECK(Field_FindFacingUnlootedChestEntry(17) == nullptr);
}

TEST_CASE("Facing queries retain all original module entries",
          "[facing-query-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074860) ==
        reinterpret_cast<const void*>(&Field_TryTriggerFacingTileEventEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800748B8) ==
        reinterpret_cast<const void*>(&Field_FindFacingRoomObjectEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074950) ==
        reinterpret_cast<const void*>(&Field_FindFacingUnlootedChestEntry));
}

TEST_CASE("Room object query target refuses stale and out of range resolution",
          "[facing-query-live][field]") {
  RoomObjectRecordFixture fixture;
  const auto domain = VS::Field::LiveInteractionTargetDomain();
  const VS::Field::RoomObjectTarget target{1, domain.generation};
  CHECK(VS::Field::ResolveLiveRoomObjectTarget(target) == &fixture.locks[1]);
  CHECK(VS::Field::ResolveLiveRoomObjectTarget({2, domain.generation}) ==
        nullptr);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveRoomObjectTarget(target) == nullptr);
  CHECK(VS::Field::InteractLiveRoomObject(target) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
}

TEST_CASE("Object interaction wait selects its mode before pausing",
          "[object-interaction-wait-core][field]") {
  RecordingObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
  CHECK(host.mode == VS::Field::FieldControlMode::kObjectInteractionWait);
  CHECK(host.calls == std::vector<int>{0, 1});
  VS::Field::BeginObjectInteractionWait(host);
  CHECK(host.calls == std::vector<int>{0, 1, 0, 1});
}

TEST_CASE("Object interaction wait releases and resumes before exploration",
          "[object-interaction-wait-core][field]") {
  RecordingObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
  host.calls.clear();
  VS::Field::EndObjectInteractionWait(host);
  CHECK(host.mode == VS::Field::FieldControlMode::kExploration);
  CHECK(host.calls == std::vector<int>{2, 3, 4});
}

TEST_CASE("Object interaction wait keeps both original module bindings",
          "[object-interaction-wait-core][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073AA4) ==
        reinterpret_cast<const void*>(&Field_BeginObjectInteractionWaitEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073ACC) ==
        reinterpret_cast<const void*>(&Field_EndObjectInteractionWaitEntry));
}

TEST_CASE("Room object interaction keeps reusable keys and consumed sigils",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost key;
  key.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  key.has_item = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(key) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(key.calls == std::vector<std::int32_t>{0, 1000, 7, 5, 1001});
  CHECK(key.state->lock_state == 0);
  RecordingRoomObjectInteractionHost sigil;
  sigil.state =
      VS::Field::RoomObjectLockState{.required_item = 466, .lock_state = 3};
  sigil.has_item = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(sigil) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(sigil.calls ==
        std::vector<std::int32_t>{0, 0x388, 466, 1000, 9, 5, 1001});
  CHECK(sigil.flag_value == 2);
}

TEST_CASE("Room object prior unlock opens silently and busy records do nothing",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  host.previous_flag = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 1000, 5, 1001});
  host.calls.clear();
  host.state->is_open = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kBusy);
  CHECK(host.calls.empty());
}

TEST_CASE("Room object rejection preserves notification sounds and idle order",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state = VS::Field::RoomObjectLockState{.lock_state = 2};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{5, 0x65, 0x66, 1002});
  host.calls.clear();
  host.state = VS::Field::RoomObjectLockState{.lock_state = 3};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 10, 0x65, 0x66, 1002});
}

TEST_CASE("Room object busy and invalid targets perform no item reads",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state = VS::Field::RoomObjectLockState{.is_triggered = true};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kBusy);
  CHECK(host.calls.empty());
  host.state.reset();
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls.empty());
  host.state = VS::Field::RoomObjectLockState{.lock_state = 4};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls.empty());
}

TEST_CASE("Room object refused unlock cannot start pose or toggle",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{5, 1001});
  host.calls.clear();
  host.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  host.has_item = true;
  host.unlock_succeeds = false;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1000});
  CHECK(host.state->lock_state == 1);
}

TEST_CASE("Native room object entry refuses null and foreign record addresses",
          "[room-object-interaction-live][field]") {
  RoomObjectRecordFixture fixture;
  Field_MpdLockTrigger foreign{};
  CHECK(Field_InteractWithLockedRoomObjectEntry(nullptr) == 1);
  CHECK(Field_InteractWithLockedRoomObjectEntry(&foreign) == 1);
  CHECK(Field_InteractWithLockedRoomObjectEntry(
            reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1})) == 1);
}

TEST_CASE("Native room object entry reads only its present busy lock record",
          "[room-object-interaction-live][field]") {
  RoomObjectRecordFixture fixture;
  fixture.locks[1].isOpen = 1;
  CHECK(Field_InteractWithLockedRoomObjectEntry(&fixture.locks[1]) == 1);
  fixture.locks[1].isOpen = 0;
  fixture.locks[1].isTriggered = 1;
  CHECK(Field_InteractWithLockedRoomObjectEntry(&fixture.locks[1]) == 1);
}

TEST_CASE("Native room object interaction keeps its original module binding",
          "[room-object-interaction-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x80073AFC) ==
      reinterpret_cast<const void*>(&Field_InteractWithLockedRoomObjectEntry));
}

TEST_CASE("Typed chest flow owns selected begin finish and contents decisions",
          "[chest-interaction-core][field]") {
  RecordingChestInteractionHost host;
  const VS::Field::ChestTarget target{0, 1};
  CheckChestBeginFinishRules(host, target);
  CheckChestContentsRules(host);
}

TEST_CASE("Typed chest selection preserves accepted and cancelled outcomes",
          "[chest-interaction-core][field]") {
  RecordingChestInteractionHost host;
  host.selected = VS::Field::ChestTarget{0, 1};
  host.chest = VS::Field::ChestLockState{.opened_flag = 7};
  CHECK(VS::Field::UpdateChestContentsSelection(host));
  host.contents_result = VS::Field::ChestSelectionResult::kAccepted;
  CHECK_FALSE(VS::Field::UpdateChestContentsSelection(host));
  CHECK(host.calls == std::vector<std::int32_t>{0x3C7, 1018, 1019});
  host.calls.clear();
  host.contents_result = VS::Field::ChestSelectionResult::kCancelled;
  CHECK_FALSE(VS::Field::UpdateChestContentsSelection(host));
  CHECK(host.calls == std::vector<std::int32_t>{1002, 1018, 1019});
}

TEST_CASE("Interaction dispatch refuses mismatched chest and trap modes",
          "[interaction-target][field]") {
  RecordingInteractionDispatchHost host;
  host.target = VS::Field::ChestTarget{0, 1};
  CHECK(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls == std::vector<int>{0, 2});
  host.calls.clear();
  host.trap_mode = true;
  CHECK_FALSE(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls.empty());
  host.target = VS::Field::TrapTarget{0, 1};
  CHECK(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls == std::vector<int>{0, 1});
}

TEST_CASE("Typed trap begin preserves actor capture and original reset order",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  const VS::Field::TrapTarget target{2, 7};
  REQUIRE(VS::Field::BeginTrapInteraction(target, 3, host));
  CHECK(host.selected == target);
  CHECK(host.selected_actor == 3);
  CHECK(host.calls == std::vector<std::int32_t>{
                          1000, 1001, 0,  1,  2,  3,  4,  5,  6,    7,
                          8,    9,    10, 11, 12, 13, 14, 15, 1002, 1003});
}

TEST_CASE("Typed trap begin refuses invalid capture before side effects",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  host.valid = false;
  CHECK_FALSE(VS::Field::BeginTrapInteraction({2, 7}, 3, host));
  CHECK(host.calls.empty());
  CHECK_FALSE(host.selected);
}

TEST_CASE("Typed trap finish disables only its selected target then clears it",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  const VS::Field::TrapTarget target{2, 7};
  host.selected = target;
  CHECK(VS::Field::FinishTrapInteraction(host));
  CHECK(host.disabled == target);
  CHECK_FALSE(host.selected);
  CHECK(host.calls == std::vector<std::int32_t>{1004, 1005, 1006, 1007});
  host.calls.clear();
  CHECK_FALSE(VS::Field::FinishTrapInteraction(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Typed trap finish clears a target refused during disable",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  host.selected = VS::Field::TrapTarget{2, 7};
  host.disable_succeeds = false;
  CHECK_FALSE(VS::Field::FinishTrapInteraction(host));
  CHECK_FALSE(host.selected);
  CHECK(host.calls == std::vector<std::int32_t>{1004, 1005, 1006, 1007});
}

TEST_CASE(
    "Interaction target state cannot expose a chest through the trap kind",
    "[interaction-target][field]") {
  VS::Field::InteractionTargetState state;
  CHECK(state.Trap() == nullptr);
  CHECK(state.Chest() == nullptr);
  CheckTargetKindSelection(state);
  state.Clear();
  CHECK(state.Trap() == nullptr);
  CHECK(state.Chest() == nullptr);
}

TEST_CASE("Interaction target domain refuses stale and out-of-range identities",
          "[interaction-target][field]") {
  const VS::Field::InteractionTargetDomain domain{7, 2, 3};
  CheckTargetDomainBounds(domain);
  CheckSelectedTargetDomain(domain);
}

TEST_CASE("Live interaction target indexes refuse room-generation replacement",
          "[interaction-target][field]") {
  const auto saved = Field_roomData;
  struct RestoreRoom {
    Field_RoomRecord record;
    ~RestoreRoom() {
      Field_roomData = record;
      VS::Field::InvalidateLiveInteractionTargets();
    }
  } restore{saved};
  std::array<Field_MpdFloorTrigger, 2> traps{};
  std::array<Items_TreasureChest, 2> chests{};
  Field_roomData.floorTriggers = traps.data();
  Field_roomData.header.floorTriggersLength = 24;
  Field_roomData.containers = chests.data();
  Field_roomData.header.containersLength = 0x468;
  const auto trap = VS::Field::FindLiveTrapTarget(&traps[1]);
  const auto chest = VS::Field::FindLiveChestTarget(&chests[1]);
  REQUIRE(trap);
  REQUIRE(chest);
  CHECK(VS::Field::ResolveLiveTrapTarget(*trap) == &traps[1]);
  CHECK(VS::Field::ResolveLiveChestTarget(*chest) == &chests[1]);
  CHECK_FALSE(VS::Field::FindLiveTrapTarget(nullptr));
  CHECK_FALSE(VS::Field::FindLiveChestTarget(nullptr));
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveTrapTarget(*trap) == nullptr);
  CHECK(VS::Field::ResolveLiveChestTarget(*chest) == nullptr);
}

TEST_CASE("Typed chest animation never stores a lock-record alias",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  REQUIRE(VS::Field::BeginLiveChestSwitch(*target));
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 2);
  CHECK(Field_gimmickAnimationQueue[0].trigger == nullptr);
  CHECK(static_cast<std::uint16_t>(fixture.chests[0].reservedA) == 0x0114);
  fixture.chests[0].reservedA = 0x0101;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(fixture.chests[0].isOpen == 1);
  CHECK(fixture.chests[0].reservedA == 0);
  CHECK(Field_pendingGimmickAnimationCount == 0);
}

TEST_CASE("Typed chest animation refuses a stale room-generation target",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  REQUIRE(VS::Field::BeginLiveChestSwitch(*target));
  VS::Field::InvalidateLiveInteractionTargets();
  const auto before = fixture.chests[0].reservedA;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(fixture.chests[0].reservedA == before);
  CHECK_FALSE(VS::Field::BeginLiveChestSwitch(*target));
}

TEST_CASE("Native chest focus selects typed record and refuses stale dispatch",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  CHECK(VS::Field::BeginLiveChestInteraction(*target, 0));
  const auto* selected = VS::Field::LiveInteractionTargetState().Chest(
      VS::Field::LiveInteractionTargetDomain());
  REQUIRE(selected);
  CHECK(*selected == *target);
  CHECK(VS::Field::LiveInteractionTargetState().Trap() == nullptr);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK_FALSE(VS::Field::TryOpenLiveChest(*target));
}

TEST_CASE("Typed switch start restores state on refused delayed admission",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.state = VS::Field::RoomSwitchState{.activation = 7};
  host.refuse = true;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  REQUIRE(host.state);
  CHECK(host.state->marker_fade == 0);
  CHECK(host.state->activation == 7);
  CHECK(host.calls == std::vector<std::int32_t>{1, 20, 2, 0, 7});
  host.refuse = false;
  CHECK(VS::Field::BeginSwitchToggle(host));
  CHECK(host.state->marker_fade == 1);
  CHECK(host.state->activation == 20);
}

TEST_CASE("Typed switch start preserves immediate and busy behavior",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.state = VS::Field::RoomSwitchState{.switch_value = 2, .activation = 7};
  CHECK(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 7, 3});
  const auto before = host.calls;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == before);
  host.state.reset();
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == before);
}

TEST_CASE("Typed switch start cannot dispatch after a refused state store",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.refuse_store = true;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Typed animation reset preserves original count tick and slot order",
          "[gimmick-animation-reset-core][field]") {
  RecordingAnimationResetHost host;
  VS::Field::ResetGimmickAnimationQueue(host);
  CHECK(host.calls == std::vector<std::size_t>{8, 9, 0, 1, 2, 3, 4, 5, 6, 7});
  host.calls.clear();
  VS::Field::ResetGimmickAnimationQueue(host);
  CHECK(host.calls == std::vector<std::size_t>{8, 9, 0, 1, 2, 3, 4, 5, 6, 7});
}

TEST_CASE(
    "Native animation reset clears every slot counters and module binding",
    "[gimmick-animation-reset-core][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B6B4) ==
        reinterpret_cast<const void*>(&Field_ResetGimmickAnimationQueueEntry));
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_triggerMarkerFrameTicks = 0xFFFF;
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_triggerMarkerFrameTicks == 0);
  for (const auto& job : Field_gimmickAnimationQueue) {
    CHECK(job.jobKind == 0);
    CHECK(job.trigger == nullptr);
  }
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_triggerMarkerFrameTicks == 0);
}

TEST_CASE("Typed animation dispatch tolerates a reset inside its callback",
          "[gimmick-animation-reset-core][field]") {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      VS::Field::GimmickAnimationKind::kFinishDoor, 0, 1, true};
  host.reset_during_finish = true;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 0);
  CHECK(host.PendingCount() == 0);
  CHECK(host.calls == std::vector<std::int32_t>{1});
}

TEST_CASE("Typed animation dispatch preserves every original countdown byte",
          "[gimmick-animation-dispatch-core][field]") {
  for (const auto kind : {VS::Field::GimmickAnimationKind::kToggleSwitch,
                          VS::Field::GimmickAnimationKind::kStartDoor}) {
    for (int byte = 0; byte < 256; ++byte) {
      CheckOriginalCountdownByte(kind, byte);
    }
  }
}

TEST_CASE("Typed animation dispatch reserves full door slot before callback",
          "[gimmick-animation-dispatch-core][field]") {
  RecordingAnimationDispatchHost host;
  for (std::size_t slot = 0; slot < host.jobs.size(); ++slot) {
    host.jobs[slot] = VS::Field::GimmickAnimationJobSnapshot{
        VS::Field::GimmickAnimationKind::kStartDoor, 1, slot + 1, true};
  }
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 8);
  REQUIRE(host.calls.size() == 16);
  CHECK(host.calls[0] == 4);
  CHECK(host.calls[1] == 3);
  CHECK(host.calls[2] == 3);
  CHECK(host.calls[3] == 4);
}

TEST_CASE("Typed animation dispatch never clears a callback replacement job",
          "[gimmick-animation-dispatch-core][field]") {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      VS::Field::GimmickAnimationKind::kFinishDoor, 0, 1, true};
  host.replace_during_finish = true;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 0);
  CHECK(host.PendingCount() == 1);
  CHECK(host.calls == std::vector<std::int32_t>{1});
}

TEST_CASE("Typed gimmick admission rejects invalid requests before slot reads",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  for (const auto kind :
       {-1, 0, 4, 257, std::numeric_limits<std::int32_t>::max()}) {
    CHECK_FALSE(VS::Field::QueueGimmickAnimation(
        static_cast<VS::Field::GimmickAnimationKind>(kind), host));
  }
  host.target = false;
  CHECK_FALSE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kFinishDoor, host));
  CHECK(host.reads == 0);
  CHECK(host.stores == 0);
  CHECK(host.count_writes == 0);
}

TEST_CASE("Typed gimmick admission counts only committed first-free slots",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  host.slots[0] = true;
  host.slots[2] = true;
  REQUIRE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kToggleSwitch, host));
  CHECK(host.stored_slot == 1);
  CHECK(host.pending == 3);
  host.refuse_store = true;
  CHECK_FALSE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kStartDoor, host));
  CHECK(host.pending == 3);
  CHECK(host.count_writes == 1);
}

TEST_CASE("Typed gimmick admission cannot drift when a full queue refuses",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  for (std::size_t slot = 0; slot < VS::Field::kGimmickAnimationSlots; ++slot) {
    REQUIRE(VS::Field::QueueGimmickAnimation(
        VS::Field::GimmickAnimationKind::kFinishDoor, host));
    CHECK(host.stored_slot == slot);
    CHECK(host.pending == slot + 1);
  }
  const auto before = host.slots;
  for (int refusal = 0; refusal < 16; ++refusal) {
    CHECK_FALSE(VS::Field::QueueGimmickAnimation(
        VS::Field::GimmickAnimationKind::kFinishDoor, host));
    CHECK(host.pending == 8);
    CHECK(host.slots == before);
  }
  CHECK(host.stores == 8);
  CHECK(host.count_writes == 8);
}

TEST_CASE("Gimmick animation queue does not count a refused ninth job",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots + 1> targets{};
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    Field_QueueGimmickAnimationEntry(1, &targets[slot]);
  }
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_QueueGimmickAnimationEntry(1, &targets.back());
  CHECK(Field_pendingGimmickAnimationCount == 8);
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 1);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == &targets[slot]);
  }
}

TEST_CASE("Native gimmick admission refuses invalid calls and preserves module",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C070) ==
        reinterpret_cast<const void*>(&Field_QueueGimmickAnimationEntry));
  Field_MpdLockTrigger target{};
  for (const auto kind :
       {-1, 0, 4, 257, std::numeric_limits<std::int32_t>::max()}) {
    Field_QueueGimmickAnimationEntry(kind, &target);
  }
  Field_QueueGimmickAnimationEntry(1, nullptr);
  Field_QueueGimmickAnimationEntry(
      1, reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1}));
  CHECK(Field_pendingGimmickAnimationCount == 0);
  for (const auto& job : Field_gimmickAnimationQueue) {
    CHECK(job.jobKind == 0);
    CHECK(job.trigger == nullptr);
  }
}

TEST_CASE("Native gimmick admission drains accepted jobs and resets cleanly",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots + 1> targets{};
  QueueOneFrameSwitchJobs(targets);
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CheckDrainedSwitchJobs(targets);
  CheckAnimationKindResets(targets.back());
}

TEST_CASE("Delayed door follow-up survives a saturated animation queue",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    ConfigureOneFrameDelayedDoor(target);
    Field_QueueGimmickAnimationEntry(3, &target);
  }
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 8);
  for (const auto& target : targets) {
    const auto found = std::ranges::find_if(
        Field_gimmickAnimationQueue, [&target](const auto& job) {
          return job.jobKind == 1 && job.trigger == &target;
        });
    CHECK(found != std::end(Field_gimmickAnimationQueue));
  }
}

TEST_CASE("Saturated door continuations drain exactly on the following tick",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    ConfigureOneFrameDelayedDoor(target);
    Field_QueueGimmickAnimationEntry(3, &target);
  }
  Field_UpdateGimmickAnimationQueueEntry();
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  for (std::size_t slot = 0; slot < targets.size(); ++slot) {
    CHECK(targets[slot].isOpen == 1);
    CHECK(targets[slot].isTriggered == 0);
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 0);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == nullptr);
  }
}

TEST_CASE("Unsaturated delayed doors retain original same-tick finish order",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  Field_MpdLockTrigger target{};
  ConfigureOneFrameDelayedDoor(target);
  Field_QueueGimmickAnimationEntry(3, &target);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(target.isOpen == 1);
  CHECK(target.isTriggered == 0);
}

TEST_CASE(
    "Native animation dispatcher refuses bad entries and preserves module",
    "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C0C0) ==
        reinterpret_cast<const void*>(&Field_UpdateGimmickAnimationQueueEntry));
  Field_gimmickAnimationQueue[0] = {2, nullptr};
  Field_pendingGimmickAnimationCount = 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
  Field_MpdLockTrigger target{};
  Field_gimmickAnimationQueue[0] = {99, &target};
  Field_pendingGimmickAnimationCount = 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
}

TEST_CASE("Refused switch start leaves its busy marker retryable",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> occupied{};
  for (auto& target : occupied) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_MpdLockTrigger rejected{};
  rejected.trigger.activation = 7;
  Field_BeginSwitchToggleEntry(&rejected);
  CHECK(Field_pendingGimmickAnimationCount == 8);
  CHECK(rejected.trigger.markerFade == 0);
  CHECK(rejected.trigger.activation == 7);
}

TEST_CASE("Native switch start retries after queue capacity is freed",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> occupied{};
  for (auto& target : occupied) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_MpdLockTrigger target{};
  target.trigger.activation = 7;
  Field_BeginSwitchToggleEntry(&target);
  REQUIRE(target.trigger.markerFade == 0);
  Field_ResetGimmickAnimationQueueEntry();
  Field_BeginSwitchToggleEntry(&target);
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(target.trigger.markerFade == 1);
  CHECK(target.trigger.activation == 20);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 2);
  CHECK(Field_gimmickAnimationQueue[0].trigger == &target);
  Field_BeginSwitchToggleEntry(&target);
  CHECK(Field_pendingGimmickAnimationCount == 1);
}

TEST_CASE("Native switch start preserves immediate state and original module",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D5FC) ==
        reinterpret_cast<const void*>(&Field_BeginSwitchToggleEntry));
  Field_BeginSwitchToggleEntry(nullptr);
  Field_BeginSwitchToggleEntry(
      reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1}));
  for (const auto value : {-1, 1, 32767, -32768}) {
    Field_MpdLockTrigger target{};
    target.trigger.lockState.switchValue = static_cast<std::int16_t>(value);
    target.trigger.kind = -1;
    target.trigger.tileX = -1;
    target.trigger.tileZ = -1;
    target.trigger.floorLayer = -1;
    Field_BeginSwitchToggleEntry(&target);
    CHECK(Field_pendingGimmickAnimationCount == 0);
    CHECK(target.trigger.lockState.switchValue == 0);
    CHECK(target.trigger.markerFade == 0);
  }
}

TEST_CASE("Room switches preserve original sounds and normalized state order",
          "[room-switches][field]") {
  for (const auto value : {0, 1, 2, 65535}) {
    RecordingSwitchHost host;
    host.state = VS::Field::RoomSwitchState{
        .switch_value = static_cast<std::uint16_t>(value),
        .marker_fade = 0xFF,
        .effect_kind = 7,
        .tile_x = 3,
        .tile_z = 9,
        .floor_layer = -1};
    REQUIRE(VS::Field::ToggleRoomSwitch(host));
    const std::vector<SwitchCall> expected{
        {SwitchCallKind::kSound, value == 0 ? 0x3B : 0x3D},
        {SwitchCallKind::kSound, value == 0 ? 0x3C : 0x3E},
        {SwitchCallKind::kEffect, 7},
        {SwitchCallKind::kStore, value == 0 ? 1 : 0},
        {SwitchCallKind::kCell, value == 0 ? 5 : 0}};
    CHECK(host.calls == expected);
    if (!host.state) {
      FAIL("Room switch state must remain valid after a completed toggle");
      return;
    }
    CHECK(host.state->marker_fade == 0);
    CHECK(host.cell_query == std::array<std::int32_t, 3>{3, 9, -1});
  }
}

TEST_CASE("Room switches use state after effect callbacks",
          "[room-switches][field]") {
  RecordingSwitchHost host;
  host.state = VS::Field::RoomSwitchState{.effect_kind = 7};
  host.after_effect = VS::Field::RoomSwitchState{.switch_value = 2,
                                                 .marker_fade = 5,
                                                 .tile_x = 11,
                                                 .tile_z = 12,
                                                 .floor_layer = 3};
  REQUIRE(VS::Field::ToggleRoomSwitch(host));
  const std::vector<SwitchCall> expected{{SwitchCallKind::kSound, 0x3B},
                                         {SwitchCallKind::kSound, 0x3C},
                                         {SwitchCallKind::kEffect, 7},
                                         {SwitchCallKind::kStore, 0},
                                         {SwitchCallKind::kCell, 0}};
  CHECK(host.calls == expected);
  CHECK(host.cell_query == std::array<std::int32_t, 3>{11, 12, 3});
}

TEST_CASE("Room switches refuse absent and invalidated targets",
          "[room-switches][field]") {
  RecordingSwitchHost host;
  CHECK_FALSE(VS::Field::ToggleRoomSwitch(host));
  CHECK(host.calls.empty());
  host.state = VS::Field::RoomSwitchState{};
  host.remove_after_sound = true;
  CHECK_FALSE(VS::Field::ToggleRoomSwitch(host));
  CHECK(host.calls.size() == 2);
}

TEST_CASE("Native switch entry preserves original module and signed word state",
          "[room-switches][room-switches-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D658) ==
        reinterpret_cast<const void*>(&Field_ToggleRoomSwitchEntry));
  Field_ToggleRoomSwitchEntry(nullptr);
  for (const auto value : {0, 1, 2, -1, -32768, 32767}) {
    Field_MpdLockTrigger record{};
    record.trigger.tileX = -1;
    record.trigger.tileZ = -1;
    record.trigger.floorLayer = -1;
    record.trigger.kind = -1;
    record.trigger.lockState.switchValue = static_cast<std::int16_t>(value);
    record.trigger.markerFade = -1;
    Field_ToggleRoomSwitchEntry(&record);
    CHECK(record.trigger.lockState.switchValue == (value == 0 ? 1 : 0));
    CHECK(record.trigger.markerFade == 0);
  }
}

TEST_CASE(
    "Native gimmick placement entry preserves the original module address",
    "[gimmick-placement][gimmick-placement-core][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A0024) ==
        reinterpret_cast<const void*>(&Field_GimmickPlacementEntry));
}

TEST_CASE("Typed gimmick placement refuses slots before record access",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  for (const auto slot : {std::numeric_limits<std::int32_t>::min(), -1, 16,
                          std::numeric_limits<std::int32_t>::max()}) {
    CHECK_FALSE(VS::Field::GimmickPlacement(slot, host));
  }
  CHECK(host.reads == 0);
  CHECK(host.support_calls == 0);
  for (const auto slot : {0, 15}) {
    CHECK_FALSE(VS::Field::GimmickPlacement(slot, host));
  }
  CHECK(host.reads == 2);
  CHECK(host.support_calls == 0);
}

TEST_CASE("Typed gimmick placement refuses motion before support sampling",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  for (const auto mode : {0, 1, -1, 32767, -32768}) {
    VS::Field::GimmickPlacementState state{
        .attached = mode == 0,
        .movement_mode = static_cast<std::int16_t>(mode)};
    host.states[0] = state;
    CHECK_FALSE(VS::Field::GimmickPlacement(0, host));
  }
  host.states[0] = VS::Field::GimmickPlacementState{.falling = true};
  CHECK_FALSE(VS::Field::GimmickPlacement(0, host));
  CHECK(host.support_calls == 0);
}

TEST_CASE("Typed gimmick placement matches original word stores and rounding",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  VS::Field::GimmickPlacementState state{
      .tile_x = 0xFF,
      .tile_z = 9,
      .orientation = 0x81,
      .position_x = -1234,
      .position_z = 2345,
      .position_tail = std::bit_cast<std::int16_t>(std::uint16_t{0xFEDC})};
  for (const auto support :
       {std::numeric_limits<std::int32_t>::min(), -3000, -1, 0, 1, 3000,
        std::numeric_limits<std::int32_t>::max()}) {
    host.support_height = support;
    for (const auto height :
         {-32768, -129, -128, -127, -1, 0, 1, 127, 128, 129, 32767}) {
      state.position_y = static_cast<std::int16_t>(height);
      host.states[0] = state;
      CheckTypedGimmickPlacement(host, state);
    }
  }
  CHECK(host.support_calls == 77);
}

TEST_CASE("Gimmick snapshots refuse invalid and absent slots without writing",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int slot = -2; slot < 18; ++slot) {
    CHECK(Field_GimmickPlacementEntry(slot, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
  Field_gimmickInstances[0] = fixture.object.get();
  CHECK(Field_GimmickPlacementEntry(0, nullptr) == -1);
}

TEST_CASE("Gimmick snapshots reject attached falling and moving records",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  Field_gimmickInstances[0] = fixture.object.get();
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int refusal = 0; refusal < 3; ++refusal) {
    fixture.object->parent = refusal == 0 ? 0 : 0xFF;
    fixture.object->objectInfo.isFalling = refusal == 1;
    fixture.object->mode = static_cast<std::int16_t>(refusal == 2);
    CHECK(Field_GimmickPlacementEntry(0, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
}

TEST_CASE("Gimmick snapshots preserve packed coordinates and height rounding",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto& object = *fixture.object;
  object.tileX = 5;
  object.tileZ = 9;
  object.objectInfo.material = 3;
  object.positionX = -1234;
  object.positionZ = 2345;
  object.reserved22 = 0x1357;
  for (const int slot : {0, 15}) {
    Field_gimmickInstances[slot] = &object;
    for (const int height : {-129, -128, -127, 0, 127, 128, 129}) {
      object.positionY = static_cast<std::int16_t>(height);
      CheckValidSnapshot(slot, object);
    }
    Field_gimmickInstances[slot] = nullptr;
  }
}

TEST_CASE(
    "Native room triggers consume the bounded stationary gimmick snapshot",
    "[gimmick-placement][room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  auto& object = *fixture.snapshots.object;
  object.tileX = 5;
  object.tileZ = 9;
  Field_gimmickInstances[0] = &object;
  PublishTrigger({4, 0, 255, 255, 2, 9, 0, 11, 0, 5, 9, 0, 0, 255});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kPlayerTile};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kStarted);
  CHECK(VS::Event::LiveScriptVm().EventId() == 9);
  CHECK(VS::Event::LiveScriptVm().Source() ==
        VS::Event::EventSource::kEventFile);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  object.mode = 1;
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
  Field_gimmickInstances[0] = nullptr;
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
}

TEST_CASE("Native room trigger contexts preserve signed reference semantics",
          "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  PublishTrigger({4, 0, 255, 255, 8, 7, 0, 6, 0, 128, 0, 32, 255});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kActorState,
      .context = VS::Event::TriggerActorPair{-32768, 8192}};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kStarted);
  CHECK(VS::Event::LiveScriptVm().EventId() == 7);
}

TEST_CASE("Native room trigger status queries require no section bytes",
          "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  VS::Event::PublishLiveRoomScriptSections({});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kPlaybackStatus};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kRunning);
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kBusy);
}

TEST_CASE(
    "C trigger entry preserves tile contexts and refuses missing contexts",
    "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  PublishTrigger({4, 0, 255, 255, 4, 12, 0, 8, 7, 9, 255});
  std::array<std::uint8_t, 4> tile{7, 0, 9, 0};
  CHECK(Event_EvaluateRoomEventTriggersEntry(4, 0, tile.data(), 1) == 1);
  CHECK(VS::Event::LiveScriptVm().EventId() == 12);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  CHECK(Event_EvaluateRoomEventTriggersEntry(4, 0, nullptr, 1) == 0);
}

TEST_CASE("Facing event entry dispatches adjacent tile through native triggers",
          "[facing-query-live][room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  auto& object = *fixture.snapshots.object;
  object.tileX = 7;
  object.tileZ = 10;
  Field_gimmickInstances[0] = &object;
  PublishTrigger({4, 0, 255, 255, 4, 12, 0, 8, 7, 9, 255});
  CHECK(Field_TryTriggerFacingTileEventEntry(0) == 1);
  CHECK(VS::Event::LiveScriptVm().EventId() == 12);
  CHECK(VS::Event::LiveScriptVm().Phase() ==
        VS::Event::PlaybackPhase::kRequested);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kRunning);
  Event_isDirectEventRequest = 1;
  CHECK(Field_TryTriggerFacingTileEventEntry(0) == 0);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  Event_isDirectEventRequest = 0;
  object.tileZ = 11;
  CHECK(Field_TryTriggerFacingTileEventEntry(0) == 0);
  CHECK(VS::Event::LiveScriptVm().Phase() == VS::Event::PlaybackPhase::kIdle);
}
