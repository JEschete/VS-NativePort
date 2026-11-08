// Hand-maintained (tools/port/owned.csv).
#include "Field/TransitionsLive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Core/Log.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/CollisionMapLive.hpp"
#include "Field/DoorRecordLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomState.hpp"

extern "C" {
#include "Field/Doors.h"
#include "Field/Locations.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
#include "Legacy/Psx.h"
#include "Render/ScreenTransition.h"

// decomp: vs_battle_screenTransitionStep
// (vs_battle_screenTransitionStep@BATTLE/BATTLE.PRG)
unsigned int Field_screenTransitionStep;
// decomp: vs_battle_screenTransitionEffect
// (vs_battle_screenTransitionEffect@BATTLE/BATTLE.PRG)
int Field_screenTransitionEffect;
// decomp: vs_battle_screenTransitionAlpha
// (vs_battle_screenTransitionAlpha@BATTLE/BATTLE.PRG)
int Field_screenTransitionAlpha;
// decomp: vs_battle_screenTransitionWipeAngle
// (vs_battle_screenTransitionWipeAngle@BATTLE/BATTLE.PRG)
int Field_screenTransitionWipeAngle;
// decomp: vs_battle_screenTransitionSpeed
// (vs_battle_screenTransitionSpeed@BATTLE/BATTLE.PRG)
int Field_screenTransitionSpeed;
}

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
constexpr int kAshley = 0;
// SetLiveActorCollisionBypass's flag: Ashley walks through the doorway.
constexpr int kBypassCollision = 1;
// The packets are double-buffered; the first is set up and copied over the
// second (18.c:468).
constexpr std::size_t kSetUpBuffer = 0;
constexpr std::size_t kCopiedBuffer = 1;
// The draw mode keeps the frame buffer's drawing area and uses no texture
// window (18.c:468 SetDrawMode(.., 0, 1, .., &zero)).
constexpr int kDrawToDisplayedArea = 0;

class LiveTransitionHost final : public TransitionHost {
 public:
  explicit LiveTransitionHost(RoomTransitionState& state) : state_(state) {}

  [[nodiscard]] int CurrentZoneId() const override {
    return Field_zoneContext.zndId;
  }
  [[nodiscard]] std::optional<LocationDestination> Location(
      int location_id) const override {
    if (location_id < 0 ||
        static_cast<std::size_t>(location_id) >= kLocationCount) {
      return std::nullopt;
    }
    const auto entry = static_cast<std::size_t>(location_id) * 2;
    return LocationDestination{
        .zone_id = static_cast<std::uint8_t>(Field_locationDestinations[entry]),
        .room_id =
            static_cast<std::uint8_t>(Field_locationDestinations[entry + 1]),
    };
  }
  [[nodiscard]] std::optional<Actors::ActorPlacementView> AshleyPlacement()
      const override {
    return Actors::LiveActorPlacementView(kAshley);
  }
  void QueueRoomLoad(int room_id) override {
    StoreLiveRoomTransition(state_);
    VS::Field::LiveQueueRoomLoad(room_id);
    state_ = LoadLiveRoomTransition();
  }
  void QueueZoneLoad(int zone_id) override {
    StoreLiveRoomTransition(state_);
    VS::Field::LiveQueueZoneLoad(zone_id);
    state_ = LoadLiveRoomTransition();
  }
  void RestoreAshleyIdlePose() override {
    Actors::RestoreLiveActorIdlePose(kAshley);
  }
  void FreezeActorAnimations() override { Actors::FreezeLiveActorAnimations(); }
  void BypassAshleyCollision() override {
    SetLiveActorCollisionBypass(kAshley, kBypassCollision);
  }
  void ClearEntranceDoor() override { SetLiveEntranceDoor(std::nullopt); }
  void FindEntranceDoorNearTile(int tile_x, int tile_z,
                                int floor_layer) override {
    FindLiveEntranceDoorNearTile(tile_x, tile_z, floor_layer);
  }
  void ReportRefusedLocation(int location_id) override {
    VS_LOG_WARN(kChannel, "teleport to location {} outside the {} locations",
                location_id, kLocationCount);
  }

 private:
  RoomTransitionState& state_;
};

