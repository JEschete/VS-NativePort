#include "Field/GimmickPlacement.hpp"

#include <algorithm>
#include <array>
#include <bit>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {

constexpr std::int32_t kHeightUnit = 128;
constexpr unsigned kHalfwordBits = 16;
constexpr std::int32_t kLowestGimmickBelowHeight = -30000;
constexpr std::int32_t kHighestGimmickTopHeight = 4096;
constexpr std::uint8_t kGimmickKindMask = 7;
constexpr std::uint8_t kGimmickMotionMask = 15;
constexpr std::uint8_t kGimmickInteractionMask = 3;
constexpr std::int32_t kOrientationQuarterTurn = 1024;
constexpr std::uint32_t kForbiddenGimmickProbeBit = std::uint32_t{1} << 20;
constexpr std::int16_t kFallingGimmickMode = 0xFC;
constexpr std::uint8_t kFallingActorMode = 3;
constexpr std::uint8_t kStackFallCountdown = 4;
constexpr std::size_t kStackDropActorSlots = 17;
constexpr std::uint32_t kStackTileWordMask = 0xFFFFFF;
constexpr std::int32_t kGimmickHeadingRound = 512;
constexpr std::int32_t kGimmickHeadingMask = 4095;
constexpr std::int32_t kGimmickCeilingClearance = 384;
constexpr std::int32_t kGimmickAboveReach = 64;
constexpr std::int32_t kGimmickTileCenter = 64;
constexpr std::uint8_t kActorRouteMask = 3;
constexpr std::uint8_t kFirstCeilingCheckedGimmickKind = 4;
constexpr std::uint8_t kQuarterTileSlidingGimmickKind = 3;
constexpr std::uint8_t kFirstCarryGimmickKind = 4;
constexpr std::uint8_t kUnusedGimmickKind = 7;
constexpr std::int32_t kInteractionHeightStep = 64;
constexpr std::int16_t kQuarterTileSlide = 32;
constexpr std::uint16_t kPushSoundFirst = 0x2C;
constexpr std::uint16_t kPushSoundSecond = 0x2D;
constexpr std::uint16_t kPlaceSoundFirst = 0x2E;
constexpr std::uint16_t kPlaceSoundSecond = 0x2F;
constexpr std::int32_t kPushAnimationBase = 0x4F;
constexpr std::int32_t kLiftAnimationBase = 0x47;
constexpr std::int32_t kPlaceAnimation = 0x4C;
constexpr std::size_t kInteractionDirections = kGimmickRollColumns;
constexpr std::int32_t kGimmickPushCommand = 2;
constexpr std::int32_t kGimmickDropCommand = 1;
constexpr std::int16_t kRollingGimmickMode = 0xF9;
constexpr std::int16_t kSlidingGimmickMode = 0xFF;
constexpr std::int16_t kQuarterSlidingGimmickMode = 0xFE;
constexpr std::uint8_t kFirstDropBoneTag = 0xF0;
constexpr std::uint8_t kSecondDropBoneTag = 0xF1;
constexpr std::int32_t kDropBoneHeightOffset = 60;

bool MatchesActiveTile(const GimmickQueryState& state,
                       const GimmickTileQuery& query) {
  return state.present && !state.skipped &&
         (state.motion_state & kGimmickMotionMask) == 0 &&
         static_cast<std::uint8_t>(state.tile_word) == query.tile_x &&
         static_cast<std::uint8_t>(state.tile_word >> kHalfwordBits) ==
             query.tile_z;
}

std::int32_t PackPosition(std::int16_t low, std::int16_t high) {
  const std::uint32_t word =
      static_cast<std::uint16_t>(low) |
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high))
       << kHalfwordBits);
  return std::bit_cast<std::int32_t>(word);
}

}  // namespace

// decomp: BATTLE.PRG func_8009F794 (30DB0.c:3320), 49 MIPS words.
std::optional<std::size_t> FindGimmickBelow(
    std::span<const GimmickQueryState> gimmicks,
    const GimmickQueryState& reference) {
  std::int32_t best_height = kLowestGimmickBelowHeight;
  std::optional<std::size_t> selected;
  const auto count = std::min(gimmicks.size(), kGimmickQuerySlots);
  for (std::size_t slot = 0; slot < count; ++slot) {
    const auto& candidate = gimmicks[slot];
    if (!candidate.present || candidate.skipped ||
        candidate.movement_mode != 0 || candidate.falling ||
        candidate.parent != kUnattachedGimmick ||
        candidate.tile_word != reference.tile_word ||
        candidate.position_y > reference.position_y ||
        candidate.position_y <= best_height) {
      continue;
    }
    best_height = candidate.position_y;
    selected = slot;
  }
  return selected;
}

