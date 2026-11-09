#include "Field/RoomSwitchesLive.h"

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <vector>

#include "Audio/SfxLive.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/InteractionTargetLive.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomSwitches.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Items/TreasureChests.h"
#include "State/StateFlags.h"
}

namespace {

constexpr int kSwitchSoundGroup = 0x7E;
constexpr unsigned kMarkerByteShift = 8;
// Original800E849C-800E84AB: seven groups and their terminating boundary.
constexpr std::size_t kSigilRangeWords = 8;
static_assert(std::size(Field_lockedDoorIndexRanges) >= kSigilRangeWords);
enum SigilPairByte : std::size_t {
  kFirstZone,
  kFirstMap,
  kSecondZone,
  kSecondMap
};

std::span<Field_MpdLockTrigger> RoomDoorRecords() {
  const auto bytes = Field_roomData.header.lockTriggersLength;
  if (Field_roomData.lockTriggers == nullptr ||
      bytes % sizeof(Field_MpdLockTrigger) != 0 ||
      reinterpret_cast<std::uintptr_t>(Field_roomData.lockTriggers) %
              alignof(Field_MpdLockTrigger) !=
          0) {
    return {};
  }
  return {Field_roomData.lockTriggers, bytes / sizeof(Field_MpdLockTrigger)};
}

std::optional<VS::Field::RoomObjectTarget> RoomDoorTarget(
    const Field_MpdLockTrigger* record) {
  const auto records = RoomDoorRecords();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (&records[index] == record) {
      return VS::Field::RoomObjectTarget{
          static_cast<std::uint32_t>(index),
          VS::Field::LiveInteractionTargetDomain().generation};
    }
  }
  return std::nullopt;
}

Field_MpdLockTrigger* ResolveRoomDoor(VS::Field::RoomObjectTarget target) {
  const auto records = RoomDoorRecords();
  if (target.generation !=
          VS::Field::LiveInteractionTargetDomain().generation ||
      target.index >= records.size()) {
    return nullptr;
  }
  return &records[target.index];
}

class LiveDoorCollisionHost final : public VS::Field::RoomDoorCollisionHost {
 public:
  explicit LiveDoorCollisionHost(VS::Field::RoomObjectTarget target)
      : target_(target) {}
  std::optional<VS::Field::RoomDoorPlacement> State() const override {
    const auto* record = ResolveRoomDoor(target_);
    if (record == nullptr) return std::nullopt;
    const auto& trigger = record->trigger;
    return VS::Field::RoomDoorPlacement{
        .tile_x = trigger.tileX,
        .tile_z = trigger.tileZ,
        .floor_layer = trigger.floorLayer,
        .kind = trigger.kind,
        .activation = static_cast<std::uint8_t>(trigger.activation)};
  }
  void ToggleCell(const VS::Field::RoomDoorTileQuery& query,
                  std::uint8_t xor_mask) override {
    auto* cell = VS::Field::LiveCollisionCellAt(query.tile_x, query.tile_z,
                                                query.floor_layer);
    if (cell != nullptr) cell->doorPassability ^= xor_mask;
  }

 private:
  VS::Field::RoomObjectTarget target_;
};

