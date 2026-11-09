#include "Field/GimmickReactions.hpp"

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {
constexpr std::int16_t kSpinMode = 0xFD;
constexpr std::int16_t kTiltMode = 0xFB;
constexpr std::int16_t kBounceMode = 0xFA;
constexpr std::int16_t kRollMode = 0xF9;
constexpr std::int16_t kBreakMode = 0xF8;
constexpr std::int16_t kSquashMode = 0xF7;
constexpr std::uint8_t kLongReactionEnd = 14;
constexpr std::uint8_t kTiltEnd = 9;
constexpr std::uint8_t kBounceEnd = 3;
constexpr std::uint8_t kBreakRestoreFrame = 9;
constexpr std::uint8_t kBreakReleaseFrame = 40;
constexpr std::int16_t kBreakSinkStep = 3;
constexpr std::size_t kXAxis = 0;
constexpr std::size_t kYAxis = 1;
constexpr std::size_t kZAxis = 2;

void AddHalfword(std::int16_t& field, std::int32_t step) {
  field = Core::Math::WrapSigned16(static_cast<std::int32_t>(field) + step);
}

GimmickReactionResult StoreFrame(GimmickReactionState state, std::uint8_t frame,
                                 std::uint8_t end, GimmickReactionHost& host) {
  const auto complete = frame == end;
  if (complete) {
    state.mode = 0;
  } else {
    state.frame = static_cast<std::uint8_t>(frame + 1);
  }
  if (!host.Store(state)) return GimmickReactionResult::kRefused;
  return complete ? GimmickReactionResult::kComplete
                  : GimmickReactionResult::kAdvanced;
}

GimmickReactionResult SpinReaction(GimmickReactionState state,
                                   std::uint8_t frame,
                                   const GimmickReactionTables& tables,
                                   GimmickReactionHost& host) {
  if (frame >= tables.spin.size()) return GimmickReactionResult::kRefused;
  const auto& row = tables.spin[frame];
  AddHalfword(state.position[kYAxis], row.height);
  AddHalfword(state.rotation[kXAxis], row.pitch);
  AddHalfword(state.rotation[kYAxis], row.yaw);
  AddHalfword(state.rotation[kZAxis], row.roll);
  return StoreFrame(state, frame, kLongReactionEnd, host);
}

GimmickReactionResult TiltReaction(GimmickReactionState state,
                                   std::uint8_t frame,
                                   const GimmickReactionTables& tables,
                                   GimmickReactionHost& host) {
  if (frame >= tables.tilt.size()) return GimmickReactionResult::kRefused;
  const auto& row = tables.tilt[frame];
  AddHalfword(state.position[kZAxis], row.depth);
  const auto angle = state.orientation == 0 || state.orientation == 3
                         ? -static_cast<std::int32_t>(row.angle)
                         : row.angle;
  AddHalfword(state.rotation[(state.orientation & 1) != 0 ? kZAxis : kXAxis],
              angle);
  return StoreFrame(state, frame, kTiltEnd, host);
}

GimmickReactionResult RollReaction(GimmickReactionState state,
                                   std::uint8_t frame,
                                   const GimmickReactionTables& tables,
                                   GimmickReactionHost& host) {
  if (frame >= tables.roll.size()) return GimmickReactionResult::kRefused;
  const auto& row = tables.roll[frame];
  AddHalfword(state.position[kYAxis], row.height);
  const auto displacement = state.orientation >= 2
                                ? -static_cast<std::int32_t>(row.displacement)
                                : row.displacement;
  const auto angle = static_cast<std::uint32_t>(
                         static_cast<std::int32_t>(state.orientation) - 1) < 2
                         ? -static_cast<std::int32_t>(row.angle)
                         : row.angle;
  AddHalfword(state.position[(state.orientation & 1) != 0 ? kXAxis : kZAxis],
              displacement);
  AddHalfword(state.rotation[(state.orientation & 1) != 0 ? kZAxis : kXAxis],
              angle);
  if (frame != kLongReactionEnd) return StoreFrame(state, frame, 0, host);
  if (!host.Store(state) ||
      !host.ResetOrientation(state.slot, state.material)) {
    return GimmickReactionResult::kRefused;
  }
  const auto current = host.State();
  if (!current) return GimmickReactionResult::kRefused;
  return StoreFrame(*current, frame, kLongReactionEnd, host);
}

GimmickReactionResult BreakReaction(GimmickReactionState state,
                                    std::uint8_t frame,
                                    GimmickReactionHost& host) {
  if (frame < kBreakRestoreFrame) return StoreFrame(state, frame, 0, host);
  AddHalfword(state.position[kYAxis], kBreakSinkStep);
  if (!host.Store(state)) return GimmickReactionResult::kRefused;
  if (frame == kBreakRestoreFrame && !host.BeginBreaking()) {
    return GimmickReactionResult::kRefused;
  }
  if (frame == kBreakReleaseFrame) {
    return host.Release(state.slot) ? GimmickReactionResult::kComplete
                                    : GimmickReactionResult::kRefused;
  }
  const auto current = host.State();
  if (!current) return GimmickReactionResult::kRefused;
  return StoreFrame(*current, frame, 0, host);
}

GimmickReactionResult SquashReaction(GimmickReactionState state,
                                     std::uint8_t frame,
                                     const GimmickReactionTables& tables,
                                     GimmickReactionHost& host) {
  if (frame >= tables.squash.size()) return GimmickReactionResult::kRefused;
  const auto& row = tables.squash[frame];
  AddHalfword(state.position[kYAxis], row.height);
  for (std::size_t axis = 0; axis < state.scale.size(); ++axis) {
    state.scale[axis] =
        Core::Math::WrapSigned16(static_cast<std::int32_t>(row.scale[axis]));
  }
  return StoreFrame(state, frame, kLongReactionEnd, host);
}
}  // namespace

// decomp: BATTLE.PRG func_800B101C (44F14.c:1919), 236 MIPS words.
GimmickReactionResult UpdateGimmickReactionAnimation(
    const GimmickReactionTables& tables, GimmickReactionHost& host) {
  const auto state = host.State();
  if (!state) return GimmickReactionResult::kRefused;
  const auto frame = state->frame == 0 ? std::uint8_t{1} : state->frame;
  switch (state->mode) {
    case kSpinMode:
      return SpinReaction(*state, frame, tables, host);
    case kTiltMode:
      return TiltReaction(*state, frame, tables, host);
    case kRollMode:
      return RollReaction(*state, frame, tables, host);
    case kBreakMode:
      return BreakReaction(*state, frame, host);
    case kSquashMode:
      return SquashReaction(*state, frame, tables, host);
    case kBounceMode: {
      if (frame >= tables.bounce.size()) return GimmickReactionResult::kRefused;
      auto next = *state;
      next.vertical_delta = Core::Math::WrapSigned16(
          static_cast<std::int32_t>(tables.bounce[frame]));
      AddHalfword(next.position[kYAxis], next.vertical_delta);
      return StoreFrame(next, frame, kBounceEnd, host);
    }
    default:
      return StoreFrame(*state, frame, 0, host);
  }
}
}  // namespace VS::Field