// decomp: BATTLE.PRG func_800A91DC (40564.c:317), 55 MIPS words.
std::optional<std::size_t> FindTopGimmickAtTile(
    std::span<const GimmickQueryState> gimmicks, const GimmickTileQuery& query,
    bool require_idle_mode) {
  std::int32_t best_height = kHighestGimmickTopHeight;
  std::optional<std::size_t> selected;
  const auto count = std::min(gimmicks.size(), kGimmickQuerySlots);
  for (std::size_t slot = 0; slot < count; ++slot) {
    const auto& candidate = gimmicks[slot];
    if (!candidate.present || candidate.skipped ||
        (require_idle_mode && candidate.movement_mode != 0) ||
        candidate.parent != kUnattachedGimmick ||
        candidate.carrier != candidate.parent ||
        static_cast<std::uint8_t>(candidate.tile_word) != query.tile_x ||
        static_cast<std::uint8_t>(candidate.tile_word >> kHalfwordBits) !=
            query.tile_z ||
        candidate.position_y > best_height) {
      continue;
    }
    best_height = candidate.position_y;
    selected = slot;
  }
  return selected;
}

// decomp: BATTLE.PRG func_8009F858 (30DB0.c:3348), 16 MIPS words.
std::optional<std::uint8_t> GimmickSubtype(
    std::span<const GimmickQueryState> gimmicks, std::int32_t slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickQuerySlots ||
      static_cast<std::size_t>(slot) >= gimmicks.size()) {
    return std::nullopt;
  }
  const auto& state = gimmicks[static_cast<std::size_t>(slot)];
  if (!state.present || (state.object_id & kGimmickKindMask) != 1) {
    return std::nullopt;
  }
  return state.subtype;
}

// decomp: BATTLE.PRG func_800A47C4 (3A1A0.c:1022), 25 MIPS words.
bool AreGimmicksMoving(std::span<const GimmickQueryState> gimmicks) {
  const auto count = std::min(gimmicks.size(), kGimmickQuerySlots);
  for (const auto& state : gimmicks.first(count)) {
    if (state.present && !state.skipped &&
        (state.motion_state & kGimmickMotionMask) == 0 &&
        state.movement_mode != 0) {
      return true;
    }
  }
  return false;
}

// decomp: MAINMENU.PRG func_800FA188 (224.c:261), 44 MIPS words.
GimmickStackMeasure MeasureTileStack(
    std::span<const GimmickQueryState> gimmicks,
    const GimmickTileQuery& query) {
  GimmickStackMeasure result;
  const auto count = std::min(gimmicks.size(), kGimmickQuerySlots);
  for (const auto& state : gimmicks.first(count)) {
    if (!MatchesActiveTile(state, query) || state.movement_mode != 0) continue;
    if ((state.interaction & kGimmickInteractionMask) == 2) {
      result.interaction_two = true;
    } else {
      result.height_offset -= kHeightUnit;
    }
  }
  return result;
}

// decomp: MAINMENU.PRG func_800FA238 (224.c:286), 37 MIPS words.
bool IsGimmickWithinHeightBand(std::span<const GimmickQueryState> gimmicks,
                               const GimmickTileQuery& query,
                               std::int32_t minimum_height) {
  const auto maximum_height =
      Core::Math::WrappingAdd(minimum_height, kHeightUnit);
  const auto count = std::min(gimmicks.size(), kGimmickQuerySlots);
  for (const auto& state : gimmicks.first(count)) {
    if (MatchesActiveTile(state, query) && state.position_y >= minimum_height &&
        state.position_y <= maximum_height) {
      return true;
    }
  }
  return false;
}

