// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActorMarkers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle {
namespace {
constexpr int kHalfShift = 16;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr std::uint32_t kLinkMask = 0xFFFFFF;
constexpr int kByteShift = 8;

constexpr int kTalkMarker = 8;
constexpr int kPinnedMarker = 0x80;
constexpr int kPinnedKindMask = 7;
constexpr int kPinnedKindLimit = 4;
constexpr int kDefaultPinnedKind = 5;
constexpr int kSideMargin = 32;
constexpr int kDepthBias = 16;
constexpr int kNearestSlot = 64;
constexpr int kFarthestSlot = 2047;
// The awareness kinds the bubble switch draws.
constexpr unsigned kNoMarker = 0;
constexpr unsigned kOneDot = 1;
constexpr unsigned kTwoDots = 2;
constexpr unsigned kThreeDots = 3;
constexpr unsigned kAlertMarker = 4;
constexpr unsigned kQuestionMarker = 5;
constexpr unsigned kSmallFrameMarker = 6;
constexpr unsigned kExclamationMarker = 7;
constexpr std::uint32_t kKindCount = 8;

constexpr std::uint32_t kDotStride = 6;
constexpr MarkerSize kDotSize{.width = 5, .height = 5};
constexpr MarkerSize kTallIconSize{.width = 7, .height = 12};
constexpr MarkerSize kSquareIconSize{.width = 16, .height = 16};
constexpr std::uint32_t kDotTexture = 0x373E125A;
constexpr std::uint32_t kAlertTexture = 0x373E1198;
constexpr std::uint32_t kQuestionTexture = 0x373E2098;
constexpr std::uint32_t kQuestionOffset = 0xFFFD0002;
constexpr std::uint32_t kExclamationTexture = 0x373E11B7;
constexpr std::uint32_t kExclamationOffset = 0xFFEA0000;
constexpr std::uint32_t kExclamationDrawMode = 0xE1000017;
constexpr std::size_t kAlertSide = 2;
constexpr std::size_t kSmallFrameSide = 4;

constexpr int kScreenLeft = 16;
constexpr int kScreenRight = 304;
constexpr int kScreenTop = 40;
constexpr int kScreenTopReach = 8;
constexpr int kScreenBottom = 208;

constexpr int kNearTalkDistance = 352;
constexpr int kFarTalkDistance = 416;
constexpr int kTalkLift = 64;
constexpr int kTalkCenter = 8;
constexpr std::uint32_t kTalkTextureBase = 0x60;
constexpr int kTalkTextureShift = 4;
constexpr std::uint32_t kTalkTexture = 0x37FD2000;
constexpr std::uint32_t kTalkDrawMode = 0xE100000C;
constexpr std::uint8_t kTalkFrameMask = 7;

constexpr std::uint32_t kFrameTag = 0x09000000;
constexpr std::uint32_t kFrameColor = 0x2C808080;
constexpr std::uint32_t kFrameClut = 0x373E0000;
constexpr std::uint32_t kFramePage = 0x00170000;

constexpr std::uint32_t kArrowCommand = 0x2D000000;
constexpr std::uint32_t kArrowColorMask = 0x00FFFFFF;
constexpr std::uint32_t kArrowFirstTexture = 0x37FD3030;
constexpr std::uint32_t kArrowSecondTexture = 0x000C303F;
constexpr std::uint32_t kArrowThirdTexture = 0x3F30;
constexpr std::uint32_t kArrowFourthTexture = 0x3F3F;
constexpr std::uint32_t kKeepHighHalf = 0xFFFF0000;
constexpr std::size_t kArrowPoints = 8;
constexpr std::size_t kArrowStep = 2;
// The packet takes the quarter turns 0, 1, 3, 2 (a strip's vertex order).
using QuadTurns = std::array<std::size_t, kMarkerQuadVertices>;
constexpr QuadTurns kArrowPacketTurns{0, 1, 3, 2};
constexpr int kCueTop = 40;
constexpr int kCueRightLimit = 288;

enum ArrowWord : std::uint8_t {
  kArrowTag,
  kArrowColor,
  kArrowFirstPoint,
  kArrowFirstUv,
  kArrowSecondPoint,
  kArrowSecondUv,
  kArrowThirdPoint,
  kArrowThirdUv,
  kArrowFourthPoint,
  kArrowFourthUv,
};

struct FrameShape {
  std::array<MarkerScreenPoint, kMarkerQuadVertices> corners;
  std::array<std::uint16_t, kMarkerQuadVertices> uvs;
};

// decomp: D_80068F04 and D_80068F64 (src/BATTLE/BATTLE.PRG/2842C.c): the
// frame corners around the marker point and their texture coordinates.
constexpr std::array<FrameShape, 6> kFrameShapes{{
    {{{{-6, -22}, {18, -22}, {-6, 1}, {18, 1}}},
     {0x1040, 0x1058, 0x2740, 0x2758}},
    {{{{-17, -22}, {7, -22}, {-17, 1}, {7, 1}}},
     {0x1057, 0x103F, 0x2757, 0x273F}},
    {{{{-10, -29}, {20, -29}, {-10, 1}, {20, 1}}},
     {0x1179, 0x1197, 0x2F79, 0x2F97}},
    {{{{-19, -29}, {9, -29}, {-19, 1}, {9, 1}}},
     {0x1196, 0x1178, 0x2F96, 0x2F78}},
    {{{{-9, -16}, {11, -16}, {-9, -3}, {11, -3}}},
     {0x11A0, 0x11B4, 0x1EA0, 0x1EB4}},
    {{{{-10, -16}, {10, -16}, {-10, -3}, {10, -3}}},
     {0x11B3, 0x119F, 0x1EB3, 0x1E9F}},
}};

// decomp: D_80068F94 (src/BATTLE/BATTLE.PRG/2842C.c): the icon offsets.
constexpr std::array<MarkerScreenPoint, 6> kIconOffsets{{
    {-2, -16},
    {-14, -16},
    {0, -21},
    {-7, -21},
    {3, -11},
    {3, -11},
}};

// decomp: the arrow's stack tables at 0x80069354 and 0x80069374.
constexpr std::array<MarkerScreenPoint, kArrowPoints> kArrowPointOffsets{{
    {-8, -8},
    {0, -11},
    {7, -8},
    {10, 0},
    {7, 7},
    {0, 10},
    {-8, 7},
    {-11, 0},
}};

// The direction flags in the order of the arrow's eight orientations.
constexpr std::array<int, kArrowPoints> kArrowDirections{
    kOffscreenTop,    kOffscreenTop | kOffscreenRight,
    kOffscreenRight,  kOffscreenRight | kOffscreenBottom,
    kOffscreenBottom, kOffscreenBottom | kOffscreenLeft,
    kOffscreenLeft,   kOffscreenLeft | kOffscreenTop,
};

std::int16_t Wrap(int value) { return static_cast<std::int16_t>(value); }

MarkerScreenPoint Offset(MarkerScreenPoint point, MarkerScreenPoint offset) {
  return {.x = Wrap(point.x + offset.x), .y = Wrap(point.y + offset.y)};
}

std::uint32_t SideBit(int actor) {
  return std::uint32_t{1} << static_cast<unsigned>(actor);
}

void UpdateSide(ActorMarkerState& state, int actor, bool near, bool far) {
  if (near) state.sides &= ~SideBit(actor);
  if (far) state.sides |= SideBit(actor);
}

std::size_t Side(const ActorMarkerState& state, int actor) {
  return (state.sides >> static_cast<unsigned>(actor)) & 1U;
}

bool DrawIcon(ActorMarkerWorld& world, int slot, std::uint32_t packed,
              MarkerSize size, std::uint32_t texture) {
  return world.DrawSprite({.ordering_slot = slot,
                           .position = UnpackMarkerPoint(packed),
                           .size = size,
                           .texture = UnpackMarkerTexture(texture),
                           .draw_mode = std::nullopt});
}

bool DrawFrame(ActorMarkerWorld& world, int slot, MarkerScreenPoint point,
               std::size_t shape) {
  ActorMarkerFrame frame{.ordering_slot = slot};
  const auto& source = kFrameShapes.at(shape);
  for (std::size_t corner = 0; corner < frame.vertices.size(); ++corner) {
    const std::uint16_t uv = source.uvs.at(corner);
    frame.vertices.at(corner) = {
        .position = Offset(point, source.corners.at(corner)),
        .u = static_cast<std::uint8_t>(uv),
        .v = static_cast<std::uint8_t>(uv >> kByteShift)};
  }
  return world.DrawFrame(frame);
}

// The clamp of a marker kept at the screen edges; the edges it was clamped to.
int ClampToScreen(MarkerScreenPoint& point) {
  int flags = 0;
  if (point.x < kScreenLeft) {
    point.x = kScreenLeft;
    flags = kOffscreenLeft;
  }
  if (point.x > kScreenRight) {
    point.x = kScreenRight;
    flags |= kOffscreenRight;
  }
  if (flags != 0) {
    if (point.y < kScreenTop) {
      point.y = kScreenTop;
      flags |= kOffscreenTop;
    }
  } else if (point.y < kScreenTopReach) {
    point.y = kScreenTop;
    flags = kOffscreenTop;
  }
  if (point.y > kScreenBottom) {
    point.y = kScreenBottom;
    flags |= kOffscreenBottom;
  }
  return flags;
}

int AbsoluteHalf(int value) {
  const int half = Wrap(value);
  return half < 0 ? -half : half;
}

bool DrawTalkMarker(ActorMarkerWorld& world, ActorMarkerState& state,
                    int actor) {
  auto& kind = state.kinds.at(static_cast<std::size_t>(actor));
  const auto speaker = world.ActorAnchorPosition(actor);
  if (!speaker.has_value()) {
    kind = 0;
    return true;
  }
  const auto player = world.ActorAnchorPosition(0);
  if (!player.has_value()) return true;
  const int distance = AbsoluteHalf(player->x - speaker->x) +
                       AbsoluteHalf(player->z - speaker->z);
  const bool near = distance < kNearTalkDistance;
  const bool far = distance > kFarTalkDistance;
  UpdateSide(state, actor, near, far);
  if (Side(state, actor) != 0) return true;
  MarkerWorldPoint lifted = *speaker;
  lifted.y = Wrap(lifted.y - kTalkLift);
  const MarkerScreenPoint screen = Offset(
      world.ProjectThroughView(lifted), {.x = -kTalkCenter, .y = -kTalkCenter});
  const int subtype = world.GimmickSubtype(actor);
  if (subtype < 0) {
    kind = 0;
    return true;
  }
  if (subtype == 0) {
    kind = static_cast<std::uint8_t>(kind + 1);
    if ((kind & kTalkFrameMask) == 0) kind = 0;
  }
  const std::uint32_t texture =
      ((static_cast<std::uint32_t>(subtype) << kTalkTextureShift) +
       kTalkTextureBase) |
      kTalkTexture;
  return world.DrawSprite({.ordering_slot = 0,
                           .position = screen,
                           .size = kSquareIconSize,
                           .texture = UnpackMarkerTexture(texture),
                           .draw_mode = kTalkDrawMode});
}

int NormalizedKind(int kind) {
  if ((kind & kPinnedMarker) == 0) return kind;
  const int pinned = kind & kPinnedKindMask;
  return static_cast<unsigned>(pinned - 1) < kPinnedKindLimit
             ? pinned
             : kDefaultPinnedKind;
}

struct AwarenessPlacement {
  int slot = 0;
  std::size_t side = 0;
  MarkerScreenPoint point;
};

bool DrawAwarenessKind(ActorMarkerWorld& world,
                       const AwarenessPlacement& placement, unsigned kind) {
  const int slot = placement.slot;
  std::size_t side = placement.side;
  const std::uint32_t packed = PackMarkerPoint(placement.point);
  switch (kind) {
    case kNoMarker:
      return true;
    case kOneDot:
    case kTwoDots:
    case kThreeDots:
      for (std::uint32_t dot = kind; dot != 0;) {
        --dot;
        const std::uint32_t position =
            packed + dot * kDotStride + PackMarkerPoint(kIconOffsets.at(side));
        if (!DrawIcon(world, slot, position, kDotSize, kDotTexture)) {
          return false;
        }
      }
      break;
    case kAlertMarker: {
      MarkerScreenPoint clamped = placement.point;
      const int edges = ClampToScreen(clamped);
      if (edges != 0) return DrawOffscreenActorArrow(world, clamped, edges);
      side += kAlertSide;
      if (!DrawIcon(world, slot,
                    packed + PackMarkerPoint(kIconOffsets.at(side)),
                    kTallIconSize, kAlertTexture)) {
        return false;
      }
      break;
    }
    case kQuestionMarker:
      if (!DrawIcon(
              world, slot,
              packed + PackMarkerPoint(kIconOffsets.at(side)) + kQuestionOffset,
              kSquareIconSize, kQuestionTexture)) {
        return false;
      }
      break;
    case kSmallFrameMarker:
      side += kSmallFrameSide;
      break;
    case kExclamationMarker:
      return world.DrawSprite(
          {.ordering_slot = slot,
           .position = UnpackMarkerPoint(packed + kExclamationOffset),
           .size = kTallIconSize,
           .texture = UnpackMarkerTexture(kExclamationTexture),
           .draw_mode = kExclamationDrawMode});
    default:
      break;
  }
  return DrawFrame(world, slot, placement.point, side);
}

bool DrawAwarenessMarker(ActorMarkerWorld& world, ActorMarkerState& state,
                         int actor, int kind) {
  const auto anchor = world.ProjectActorAnchor(actor);
  if (!anchor.has_value()) return true;
  const auto player = world.ProjectActorAnchor(0);
  if (!player.has_value()) return true;
  const int depth = anchor->depth - kDepthBias;
  const int x = anchor->screen.x;
  const int player_x = player->screen.x;
  UpdateSide(state, actor, player_x + kSideMargin < x,
             x < player_x - kSideMargin);
  int slot = depth;
  if (slot < kNearestSlot) {
    slot = kNearestSlot;
  } else if (slot > kFarthestSlot) {
    slot = kFarthestSlot;
  }
  const unsigned selector = static_cast<unsigned>(kind);
  return DrawAwarenessKind(
      world,
      {.slot = slot, .side = Side(state, actor), .point = anchor->screen},
      selector < kKindCount ? selector : kKindCount);
}
}  // namespace

