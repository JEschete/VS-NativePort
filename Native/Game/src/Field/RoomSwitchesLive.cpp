#include "Field/RoomSwitchesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <vector>

#include "Audio/SfxLive.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Doors.hpp"
#include "Field/GimmickAnimationQueueLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomSwitches.hpp"
#include "Items/ItemRecords.hpp"
#include "State/StateFlags.hpp"

namespace {

constexpr int kSwitchSoundGroup = 0x7E;
constexpr unsigned kMarkerByteShift = 8;
// Original800E849C-800E84AB: seven groups and their terminating boundary.
constexpr std::size_t kSigilRangeWords = 8;
static_assert(std::size(VS::Field::lockedDoorIndexRanges) >= kSigilRangeWords);
enum SigilPairByte : std::size_t {
  kFirstZone,
  kFirstMap,
  kSecondZone,
  kSecondMap
};

std::span<VS::Field::MpdLockTrigger> RoomDoorRecords() {
  const auto bytes = VS::Field::roomData.header.lock_triggers_length;
  if (VS::Field::roomData.lock_triggers == nullptr ||
      bytes % sizeof(VS::Field::MpdLockTrigger) != 0 ||
      reinterpret_cast<std::uintptr_t>(VS::Field::roomData.lock_triggers) %
              alignof(VS::Field::MpdLockTrigger) !=
          0) {
    return {};
  }
  return {VS::Field::roomData.lock_triggers,
          bytes / sizeof(VS::Field::MpdLockTrigger)};
}

std::optional<VS::Field::RoomObjectTarget> RoomDoorTarget(
    const VS::Field::MpdLockTrigger* record) {
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

VS::Field::MpdLockTrigger* ResolveRoomDoor(VS::Field::RoomObjectTarget target) {
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
        .tile_x = trigger.tile_x,
        .tile_z = trigger.tile_z,
        .floor_layer = trigger.floor_layer,
        .kind = trigger.kind,
        .activation = static_cast<std::uint8_t>(trigger.activation)};
  }
  void ToggleCell(const VS::Field::RoomDoorTileQuery& query,
                  std::uint8_t xor_mask) override {
    auto* cell = VS::Field::LiveCollisionCellAt(query.tile_x, query.tile_z,
                                                query.floor_layer);
    if (cell != nullptr) cell->passability ^= xor_mask;
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
        .placement = {.tile_x = trigger.tile_x,
                      .tile_z = trigger.tile_z,
                      .floor_layer = trigger.floor_layer,
                      .kind = trigger.kind,
                      .activation =
                          static_cast<std::uint8_t>(trigger.activation)},
        .key_or_sigil = std::bit_cast<std::uint16_t>(record->key_or_sigil),
        .open_value = record->is_open,
        .linked_index = record->linked_lock_index,
        .effect =
            std::bit_cast<std::uint8_t>(trigger.lock_state.lock.room_effect),
        .busy = std::bit_cast<std::uint8_t>(record->is_triggered),
        .delay = std::bit_cast<std::uint8_t>(record->delay_frames)};
  }
  bool QueueAvailable() const override {
    return ResolveRoomDoor(target_) != nullptr &&
           std::ranges::any_of(
               VS::Field::gimmickAnimationQueue,
               [](const auto& job) { return job.job_kind == 0; });
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
    record->is_triggered = std::bit_cast<char>(busy);
    return true;
  }
  bool StoreDelay(std::uint8_t delay) override {
    auto* record = ResolveRoomDoor(target_);
    if (record == nullptr) return false;
    record->delay_frames = std::bit_cast<char>(delay);
    return true;
  }
  bool StoreOpen(VS::Field::DoorMember member, std::int16_t value,
                 std::uint8_t busy) override {
    auto* record = Record(member);
    if (record == nullptr) return false;
    record->is_triggered = std::bit_cast<char>(busy);
    record->is_open = value;
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
    if (primary->linked_lock_index < 0) return std::nullopt;
    const VS::Field::RoomObjectTarget linked{
        static_cast<std::uint32_t>(primary->linked_lock_index),
        target_.generation};
    return ResolveRoomDoor(linked) != nullptr ? std::optional{linked}
                                              : std::nullopt;
  }
  VS::Field::MpdLockTrigger* Record(VS::Field::DoorMember member) const {
    const auto target = Target(member);
    return target ? ResolveRoomDoor(*target) : nullptr;
  }
  VS::Field::RoomObjectTarget target_;
};