// decomp: BATTLE.PRG func_800A0104 (30DB0.c:3690), 49 MIPS words.
bool SetGimmickOrientation(std::int32_t orientation,
                           std::span<const GimmickOrientationRow> rotations,
                           GimmickOrientationHost& host) {
  const auto kind = host.ObjectKind();
  if (!kind || (*kind & kGimmickKindMask) >= 2) return false;
  if (!host.StoreMaterial(static_cast<std::uint8_t>(orientation))) return false;
  if (orientation < 0 ||
      static_cast<std::size_t>(orientation) >= kGimmickOrientationCount ||
      static_cast<std::size_t>(orientation) >= rotations.size()) {
    return false;
  }
  const auto& row = rotations[static_cast<std::size_t>(orientation)];
  const Core::Math::Angles3 angles{
      Core::Math::WrapSigned16(row.quarter_x * kOrientationQuarterTurn),
      Core::Math::WrapSigned16(row.quarter_y * kOrientationQuarterTurn),
      Core::Math::WrapSigned16(row.quarter_z * kOrientationQuarterTurn)};
  return host.StoreRotation(Core::Math::BuildEulerRotationMatrix(angles)) &&
         host.ResetRotation();
}

// decomp: BATTLE.PRG func_800A3DB4 (3A1A0.c:708), 46 MIPS words.
bool CanPlaceGimmickAtPoint(const GimmickPlacementPoint& point,
                            GimmickPointHost& host) {
  const auto height = host.SupportHeight(point);
  if (height < point.minimum_height) return false;
  if ((host.LastProbeWord() & kForbiddenGimmickProbeBit) != 0) return false;
  return !host.ActorOccupies(
      {{point.x / kHeightUnit, point.z / kHeightUnit}, height});
}

namespace {
std::array<GimmickQueryState, kGimmickQuerySlots> StackQueryStates(
    GimmickStackDropHost& host) {
  std::array<GimmickQueryState, kGimmickQuerySlots> states{};
  for (std::size_t slot = 0; slot < states.size(); ++slot) {
    if (const auto state = host.Gimmick(slot)) states[slot] = state->query;
  }
  return states;
}

bool AttachStackSurvivors(std::size_t base_slot, GimmickStackDropHost& host) {
  for (std::size_t slot = 0; slot < kGimmickQuerySlots; ++slot) {
    auto state = host.Gimmick(slot);
    if (!state || state->query.skipped || host.SameGimmick(slot, base_slot)) {
      continue;
    }
    state->query.carrier = kUnattachedGimmick;
    if (!host.StoreGimmick(slot, *state)) return false;
    const auto base = host.Gimmick(base_slot);
    if (!base) return false;
    if (state->query.movement_mode == 0 && !state->query.falling &&
        state->query.parent == kUnattachedGimmick &&
        (state->query.tile_word & kStackTileWordMask) ==
            (base->query.tile_word & kStackTileWordMask) &&
        state->query.position_y <= base->query.position_y) {
      state->query.carrier = static_cast<std::uint8_t>(base_slot);
      if (!host.StoreGimmick(slot, *state)) return false;
    }
  }
  return true;
}

bool BeginGimmickBaseFall(std::size_t slot, GimmickStackDropHost& host) {
  auto base = host.Gimmick(slot);
  if (!base) return false;
  base->velocity_y = 0;
  base->query.movement_mode = kFallingGimmickMode;
  if (!host.StoreGimmick(slot, *base) || !host.RemoveOccupant(slot)) {
    return false;
  }
  base = host.Gimmick(slot);
  if (!base) return false;
  if ((base->query.interaction & kGimmickInteractionMask) == 2 &&
      !host.RemoveOccupant(slot)) {
    return false;
  }
  return AttachStackSurvivors(slot, host);
}

void BeginSupportedActorFall(GimmickSupportedActor& actor) {
  auto& state = actor.state;
  auto& motion = state.motion;
  if ((motion.movement_mode & kGimmickKindMask) != kFallingActorMode) {
    motion.root_motion = {};
    motion.movement_mode = kFallingActorMode;
    if (motion.slot != 0) {
      motion.destination_mode = 1;
      motion.destination_x = motion.current_tile.x;
      motion.destination_z = motion.current_tile.z;
    }
    motion.jump_countdown = kStackFallCountdown;
  }
  motion.locomotion_state = 0;
  motion.movement_pending = false;
  state.support_gimmick = 0;
}

bool DropSupportedActors(std::size_t top_slot, GimmickStackDropHost& host) {
  std::int32_t first_actor = 0;
  for (std::size_t count = 0; count < kStackDropActorSlots; ++count) {
    const auto top = host.Gimmick(top_slot);
    if (!top) return true;
    auto actor = host.ActorAbove(top->query, first_actor);
    if (!actor) return true;
    const auto next = static_cast<std::int32_t>(actor->state.motion.slot) + 1;
    BeginSupportedActorFall(*actor);
    if (!host.StoreActor(*actor)) return false;
    const auto advanced = next == 1 ? 2 : next;
    if (advanced <= first_actor) return false;
    first_actor = advanced;
  }
  return false;
}
}  // namespace