std::uint32_t PackMarkerPoint(MarkerScreenPoint point) {
  return static_cast<std::uint16_t>(point.x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(point.y))
          << kHalfShift);
}

MarkerScreenPoint UnpackMarkerPoint(std::uint32_t packed) {
  return {.x = static_cast<std::int16_t>(packed & kHalfMask),
          .y = static_cast<std::int16_t>(packed >> kHalfShift)};
}

std::uint32_t PackMarkerSize(MarkerSize size) {
  return size.width | (static_cast<std::uint32_t>(size.height) << kHalfShift);
}

std::uint32_t PackMarkerTexture(MarkerTexture texture) {
  return texture.u | (static_cast<std::uint32_t>(texture.v) << kByteShift) |
         (static_cast<std::uint32_t>(texture.clut) << kHalfShift);
}

MarkerTexture UnpackMarkerTexture(std::uint32_t packed) {
  return {.u = static_cast<std::uint8_t>(packed),
          .v = static_cast<std::uint8_t>(packed >> kByteShift),
          .clut = static_cast<std::uint16_t>(packed >> kHalfShift)};
}

MarkerFramePacket BuildActorMarkerFramePacket(const ActorMarkerFrame& frame,
                                              std::uint32_t next_link) {
  const auto& [v0, v1, v2, v3] = frame.vertices;
  const auto uv = [](const MarkerFrameVertex& vertex) {
    return vertex.u | (static_cast<std::uint32_t>(vertex.v) << kByteShift);
  };
  return {(next_link & kLinkMask) | kFrameTag, kFrameColor,
          PackMarkerPoint(v0.position),        uv(v0) | kFrameClut,
          PackMarkerPoint(v1.position),        uv(v1) | kFramePage,
          PackMarkerPoint(v2.position),        uv(v2),
          PackMarkerPoint(v3.position),        uv(v3)};
}

