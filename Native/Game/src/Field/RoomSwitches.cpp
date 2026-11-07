#include "Field/RoomSwitches.hpp"

namespace VS::Field {
namespace {

constexpr std::int32_t kOpeningSoundFirst = 0x3B;
constexpr std::int32_t kOpeningSoundSecond = 0x3C;
constexpr std::int32_t kClosingSoundFirst = 0x3D;
constexpr std::int32_t kClosingSoundSecond = 0x3E;
constexpr std::uint8_t kOpenPassability = 5;
constexpr std::uint8_t kSwitchStartFrames = 20;

}  // namespace

// decomp: BATTLE.PRG func_8008D658 (src/BATTLE/BATTLE.PRG/146C.c:16883).
bool ToggleRoomSwitch(RoomSwitchHost& host) {
  auto state = host.State();
  if (!state) {
    return false;
  }
  const bool opening = state->switch_value == 0;
  host.PlaySound(opening ? kOpeningSoundFirst : kClosingSoundFirst);
  host.PlaySound(opening ? kOpeningSoundSecond : kClosingSoundSecond);
  state = host.State();
  if (!state) {
    return false;
  }
  host.StartEffect(state->effect_kind);
  state = host.State();
  if (!state) {
    return false;
  }
  state->marker_fade = 0;
  state->switch_value = static_cast<std::uint16_t>(state->switch_value == 0);
  host.StoreSwitch(state->switch_value, state->marker_fade);
  host.SetCellPassability(*state,
                          state->switch_value != 0 ? kOpenPassability : 0);
  return true;
}

// decomp: BATTLE.PRG func_8008D5FC (src/BATTLE/BATTLE.PRG/146C.c:16870).
// #320: refused queue admission must not leave marker1/activation20 latched.
bool BeginSwitchToggle(RoomSwitchStartHost& host) {
  const auto state = host.State();
  if (!state || state->marker_fade != 0) {
    return false;
  }
  const auto activation =
      state->switch_value == 0 ? kSwitchStartFrames : state->activation;
  if (!host.StoreStartState(1, activation)) {
    return false;
  }
  const bool accepted =
      state->switch_value == 0 ? host.QueueToggle() : host.ToggleImmediately();
  if (!accepted) {
    static_cast<void>(
        host.StoreStartState(state->marker_fade, state->activation));
  }
  return accepted;
}

}  // namespace VS::Field
