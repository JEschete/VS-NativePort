#include "Field/RoomSwitchesLive.h"

#include "Audio/SfxLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomSwitches.hpp"

extern "C" {
#include "Field/Mpd.h"
}

namespace {

constexpr int kSwitchSoundGroup = 0x7E;

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

}  // namespace

// decomp: BATTLE.PRG func_8008D658 (src/BATTLE/BATTLE.PRG/146C.c:16883).
void Field_ToggleRoomSwitchEntry(Field_MpdLockTrigger* switch_trigger) {
  LiveRoomSwitchHost host(switch_trigger);
  static_cast<void>(VS::Field::ToggleRoomSwitch(host));
}
