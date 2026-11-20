// Hand-maintained (tools/port/owned.csv).
#include "Event/EventSceneTransition.hpp"

#include <cstddef>
#include <cstdint>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"
#include "Field/DoorRecord.hpp"

namespace VS::Event {
namespace {
// ScriptClearRoomActorCache's zone and room.
constexpr std::size_t kCacheZoneOperand = 1;
constexpr std::size_t kCacheRoomOperand = 2;
// ScriptSetHudVisible's visibility and the bit the hide command flips.
constexpr std::size_t kHudVisibleOperand = 1;
constexpr std::uint8_t kHudVisibleBit = 0x01;
// ScriptRunMenuTransition's mode.
constexpr std::size_t kMenuModeOperand = 1;
// ScriptChangeRoom's operands and the bits of its options byte.
constexpr std::size_t kZoneOperand = 1;
constexpr std::size_t kRoomOperand = 2;
constexpr std::size_t kEntryTileXOperand = 3;
constexpr std::size_t kEntryDirectionOperand = 4;
constexpr std::size_t kEntryTileZOperand = 5;
constexpr std::size_t kEntryElevationOperand = 6;
constexpr std::size_t kOptionsOperand = 7;
constexpr std::size_t kEffectOperand = 8;
constexpr std::size_t kSpeedOperand = 9;
constexpr std::uint8_t kEntryStepBit = 0x01;
constexpr std::uint8_t kWalksInBit = 0x02;
constexpr std::uint8_t kKeepEventBit = 0x04;
constexpr std::uint8_t kZoneMusicSwitchBit = 0x08;
// A destination zone of 0 changes room within the current zone.
constexpr std::uint8_t kCurrentZone = 0;

// What a menu transition mode runs.
enum class MenuTransitionKind : std::uint8_t {
  kCubePuzzle,
  kTimeAttackStart,
  kGameClear,
  kPostActionRewards,
  kGameMenu,
};

bool HasOperands(ScriptCommand command, std::size_t length,
                 SceneTransitionHost& host) {
  if (command.size() >= length) return true;
  host.Refuse(SceneTransitionRefusal::kShortCommand,
              static_cast<int>(command.size()));
  return false;
}

MenuTransitionKind KindOf(std::uint8_t mode) {
  if (mode >= kFirstCubePuzzleMode && mode < kTimeAttackStartMode) {
    return MenuTransitionKind::kCubePuzzle;
  }
  switch (mode) {
    case kTimeAttackStartMode:
      return MenuTransitionKind::kTimeAttackStart;
    case kGameClearMode:
      return MenuTransitionKind::kGameClear;
    case kPostActionRewardsMode:
      return MenuTransitionKind::kPostActionRewards;
    default:
      return MenuTransitionKind::kGameMenu;
  }
}

// 4A0A8.c:1828-1874: opens what `mode` runs.
void StartMenuTransition(std::uint8_t mode, SceneTransitionHost& host) {
  switch (KindOf(mode)) {
    case MenuTransitionKind::kCubePuzzle:
      host.SetScriptFlag(kResultsScreenFlag,
                         kCubePuzzleScreenOfMode.at(static_cast<std::size_t>(
                             mode - kFirstCubePuzzleMode)));
      host.BeginEventRewardResults();
      break;
    case MenuTransitionKind::kTimeAttackStart:
      host.SetScriptFlag(kResultsScreenFlag, kTimeAttackStartScreen);
      host.BeginEventRewardResults();
      break;
    case MenuTransitionKind::kGameClear:
      // During a time attack the game clear shows the time attack's results.
      if (host.ScriptFlag(kResultsScreenFlag) == kTimeAttackStartScreen) {
        host.SetScriptFlag(kResultsScreenFlag, kTimeAttackResultsScreen);
      }
      host.BeginEventRewardResults();
      host.SetMusicSlot(kResultsMusicSlot);
      break;
    case MenuTransitionKind::kPostActionRewards:
      host.BeginPostActionRewards();
      break;
    case MenuTransitionKind::kGameMenu:
      host.OpenGameMenu(mode, kScriptGameMenuMode);
      break;
  }
}

// 4A0A8.c:1849-1855 and 1868-1872: holds while the reward sequence runs.
ScriptStep FinishRewardSequence(bool reset_screen, bool& active,
                                SceneTransitionHost& host) {
  if (host.UpdateRewardSequence()) return ScriptStep::kHold;
  if (reset_screen) host.SetScriptFlag(kResultsScreenFlag, kGameClearScreen);
  active = false;
  return ScriptStep::kAdvanceAndYield;
}

// 4A0A8.c:1876-1882: holds until the game menu reports itself closed.
ScriptStep FinishGameMenu(bool& active, SceneTransitionHost& host) {
  host.UpdateMenuOverlay();
  if (host.GameMenuStatus() != kGameMenuClosed) return ScriptStep::kHold;
  host.ReleaseMenuRows();
  active = false;
  return ScriptStep::kAdvanceAndYield;
}

ScriptStep FinishMenuTransition(std::uint8_t mode, bool& active,
                                SceneTransitionHost& host) {
  switch (KindOf(mode)) {
    case MenuTransitionKind::kCubePuzzle:
    case MenuTransitionKind::kGameClear:
      return FinishRewardSequence(true, active, host);
    case MenuTransitionKind::kTimeAttackStart:
    case MenuTransitionKind::kPostActionRewards:
      return FinishRewardSequence(false, active, host);
    case MenuTransitionKind::kGameMenu:
      return FinishGameMenu(active, host);
  }
  return ScriptStep::kHold;
}

// 4A0A8.c:2023-2031: the door the script describes. The original left the
// rest of its stack record unset; the native door keeps them zero.
Field::DoorRecord ScriptedDoor(ScriptCommand command) {
  const std::uint8_t options = OperandByte(command, kOptionsOperand);
  return Field::DoorRecord{
      .destination_zone_id = OperandByte(command, kZoneOperand),
      .destination_room_id = OperandByte(command, kRoomOperand),
      .entry_direction = static_cast<std::uint8_t>(
          OperandByte(command, kEntryDirectionOperand) &
          Field::kDoorEntryDirectionMask),
      .entry_tile_x = static_cast<std::uint8_t>(
          OperandByte(command, kEntryTileXOperand) & Field::kDoorTileMask),
      .entry_tile_z = static_cast<std::uint8_t>(
          OperandByte(command, kEntryTileZOperand) & Field::kDoorTileMask),
      .entry_elevation = static_cast<std::uint8_t>(
          OperandByte(command, kEntryElevationOperand) &
          Field::kDoorEntryElevationMask),
      .has_entry_step = (options & kEntryStepBit) != 0,
      .walks_in = (options & kWalksInBit) != 0,
  };
}
}  // namespace

// decomp: BATTLE.PRG func_800B91C4 (F01099, src/BATTLE/BATTLE.PRG/4A0A8.c:1780,
// 0x800B91C4-0x800B91F0)
ScriptStep ScriptClearRoomActorCache(ScriptCommand command,
                                     SceneTransitionHost& host) {
  if (!HasOperands(command, kClearRoomActorCacheLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t zone = OperandByte(command, kCacheZoneOperand);
  const std::uint8_t room = OperandByte(command, kCacheRoomOperand);
  host.ForgetRoomEnemies(room, zone);
  return ScriptStep::kAdvance;
}

// decomp: BATTLE.PRG func_800B92D0 (F01104, src/BATTLE/BATTLE.PRG/4A0A8.c:1813,
// 0x800B92D0-0x800B92F4)
ScriptStep ScriptSetHudVisible(ScriptCommand command,
                               SceneTransitionHost& host) {
  if (!HasOperands(command, kSetHudVisibleLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t visible = OperandByte(command, kHudVisibleOperand);
  host.SetGameplayHudHidden(
      static_cast<std::uint8_t>(visible ^ kHudVisibleBit));
  return ScriptStep::kAdvance;
}

// decomp: BATTLE.PRG func_800B92F4 (F01105, src/BATTLE/BATTLE.PRG/4A0A8.c:1819,
// 0x800B92F4-0x800B94D8)
ScriptStep ScriptRunMenuTransition(ScriptCommand command,
                                   bool& menu_transition_active,
                                   SceneTransitionHost& host) {
  if (!HasOperands(command, kRunMenuTransitionLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t mode = OperandByte(command, kMenuModeOperand);
  if (menu_transition_active) {
    return FinishMenuTransition(mode, menu_transition_active, host);
  }
  StartMenuTransition(mode, host);
  menu_transition_active = true;
  return ScriptStep::kHold;
}

// decomp: BATTLE.PRG func_800B98C0 (F01120, src/BATTLE/BATTLE.PRG/4A0A8.c:2018,
// 0x800B98C0-0x800B9A40)
ScriptStep ScriptChangeRoom(ScriptCommand command,
                            std::int16_t& room_change_state,
                            SceneTransitionHost& host) {
  if (!HasOperands(command, kChangeRoomLength, host)) {
    return ScriptStep::kAdvance;
  }
  if (room_change_state == kRoomChangeIdle) {
    const Field::DoorRecord door = ScriptedDoor(command);
    const std::uint8_t options = OperandByte(command, kOptionsOperand);
    const std::uint8_t effect = OperandByte(command, kEffectOperand);
    const std::uint8_t speed = OperandByte(command, kSpeedOperand);
    if (door.destination_zone_id == kCurrentZone) {
      host.BeginScriptedRoomTransition(door, effect, speed);
    } else {
      host.SetZoneMusicSwitchEnabled((options & kZoneMusicSwitchBit) != 0);
      host.BeginScriptedZoneTransition(door, effect, speed);
    }
    room_change_state = (options & kKeepEventBit) != 0 ? kRoomChangeKeepsEvent
                                                       : kRoomChangeStopsEvent;
  }
  if (room_change_state != kRoomChangeArrived) return ScriptStep::kHold;
  room_change_state = kRoomChangeIdle;
  return ScriptStep::kAdvance;
}
}  // namespace VS::Event
