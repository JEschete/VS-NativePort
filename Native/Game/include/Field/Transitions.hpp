#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorPlacement.hpp"
#include "Field/DoorRecord.hpp"
#include "Field/RoomState.hpp"
#include "Render/ScreenOverlay.hpp"

// The room and zone transition rules (BATTLE.PRG 146C.c:8748-8853 and
// INITBTL.PRG 18.c:468): starting the fades, leaving through a door,
// teleporting to a named location and the style the transition packets start
// with. The rules keep the transition and the pending door as values; the
// host queues the loads and performs the actor and door effects.
namespace VS::Field {
// The screen transition's progress (Field_screenTransitionStep).
enum class TransitionStep : std::uint32_t {
  kIdle = 0,
  kRoomExit = 1,          // BeginRoomTransition
  kZoneExit = 2,          // BeginZoneTransition
  kRoomEntryLoading = 3,  // the destination room is loading
  kRoomEntryReveal = 4,   // room load completed into exploration
  kFadeOut = 5,           // BeginScreenFadeOut
  kFadedOut = 6,
  kFadeIn = 7,  // BeginScreenFadeIn
};

// What scripts and the encounter loop see of the step.
enum class TransitionPhase : std::int32_t {
  kIdle = 0,
  kExiting = 1,
  kEntryLoading = 2,
  kEntryReveal = 3,
  kFadingOut = 4,
  kFadedOut = 5,
  kFadingIn = 6,
};

// The screen transition and the door the player leaves by.
struct RoomTransitionState {
  Render::ScreenTransition screen{};
  DoorRecord pending_door{};

  friend bool operator==(const RoomTransitionState&,
                         const RoomTransitionState&) = default;
};

// How a screen fade or wipe is drawn: the effect, its speed and the wipe's
// angle.
struct ScreenTransitionStyle {
  int effect = 0;
  int speed = 0;
  int wipe_angle = 0;
};

// A teleport's destination tile in the location's room.
struct TeleportRequest {
  int location_id = 0;
  int tile_x = 0;
  int tile_z = 0;
  int effect = 0;
  int speed = 0;
};

class TransitionHost {
 public:
  TransitionHost() = default;
  TransitionHost(const TransitionHost&) = delete;
  TransitionHost& operator=(const TransitionHost&) = delete;
  TransitionHost(TransitionHost&&) = delete;
  TransitionHost& operator=(TransitionHost&&) = delete;
  virtual ~TransitionHost() = default;

  [[nodiscard]] virtual int CurrentZoneId() const = 0;
  // Nothing for an id outside the location table.
  [[nodiscard]] virtual std::optional<LocationDestination> Location(
      int location_id) const = 0;
  // Nothing when Ashley has no placement.
  [[nodiscard]] virtual std::optional<Actors::ActorPlacementView>
  AshleyPlacement() const = 0;
  // The transition and pending door are current while these run.
  virtual void QueueRoomLoad(int room_id) = 0;
  virtual void QueueZoneLoad(int zone_id) = 0;
  virtual void RestoreAshleyIdlePose() = 0;
  virtual void FreezeActorAnimations() = 0;
  virtual void BypassAshleyCollision() = 0;
  virtual void ClearEntranceDoor() = 0;
  virtual void FindEntranceDoorNearTile(int tile_x, int tile_z,
                                        int floor_layer) = 0;
  virtual void ReportRefusedLocation(int location_id) = 0;
};

// decomp: BATTLE.PRG func_8007C398 (146C.c:8748)
[[nodiscard]] TransitionPhase ScreenTransitionPhase(TransitionStep step);
// decomp: BATTLE.PRG func_8007C444 (146C.c:8774)
void ConfigureScreenTransition(Render::ScreenTransition& screen,
                               const ScreenTransitionStyle& style);
// decomp: BATTLE.PRG func_8007C460 (146C.c:8781)
void SetTransitionWipeAngle(Render::ScreenTransition& screen, int wipe_angle);
// decomp: BATTLE.PRG func_8007C46C (146C.c:8783)
// A room or zone exit already under way keeps its step.
void BeginScreenFadeOut(Render::ScreenTransition& screen,
                        const ScreenTransitionStyle& style);
// decomp: BATTLE.PRG func_8007C4AC (146C.c:8795)
void BeginScreenFadeIn(Render::ScreenTransition& screen,
                       const ScreenTransitionStyle& style);
// decomp: BATTLE.PRG func_8007C4E0 (146C.c:8805)
// Leaves through `door` to a room of the current zone.
void BeginRoomTransition(RoomTransitionState& state, TransitionHost& host,
                         const DoorRecord& door, int effect, int speed);
// decomp: BATTLE.PRG func_8007C580 (146C.c:8820)
// Then takes the door under Ashley as the entrance.
void BeginScriptedRoomTransition(RoomTransitionState& state,
                                 TransitionHost& host, const DoorRecord& door,
                                 int effect, int speed);
// decomp: BATTLE.PRG func_8007C5C0 (146C.c:8830)
void BeginZoneTransition(RoomTransitionState& state, TransitionHost& host,
                         const DoorRecord& door, int effect, int speed);
// decomp: BATTLE.PRG func_8007C654 (146C.c:8843)
void BeginScriptedZoneTransition(RoomTransitionState& state,
                                 TransitionHost& host, const DoorRecord& door,
                                 int effect, int speed);
// decomp: BATTLE.PRG func_8007C694 (146C.c:8853)
// Moves Ashley to a tile of the location's room, keeping her facing; a
// location outside the table is refused and changes nothing.
bool TeleportToLocation(RoomTransitionState& state, TransitionHost& host,
                        const TeleportRequest& request);

// The transition packets' starting colours and blend
// (INITBTL.PRG _initTransitionState, 18.c:468).
struct TransitionColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const TransitionColor&,
                         const TransitionColor&) = default;
};