class LiveDoorAnimationHost final : public VS::Field::RoomDoorAnimationHost {
 public:
  explicit LiveDoorAnimationHost(VS::Field::RoomObjectTarget target)
      : target_(target) {}
  std::optional<VS::Field::RoomDoorAnimationState> State(
      VS::Field::DoorMember member) const override {
    const auto* record = Record(member);
    if (record == nullptr) return std::nullopt;
    const auto& trigger = record->trigger;
    return VS::Field::RoomDoorAnimationState{
        .placement = {.tile_x = trigger.tileX,
                      .tile_z = trigger.tileZ,
                      .floor_layer = trigger.floorLayer,
                      .kind = trigger.kind,
                      .activation =
                          static_cast<std::uint8_t>(trigger.activation)},
        .key_or_sigil = std::bit_cast<std::uint16_t>(record->keyOrSigil),
        .open_value = record->isOpen,
        .linked_index = record->linkedLockIndex,
        .effect =
            std::bit_cast<std::uint8_t>(trigger.lockState.lock.roomEffect),
        .busy = std::bit_cast<std::uint8_t>(record->isTriggered),
        .delay = std::bit_cast<std::uint8_t>(record->delayFrames)};
  }
  bool QueueAvailable() const override {
    return ResolveRoomDoor(target_) != nullptr &&
           std::ranges::any_of(
               Field_gimmickAnimationQueue,
               [](const auto& job) { return job.jobKind == 0; });
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kSwitchSoundGroup, sound);
  }
  void StartEffect(std::uint8_t effect) override {
    VS::Field::LiveStartRoomEffect(effect);
  }
  bool EffectActive(std::uint8_t effect) const override {
    return VS::Field::LiveIsRoomEffectActive(effect);
  }
  bool ToggleCollision(VS::Field::DoorMember member) override {
    const auto target = Target(member);
    return target && VS::Field::ToggleLiveDoorCollision(*target);
  }
  bool StoreBusy(VS::Field::DoorMember member, std::uint8_t busy) override {
    auto* record = Record(member);
    if (record == nullptr) return false;
    record->isTriggered = std::bit_cast<char>(busy);
    return true;
  }
  bool StoreDelay(std::uint8_t delay) override {
    auto* record = ResolveRoomDoor(target_);
    if (record == nullptr) return false;
    record->delayFrames = std::bit_cast<char>(delay);
    return true;
  }
  bool StoreOpen(VS::Field::DoorMember member, std::int16_t value,
                 std::uint8_t busy) override {
    auto* record = Record(member);
    if (record == nullptr) return false;
    record->isTriggered = std::bit_cast<char>(busy);
    record->isOpen = value;
    return true;
  }
  bool Queue(VS::Field::GimmickAnimationKind kind) override {
    return VS::Field::QueueLiveGimmickAnimation(static_cast<int>(kind),
                                                target_);
  }
  void MarkSigil(std::uint32_t sigil_index) override {
    static_cast<void>(VS::Field::MarkLiveSigilDoorUnlocked(sigil_index));
  }

 private:
  std::optional<VS::Field::RoomObjectTarget> Target(
      VS::Field::DoorMember member) const {
    const auto* primary = ResolveRoomDoor(target_);
    if (primary == nullptr) return std::nullopt;
    if (member == VS::Field::DoorMember::kPrimary) return target_;
    if (primary->linkedLockIndex < 0) return std::nullopt;
    const VS::Field::RoomObjectTarget linked{
        static_cast<std::uint32_t>(primary->linkedLockIndex),
        target_.generation};
    return ResolveRoomDoor(linked) != nullptr ? std::optional{linked}
                                              : std::nullopt;
  }
  Field_MpdLockTrigger* Record(VS::Field::DoorMember member) const {
    const auto target = Target(member);
    return target ? ResolveRoomDoor(*target) : nullptr;
  }
  VS::Field::RoomObjectTarget target_;
};

using RoomPair = VS::Field::SigilDoorRoomPair;
using SigilPairs = std::array<RoomPair, std::size(Field_lockedDoorRoomPairs)>;

SigilPairs LiveSigilPairs() {
  SigilPairs pairs{};
  for (std::size_t index = 0; index < pairs.size(); ++index) {
    const auto& original = Field_lockedDoorRoomPairs[index];
    pairs[index] = {
        .first = {std::bit_cast<std::uint8_t>(original[kFirstZone]),
                  std::bit_cast<std::uint8_t>(original[kFirstMap])},
        .second = {std::bit_cast<std::uint8_t>(original[kSecondZone]),
                   std::bit_cast<std::uint8_t>(original[kSecondMap])}};
  }
  return pairs;
}

class LiveRoomSwitchHost final : public VS::Field::RoomSwitchHost {
 public:
  explicit LiveRoomSwitchHost(Field_MpdLockTrigger* trigger)
      : trigger_(trigger) {}

  std::optional<VS::Field::RoomSwitchState> State() const override {
    if (trigger_ == nullptr) {
      return std::nullopt;
    }
    const auto& trigger = trigger_->trigger;
    return VS::Field::RoomSwitchState{
        .switch_value =
            static_cast<std::uint16_t>(trigger.lockState.switchValue),
        .marker_fade = static_cast<std::uint8_t>(trigger.markerFade),
        .effect_kind = trigger.kind,
        .tile_x = trigger.tileX,
        .tile_z = trigger.tileZ,
        .floor_layer = trigger.floorLayer};
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kSwitchSoundGroup, sound);
  }
  void StartEffect(std::int32_t kind) override {
    VS::Field::LiveStartRoomEffect(kind);
  }
  void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) override {
    trigger_->trigger.markerFade =
        static_cast<decltype(trigger_->trigger.markerFade)>(marker_fade);
    trigger_->trigger.lockState.switchValue =
        static_cast<decltype(trigger_->trigger.lockState.switchValue)>(value);
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& state,
                          std::uint8_t passability) override {
    auto* cell = VS::Field::LiveCollisionCellAt(state.tile_x, state.tile_z,
                                                state.floor_layer);
    if (cell != nullptr) {
      cell->doorPassability = passability;
    }
  }

 private:
  Field_MpdLockTrigger* trigger_ = nullptr;
};