// Runs `rule` over the owned transition and pending door.
template <typename Rule>
void RunTransition(Rule&& rule) {
  RoomTransitionState state = LoadLiveRoomTransition();
  LiveTransitionHost host(state);
  rule(state, host);
  StoreLiveRoomTransition(state);
}

// Runs `rule` over the owned screen transition.
template <typename Rule>
void RunScreenTransition(Rule&& rule) {
  RoomTransitionState state = LoadLiveRoomTransition();
  rule(state.screen);
  StoreLiveRoomTransition(state);
}

bool RefuseMissingDoor(const Field_MpdDoorRecord* door, const char* what) {
  if (door != nullptr) {
    return false;
  }
  VS_LOG_WARN(kChannel, "{} without a door", what);
  return true;
}

void SetColor(POLY_F4& packet, const TransitionColor& color) {
  setRGB0(&packet, color.red, color.green, color.blue);
}

template <typename Packet>
void CoverScreen(Packet& packet, const TransitionPrimitiveStyle& style) {
  setXY4(&packet, 0, 0, style.screen_width, 0, 0, style.screen_height,
         style.screen_width, style.screen_height);
}

int SemiTransparency(bool semi_transparent) { return semi_transparent ? 1 : 0; }

class LiveTransitionPrimitives final : public TransitionPrimitiveTarget {
 public:
  void InitializeFade(const TransitionPrimitiveStyle& style) override {
    POLY_F4& fade = Render_legacyTransitionFadePackets[kSetUpBuffer];
    SetPolyF4(&fade);
    SetColor(fade, style.fade_color);
    CoverScreen(fade, style);
    SetSemiTrans(&fade, SemiTransparency(style.fade_semi_transparent));
    Render_legacyTransitionFadePackets[kCopiedBuffer] = fade;
  }
  void InitializeWipe(const TransitionPrimitiveStyle& style) override {
    Render_LegacyTransitionWipePacket& wipe =
        Render_legacyTransitionWipePackets[kSetUpBuffer];
    SetPolyF4(&wipe.coveredQuad);
    SetSemiTrans(&wipe.coveredQuad,
                 SemiTransparency(style.wipe_semi_transparent));
    SetColor(wipe.coveredQuad, style.wipe_color);
    // The wipe's corners are placed each frame (ScreenOverlayLive.cpp).
    wipe.edgeQuad = wipe.coveredQuad;
    Render_legacyTransitionWipePackets[kCopiedBuffer] = wipe;
  }
  void InitializeEdge(const TransitionPrimitiveStyle& style) override {
    POLY_G4& edge = Render_legacyTransitionEdgePackets[kSetUpBuffer];
    const TransitionColor& lit = style.edge_lit_color;
    const TransitionColor& dark = style.edge_dark_color;
    SetPolyG4(&edge);
    setRGB0(&edge, lit.red, lit.green, lit.blue);
    setRGB2(&edge, lit.red, lit.green, lit.blue);
    setRGB1(&edge, dark.red, dark.green, dark.blue);
    setRGB3(&edge, dark.red, dark.green, dark.blue);
    CoverScreen(edge, style);
    SetSemiTrans(&edge, SemiTransparency(style.edge_semi_transparent));
    Render_legacyTransitionEdgePackets[kCopiedBuffer] = edge;
  }
  void InitializeDrawMode(const TransitionPrimitiveStyle& style) override {
    DR_MODE& mode = Render_legacyTransitionDrawModes[kSetUpBuffer];
    RECT no_texture_window{};
    SetDrawMode(&mode, kDrawToDisplayedArea, style.dithered ? 1 : 0,
                getTPage(0, style.blend_mode, 0, 0), &no_texture_window);
    Render_legacyTransitionDrawModes[kCopiedBuffer] = mode;
  }
  void SetTransitionAlpha(std::int32_t alpha) override {
    Field_screenTransitionAlpha = alpha;
  }
};
}  // namespace

RoomTransitionState LoadLiveRoomTransition() {
  return RoomTransitionState{
      .screen =
          Render::ScreenTransition{
              .alpha = Field_screenTransitionAlpha,
              .step = Field_screenTransitionStep,
              .effect = Field_screenTransitionEffect,
              .wipe_angle = Field_screenTransitionWipeAngle,
              .speed = Field_screenTransitionSpeed,
          },
      .pending_door = DoorRecordOf(Field_pendingDestinationDoor),
  };
}

