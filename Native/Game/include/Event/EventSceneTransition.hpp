#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's scene handoff opcodes: forgetting a room's remembered
// enemies, showing or hiding the gameplay HUD, running a menu or reward
// sequence until it closes, and changing room or zone. The rules take the
// decoded instruction, the opcode-owned state and a host for the room enemy
// memory, the HUD, script flags, the reward flow, the menu framework, the
// zone music switch and the scripted transitions; they keep the original
// call order and return the VM step the original returned.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"
#include "Field/DoorRecord.hpp"

namespace VS::Event {
// Instruction lengths (the opcode size table, D_800E9C30).
inline constexpr std::size_t kClearRoomActorCacheLength = 3;
inline constexpr std::size_t kSetHudVisibleLength = 2;
inline constexpr std::size_t kRunMenuTransitionLength = 4;
inline constexpr std::size_t kChangeRoomLength = 10;

// Script flag 0xB6 (menuFSubmenu): the screen the Results menu (MENUF.PRG)
// shows, 0 the game clear, 1 and 2 the time attack's start and results, 4 to
// 6 the cube puzzle's start, results and quit.
inline constexpr int kResultsScreenFlag = 0xB6;
inline constexpr std::uint8_t kGameClearScreen = 0;
inline constexpr std::uint8_t kTimeAttackStartScreen = 1;
inline constexpr std::uint8_t kTimeAttackResultsScreen = 2;
// The menu transition's reward modes; any other mode opens that game menu.
// Modes 10-12 show a cube puzzle screen, 13 starts the time attack, 14 shows
// the game clear or, during a time attack, its results, and 15 runs the
// post-action rewards.
inline constexpr std::uint8_t kFirstCubePuzzleMode = 10;
inline constexpr std::uint8_t kTimeAttackStartMode = 13;
inline constexpr std::uint8_t kGameClearMode = 14;
inline constexpr std::uint8_t kPostActionRewardsMode = 15;
// D_800E9BF0: the cube puzzle screens of modes 10-12 (quit, start, results).
inline constexpr std::size_t kCubePuzzleModeCount = 3;
using CubePuzzleScreens = std::array<std::uint8_t, kCubePuzzleModeCount>;
inline constexpr CubePuzzleScreens kCubePuzzleScreenOfMode = {6, 4, 5};
// The music slot the Results menu plays in (it loads its music into slot 2);
// the game clear mode points the music commands at it.
inline constexpr std::uint8_t kResultsMusicSlot = 2;
// OpenGameMenu's mode for a menu an event script opens.
inline constexpr int kScriptGameMenuMode = 2;
// The menu status (ActionSelection menuStatus) of a closed game menu.
inline constexpr std::int16_t kGameMenuClosed = 2;

// roomChangeState (D_800F4BE2), shared with LoadRoomEventSections.
inline constexpr std::int16_t kRoomChangeIdle = 0;
inline constexpr std::int16_t kRoomChangeKeepsEvent = 1;
inline constexpr std::int16_t kRoomChangeArrived = 2;
inline constexpr std::int16_t kRoomChangeStopsEvent = 3;

// Script requests the commands refuse rather than overrun.
enum class SceneTransitionRefusal : std::uint8_t {
  kShortCommand,  // an instruction shorter than its operands
};

// The game services the scene handoff opcodes call.
class SceneTransitionHost {
 public:
  SceneTransitionHost() = default;
  SceneTransitionHost(const SceneTransitionHost&) = delete;
  SceneTransitionHost& operator=(const SceneTransitionHost&) = delete;
  SceneTransitionHost(SceneTransitionHost&&) = delete;
  SceneTransitionHost& operator=(SceneTransitionHost&&) = delete;
  virtual ~SceneTransitionHost() = default;

  virtual void Refuse(SceneTransitionRefusal refusal, int value) = 0;

  // func_8007BBB8: drops the remembered enemies of `room` in `zone`.
  virtual void ForgetRoomEnemies(std::uint8_t room, std::uint8_t zone) = 0;
  // func_8007BD8C: the gameplay HUD's hide command; 0 shows it.
  virtual void SetGameplayHudHidden(std::uint8_t command) = 0;