class LiveRoomSwitchStartHost final : public VS::Field::RoomSwitchStartHost {
 public:
  explicit LiveRoomSwitchStartHost(Field_MpdLockTrigger* trigger)
      : trigger_(trigger) {}
  std::optional<VS::Field::RoomSwitchState> State() const override {
    if (trigger_ == nullptr || reinterpret_cast<std::uintptr_t>(trigger_) %
                                       alignof(Field_MpdLockTrigger) !=
                                   0) {
      return std::nullopt;
    }
    return VS::Field::RoomSwitchState{
        .switch_value =
            static_cast<std::uint16_t>(trigger_->trigger.lockState.switchValue),
        .marker_fade = static_cast<std::uint8_t>(trigger_->trigger.markerFade),
        .activation = static_cast<std::uint8_t>(trigger_->trigger.activation)};
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    if (!State()) {
      return false;
    }
    trigger_->trigger.markerFade =
        static_cast<decltype(trigger_->trigger.markerFade)>(marker);
    trigger_->trigger.activation =
        static_cast<decltype(trigger_->trigger.activation)>(activation);
    return true;
  }
  bool QueueToggle() override {
    return VS::Field::QueueLiveGimmickAnimation(2, trigger_);
  }
  bool ToggleImmediately() override {
    if (!State()) {
      return false;
    }
    Field_ToggleRoomSwitchEntry(trigger_);
    return true;
  }

 private:
  Field_MpdLockTrigger* trigger_ = nullptr;
};

class LiveChestSwitchHost final : public VS::Field::RoomSwitchHost,
                                  public VS::Field::RoomSwitchStartHost {
 public:
  explicit LiveChestSwitchHost(VS::Field::ChestTarget target)
      : target_(target) {}
  std::optional<VS::Field::RoomSwitchState> State() const override {
    const auto* chest = VS::Field::ResolveLiveChestTarget(target_);
    if (chest == nullptr) {
      return std::nullopt;
    }
    const auto bytes = static_cast<std::uint16_t>(chest->reservedA);
    return VS::Field::RoomSwitchState{
        .switch_value = static_cast<std::uint16_t>(chest->isOpen),
        .marker_fade = static_cast<std::uint8_t>(bytes >> kMarkerByteShift),
        .activation = static_cast<std::uint8_t>(bytes),
        .effect_kind = chest->lidRotationId,
        .tile_x = chest->tileX,
        .tile_z = chest->tileZ,
        .floor_layer = chest->layer};
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kSwitchSoundGroup, sound);
  }
  void StartEffect(std::int32_t kind) override {
    VS::Field::LiveStartRoomEffect(kind);
  }
  void StoreSwitch(std::uint16_t value, std::uint8_t marker) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target_);
    if (chest == nullptr) {
      return;
    }
    chest->isOpen = static_cast<std::int16_t>(value);
    const auto activation = static_cast<std::uint8_t>(chest->reservedA);
    chest->reservedA = static_cast<std::int16_t>(
        (static_cast<std::uint16_t>(marker) << kMarkerByteShift) | activation);
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& state,
                          std::uint8_t passability) override {
    auto* cell = VS::Field::LiveCollisionCellAt(state.tile_x, state.tile_z,
                                                state.floor_layer);
    if (cell != nullptr) {
      cell->doorPassability = passability;
    }
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target_);
    if (chest == nullptr) {
      return false;
    }
    chest->reservedA = static_cast<std::int16_t>(
        (static_cast<std::uint16_t>(marker) << kMarkerByteShift) | activation);
    return true;
  }
  bool QueueToggle() override {
    return VS::Field::QueueLiveGimmickAnimation(2, target_);
  }
  bool ToggleImmediately() override {
    return VS::Field::ToggleRoomSwitch(*this);
  }

 private:
  VS::Field::ChestTarget target_;
};

}  // namespace