// decomp: BATTLE.PRG func_8009F530 (30DB0.c:3256), 153 MIPS words.
bool DropUnsupportedGimmickStack(std::int32_t slot,
                                 GimmickStackDropHost& host) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= kGimmickQuerySlots) {
    return false;
  }
  const auto index = static_cast<std::size_t>(slot);
  auto removed = host.Gimmick(index);
  if (!removed) return false;
  removed->query.falling = true;
  if (!host.StoreGimmick(index, *removed)) return false;
  const auto states = StackQueryStates(host);
  const GimmickTileQuery tile{
      static_cast<std::uint8_t>(removed->query.tile_word),
      static_cast<std::uint8_t>(removed->query.tile_word >> kHalfwordBits)};
  const auto top = FindTopGimmickAtTile(states, tile, false).value_or(0);
  const auto below = FindGimmickBelow(states, removed->query);
  if (below && !BeginGimmickBaseFall(*below, host)) return false;
  return DropSupportedActors(top, host);
}

namespace {
GimmickWorldPoint TileCenter(const GimmickTileQuery& tile) {
  return {tile.tile_x * kHeightUnit + kGimmickTileCenter,
          tile.tile_z * kHeightUnit + kGimmickTileCenter};
}

bool ClearGimmickCeilings(const Actors::MovementActor& actor,
                          const GimmickWorldPoint& target,
                          GimmickInteractionQueryHost& host) {
  const auto limit = actor.motion.position.y - kGimmickCeilingClearance;
  return limit >= host.CeilingHeight(
                      {actor.motion.position.x, actor.motion.position.z}) &&
         limit >= host.CeilingHeight(target);
}

std::optional<std::size_t> HeldGimmickAhead(const Actors::MovementActor& actor,
                                            const GimmickWorldPoint& adjacent,
                                            GimmickInteractionQueryHost& host) {
  const auto slot = static_cast<std::size_t>(actor.attach_bone);
  if (slot >= kGimmickQuerySlots) return std::nullopt;
  const auto object = host.Gimmick(slot);
  if (!object || object->movement_mode != 0) return std::nullopt;
  if (!host.PointAllowed(
          {adjacent.x, adjacent.z, actor.motion.position.y - kHeightUnit}) ||
      !ClearGimmickCeilings(actor, adjacent, host)) {
    return std::nullopt;
  }
  return slot;
}

std::optional<std::size_t> GroundGimmickAhead(
    const Actors::MovementActor& actor, const GimmickTileQuery& adjacent_tile,
    const GimmickTileQuery& far_tile, GimmickInteractionQueryHost& host) {
  const auto slot = host.TopAt(adjacent_tile);
  if (!slot || *slot == 0 || *slot >= kGimmickQuerySlots) return std::nullopt;
  const auto object = host.Gimmick(*slot);
  if (!object || (object->interaction & kGimmickInteractionMask) == 1 ||
      object->movement_mode != 0 ||
      object->position_y < actor.motion.position.y - kHeightUnit ||
      object->position_y > actor.motion.position.y + kGimmickAboveReach) {
    return std::nullopt;
  }
  if ((object->object_id & kGimmickKindMask) <
      kFirstCeilingCheckedGimmickKind) {
    const auto far = TileCenter(far_tile);
    if (!host.PointAllowed({far.x, far.z, object->position_y})) {
      return std::nullopt;
    }
  } else if (!ClearGimmickCeilings(actor, TileCenter(adjacent_tile), host)) {
    return std::nullopt;
  }
  return host.ActorAbove(*object) ? std::nullopt : slot;
}
}  // namespace

