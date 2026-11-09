#include "Field/GimmickDynamics.hpp"

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {
constexpr std::size_t kXAxis = 0;
constexpr std::size_t kYAxis = 1;
constexpr std::size_t kZAxis = 2;
constexpr std::int32_t kAngleMask = 4095;
constexpr std::int32_t kTileSize = 128;
constexpr std::int32_t kTileCenter = 64;
constexpr std::int16_t kSlideMode = 255;
constexpr std::int16_t kSlowSlideMode = 254;
constexpr std::int16_t kSpinMode = 253;
constexpr std::int16_t kFallMode = 252;
constexpr std::int16_t kTiltMode = 251;
constexpr std::int16_t kBounceMode = 250;
constexpr std::int16_t kRollMode = 249;
constexpr std::int16_t kBreakMode = 248;
constexpr std::int16_t kSquashMode = 247;
constexpr std::int16_t kGravity = 3;
constexpr std::uint8_t kKindMask = 7;
constexpr std::uint8_t kStackReactionKind = 5;
constexpr std::uint8_t kDurableKind = 1;
constexpr std::uint8_t kSameKindReaction = 2;
constexpr std::uint8_t kDifferentKindReaction = 1;
constexpr std::uint16_t kLandingSound = 0x30;
constexpr std::uint16_t kDifferentKindSound = 0x31;

std::int16_t AddHalfword(std::int16_t field, std::int32_t step) {
  return Core::Math::WrapSigned16(Core::Math::WrappingAdd(field, step));
}

void PositionTiles(GimmickDynamicsState& state) {
  state.tile_x =
      static_cast<std::uint8_t>(state.reaction.position[kXAxis] / kTileSize);
  state.tile_z =
      static_cast<std::uint8_t>(state.reaction.position[kZAxis] / kTileSize);
}

bool ReleaseCurrentTile(GimmickDynamicsHost& host) {
  const auto state = host.State();
  return state && host.ReleaseTile({state->tile_x, state->tile_z});
}

bool CarryChildren(GimmickDynamicsHost& host) {
  for (std::size_t slot = 0; slot < kGimmickQuerySlots; ++slot) {
    const auto child = host.Child(slot);
    const auto parent = host.State();
    if (!parent) return false;
    if (!child || child->skipped || child->carrier != parent->reaction.slot) {
      continue;
    }
    const auto height = AddHalfword(child->reaction.position[kYAxis],
                                    parent->reaction.vertical_delta);
    const auto carrier =
        parent->reaction.mode == 0 ? kUnattachedGimmick : child->carrier;
    if (!host.StoreChild(slot, height, carrier)) return false;
  }
  return true;
}

bool PlayCurrentSound(GimmickDynamicsHost& host, std::uint16_t effect) {
  const auto state = host.State();
  return state && host.PlaySound(effect, state->reaction.position);
}

bool SetLandingMode(GimmickDynamicsHost& host) {
  auto state = host.State();
  if (!state) return false;
  if ((state->kind & kKindMask) == kDurableKind) {
    state->durability = static_cast<std::uint8_t>(state->durability - 1);
    if (state->durability == 0) {
      state->reaction.mode = kBreakMode;
      state->reaction.frame = 0;
      return host.Store(*state);
    }
    if (!host.Store(*state)) return false;
    state = host.State();
    if (!state) return false;
  }
  const auto mode = state->reaction.mode;
  if (mode == kSlideMode || mode == kRollMode) {
    state->reaction.mode = 0;
  } else {
    if (!PlayCurrentSound(host, kLandingSound)) return false;
    state = host.State();
    if (!state) return false;
    state->reaction.mode = mode == kSlowSlideMode ? kTiltMode : kBounceMode;
    state->reaction.frame = 0;
  }
  return host.Store(*state) && CarryChildren(host);
}

std::optional<bool> LandOnReactionStack(GimmickDynamicsHost& host) {
  auto state = host.State();
  if (!state) return std::nullopt;
  if ((state->kind & kKindMask) < kStackReactionKind) return false;
  state->interaction = 0;
  if (!host.Store(*state)) return std::nullopt;
  const auto kind = host.TopKind({state->tile_x, state->tile_z});
  state = host.State();
  if (!state) return std::nullopt;
  if (!kind || (*kind & kKindMask) < kStackReactionKind) return false;
  const auto same = (*kind & kKindMask) == (state->kind & kKindMask);
  state->interaction = same ? kSameKindReaction : kDifferentKindReaction;
  if (same) {
    state->reaction.mode = kSpinMode;
    state->reaction.frame = 0;
  }
  if (!host.Store(*state) ||
      !PlayCurrentSound(host, same ? kLandingSound : kDifferentKindSound)) {
    return std::nullopt;
  }
  if (!same) {
    state = host.State();
    if (!state) return std::nullopt;
    state->reaction.mode = 0;
    if (!host.Store(*state) || !host.AddOccupant() ||
        !ReleaseCurrentTile(host)) {
      return std::nullopt;
    }
  }
  return CarryChildren(host) ? std::optional<bool>{true} : std::nullopt;
}

bool FallAndLand(GimmickDynamicsHost& host) {
  auto state = host.State();
  if (!state) return false;
  PositionTiles(*state);
  if (!host.Store(*state)) return false;
  const auto support = host.SupportHeight(state->reaction.position, 2);
  state = host.State();
  if (!support || !state) return false;
  if (state->reaction.position[kYAxis] < *support) {
    state->reaction.vertical_delta =
        AddHalfword(state->reaction.vertical_delta, kGravity);
    state->reaction.position[kYAxis] = AddHalfword(
        state->reaction.position[kYAxis], state->reaction.vertical_delta);
    if (state->reaction.position[kYAxis] < *support) {
      state->reaction.mode = kFallMode;
      return host.Store(*state) && CarryChildren(host);
    }
  }
  const auto previous_y =
      static_cast<std::uint16_t>(state->reaction.position[kYAxis]);
  state->reaction.position[kYAxis] = Core::Math::WrapSigned16(*support);
  state->reaction.vertical_delta =
      Core::Math::WrapSigned16(Core::Math::WrappingAdd(
          state->reaction.vertical_delta,
          Core::Math::WrappingSubtract(*support, previous_y)));
  if (!host.Store(*state)) return false;
  const auto stack = LandOnReactionStack(host);
  if (!stack) return false;
  if (*stack) return true;
  state = host.State();
  if (!state) return false;
  PositionTiles(*state);
  return host.Store(*state) && host.AddOccupant() && ReleaseCurrentTile(host) &&
         SetLandingMode(host);
}

void HalfSlideAxis(std::int16_t& position, std::int16_t& remaining) {
  const auto step = static_cast<std::int32_t>(remaining) / 2;
  remaining = AddHalfword(remaining, -step);
  position = AddHalfword(position, step);
  if (remaining >= -1 && remaining < 2) {
    position = AddHalfword(position, remaining);
    remaining = 0;
  }
}

bool HalfSlide(GimmickDynamicsHost& host, GimmickDynamicsState state) {
  if (state.slide[0] == 0 && state.slide[1] == 0) {
    state.reaction.frame = 0;
    return host.Store(state) && FallAndLand(host);
  }
  state.reaction.frame = 1;
  HalfSlideAxis(state.reaction.position[kXAxis], state.slide[0]);
  HalfSlideAxis(state.reaction.position[kZAxis], state.slide[1]);
  return host.Store(state);
}

bool SlowSlide(GimmickDynamicsHost& host, GimmickDynamicsState state) {
  if ((static_cast<std::uint16_t>(state.reaction.position[kXAxis]) &
       (kTileSize - 1)) == kTileCenter &&
      (static_cast<std::uint16_t>(state.reaction.position[kZAxis]) &
       (kTileSize - 1)) == kTileCenter) {
    const auto step = state.reaction.orientation < 2 ? -kTileSize : kTileSize;
    auto point = state.reaction.position;
    const auto axis = (state.reaction.orientation & 1) != 0 ? kXAxis : kZAxis;
    point[axis] = AddHalfword(point[axis], step);
    if (!host.PointAllowed(
            {point[kXAxis], point[kZAxis], state.reaction.position[kYAxis]})) {
      return FallAndLand(host);
    }
    const auto current = host.State();
    if (!current) return false;
    const auto height = host.SupportHeight(current->reaction.position, 1);
    const auto updated = host.State();
    if (!updated || !height) return false;
    if (*height != updated->reaction.position[kYAxis]) return FallAndLand(host);
    state = *updated;
    state.tile_x = state.tile_z = kUnattachedGimmick;
    if (!host.Store(state)) return false;
  }
  const auto current = host.State();
  if (!current) return false;
  state = *current;
  state.reaction.position[kXAxis] =
      AddHalfword(state.reaction.position[kXAxis], state.slide[0]);
  state.reaction.position[kZAxis] =
      AddHalfword(state.reaction.position[kZAxis], state.slide[1]);
  return host.Store(state);
}

bool CountdownMove(GimmickDynamicsHost& host, GimmickDynamicsState state) {
  const auto count = static_cast<std::int32_t>(state.reaction.mode);
  const std::array<std::int16_t, kGimmickReactionAxes> remaining{
      state.slide[0], state.reaction.vertical_delta, state.slide[1]};
  std::array<std::int16_t, kGimmickReactionAxes> next{};
  for (std::size_t axis = 0; axis < next.size(); ++axis) {
    const auto step = static_cast<std::int32_t>(remaining[axis]) / count;
    next[axis] = AddHalfword(remaining[axis], -step);
    state.reaction.position[axis] =
        AddHalfword(state.reaction.position[axis], step);
  }
  state.slide = {next[kXAxis], next[kZAxis]};
  state.reaction.vertical_delta = next[kYAxis];
  state.reaction.mode = AddHalfword(state.reaction.mode, -1);
  if (!host.Store(state)) return false;
  return state.reaction.mode != 0 || state.parent != kUnattachedGimmick ||
         FallAndLand(host);
}
}  // namespace