using RoomPair = VS::Field::SigilDoorRoomPair;
using SigilPairs = std::array<RoomPair, VS::Field::kLockedDoorPairCount>;

SigilPairs LiveSigilPairs() {
  SigilPairs pairs{};
  for (std::size_t index = 0; index < pairs.size(); ++index) {
    const auto& original = VS::Field::lockedDoorRoomPairs[index];
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
  explicit LiveRoomSwitchHost(VS::Field::MpdLockTrigger* trigger)
      : trigger_(trigger) {}

  std::optional<VS::Field::RoomSwitchState> State() const override {
    if (trigger_ == nullptr) {
      return std::nullopt;
    }
    const auto& trigger = trigger_->trigger;
    return VS::Field::RoomSwitchState{
        .switch_value =
            static_cast<std::uint16_t>(trigger.lock_state.switch_value),
        .marker_fade = static_cast<std::uint8_t>(trigger.marker_fade),
        .effect_kind = trigger.kind,
        .tile_x = trigger.tile_x,
        .tile_z = trigger.tile_z,
        .floor_layer = trigger.floor_layer};
  }
  void PlaySound(std::int32_t sound) override {
    VS::Audio::LivePlaySfxCentered(kSwitchSoundGroup, sound);
  }
  void StartEffect(std::int32_t kind) override {
    VS::Field::LiveStartRoomEffect(kind);
  }
  void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) override {
    trigger_->trigger.marker_fade =
        static_cast<decltype(trigger_->trigger.marker_fade)>(marker_fade);
    trigger_->trigger.lock_state.switch_value =
        static_cast<decltype(trigger_->trigger.lock_state.switch_value)>(value);
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& state,
                          std::uint8_t passability) override {
    auto* cell = VS::Field::LiveCollisionCellAt(state.tile_x, state.tile_z,
                                                state.floor_layer);
    if (cell != nullptr) {
      cell->passability = passability;
    }
  }

 private:
  VS::Field::MpdLockTrigger* trigger_ = nullptr;
};

class LiveRoomSwitchStartHost final : public VS::Field::RoomSwitchStartHost {
 public:
  explicit LiveRoomSwitchStartHost(VS::Field::MpdLockTrigger* trigger)
      : trigger_(trigger) {}
  std::optional<VS::Field::RoomSwitchState> State() const override {
    if (trigger_ == nullptr || reinterpret_cast<std::uintptr_t>(trigger_) %
                                       alignof(VS::Field::MpdLockTrigger) !=
                                   0) {
      return std::nullopt;
    }
    return VS::Field::RoomSwitchState{
        .switch_value = static_cast<std::uint16_t>(
            trigger_->trigger.lock_state.switch_value),
        .marker_fade = static_cast<std::uint8_t>(trigger_->trigger.marker_fade),
        .activation = static_cast<std::uint8_t>(trigger_->trigger.activation)};
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    if (!State()) {
      return false;
    }
    trigger_->trigger.marker_fade =
        static_cast<decltype(trigger_->trigger.marker_fade)>(marker);
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
    VS::Field::ToggleRoomSwitchEntry(trigger_);
    return true;
  }