void WriteOffscreenArrowPacket(OffscreenArrowPacket storage,
                               const OffscreenActorArrow& arrow,
                               std::uint32_t next_link) {
  const auto& [first, second, third, fourth] = arrow.vertices;
  storage[kArrowTag] = (next_link & kLinkMask) | kFrameTag;
  storage[kArrowColor] =
      (storage[kArrowColor] & kArrowColorMask) | kArrowCommand;
  storage[kArrowFirstPoint] = PackMarkerPoint(first);
  storage[kArrowFirstUv] = kArrowFirstTexture;
  storage[kArrowSecondPoint] = PackMarkerPoint(second);
  storage[kArrowSecondUv] = kArrowSecondTexture;
  storage[kArrowThirdPoint] = PackMarkerPoint(third);
  storage[kArrowThirdUv] =
      (storage[kArrowThirdUv] & kKeepHighHalf) | kArrowThirdTexture;
  storage[kArrowFourthPoint] = PackMarkerPoint(fourth);
  storage[kArrowFourthUv] =
      (storage[kArrowFourthUv] & kKeepHighHalf) | kArrowFourthTexture;
}

// decomp: BATTLE.PRG func_80095C18 (F00731, src/BATTLE/BATTLE.PRG/2842C.c:2399,
// 0x80095C18-0x80096254)
bool DrawActorMarker(ActorMarkerWorld& world, ActorMarkerState& state,
                     int actor, int kind) {
  if (actor < 0 || static_cast<std::size_t>(actor) >= kActorMarkerCount) {
    return false;
  }
  if ((kind & kTalkMarker) != 0) return DrawTalkMarker(world, state, actor);
  return DrawAwarenessMarker(world, state, actor, NormalizedKind(kind));
}