// decomp: BATTLE.PRG func_800A38E0 (3A1A0.c:522), 185 MIPS words.
std::optional<std::size_t> FindInteractableGimmickAhead(
    std::int32_t actor_slot, GimmickInteractionQueryHost& host) {
  if (actor_slot < 0 ||
      static_cast<std::size_t>(actor_slot) >= kStackDropActorSlots) {
    return std::nullopt;
  }
  const auto actor = host.Actor(actor_slot);
  if (!actor || (actor->motion.route_phase & kActorRouteMask) != 0 ||
      (actor->motion.movement_mode & kGimmickKindMask) != 0 ||
      (actor->motion.locomotion_state & kActorRouteMask) != 0) {
    return std::nullopt;
  }
  const auto direction = static_cast<std::uint8_t>(
      ((static_cast<std::int32_t>(actor->motion.facing) +
        kGimmickHeadingRound) &
       kGimmickHeadingMask) /
      kOrientationQuarterTurn);
  if (!host.StoreDirection(direction)) return std::nullopt;
  GimmickTileQuery adjacent{actor->motion.current_tile.x,
                            actor->motion.current_tile.z};
  auto far = adjacent;
  const auto step = direction < 2 ? -1 : 1;
  if ((direction & 1) != 0) {
    adjacent.tile_x += step;
    far.tile_x += step * 2;
  } else {
    adjacent.tile_z += step;
    far.tile_z += step * 2;
  }
  if (adjacent.tile_x < 0 || adjacent.tile_z < 0) return std::nullopt;
  return actor->motion.action_pose_active
             ? HeldGimmickAhead(*actor, TileCenter(adjacent), host)
             : GroundGimmickAhead(*actor, adjacent, far, host);
}

namespace {
bool RollGimmickMaterial(std::size_t slot, GimmickInteractionStartHost& host) {
  const auto material = host.Material(slot);
  const auto direction = host.Direction();
  if (!material || !direction || *direction < 0 ||
      static_cast<std::size_t>(*direction) >= kInteractionDirections) {
    return false;
  }
  const auto rows = host.RollOrientations();
  const auto index =
      static_cast<std::size_t>(*material) * kInteractionDirections +
      static_cast<std::size_t>(*direction);
  return index < rows.size() && host.StoreMaterial(slot, rows[index]);
}

bool StartGimmickPush(std::int32_t actor_slot, std::size_t slot,
                      std::uint8_t kind, std::int32_t height,
                      GimmickInteractionStartHost& host) {
  if (kind < 2 && !RollGimmickMaterial(slot, host)) return false;
  if (kind == kQuarterTileSlidingGimmickKind && !host.ClearSlide(slot)) {
    return false;
  }
  const auto direction = host.Direction();
  if (!direction) return false;
  const auto distance =
      kind == kQuarterTileSlidingGimmickKind ? kQuarterTileSlide : kHeightUnit;
  const auto step =
      static_cast<std::int16_t>(*direction < 2 ? -distance : distance);
  const auto along_x = (*direction & 1) != 0;
  if (!host.StartSlide(slot, along_x ? step : 0, along_x ? 0 : step) ||
      !host.PlaySounds(actor_slot, kPushSoundFirst, kPushSoundSecond) ||
      !host.PlayAnimation(actor_slot, height + kPushAnimationBase) ||
      !host.SetWeaponPose(actor_slot)) {
    return false;
  }
  const auto object = host.Object(slot);
  return object &&
         host.ReserveTile({(static_cast<std::int32_t>(object->position_x) +
                            object->slide_x) /
                               kHeightUnit,
                           (static_cast<std::int32_t>(object->position_z) +
                            object->slide_z) /
                               kHeightUnit});
}

bool StartGimmickCarry(std::int32_t actor_slot, std::int32_t height,
                       GimmickInteractionStartHost& host) {
  const auto initial_actor = host.Actor(actor_slot);
  if (!initial_actor) return false;
  const auto placing = initial_actor->motion.action_pose_active;
  if (!host.PlayAnimation(actor_slot, placing ? kPlaceAnimation
                                              : height + kLiftAnimationBase) ||
      !host.PlaySounds(actor_slot, placing ? kPlaceSoundFirst : kPushSoundFirst,
                       placing ? kPlaceSoundSecond : kPushSoundSecond) ||
      !host.SetWeaponPose(actor_slot)) {
    return false;
  }
  const auto actor = host.Actor(actor_slot);
  const auto direction = host.Direction();
  if (!actor || !direction) return false;
  const auto step = *direction < 2 ? -1 : 1;
  return host.ReserveTile(
      {actor->motion.current_tile.x + ((*direction & 1) != 0 ? step : 0),
       actor->motion.current_tile.z + ((*direction & 1) != 0 ? 0 : step)});
}

bool RecenterInteractingActor(std::int32_t actor_slot,
                              GimmickInteractionStartHost& host) {
  const auto actor = host.Actor(actor_slot);
  const auto direction = host.Direction();
  if (!actor || !direction) return false;
  auto tile = actor->motion.current_tile;
  tile.spare = actor->tile_spare;
  const auto step = *direction < 2 ? -1 : 1;
  if ((*direction & 1) != 0) {
    tile.x =
        static_cast<std::uint8_t>(static_cast<std::int32_t>(tile.x) + step);
  } else {
    tile.z =
        static_cast<std::uint8_t>(static_cast<std::int32_t>(tile.z) + step);
  }
  if (!host.TurnToward(actor_slot, tile)) return false;
  const auto updated = host.Actor(actor_slot);
  return updated &&
         host.TranslateToward(
             actor_slot, {Core::Math::WrapSigned16(
                              updated->motion.current_tile.x * kHeightUnit +
                              kGimmickTileCenter),
                          updated->motion.position.y,
                          Core::Math::WrapSigned16(
                              updated->motion.current_tile.z * kHeightUnit +
                              kGimmickTileCenter),
                          0});
}
}  // namespace