struct TransitionPrimitiveStyle {
  std::int16_t screen_width = 0;
  std::int16_t screen_height = 0;
  TransitionColor fade_color{};
  bool fade_semi_transparent = false;
  TransitionColor wipe_color{};
  bool wipe_semi_transparent = false;
  // The edge gradient: its left vertices lit, its right ones dark.
  TransitionColor edge_lit_color{};
  TransitionColor edge_dark_color{};
  bool edge_semi_transparent = false;
  // The draw mode's blend (subtractive) and dithering.
  std::uint8_t blend_mode = 0;
  bool dithered = false;
  std::int32_t initial_alpha = 0;

  friend bool operator==(const TransitionPrimitiveStyle&,
                         const TransitionPrimitiveStyle&) = default;
};

inline constexpr TransitionPrimitiveStyle kTransitionPrimitiveStyle{
    .screen_width = 320,
    .screen_height = 240,
    .fade_color = {0, 0, 0},
    .fade_semi_transparent = true,
    .wipe_color = {0, 0, 1},
    .wipe_semi_transparent = false,
    .edge_lit_color = {255, 255, 255},
    .edge_dark_color = {0, 0, 0},
    .edge_semi_transparent = true,
    .blend_mode = 2,
    .dithered = true,
    .initial_alpha = Render::kScreenTransitionOpaque,
};

class TransitionPrimitiveTarget {
 public:
  TransitionPrimitiveTarget() = default;
  TransitionPrimitiveTarget(const TransitionPrimitiveTarget&) = delete;
  TransitionPrimitiveTarget& operator=(const TransitionPrimitiveTarget&) =
      delete;
  TransitionPrimitiveTarget(TransitionPrimitiveTarget&&) = delete;
  TransitionPrimitiveTarget& operator=(TransitionPrimitiveTarget&&) = delete;
  virtual ~TransitionPrimitiveTarget() = default;

  virtual void InitializeFade(const TransitionPrimitiveStyle& style) = 0;
  virtual void InitializeWipe(const TransitionPrimitiveStyle& style) = 0;
  virtual void InitializeEdge(const TransitionPrimitiveStyle& style) = 0;
  virtual void InitializeDrawMode(const TransitionPrimitiveStyle& style) = 0;
  virtual void SetTransitionAlpha(std::int32_t alpha) = 0;
};

// decomp: INITBTL.PRG _initTransitionState (18.c:468)
// Sets up the fade, wipe and edge packets and their draw mode, then covers
// the screen.
void InitializeRoomTransitionPrimitives(
    TransitionPrimitiveTarget& target,
    const TransitionPrimitiveStyle& style = kTransitionPrimitiveStyle);
}  // namespace VS::Field