 private:
  VS::Field::MpdLockTrigger* trigger_ = nullptr;
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
    const auto bytes = static_cast<std::uint16_t>(chest->toggle_state);
    return VS::Field::RoomSwitchState{
        .switch_value = static_cast<std::uint16_t>(chest->is_open),
        .marker_fade = static_cast<std::uint8_t>(bytes >> kMarkerByteShift),
        .activation = static_cast<std::uint8_t>(bytes),
        .effect_kind = chest->lid_rotation_id,
        .tile_x = chest->tile_x,
        .tile_z = chest->tile_z,
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
    chest->is_open = static_cast<std::int16_t>(value);
    const auto activation = static_cast<std::uint8_t>(chest->toggle_state);
    chest->toggle_state = static_cast<std::int16_t>(
        (static_cast<std::uint16_t>(marker) << kMarkerByteShift) | activation);
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& state,
                          std::uint8_t passability) override {
    auto* cell = VS::Field::LiveCollisionCellAt(state.tile_x, state.tile_z,
                                                state.floor_layer);
    if (cell != nullptr) {
      cell->passability = passability;
    }
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    auto* chest = VS::Field::ResolveLiveChestTarget(target_);
    if (chest == nullptr) {
      return false;
    }
    chest->toggle_state = static_cast<std::int16_t>(
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
        {.tile_x = trigger.tile_x,
         .tile_z = trigger.tile_z,
         .floor_layer = trigger.floor_layer,
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
  if (VS::Field::roomData.minimap_scene == nullptr) return false;
  const auto* scene =
      reinterpret_cast<const std::byte*>(VS::Field::roomData.minimap_scene);
  const auto room = CurrentSceneRoomAddress(
      {scene, VS::Field::roomData.header.minimap_scene_length});
  if (!room) return false;
  const auto pairs = LiveSigilPairs();
  const SigilDoorTables tables{
      std::span(VS::Field::lockedDoorIndexRanges).first(kSigilRangeWords),
      pairs};
  return MarkSigilDoorUnlocked(
      tables,
      {.zone_id = room->zone_id,
       .map_id = room->map_id,
       .sigil_index = sigil_index},
      std::as_writable_bytes(
          std::span(VS::State::stateFlags.sigil_doors_unlocked)));
}

std::uint8_t LiveRoomSigilDoorUnlockByte(const SigilDoorRoomQuery& query) {
  const auto pairs = LiveSigilPairs();
  const SigilDoorTables tables{
      std::span(VS::Field::lockedDoorIndexRanges).first(kSigilRangeWords),
      pairs};
  return RoomSigilDoorUnlockByte(
      tables, query,
      std::as_bytes(std::span(VS::State::stateFlags.sigil_doors_unlocked)));
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

void VS::Field::ToggleDoorCollisionEntry(VS::Field::MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  if (target) static_cast<void>(VS::Field::ToggleLiveDoorCollision(*target));
}
int VS::Field::BeginDoorToggleEntry(VS::Field::MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  return target && VS::Field::BeginLiveDoorToggle(*target);
}
int VS::Field::StartDoorAnimationEntry(VS::Field::MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  return target && VS::Field::StartLiveDoorAnimation(*target);
}
int VS::Field::FinishDoorAnimationEntry(VS::Field::MpdLockTrigger* door) {
  const auto target = RoomDoorTarget(door);
  return target && VS::Field::FinishLiveDoorAnimation(*target);
}

// decomp: BATTLE.PRG func_8008BC04 (146C.c:16215).
VS::Field::MpdLockTrigger* VS::Field::InteractiveDoorAtTileEntry(
    int tile_x, int tile_z, int floor_layer) {
  const auto target = VS::Field::FindLiveInteractiveDoorAtTile(
      {.tile_x = tile_x, .tile_z = tile_z, .floor_layer = floor_layer});
  return target ? VS::Field::ResolveLiveRoomObjectTarget(*target) : nullptr;
}

// decomp: BATTLE.PRG func_8008E6DC (146C.c:17399).
void VS::Field::MarkSigilDoorUnlockedEntry(int sigil_index) {
  static_cast<void>(VS::Field::MarkLiveSigilDoorUnlocked(
      static_cast<std::uint32_t>(sigil_index)));
}

// decomp: BATTLE.PRG func_8008E7BC (146C.c:17419).
int VS::Field::IsRoomSigilDoorUnlockedEntry(int zone_id, int map_id,
                                            unsigned int sigil_index) {
  return VS::Field::LiveRoomSigilDoorUnlockByte(
      {.zone_id = zone_id, .map_id = map_id, .sigil_index = sigil_index});
}

// decomp: BATTLE.PRG func_8008D658 (src/BATTLE/BATTLE.PRG/146C.c:16883).
void VS::Field::ToggleRoomSwitchEntry(
    VS::Field::MpdLockTrigger* switch_trigger) {
  LiveRoomSwitchHost host(switch_trigger);
  static_cast<void>(VS::Field::ToggleRoomSwitch(host));
}

// decomp: BATTLE.PRG func_8008D5FC (src/BATTLE/BATTLE.PRG/146C.c:16870).
void VS::Field::BeginSwitchToggleEntry(
    VS::Field::MpdLockTrigger* switch_trigger) {
  LiveRoomSwitchStartHost host(switch_trigger);
  static_cast<void>(VS::Field::BeginSwitchToggle(host));
}