void StoreLiveRoomTransition(const RoomTransitionState& state) {
  Field_screenTransitionAlpha = state.screen.alpha;
  Field_screenTransitionStep = state.screen.step;
  Field_screenTransitionEffect = state.screen.effect;
  Field_screenTransitionWipeAngle = state.screen.wipe_angle;
  Field_screenTransitionSpeed = state.screen.speed;
  StoreDoorRecord(state.pending_door, Field_pendingDestinationDoor);
}

int LiveScreenTransitionPhase() {
  return static_cast<int>(ScreenTransitionPhase(
      static_cast<TransitionStep>(Field_screenTransitionStep)));
}

void LiveConfigureScreenTransition(int effect, int speed, int wipe_angle) {
  RunScreenTransition([&](Render::ScreenTransition& screen) {
    ConfigureScreenTransition(
        screen, {.effect = effect, .speed = speed, .wipe_angle = wipe_angle});
  });
}

void LiveSetTransitionWipeAngle(int wipe_angle) {
  RunScreenTransition([&](Render::ScreenTransition& screen) {
    SetTransitionWipeAngle(screen, wipe_angle);
  });
}

void LiveBeginScreenFadeOut(int effect, int speed, int wipe_angle) {
  RunScreenTransition([&](Render::ScreenTransition& screen) {
    BeginScreenFadeOut(
        screen, {.effect = effect, .speed = speed, .wipe_angle = wipe_angle});
  });
}

void LiveBeginScreenFadeIn(int effect, int speed, int wipe_angle) {
  RunScreenTransition([&](Render::ScreenTransition& screen) {
    BeginScreenFadeIn(
        screen, {.effect = effect, .speed = speed, .wipe_angle = wipe_angle});
  });
}

int LiveBeginRoomTransition(const Field_MpdDoorRecord* door, int effect,
                            int speed) {
  if (RefuseMissingDoor(door, "room transition")) {
    return 0;
  }
  const DoorRecord destination = DoorRecordOf(*door);
  RunTransition([&](RoomTransitionState& state, TransitionHost& host) {
    BeginRoomTransition(state, host, destination, effect, speed);
  });
  return 1;
}

void LiveBeginScriptedRoomTransition(const Field_MpdDoorRecord* door,
                                     int effect, int speed) {
  if (RefuseMissingDoor(door, "scripted room transition")) {
    return;
  }
  const DoorRecord destination = DoorRecordOf(*door);
  RunTransition([&](RoomTransitionState& state, TransitionHost& host) {
    BeginScriptedRoomTransition(state, host, destination, effect, speed);
  });
}

int LiveBeginZoneTransition(const Field_MpdDoorRecord* door, int effect,
                            int speed) {
  if (RefuseMissingDoor(door, "zone transition")) {
    return 0;
  }
  const DoorRecord destination = DoorRecordOf(*door);
  RunTransition([&](RoomTransitionState& state, TransitionHost& host) {
    BeginZoneTransition(state, host, destination, effect, speed);
  });
  return 1;
}

void LiveBeginScriptedZoneTransition(const Field_MpdDoorRecord* door,
                                     int effect, int speed) {
  if (RefuseMissingDoor(door, "scripted zone transition")) {
    return;
  }
  const DoorRecord destination = DoorRecordOf(*door);
  RunTransition([&](RoomTransitionState& state, TransitionHost& host) {
    BeginScriptedZoneTransition(state, host, destination, effect, speed);
  });
}

int LiveTeleportToLocation(int location_id, int tile_x, int tile_z, int effect,
                           int speed) {
  bool moved = false;
  RunTransition([&](RoomTransitionState& state, TransitionHost& host) {
    moved = TeleportToLocation(state, host,
                               TeleportRequest{
                                   .location_id = location_id,
                                   .tile_x = tile_x,
                                   .tile_z = tile_z,
                                   .effect = effect,
                                   .speed = speed,
                               });
  });
  return moved ? 1 : 0;
}

void LiveInitializeRoomTransitionPrimitives() {
  LiveTransitionPrimitives target;
  InitializeRoomTransitionPrimitives(target);
}
}  // namespace VS::Field
