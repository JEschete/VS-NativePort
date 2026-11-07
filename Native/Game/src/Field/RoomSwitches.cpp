#include "Field/RoomSwitches.hpp"

namespace VS::Field {
namespace {

constexpr std::int32_t kOpeningSoundFirst = 0x3B;
constexpr std::int32_t kOpeningSoundSecond = 0x3C;
constexpr std::int32_t kClosingSoundFirst = 0x3D;
constexpr std::int32_t kClosingSoundSecond = 0x3E;
constexpr std::uint8_t kOpenPassability = 5;

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

}  // namespace VS::Field
