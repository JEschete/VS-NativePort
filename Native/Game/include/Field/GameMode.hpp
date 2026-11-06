#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Field/Transitions.hpp"

// The field mode rules (BATTLE.PRG 146C.c:1866-1907, 3221 and 4659-4686):
// pausing the field for dialogs, objects and menus, returning to
// exploration and acting on the battle action menu's choice. The rules pick
// the modes; the host performs every effect on actors, the HUD, the room and
// the menu.
namespace VS::Field {
// Field_controlMode values, named after the functions that select them.
enum class FieldControlMode : std::int32_t {
  kNone = 0,
  kExploration = 1,             // EnterFieldExplorationMode
  kBattleAction = 2,            // VS::Battle::LiveBeginActorActionSelection
  kCutscene = 3,                // Event_BeginQueuedCutscene
  kStandaloneEffect = 4,        // Effects_BeginAshleyStandaloneEffect
  kActionMenu = 5,              // BeginBattleActionMenu
  kShortcutMenu = 6,            // BeginShortcutMenu
  kFirstPersonView = 7,         // Field::BeginFirstPersonView
  kTrapInteraction = 8,         // Field_BeginTrapInteraction
  kChestInteraction = 9,        // Field_BeginChestInteraction
  kWeaponToggle = 10,           // Battle_BeginAshleyWeaponToggle
  kObjectInteractionWait = 11,  // Field_BeginObjectInteractionWait
  kGimmickMotion = 12,          // Battle_WaitForPostActionMovement
};

// Battle_actionFlowMode values the field modes select.
inline constexpr int kActionFlowIdle = 0;
inline constexpr int kActionFlowInitialTargetSelection = 1;

// Battle_SetHudBattleMode bits: the weapon is drawn, the action menu is open.
inline constexpr int kHudWeaponDrawn = 1;
inline constexpr int kHudActionMenu = 2;

// The actors other than Ashley whose poses a pause settles.
inline constexpr int kFirstPausedActor = 1;
inline constexpr int kPausedActorEnd = 16;

// A room palette or scene colour transform (Render/PaletteAnimationLive.hpp).
struct ColorTransform {
  int mode = 0;
  int duration = 0;
  std::int16_t red_offset = 0;
  std::int16_t green_offset = 0;
  std::int16_t blue_offset = 0;

  friend bool operator==(const ColorTransform&,
                         const ColorTransform&) = default;
};

// The menu darkens the room palettes; resuming restores the scene colours.
inline constexpr ColorTransform kMenuRoomPaletteTransform{13, 4, -4, -4, 0};
inline constexpr ColorTransform kResumeSceneColorTransform{8, 4, 0, 0, 0};

// What the battle action menu chose (Menu_Framework_ActionSelection).
enum class ActionMenuChoice : std::int16_t {
  kBreakArt = 6,
  kSpell = 7,
  kItem = 8,
  kTeleport = 10,
};

struct ActionMenuResult {
  std::int16_t kind = 0;
  // The ability to execute for a break art, spell or item.
  std::uint16_t action_id = 0;
  // The teleport spell's destination and cost.
  int location_id = 0;
  int tile_x = 0;
  int tile_z = 0;
  int mp_cost = 0;
};

// The teleport spell's script flag, transition and closing pose.
inline constexpr int kTeleportScriptFlag = 0xB7;
inline constexpr int kTeleportEffect = 0;
inline constexpr int kTeleportSpeed = 8;
inline constexpr int kTeleportActionPose = 7;

class FieldModeHost {
 public:
  FieldModeHost() = default;
  FieldModeHost(const FieldModeHost&) = delete;
  FieldModeHost& operator=(const FieldModeHost&) = delete;
  FieldModeHost(FieldModeHost&&) = delete;
  FieldModeHost& operator=(FieldModeHost&&) = delete;
  virtual ~FieldModeHost() = default;

  virtual void SetControlMode(FieldControlMode mode) = 0;
  virtual void SetActionFlowMode(int mode) = 0;
  virtual void SetInteractionPaused(bool paused) = 0;
  virtual void MarkCurrentRoomVisited() = 0;

  virtual void RestoreActorIdlePose(int actor_id) = 0;
  virtual void FreezeActorAnimations() = 0;
  virtual void UnfreezeActorAnimations() = 0;
  virtual void SetActorMarkersHidden(bool hidden) = 0;
  virtual void SetMinimapVisible(bool visible) = 0;
  virtual void TransformRoomPalettes(const ColorTransform& transform) = 0;
  virtual void TransformSceneColors(const ColorTransform& transform) = 0;
  virtual void SetClutAnimationEnabled(bool enabled) = 0;
  virtual void SetRoomAnimationsPaused(bool paused) = 0;
  virtual void SetMovingPlatformsPaused(bool paused) = 0;
  virtual void SetRoomAmbienceDucked(bool ducked) = 0;

  [[nodiscard]] virtual bool IsWeaponDrawn() const = 0;
  virtual void SetHudBattleMode(int mode) = 0;
  virtual void ReleaseMenuRows() = 0;
  virtual void ClearActionMenuStatus() = 0;
  virtual void BeginActorActionSelection(int actor_id,
                                         std::uint16_t action_id) = 0;
  virtual void SpawnTrapTileEffectAtAshley() = 0;
  virtual void SetScriptFlag(int flag, int value) = 0;
  virtual void TeleportToLocation(const TeleportRequest& request) = 0;
  [[nodiscard]] virtual std::int16_t AshleyMp() const = 0;
  virtual void SetAshleyMp(std::int16_t mp) = 0;
  virtual void PlayAshleyActionPose(int pose) = 0;
};

// decomp: BATTLE.PRG func_8006C2FC (146C.c:1866)
void PauseFieldForInteractionDialog(FieldModeHost& host);
// decomp: BATTLE.PRG func_8006C350 (146C.c:1880)
void PauseFieldForObjectInteraction(FieldModeHost& host);
// decomp: BATTLE.PRG func_8006C39C (146C.c:1893)
void PauseFieldForMenu(FieldModeHost& host);
// decomp: BATTLE.PRG func_8006C40C (146C.c:1907)
void ResumeFieldAfterMenu(FieldModeHost& host);
// decomp: BATTLE.PRG func_8006F5CC (146C.c:3221)
void EnterFieldExplorationMode(FieldModeHost& host);
// decomp: BATTLE.PRG func_8007352C (146C.c:4659)
void BeginShortcutMenu(FieldModeHost& host);
// decomp: BATTLE.PRG func_80073554 (146C.c:4665)
void EndShortcutMenu(FieldModeHost& host);
// decomp: BATTLE.PRG func_8007357C (146C.c:4671)
void BeginBattleActionMenu(FieldModeHost& host);
// decomp: BATTLE.PRG func_800735CC (146C.c:4679)
void CloseBattleActionMenu(FieldModeHost& host);
// decomp: BATTLE.PRG func_800735F8 (146C.c:4686)
// A break art, spell or item starts its action selection; anything else
// returns to exploration and closes the menu, the teleport spell first
// moving Ashley and paying its cost when she can.
void DispatchBattleActionMenuResult(FieldModeHost& host,
                                    const ActionMenuResult& result);
}  // namespace VS::Field