// decomp: BATTLE.PRG func_800B0908 (44F14.c:1717), 453 MIPS words.
bool UpdateGimmickMotion(GimmickDynamicsHost& host) {
  auto state = host.State();
  if (!state) return false;
  if (state->delay != 0) {
    state->delay = static_cast<std::uint8_t>(state->delay - 1);
    return host.Store(*state);
  }
  if (state->rotation_ticks != 0) {
    for (std::size_t axis = 0; axis < state->rotation_remaining.size();
         ++axis) {
      const auto step =
          static_cast<std::int32_t>(state->rotation_remaining[axis]) /
          state->rotation_ticks;
      state->rotation_remaining[axis] =
          AddHalfword(state->rotation_remaining[axis], -step);
      state->reaction.rotation[axis] = static_cast<std::int16_t>(
          Core::Math::WrappingAdd(state->reaction.rotation[axis], step) &
          kAngleMask);
    }
    state->rotation_ticks = AddHalfword(state->rotation_ticks, -1);
    if (!host.Store(*state)) return false;
    state = host.State();
    if (!state) return false;
  }
  switch (state->reaction.mode) {
    case 0:
      return true;
    case kSlideMode:
      return HalfSlide(host, *state);
    case kSlowSlideMode:
      return SlowSlide(host, *state);
    case kFallMode:
      return FallAndLand(host);
    case kSpinMode: {
      const auto result = host.React();
      if (result == GimmickReactionResult::kRefused) return false;
      return result != GimmickReactionResult::kComplete ||
             (host.AddOccupant() && host.AddOccupant() &&
              ReleaseCurrentTile(host));
    }
    case kBounceMode:
      return host.React() != GimmickReactionResult::kRefused &&
             CarryChildren(host);
    case kRollMode: {
      const auto result = host.React();
      if (result == GimmickReactionResult::kRefused) return false;
      return result != GimmickReactionResult::kComplete || FallAndLand(host);
    }
    case kTiltMode:
    case kBreakMode:
    case kSquashMode:
      return host.React() != GimmickReactionResult::kRefused;
    default:
      return CountdownMove(host, *state);
  }
}
}  // namespace VS::Field