namespace VS::Field {
bool ToggleLiveDoorCollision(RoomObjectTarget target) {
  LiveDoorCollisionHost host(target);
  return ToggleDoorCollision(host);
}
bool BeginLiveDoorToggle(RoomObjectTarget target) {
  LiveDoorAnimationHost host(target);
  return BeginDoorToggle(host);
}
bool StartLiveDoorAnimation(RoomObjectTarget target) {
  LiveDoorAnimationHost host(target);
  return StartDoorAnimation(host);
}
bool FinishLiveDoorAnimation(RoomObjectTarget target) {
  LiveDoorAnimationHost host(target);
  return FinishDoorAnimation(host);
}

std::optional<RoomObjectTarget> FindLiveInteractiveDoorAtTile(
    const RoomDoorTileQuery& query) {
  const auto records = RoomDoorRecords();
  std::vector<RoomDoorPlacement> doors;
  doors.reserve(records.size());
  for (const auto& record : records) {
    const auto& trigger = record.trigger;
    doors.push_back(
        {.tile_x = trigger.tileX,
         .tile_z = trigger.tileZ,
         .floor_layer = trigger.floorLayer,
         .kind = trigger.kind,
         .activation = static_cast<std::uint8_t>(trigger.activation)});
  }
  const auto index = InteractiveDoorAtTile(doors, query);
  return index ? std::optional{RoomObjectTarget{
                     static_cast<std::uint32_t>(*index),
                     LiveInteractionTargetDomain().generation}}
               : std::nullopt;
}

bool MarkLiveSigilDoorUnlocked(std::uint32_t sigil_index) {
  if (Field_roomData.minimapScene == nullptr) return false;
  const auto* scene =
      reinterpret_cast<const std::byte*>(Field_roomData.minimapScene);
  const auto room = CurrentSceneRoomAddress(
      {scene, Field_roomData.header.minimapSceneLength});
  if (!room) return false;
  const auto pairs = LiveSigilPairs();
  const SigilDoorTables tables{
      std::span(Field_lockedDoorIndexRanges).first(kSigilRangeWords), pairs};
  return MarkSigilDoorUnlocked(
      tables,
      {.zone_id = room->zone_id,
       .map_id = room->map_id,
       .sigil_index = sigil_index},
      std::as_writable_bytes(std::span(Audio_Seq_unusedStateFlags.opaque186)));
}

std::uint8_t LiveRoomSigilDoorUnlockByte(const SigilDoorRoomQuery& query) {
  const auto pairs = LiveSigilPairs();
  const SigilDoorTables tables{
      std::span(Field_lockedDoorIndexRanges).first(kSigilRangeWords), pairs};
  return RoomSigilDoorUnlockByte(
      tables, query,
      std::as_bytes(std::span(Audio_Seq_unusedStateFlags.opaque186)));
}

bool BeginLiveChestSwitch(ChestTarget target) {
  LiveChestSwitchHost host(target);
  return BeginSwitchToggle(host);
}
bool ToggleLiveChestSwitch(ChestTarget target) {
  LiveChestSwitchHost host(target);
  return ToggleRoomSwitch(host);
}
}  // namespace VS::Field

void Field_ToggleDoorCollisionEntry(Field_MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  if (target) static_cast<void>(VS::Field::ToggleLiveDoorCollision(*target));
}
int Field_BeginDoorToggleEntry(Field_MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  return target && VS::Field::BeginLiveDoorToggle(*target);
}
int Field_StartDoorAnimationEntry(Field_MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  return target && VS::Field::StartLiveDoorAnimation(*target);
}
int Field_FinishDoorAnimationEntry(Field_MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  return target && VS::Field::FinishLiveDoorAnimation(*target);
}

// decomp: BATTLE.PRG func_8008BC04 (146C.c:16215).
Field_MpdLockTrigger* Field_InteractiveDoorAtTileEntry(int tile_x, int tile_z,
                                                       int floor_layer) {
  const auto target = VS::Field::FindLiveInteractiveDoorAtTile(
      {.tile_x = tile_x, .tile_z = tile_z, .floor_layer = floor_layer});
  return target ? VS::Field::ResolveLiveRoomObjectTarget(*target) : nullptr;
}

// decomp: BATTLE.PRG func_8008E6DC (146C.c:17399).
void Field_MarkSigilDoorUnlockedEntry(int sigil_index) {
  static_cast<void>(VS::Field::MarkLiveSigilDoorUnlocked(
      static_cast<std::uint32_t>(sigil_index)));
}

// decomp: BATTLE.PRG func_8008E7BC (146C.c:17419).
int Field_IsRoomSigilDoorUnlockedEntry(int zone_id, int map_id,
                                       unsigned int sigil_index) {
  return VS::Field::LiveRoomSigilDoorUnlockByte(
      {.zone_id = zone_id, .map_id = map_id, .sigil_index = sigil_index});
}

// decomp: BATTLE.PRG func_8008D658 (src/BATTLE/BATTLE.PRG/146C.c:16883).
void Field_ToggleRoomSwitchEntry(Field_MpdLockTrigger* switch_trigger) {
  LiveRoomSwitchHost host(switch_trigger);
  static_cast<void>(VS::Field::ToggleRoomSwitch(host));
}

// decomp: BATTLE.PRG func_8008D5FC (src/BATTLE/BATTLE.PRG/146C.c:16870).
void Field_BeginSwitchToggleEntry(Field_MpdLockTrigger* switch_trigger) {
  LiveRoomSwitchStartHost host(switch_trigger);
  static_cast<void>(VS::Field::BeginSwitchToggle(host));
}