// decomp: BATTLE.PRG func_800A3E6C (3A1A0.c:728), 217 MIPS words and seven-way
// table80069460.
bool BeginGimmickInteraction(std::int32_t actor_slot,
                             GimmickInteractionStartHost& host) {
  if (actor_slot < 0 ||
      static_cast<std::size_t>(actor_slot) >= kStackDropActorSlots) {
    return false;
  }
  const auto initial_actor = host.Actor(actor_slot);
  if (!initial_actor || initial_actor->motion.weapon_pose_active) return false;
  const auto slot = host.SelectAhead(actor_slot);
  const auto direction = host.Direction();
  if (!slot || *slot >= kGimmickQuerySlots || !direction ||
      !host.StoreOrientation(*slot, static_cast<std::uint16_t>(*direction))) {
    return false;
  }
  const auto actor = host.Actor(actor_slot);
  const auto initial_object = host.Object(*slot);
  if (!actor || !initial_object) return false;
  const auto height = (static_cast<std::int32_t>(actor->motion.position.y) -
                       initial_object->query.position_y) /
                          kInteractionHeightStep +
                      1;
  if (!host.PublishSelected(*slot) || !host.ClearSlide(*slot)) return false;
  const auto object = host.Object(*slot);
  if (!object) return false;
  const auto kind =
      static_cast<std::uint8_t>(object->query.object_id & kGimmickKindMask);
  if (kind < kFirstCarryGimmickKind) {
    if (!StartGimmickPush(actor_slot, *slot, kind, height, host)) return false;
  } else if (kind < kUnusedGimmickKind) {
    if (!StartGimmickCarry(actor_slot, height, host)) return false;
  }
  return RecenterInteractingActor(actor_slot, host);
}

