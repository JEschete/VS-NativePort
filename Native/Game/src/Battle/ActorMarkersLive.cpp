// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActorMarkersLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/SkeletonLive.hpp"
#include "Battle/ActorMarkers.hpp"
#include "Battle/Feedback.h"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Render/Render.hpp"
#include "Render/Sprites.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Battle {
namespace {
constexpr int kAnchorTag = 251;
constexpr int kAnchorPositionTag = 255;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

std::uint64_t refusals = 0;

// A refused draw is dropped, as the sprite queue drops a packet it has no
// room for; the warning is logged at powers of two so it cannot flood.
void Refuse(const char* what) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("battle.markers",
                      "Actor marker drawing refused: {} ({} refusals)", what,
                      refusals);
  }
}

[[nodiscard]] u_long* SceneEntry(int slot) {
  u_long* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
  if (entry == nullptr) {
    Refuse("no current scene ordering-table entry");
  }
  return entry;
}

[[nodiscard]] u_long* Reserve(std::size_t words) {
  u_long* const storage = Render::ReserveLivePrimitiveWords(words);
  if (storage == nullptr) {
    Refuse("insufficient primitive capacity");
  }
  return storage;
}

[[nodiscard]] bool Emit(u_long* chain, std::span<const std::uint32_t> packet) {
  Render::LivePacketQueue queue(chain);
  if (!queue.Emit(packet)) {
    Refuse("packet refused by the current frame owner");
    return false;
  }
  return true;
}

class LiveActorMarkerWorld final : public ActorMarkerWorld {
 public:
  std::optional<ProjectedMarkerAnchor> ProjectActorAnchor(int actor) override {
    SVECTOR screen{};
    if (Actors::ProjectLiveActorTaggedBone(actor, kAnchorTag, &screen, 0) !=
        0) {
      return std::nullopt;
    }
    return ProjectedMarkerAnchor{.screen = {.x = screen.vx, .y = screen.vy},
                                 .depth = screen.vz};
  }
  std::optional<MarkerWorldPoint> ActorAnchorPosition(int actor) override {
    SVECTOR position{};
    if (Actors::LiveActorTaggedBonePosition(actor, kAnchorPositionTag,
                                            &position, 0) != 0) {
      return std::nullopt;
    }
    return MarkerWorldPoint{
        .x = position.vx, .y = position.vy, .z = position.vz};
  }
  MarkerScreenPoint ProjectThroughView(MarkerWorldPoint point) override {
    Render::LoadLiveFrameViewMatrix();
    SVECTOR vector{.vx = point.x, .vy = point.y, .vz = point.z, .pad = 0};
    Core_GteWord screen = 0;
    Core_GteWord depth = 0;
    Core_GteWord flag = 0;
    static_cast<void>(Core_Gte_RotTransPers(&vector, &screen, &depth, &flag));
    return UnpackMarkerPoint(static_cast<std::uint32_t>(screen));
  }
  int GimmickSubtype(int actor) override {
    return VS::Field::GimmickSubtypeEntry(actor);
  }
  bool DrawSprite(const ActorMarkerSprite& sprite) override {
    u_long* const chain = SceneEntry(sprite.ordering_slot);
    if (chain == nullptr || Reserve(Render::kSpritePacketWords) == nullptr) {
      return false;
    }
    u_long* const packet = Render::QueueLiveSprite(
        Render::kDefaultSpriteBrightness,
        static_cast<int>(PackMarkerPoint(sprite.position)),
        static_cast<int>(PackMarkerSize(sprite.size)), chain);
    if (packet == nullptr) {
      Refuse("sprite has no packet");
      return false;
    }
    if (sprite.draw_mode.has_value()) {
      packet[Render::kSpriteDrawModeWord] = *sprite.draw_mode;
    }
    packet[Render::kSpriteTextureWord] = PackMarkerTexture(sprite.texture);
    return true;
  }
  bool DrawFrame(const ActorMarkerFrame& frame) override {
    u_long* const chain = SceneEntry(frame.ordering_slot);
    if (chain == nullptr || Reserve(kMarkerFramePacketWords) == nullptr) {
      return false;
    }
    return Emit(chain, BuildActorMarkerFramePacket(
                           frame, static_cast<std::uint32_t>(*chain)));
  }
  bool DrawArrow(const OffscreenActorArrow& arrow) override {
    u_long* const chain = SceneEntry(kOffscreenArrowOrderingSlot);
    if (chain == nullptr) {
      return false;
    }
    u_long* const reserved = Reserve(kMarkerFramePacketWords);
    if (reserved == nullptr) {
      return false;
    }
    const std::span<const u_long> storage(reserved, kMarkerFramePacketWords);
    MarkerFramePacket packet{};
    std::ranges::copy(storage, packet.begin());
    WriteOffscreenArrowPacket(packet, arrow,
                              static_cast<std::uint32_t>(*chain));
    return Emit(chain, packet);
  }
};