  // Script flags (vs_battle_getStateFlag, vs_battle_setStateFlag).
  [[nodiscard]] virtual std::uint8_t ScriptFlag(int id) = 0;
  virtual void SetScriptFlag(int id, std::uint8_t value) = 0;

  // The reward flow (RewardFlow.hpp): func_8007B9A0, func_8007B9CC and
  // func_8007B9FC, which is true while the sequence still runs.
  virtual void BeginEventRewardResults() = 0;
  virtual void BeginPostActionRewards() = 0;
  [[nodiscard]] virtual bool UpdateRewardSequence() = 0;
  // VS::Event::musicSlot (scriptMusicSlot, D_800F4C69), the slot the music
  // commands address.
  virtual void SetMusicSlot(std::uint8_t slot) = 0;

  // The menu framework: func_800CACD0, func_800C97BC, the action selection's
  // menu status (func_800CB66C) and func_800CB114.
  virtual void OpenGameMenu(std::uint8_t menu, int mode) = 0;
  virtual void UpdateMenuOverlay() = 0;
  [[nodiscard]] virtual std::int16_t GameMenuStatus() = 0;
  virtual void ReleaseMenuRows() = 0;

  // func_8007BE10, and the scripted transitions func_8007C580 (a room of the
  // current zone) and func_8007C654 (another zone).
  virtual void SetZoneMusicSwitchEnabled(bool enabled) = 0;
  virtual void BeginScriptedRoomTransition(const Field::DoorRecord& door,
                                           std::uint8_t effect,
                                           std::uint8_t speed) = 0;
  virtual void BeginScriptedZoneTransition(const Field::DoorRecord& door,
                                           std::uint8_t effect,
                                           std::uint8_t speed) = 0;
};

// decomp: BATTLE.PRG func_800B91C4 (F01099, 4A0A8.c:1780)
// Opcode 0x53: forgets the remembered enemies of the room in byte 2 of the
// zone in byte 1, so they spawn again.
ScriptStep ScriptClearRoomActorCache(ScriptCommand command,
                                     SceneTransitionHost& host);

// decomp: BATTLE.PRG func_800B92D0 (F01104, 4A0A8.c:1813)
// Opcode 0x58: byte 1 is 1 to show the gameplay HUD and 0 to hide it. The
// hide command is the whole byte with its low bit flipped, as the original
// passed it.
ScriptStep ScriptSetHudVisible(ScriptCommand command,
                               SceneTransitionHost& host);

// decomp: BATTLE.PRG func_800B92F4 (F01105, 4A0A8.c:1819)
// Opcode 0x59: the first run opens the Results reward sequence (modes 10-14),
// the post-action rewards (15) or game menu `mode` (any other), sets
// `menu_transition_active` (isMenuTransitionActive, D_800F4BF8) and
// holds. Later runs hold until the sequence or menu ends, then clear the flag
// and advance and yield. Modes 10-12 and 14 also reset the Results screen
// flag to the game clear when their sequence ends; the time attack start (13)
// keeps its screen so the game clear mode later shows the time attack's
// results, and 15 leaves the flag alone.
ScriptStep ScriptRunMenuTransition(ScriptCommand command,
                                   bool& menu_transition_active,
                                   SceneTransitionHost& host);

// decomp: BATTLE.PRG func_800B98C0 (F01120, 4A0A8.c:2018)
// Opcode 0x68: with no change in progress, starts the scripted transition to
// room byte 2 of zone byte 1 (0 keeps the zone) entering on tile (byte 3,
// byte 5) on surface byte 4 with facing quadrant byte 6, with effect byte 8
// and speed byte 9. Byte 7 holds the entry step (bit 0), the reserved entry bit
// (bit 1), whether the event keeps running in the new room (bit 2) and, for a
// zone, the zone music switch (bit 3). Holds until the room loader reports the
// arrival, then clears `room_change_state` and advances.
ScriptStep ScriptChangeRoom(ScriptCommand command,
                            std::int16_t& room_change_state,
                            SceneTransitionHost& host);
}  // namespace VS::Event