namespace {
bool ApplyGimmickPushCommand(GimmickInteractionCommandHost& host) {
  const auto object = host.Object();
  if (!object) return false;
  const auto kind = object->state.query.object_id & kGimmickKindMask;
  std::optional<std::int16_t> mode;
  if (kind < 2) mode = kRollingGimmickMode;
  if (kind == 2) mode = kSlidingGimmickMode;
  if (kind == kQuarterTileSlidingGimmickKind) mode = kQuarterSlidingGimmickMode;
  return host.PreparePush(mode) && host.RemoveOccupant();
}

bool ApplyGimmickDropCommand(GimmickInteractionCommandHost& host) {
  const auto actor = host.Actor();
  const auto direction = host.Direction();
  if (!actor || !direction) return false;
  const auto step = *direction < 2 ? -1 : 1;
  const GimmickTileQuery tile{
      actor->motion.current_tile.x + ((*direction & 1) != 0 ? step : 0),
      actor->motion.current_tile.z + ((*direction & 1) != 0 ? 0 : step)};
  const auto object = host.Object();
  const auto rotation_direction = host.Direction();
  if (!object || !rotation_direction) return false;
  const auto yaw = static_cast<std::uint16_t>(
      Core::Math::WrappingAdd(
          object->yaw, Core::Math::WrappingMultiply(*rotation_direction,
                                                    kOrientationQuarterTurn)) &
      kGimmickHeadingMask);
  if (!host.StoreDropPlacement(tile, yaw)) return false;
  const auto first = host.ParentBoneHeight(kFirstDropBoneTag);
  const auto second = host.ParentBoneHeight(kSecondDropBoneTag);
  if (!first || !second) return false;
  const auto height = Core::Math::WrapSigned16(
      (static_cast<std::int32_t>(*first) + *second) / 2 +
      kDropBoneHeightOffset);
  return host.FinishDrop(height) && host.SetCarrying(false);
}

bool ApplyGimmickAttachCommand(GimmickInteractionCommandHost& host) {
  const auto initial = host.Object();
  if (!initial ||
      (initial->state.query.motion_state & kGimmickMotionMask) != 0) {
    return host.PlayIdleAnimation();
  }
  const GimmickTileQuery tile{
      static_cast<std::uint8_t>(initial->state.query.tile_word),
      static_cast<std::uint8_t>(initial->state.query.tile_word >>
                                kHalfwordBits)};
  if (!host.ReleaseTile(tile) || !host.RemoveOccupant()) return false;
  const auto after_removal = host.Object();
  if (!after_removal) return false;
  if ((after_removal->state.query.interaction & kGimmickInteractionMask) == 2 &&
      !host.RemoveOccupant()) {
    return false;
  }
  const auto direction = host.Direction();
  const auto object = host.Object();
  if (!direction || !object) return false;
  auto x = object->state.slide_x;
  auto z = object->state.slide_z;
  const auto step = *direction < 2 ? kHeightUnit : -kHeightUnit;
  if ((*direction & 1) != 0) {
    x = Core::Math::WrapSigned16(static_cast<std::int32_t>(x) + step);
  } else {
    z = Core::Math::WrapSigned16(static_cast<std::int32_t>(z) + step);
  }
  if (!host.StoreSlide(x, z)) return false;
  const auto current = host.Object();
  const auto turn_direction = host.Direction();
  if (!current || !turn_direction) return false;
  const auto facing = static_cast<std::uint16_t>(
      Core::Math::WrappingAdd(
          current->yaw, Core::Math::WrappingMultiply(
                            Core::Math::WrappingSubtract(kGimmickRollColumns,
                                                         *turn_direction),
                            kOrientationQuarterTurn)) &
      kGimmickHeadingMask);
  if (!host.StoreYaw(facing)) return false;
  const auto rounded = static_cast<std::uint16_t>(
      ((facing + kGimmickHeadingRound) & kGimmickHeadingMask) /
      kOrientationQuarterTurn * kOrientationQuarterTurn);
  if (!host.StoreYaw(rounded)) return false;
  const auto actor = host.Actor();
  return actor && host.AttachObject(actor->motion.slot) &&
         host.SetCarrying(true);
}
}  // namespace

// decomp: BATTLE.PRG func_800A41D0 (3A1A0.c:842), 177 MIPS words.
bool ApplyGimmickInteractionCommand(std::int32_t mode,
                                    GimmickInteractionCommandHost& host) {
  if (mode == kGimmickPushCommand) return ApplyGimmickPushCommand(host);
  if (mode == kGimmickDropCommand) return ApplyGimmickDropCommand(host);
  return ApplyGimmickAttachCommand(host);
}

// decomp: BATTLE.PRG func_800A0024 (src/BATTLE/BATTLE.PRG/30DB0.c:3654).
// CPU 800A0054-800A00E8: stationary checks, wrapped height and packed stores.
std::optional<GimmickPlacementSnapshot> GimmickPlacement(
    std::int32_t slot, GimmickPlacementHost& host) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= host.GimmickSlots()) {
    return std::nullopt;
  }
  const auto state = host.Gimmick(static_cast<std::size_t>(slot));
  if (!state || state->attached || state->falling ||
      state->movement_mode != 0) {
    return std::nullopt;
  }
  const auto difference =
      Core::Math::WrappingAdd(host.SupportHeight(*state),
                              -static_cast<std::int32_t>(state->position_y));
  return GimmickPlacementSnapshot{
      state->tile_x,
      static_cast<std::uint8_t>(difference / kHeightUnit),
      state->tile_z,
      state->orientation,
      PackPosition(state->position_x, state->position_y),
      PackPosition(state->position_z, state->position_tail)};
}

}  // namespace VS::Field