ActorMarkerState ReadMarkers() {
  ActorMarkerState state;
  const auto kinds = std::span(Battle_actorMarkers).first<kActorMarkerCount>();
  std::ranges::transform(kinds, state.kinds.begin(), [](char kind) {
    return static_cast<std::uint8_t>(kind);
  });
  state.sides = static_cast<std::uint32_t>(Battle_actorMarkerSides);
  state.hidden = Battle_actorMarkersHidden;
  return state;
}

void WriteMarkers(const ActorMarkerState& state) {
  const auto kinds = std::span(Battle_actorMarkers).first<kActorMarkerCount>();
  std::ranges::transform(state.kinds, kinds.begin(), [](std::uint8_t kind) {
    return static_cast<char>(kind);
  });
  Battle_actorMarkerSides = static_cast<int>(state.sides);
}
}  // namespace

// A draw the frame cannot take was reported by the world and is dropped; the
// markers already drawn and the side bits stay.
// decomp: F00731 func_80095C18 (src/BATTLE/BATTLE.PRG/2842C.c:2399).
void LiveDrawActorMarker(int actor, int kind) {
  if (actor < 0 || static_cast<std::size_t>(actor) >= kActorMarkerCount) {
    Refuse("actor slot outside the sixteen marker slots");
    return;
  }
  LiveActorMarkerWorld world;
  ActorMarkerState state = ReadMarkers();
  static_cast<void>(DrawActorMarker(world, state, actor, kind));
  WriteMarkers(state);
}

// decomp: F00733 func_8009627C (src/BATTLE/BATTLE.PRG/2842C.c:2582).
void LiveDrawActorMarkers() {
  LiveActorMarkerWorld world;
  ActorMarkerState state = ReadMarkers();
  static_cast<void>(DrawActorMarkers(world, state));
  WriteMarkers(state);
}

// decomp: F00737 func_8009651C (src/BATTLE/BATTLE.PRG/2842C.c:2642).
void LiveDrawOffscreenActorArrow(SVECTOR* position, int direction) {
  if (position == nullptr) {
    Refuse("off-screen arrow without a position");
    return;
  }
  LiveActorMarkerWorld world;
  static_cast<void>(DrawOffscreenActorArrow(
      world, {.x = position->vx, .y = position->vy}, direction));
}

// decomp: F00740 func_80096FF0 (src/BATTLE/BATTLE.PRG/2842C.c:2838).
void LiveShowAbilityTimingCue(int cue_duration_frames) {
  LiveActorMarkerWorld world;
  SVECTOR& position = Battle_abilityTimingCuePosition;
  AbilityTimingCue cue{
      .anchor = {.screen = {.x = position.vx, .y = position.vy},
                 .depth = position.vz},
      .frames = Battle_abilityTimingCueFrames};
  ShowAbilityTimingCue(world, VS::Input::settings.ability_timing_display != 0,
                       cue, cue_duration_frames);
  position.vx = cue.anchor.screen.x;
  position.vy = cue.anchor.screen.y;
  position.vz = cue.anchor.depth;
  Battle_abilityTimingCueFrames = cue.frames;
}
}  // namespace VS::Battle