// decomp: BATTLE.PRG func_8009627C (F00733, src/BATTLE/BATTLE.PRG/2842C.c:2582,
// 0x8009627C-0x800962E4)
bool DrawActorMarkers(ActorMarkerWorld& world, ActorMarkerState& state) {
  if (state.hidden != 0) return true;
  for (std::size_t actor = 1; actor < kActorMarkerCount; ++actor) {
    const std::uint8_t kind = state.kinds.at(actor);
    if (kind != 0 &&
        !DrawActorMarker(world, state, static_cast<int>(actor), kind)) {
      return false;
    }
  }
  return true;
}

// decomp: BATTLE.PRG func_8009651C (F00737, src/BATTLE/BATTLE.PRG/2842C.c:2642,
// 0x8009651C-0x80096768)
bool DrawOffscreenActorArrow(ActorMarkerWorld& world,
                             MarkerScreenPoint position, int direction) {
  std::size_t index = 0;
  while (index < kArrowDirections.size() &&
         kArrowDirections.at(index) != direction) {
    ++index;
  }
  if (index == kArrowDirections.size()) return true;
  const auto point = [&](std::size_t turn) {
    return Offset(position, kArrowPointOffsets.at((index + turn * kArrowStep) %
                                                  kArrowPoints));
  };
  OffscreenActorArrow arrow;
  for (std::size_t vertex = 0; vertex < kMarkerQuadVertices; ++vertex) {
    arrow.vertices.at(vertex) = point(kArrowPacketTurns.at(vertex));
  }
  return world.DrawArrow(arrow);
}

// decomp: BATTLE.PRG func_80096FF0 (F00740, src/BATTLE/BATTLE.PRG/2842C.c:2838,
// 0x80096FF0-0x800970BC)
void ShowAbilityTimingCue(ActorMarkerWorld& world, bool display_enabled,
                          AbilityTimingCue& cue, int duration_frames) {
  if (!display_enabled) return;
  const auto anchor = world.ProjectActorAnchor(0);
  if (!anchor.has_value()) return;
  cue.anchor = *anchor;
  auto& screen = cue.anchor.screen;
  if (screen.y < kCueTop) {
    const int lift = kCueTop - screen.y;
    screen.x = Wrap(screen.x + lift < kCueRightLimit ? screen.x + lift
                                                     : screen.x - lift);
    screen.y = Wrap(kCueTop + lift);
  }
  cue.frames = Wrap(cue.frames + duration_frames);
}
}  // namespace VS::Battle
