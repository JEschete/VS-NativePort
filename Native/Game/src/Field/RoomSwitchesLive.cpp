#include "Field/RoomSwitchesLive.h"

#include "Audio/SfxLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/InteractionTargetLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomSwitches.hpp"

extern "C" {
#include "Field/Mpd.h"
#include "Items/TreasureChests.h"
}

namespace {

constexpr int kSwitchSoundGroup = 0x7E;
constexpr unsigned kMarkerByteShift = 8;

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
bool BeginLiveChestSwitch(ChestTarget target) {
  LiveChestSwitchHost host(target);
  return BeginSwitchToggle(host);
}
bool ToggleLiveChestSwitch(ChestTarget target) {
  LiveChestSwitchHost host(target);
  return ToggleRoomSwitch(host);
}
}  // namespace VS::Field

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
