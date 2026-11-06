// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptActorCommands.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Event {
namespace {
using Actors::ActorPlacementView;
using Actors::MotionVector;
using Actors::PackedTile;

constexpr unsigned kByteBits = 8;
constexpr std::size_t kFirstOperand = 1;

// Rates: 0xFF in a rate byte keeps the actor's own rate (-1); 0 is instant.
constexpr std::uint8_t kOwnRateByte = 0xFF;
constexpr int kOwnRate = -1;
constexpr int kInstant = 0;

// Script world targets: a tile's centre, and a w that keeps the facing.
constexpr int kTileShift = 7;
constexpr int kHalfTile = 0x40;
constexpr std::int16_t kKeepFacing = std::numeric_limits<std::int16_t>::min();
constexpr int kFacingUnitsPerByte = 16;
constexpr int kRelativeTargetBit = 1;

// Actor references: a slot reference, a group, and the non-actor references
// (0x2000 and up) the attachment and look-at commands treat specially.
constexpr std::uint16_t kSlotReference = 0x1000;
constexpr std::uint16_t kSlotMask = 0x1F;
constexpr int kModelNotFound = 1;
constexpr std::uint16_t kGroupSelector = 0x2000;
constexpr std::uint16_t kEnemyGroupSelector = 0x2003;
constexpr int kEnemyKind = 4;
constexpr int kNonActorReference = 0x2000;
constexpr int kAllActorsReference = 0x2001;
constexpr int kAllActors = -1;

// Animation operands: the top two bits pick a bank, the rest blend frames.
constexpr unsigned kBankShift = 6;
constexpr unsigned kBlendMask = 0x3F;
constexpr std::uint8_t kNoAnimationByte = 0xFF;
constexpr int kNoAnimation = -1;

// Group display options: keep or set each equipment visibility bit, detach.
constexpr std::uint32_t kFirstEquipmentVisible = 0x800;
constexpr std::uint32_t kSecondEquipmentVisible = 0x1000;
constexpr unsigned kShowFirstEquipment = 1;
constexpr unsigned kKeepFirstEquipment = 2;
constexpr unsigned kShowSecondEquipment = 4;
constexpr unsigned kKeepSecondEquipment = 8;
constexpr unsigned kDetachOption = 0x20;
constexpr std::uint32_t kActiveDisplayFlag = 1;

// Facing turns, in 4096ths of a revolution.
constexpr int kHalfTurn = 2048;
constexpr int kFullTurn = 4096;
constexpr std::int8_t kTurnNegative = -1;
constexpr std::int8_t kTurnPositive = 1;

// Waits: state bits that read as busy once flipped, and the path-wait bit.
constexpr std::uint32_t kSettledFlags = 0x81;
constexpr std::uint16_t kPathWaitBit = 1;

// Head tracking targets and the default tracking rate.
constexpr int kTrackAngles = -4;
constexpr int kTrackNothing = -2;
constexpr int kTrackPoint = -1;
constexpr int kDefaultTrackingRate = 0x1000;

// Shared object model selectors.
constexpr unsigned kObjectModeShift = 2;
constexpr unsigned kObjectModeMask = 3;
constexpr unsigned kObjectSlotMask = 3;
constexpr unsigned kTextureSlotShift = 4;
constexpr int kFirstEffectTextureSlot = 0x1A;
constexpr unsigned kLoadModel = 0;
constexpr unsigned kLoadModelWithTextures = 1;
constexpr unsigned kLoadEffectTextures = 2;
constexpr unsigned kReleaseEffectTextures = 4;
constexpr std::uint8_t kRefreshEquipment = 0xFE;

// Distance flags: a byte, 0xFF past it; bearings in 256ths of a turn.
constexpr int kByteRange = 0x100;
constexpr int kByteMask = 0xFF;
constexpr std::uint8_t kOutOfRange = 0xFF;
constexpr std::uint8_t kNoTile = 0xFF;
constexpr int kBearingShift = 4;
constexpr int kDistanceUnit = 128;

constexpr std::int16_t kDialogAnchorHeight = -1000;

// Reads one instruction's operands in order, little-endian, after the opcode.
class Operands {
 public:
  Operands(ScriptCommand command, ScriptActorHost& host)
      : command_(command), host_(host) {}

  [[nodiscard]] std::uint8_t Byte() {
    if (cursor_ >= command_.size()) {
      if (!refused_) {
        host_.Refuse(ScriptActorRefusal::kShortCommand,
                     static_cast<int>(command_.size()));
        refused_ = true;
      }
      ++cursor_;
      return 0;
    }
    return command_[cursor_++];
  }
  [[nodiscard]] std::int16_t Half() {
    const unsigned low = Byte();
    const unsigned high = Byte();
    return static_cast<std::int16_t>(low | (high << kByteBits));
  }
  [[nodiscard]] std::uint16_t Reference() {
    return static_cast<std::uint16_t>(Half());
  }
  void Skip() { ++cursor_; }

 private:
  ScriptCommand command_;
  ScriptActorHost& host_;
  std::size_t cursor_ = kFirstOperand;
  bool refused_ = false;
};

[[nodiscard]] int Rate(std::uint8_t value) {
  return value != kOwnRateByte ? value : kOwnRate;
}

[[nodiscard]] std::int16_t Wrap16(int value) {
  return static_cast<std::int16_t>(value);
}

[[nodiscard]] std::int16_t TileCentre(std::uint8_t tile) {
  return Wrap16((tile << kTileShift) + kHalfTile);
}

void MarkControlled(ScriptActorHost& host, int actor) {
  // The original indexed its 17 marks with any resolved id (#305).
  if (actor < 0 || actor >= kScriptActorSlots) {
    host.Refuse(ScriptActorRefusal::kControlledSlot, actor);
    return;
  }
  host.MarkScriptControlled(actor);
}

void FinishScriptedMove(ScriptActorHost& host, int actor) {
  host.ClearMovementModes(actor);
  MarkControlled(host, actor);
}

// The original read an absent actor's placement from unset stack memory.
[[nodiscard]] ActorPlacementView PlacementOf(ScriptActorHost& host, int actor) {
  const std::optional<ActorPlacementView> placement =
      host.ActorPlacement(actor);
  if (placement.has_value()) return *placement;
  host.Refuse(ScriptActorRefusal::kMissingPlacement, actor);
  return {};
}

template <typename Visit>
void ForEachSelected(ScriptActorHost& host, std::uint16_t selector,
                     Visit visit) {
  ActorSelection selection = BuildScriptActorSelection(host, selector);
  for (int actor = NextScriptSelectedActor(host, selection);
       actor != kSelectionEnd;
       actor = NextScriptSelectedActor(host, selection)) {
    visit(actor);
  }
}

[[nodiscard]] int BankedAnimation(const ScriptActorHost& host,
                                  std::uint8_t bank_and_blend,
                                  std::uint8_t index) {
  return host.AnimationBankBase(bank_and_blend >> kBankShift) + index;
}

// Operands: animation index, an unread byte, then bank and blend frames.
[[nodiscard]] AnimationRequest ReadAnimation(Operands& operands,
                                             const ScriptActorHost& host) {
  const std::uint8_t index = operands.Byte();
  operands.Skip();
  const std::uint8_t bank = operands.Byte();
  return {.animation = BankedAnimation(host, bank, index),
          .blend_frames = static_cast<int>(bank & kBlendMask)};
}

// Operands: animation index, start frame, an unread byte, then bank and
// blend frames.
[[nodiscard]] AnimationRequest ReadAnimationAtFrame(
    Operands& operands, const ScriptActorHost& host) {
  const std::uint8_t index = operands.Byte();
  const std::uint8_t start = operands.Byte();
  operands.Skip();
  const std::uint8_t bank = operands.Byte();
  return {.animation = BankedAnimation(host, bank, index),
          .blend_frames = static_cast<int>(bank & kBlendMask),
          .start_frame = start};
}

[[nodiscard]] PackedTile ReadTile(Operands& operands) {
  PackedTile tile;
  tile.x = operands.Byte();
  tile.layer = operands.Byte();
  tile.z = operands.Byte();
  return tile;
}

// Operands: x, y and z; x's low bit makes the point relative to the actor.
[[nodiscard]] MotionVector ReadWorldTarget(Operands& operands,
                                           ScriptActorHost& host, int actor) {
  const std::int16_t raw_x = operands.Half();
  const std::int16_t y = operands.Half();
  const std::int16_t z = operands.Half();
  const int x = raw_x >> 1;
  if ((raw_x & kRelativeTargetBit) == 0) {
    return {.x = Wrap16(x), .y = y, .z = z};
  }
  const MotionVector base = PlacementOf(host, actor).position;
  return {.x = Wrap16(base.x + x),
          .y = Wrap16(base.y + y),
          .z = Wrap16(base.z + z)};
}

[[nodiscard]] MotionVector ReadVector(Operands& operands) {
  MotionVector vector;
  vector.x = operands.Half();
  vector.y = operands.Half();
  vector.z = operands.Half();
  return vector;
}

[[nodiscard]] Render::ColorCode ReadColor(Operands& operands) {
  Render::ColorCode color;
  color.red = operands.Byte();
  color.green = operands.Byte();
  color.blue = operands.Byte();
  return color;
}

[[nodiscard]] bool IsBlack(Render::ColorCode color) {
  return (color.red | color.green | color.blue) == 0;
}

[[nodiscard]] int TrackingRate(std::uint8_t value) {
  return value != 0 ? value : kDefaultTrackingRate;
}

// The x and z offsets from the second actor to the first, in 16 bits.
struct PlanarDelta {
  std::int16_t x = 0;
  std::int16_t z = 0;
};

[[nodiscard]] PlanarDelta DeltaBetween(ScriptActorHost& host,
                                       std::uint16_t first,
                                       std::uint16_t second) {
  const MotionVector from =
      PlacementOf(host, host.ResolveActor(first)).position;
  const MotionVector to = PlacementOf(host, host.ResolveActor(second)).position;
  return {.x = Wrap16(from.x - to.x), .z = Wrap16(from.z - to.z)};
}

// The original summed the squares in 32 bits, so two extreme offsets wrap.
[[nodiscard]] int PlanarDistance(PlanarDelta delta) {
  const auto x = static_cast<std::uint32_t>(delta.x * delta.x);
  const auto z = static_cast<std::uint32_t>(delta.z * delta.z);
  return Core::Math::SquareRootInteger(static_cast<std::int32_t>(x + z));
}

[[nodiscard]] std::int16_t FacingDelta(std::uint8_t facing,
                                       std::int8_t direction, int current) {
  int delta = (facing * kFacingUnitsPerByte) - current;
  if (delta < -kHalfTurn) delta += kFullTurn;
  if (delta > kHalfTurn) delta -= kFullTurn;
  if (direction == kTurnNegative && delta > 0) delta -= kFullTurn;
  if (direction == kTurnPositive && delta < 0) delta += kFullTurn;
  return Wrap16(delta);
}
}  // namespace

ActorSelection BuildScriptActorSelection(ScriptActorHost& host,
                                         std::uint16_t selector) {
  ActorSelection selection;
  if ((selector & kGroupSelector) == 0) {
    selection.ids[0] = static_cast<std::uint8_t>(host.ResolveActor(selector));
    selection.remaining = 1;
    return selection;
  }
  const bool enemies_only = selector == kEnemyGroupSelector;
  for (int actor = 0; actor < kScriptActorSlots; ++actor) {
    const std::optional<int> kind = host.PresentActorKind(actor);
    if (!kind.has_value() || (enemies_only && *kind != kEnemyKind)) continue;
    selection.ids[selection.remaining] = static_cast<std::uint8_t>(actor);
    ++selection.remaining;
  }
  return selection;
}

int NextScriptSelectedActor(ScriptActorHost& host, ActorSelection& selection) {
  if (selection.remaining == 0) return kSelectionEnd;
  if (selection.cursor >= kSelectionCapacity) {
    host.Refuse(ScriptActorRefusal::kSelectionCursor, selection.cursor);
    selection.remaining = 0;
    return kSelectionEnd;
  }
  --selection.remaining;
  return selection.ids[selection.cursor++];
}

void SetEventActorActivity(ScriptActorHost& host, int enabled) {
  for (int actor = 1; actor < kScriptActorSlots; ++actor) {
    if (!host.PresentActorKind(actor).has_value()) continue;
    const std::uint32_t flags = host.StateFlags(actor);
    host.SetDisplayFlags(actor, enabled != 0 ? flags | kActiveDisplayFlag
                                             : flags & ~kActiveDisplayFlag);
  }
  host.SetOtherLogicEnabled(enabled);
}

int ActorDialogAnchor(ScriptActorHost& host, int actor, MotionVector& anchor) {
  anchor.x = 0;
  anchor.y = kDialogAnchorHeight;
  anchor.z = 0;
  const DialogAnchorProjection projection =
      host.ProjectDialogAnchor(actor, anchor);
  anchor = projection.anchor;
  anchor.z = Wrap16(Core::Math::Arctangent2(projection.screen_y - anchor.y,
                                            projection.screen_x - anchor.x));
  return anchor.z;
}

ScriptStep ScriptResetActorGroupState(ScriptCommand command,
                                      ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t selector = operands.Reference();
  const int animate = operands.Byte();
  ForEachSelected(host, selector,
                  [&](int actor) { host.InterruptAction(actor, animate); });
  return ScriptStep::kAdvance;
}

ScriptStep ScriptStoreActorTile(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const int flag = operands.Reference();
  int actor = 0;
  if ((reference & kSlotReference) != 0) {
    actor = reference & kSlotMask;
    if (!host.PresentActorKind(actor).has_value()) {
      host.SetFlag(flag, kNoTile);
      return ScriptStep::kAdvance;
    }
  } else {
    actor = host.FindActorByModelId(reference);
    if (actor == kModelNotFound) {
      host.SetFlag(flag, kNoTile);
      return ScriptStep::kAdvance;
    }
  }
  const PackedTile tile = PlacementOf(host, actor).tile;
  host.SetFlag(flag, tile.x);
  host.SetFlag(flag + 1, tile.layer);
  host.SetFlag(flag + 2, tile.z);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSpawnActor(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const int model = operands.Half();
  Actors::SpawnPlacement placement;
  placement.tile_x = operands.Byte();
  placement.direction = operands.Byte();
  placement.tile_z = operands.Byte();
  placement.height = operands.Byte();
  host.QueueScriptedSpawn(host.ResolveActor(reference), model, placement);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptNavigateActorToTile(ScriptCommand command,
                                     ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  const std::uint8_t tile_x = operands.Byte();
  operands.Skip();
  const std::uint8_t tile_z = operands.Byte();
  const MotionVector target{
      .x = TileCentre(tile_x), .z = TileCentre(tile_z), .w = kKeepFacing};
  if (host.SkipActive()) {
    host.TranslateTo(actor, target, kInstant);
  } else {
    host.BeginScriptedPath(actor, target);
  }
  FinishScriptedMove(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptPlayActorAnimation(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const AnimationRequest request = ReadAnimation(operands, host);
  host.PlayAnimation(host.ResolveActor(reference), request);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorGroupFlags(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t selector = operands.Reference();
  const std::uint8_t display = operands.Byte();
  const unsigned options = operands.Byte();
  ForEachSelected(host, selector, [&](int actor) {
    const std::uint32_t flags = host.StateFlags(actor);
    std::uint32_t first = flags & kFirstEquipmentVisible;
    std::uint32_t second = flags & kSecondEquipmentVisible;
    if ((options & kKeepFirstEquipment) == 0) {
      first = (options & kShowFirstEquipment) != 0 ? kFirstEquipmentVisible : 0;
    }
    if ((options & kKeepSecondEquipment) == 0) {
      second =
          (options & kShowSecondEquipment) != 0 ? kSecondEquipmentVisible : 0;
    }
    host.SetDetached(actor, (options & kDetachOption) != 0);
    host.SetDisplayFlags(actor, first | second | display);
    host.ResetWeaponVisibilityBlend(actor);
  });
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorSoundMix(ScriptCommand command,
                                  ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  Actors::SoundEffectSlot slot;
  slot.kind = operands.Byte();
  slot.level = operands.Byte();
  slot.flags = operands.Byte();
  slot.parameter = operands.Byte();
  int actor = host.ResolveActor(reference);
  if (actor == kAllActorsReference) actor = kAllActors;
  host.SetSoundOverrides(actor, slot);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptWalkActorToTile(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  PackedTile tile = ReadTile(operands);
  tile.spare = operands.Byte();
  const std::uint8_t move_rate = operands.Byte();
  const std::uint8_t turn_rate = operands.Byte();
  if (host.SkipActive()) {
    host.MoveToTile(actor, tile, kInstant, kInstant);
  } else {
    host.MoveToTile(actor, tile, Rate(move_rate), Rate(turn_rate));
  }
  FinishScriptedMove(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptWalkActorToWorldPos(ScriptCommand command,
                                     ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  MotionVector target = ReadWorldTarget(operands, host, actor);
  const std::uint8_t facing = operands.Byte();
  const std::uint8_t move_rate = operands.Byte();
  const std::uint8_t turn_rate = operands.Byte();
  target.w = facing != kOwnRateByte ? Wrap16(facing * kFacingUnitsPerByte)
                                    : kKeepFacing;
  if (host.SkipActive()) {
    host.MoveToPosition(actor, target, kInstant, kInstant);
  } else {
    host.MoveToPosition(actor, target, Rate(move_rate), Rate(turn_rate));
  }
  FinishScriptedMove(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSlideActorToTile(ScriptCommand command,
                                  ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  PackedTile tile = ReadTile(operands);
  tile.spare = kNoTile;
  const std::uint8_t move_rate = operands.Byte();
  host.TranslateToTile(actor, tile,
                       host.SkipActive() ? kInstant : Rate(move_rate));
  FinishScriptedMove(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSlideActorToWorldPos(ScriptCommand command,
                                      ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  MotionVector target = ReadWorldTarget(operands, host, actor);
  target.w = kKeepFacing;
  const std::uint8_t move_rate = operands.Byte();
  host.TranslateTo(actor, target,
                   host.SkipActive() ? kInstant : Rate(move_rate));
  FinishScriptedMove(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorAttachment(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t actor_reference = operands.Reference();
  const std::uint16_t parent_reference = operands.Reference();
  const int bone = operands.Byte();
  const int actor = host.ResolveActor(actor_reference);
  const int parent = host.ResolveActor(parent_reference);
  if (parent >= kNonActorReference) {
    host.ReleaseAttachmentLock(actor);
  } else {
    host.AttachToParentBone(actor, {.parent = parent, .bone = bone});
  }
  return ScriptStep::kAdvance;
}

ScriptStep ScriptNavigateActorToWorldPos(ScriptCommand command,
                                         ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  MotionVector target;
  target.x = operands.Half();
  target.z = operands.Half();
  target.w = kKeepFacing;
  if (host.SkipActive()) {
    host.TranslateTo(actor, target, kInstant);
  } else {
    host.BeginScriptedPath(actor, target);
  }
  FinishScriptedMove(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptTurnActorToFacing(ScriptCommand command,
                                   ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  const std::uint8_t facing = operands.Byte();
  const auto direction = static_cast<std::int8_t>(operands.Byte());
  const int turn_rate = Rate(operands.Byte());
  if (direction != 0) {
    const int current = PlacementOf(host, actor).position.w;
    host.TurnBy(actor, FacingDelta(facing, direction, current), turn_rate);
  } else {
    host.TurnTo(actor, Wrap16(facing * kFacingUnitsPerByte), turn_rate);
  }
  MarkControlled(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptFaceOtherActor(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t actor_reference = operands.Reference();
  const std::uint16_t target_reference = operands.Reference();
  const int turn_rate = Rate(operands.Byte());
  const int actor = host.ResolveActor(actor_reference);
  const PackedTile tile =
      PlacementOf(host, host.ResolveActor(target_reference)).tile;
  host.TurnTowardTile(actor, tile, turn_rate);
  MarkControlled(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptFaceTile(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  // The original left the facing byte unset; a turn onto the actor's own tile
  // reads it, so it is zero here.
  const PackedTile tile = ReadTile(operands);
  host.TurnTowardTile(actor, tile, Rate(operands.Byte()));
  MarkControlled(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptScaleActor(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const MotionVector scale = ReadVector(operands);
  const int steps = operands.Byte();
  host.ScaleTo(host.ResolveActor(reference), scale, steps);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptRemoveActorGroup(ScriptCommand command,
                                  ScriptActorHost& host) {
  Operands operands(command, host);
  ForEachSelected(host, operands.Reference(),
                  [&](int actor) { host.RemoveScriptedActor(actor); });
  return ScriptStep::kAdvance;
}

ScriptStep ScriptWaitForActorFlags(ScriptCommand command,
                                   ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  const std::uint16_t mask = operands.Reference();
  auto waiting =
      static_cast<std::uint16_t>(host.StateFlags(actor) ^ kSettledFlags);
  if ((mask & kPathWaitBit) != 0 && !host.ScriptedPathFinished(actor)) {
    waiting |= kPathWaitBit;
  }
  return (waiting & mask) != 0 ? ScriptStep::kHold : ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorColorOverride(ScriptCommand command,
                                       ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const Render::ColorCode tint = ReadColor(operands);
  const int actor = host.ResolveActor(reference);
  host.SetTint(actor, IsBlack(tint) ? std::nullopt
                                    : std::optional<Render::ColorCode>(tint));
  return ScriptStep::kAdvance;
}

ScriptStep ScriptQueueActorAnimation(ScriptCommand command,
                                     ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const AnimationRequest request = ReadAnimation(operands, host);
  host.QueueAnimation(host.ResolveActor(reference), request);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptRotateActorAxes(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  const bool relative = operands.Byte() != 0;
  const MotionVector rotation = ReadVector(operands);
  const int turn_rate = Rate(operands.Byte());
  if (relative) {
    host.RotateBy(actor, rotation, turn_rate);
  } else {
    host.RotateTo(actor, rotation, turn_rate);
  }
  MarkControlled(host, actor);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorHeadAngles(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  MotionVector angles;
  angles.x = operands.Half();
  angles.y = operands.Half();
  const int rate = TrackingRate(operands.Byte());
  host.SetHeadTracking(host.ResolveActor(reference), kTrackAngles, angles,
                       rate);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptPlayActorAnimationAtFrame(ScriptCommand command,
                                           ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const AnimationRequest request = ReadAnimationAtFrame(operands, host);
  host.PlayAnimationAtFrame(host.ResolveActor(reference), request);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptQueueActorAnimationAtFrame(ScriptCommand command,
                                            ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const AnimationRequest request = ReadAnimationAtFrame(operands, host);
  host.QueueAnimation(host.ResolveActor(reference), request);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorIdleAnimation(ScriptCommand command,
                                       ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  const std::uint8_t bank = operands.Byte();
  const std::uint8_t index = operands.Byte();
  host.SetTravelAnimation(actor, index == kNoAnimationByte
                                     ? kNoAnimation
                                     : host.AnimationBankBase(bank) + index);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptLookAtActor(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t actor_reference = operands.Reference();
  const std::uint16_t target_reference = operands.Reference();
  const int rate = TrackingRate(operands.Byte());
  const int target = host.ResolveActor(target_reference);
  const int actor = host.ResolveActor(actor_reference);
  host.SetHeadTracking(actor,
                       target != kNonActorReference ? target : kTrackNothing,
                       std::nullopt, rate);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptLookAtWorldPos(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const MotionVector point = ReadVector(operands);
  const int rate = TrackingRate(operands.Byte());
  host.SetHeadTracking(host.ResolveActor(reference), kTrackPoint, point, rate);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptLoadObjectModel(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  const int model = operands.Half();
  const unsigned selector = operands.Byte();
  const ObjectModelRequest request{
      .slot = static_cast<int>(selector & kObjectSlotMask),
      .model = model,
      .texture_slot = static_cast<int>(selector >> kTextureSlotShift) +
                      kFirstEffectTextureSlot};
  switch ((selector >> kObjectModeShift) & kObjectModeMask) {
    case kLoadModel:
      host.QueueSharedSequence(request.slot, model);
      break;
    case kLoadModelWithTextures:
      host.QueueSharedSequenceWithTextures(request);
      break;
    case kLoadEffectTextures:
      host.QueueSequenceEffectTextures(request);
      break;
    default:
      break;
  }
  return ScriptStep::kAdvance;
}

// The original held while the first free asset request was past slot 0, and
// went on once slot 0 was free or every request was busy.
ScriptStep ScriptWaitForObjectLoad(ScriptCommand /*command*/,
                                   ScriptActorHost& host) {
  return host.FindFreeAssetRequest() > 0 ? ScriptStep::kHold
                                         : ScriptStep::kAdvance;
}

ScriptStep ScriptReleaseObjectModel(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  const unsigned selector = operands.Byte();
  if ((selector & kReleaseEffectTextures) != 0) {
    host.ReleaseSequenceEffectTextures(
        static_cast<int>(selector & kObjectSlotMask));
  } else if (host.UnloadSharedSequence(static_cast<int>(selector)) < 0) {
    return ScriptStep::kHold;
  }
  return host.SkipActive() ? ScriptStep::kAdvanceAndYield
                           : ScriptStep::kAdvance;
}

ScriptStep ScriptSelectObjectTextureBank(ScriptCommand command,
                                         ScriptActorHost& host) {
  Operands operands(command, host);
  host.SetSharedSequenceBank(operands.Byte());
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorOverlayPass(ScriptCommand command,
                                     ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  Actors::CompanionStyle style;
  style.offset.x = static_cast<std::int8_t>(operands.Byte());
  style.offset.y = static_cast<std::int8_t>(operands.Byte());
  style.offset.z = static_cast<std::int8_t>(operands.Byte());
  style.offset.depth_bias = operands.Byte();
  style.color = ReadColor(operands);
  const int actor = host.ResolveActor(reference);
  host.SetCompanion(actor, IsBlack(style.color)
                               ? std::nullopt
                               : std::optional<Actors::CompanionStyle>(style));
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorWeaponDrawn(ScriptCommand command,
                                     ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t selector = operands.Reference();
  const std::uint8_t index = operands.Byte();
  // The original read any byte index from its 121-entry table (#305).
  const std::optional<int> mode = host.WeaponDrawState(index);
  if (!mode.has_value()) {
    host.Refuse(ScriptActorRefusal::kWeaponDrawState, index);
    return ScriptStep::kAdvance;
  }
  ForEachSelected(host, selector,
                  [&](int actor) { host.SetBattleMode(actor, *mode); });
  return ScriptStep::kAdvance;
}

ScriptStep ScriptPlayActorAnimationSound(ScriptCommand command,
                                         ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const int sound = operands.Byte();
  host.PlayWeaponDrawSound(host.ResolveActor(reference), sound);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetNpcUpdatesEnabled(ScriptCommand command,
                                      ScriptActorHost& host) {
  Operands operands(command, host);
  host.SetOtherLogicEnabled(operands.Byte());
  return ScriptStep::kAdvance;
}

ScriptStep ScriptRestoreActorVitals(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  host.RestoreVitals(host.ResolveActor(operands.Reference()));
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorEquipmentModels(ScriptCommand command,
                                         ScriptActorHost& host) {
  Operands operands(command, host);
  const int actor = host.ResolveActor(operands.Reference());
  const std::uint8_t blade = operands.Byte();
  operands.Skip();
  const std::uint8_t shield = operands.Byte();
  if (blade == kRefreshEquipment) {
    host.RefreshEquipmentModels(actor);
  } else {
    host.LoadEquipmentModels(actor, {.blade = blade, .shield = shield});
  }
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorYawOscillation(ScriptCommand command,
                                        ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const int rate = operands.Byte();
  host.SetRootSpin(host.ResolveActor(reference), rate);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptClearAshleyStatusEffects(ScriptCommand /*command*/,
                                          ScriptActorHost& host) {
  host.PreserveAshleyStatuses();
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetActorRenderMode(ScriptCommand command,
                                    ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t reference = operands.Reference();
  const int mode = operands.Byte();
  host.StartAppearanceTransition(host.ResolveActor(reference), mode);
  return ScriptStep::kAdvance;
}

ScriptStep CommandSetEnteredDoor(ScriptCommand command, ScriptActorHost& host) {
  Operands operands(command, host);
  host.MarkEnteredDoor(operands.Byte());
  return ScriptStep::kAdvance;
}

ScriptStep CommandStoreActorRangeBearing(ScriptCommand command,
                                         ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t first = operands.Reference();
  const std::uint16_t second = operands.Reference();
  const std::uint8_t scale = operands.Byte();
  const int flag = operands.Half();
  const PlanarDelta delta = DeltaBetween(host, first, second);
  std::uint8_t range = kOutOfRange;
  if (scale == 0) {
    // The original divided by the zero scale (#304).
    host.Refuse(ScriptActorRefusal::kZeroDistanceScale, flag);
  } else {
    const int scaled = PlanarDistance(delta) / scale;
    if (scaled < kByteRange) range = static_cast<std::uint8_t>(scaled);
  }
  host.SetFlag(flag, range);
  const int bearing = Core::Math::Arctangent2(delta.x, delta.z);
  host.SetFlag(flag + 1, static_cast<std::uint8_t>((bearing >> kBearingShift) &
                                                   kByteMask));
  return ScriptStep::kAdvance;
}

ScriptStep CommandSetActorActivity(ScriptCommand command,
                                   ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t selector = operands.Reference();
  const int enabled = operands.Byte();
  ForEachSelected(host, selector,
                  [&](int actor) { host.SetLogicEnabled(actor, enabled); });
  return ScriptStep::kAdvance;
}

ScriptStep CommandAwardMiscLoot(ScriptCommand command, ScriptActorHost& host) {
  if (!host.MenuTransitionActive()) {
    Operands operands(command, host);
    ScriptLoot loot;
    loot.first_item = operands.Half();
    loot.first_count = operands.Byte();
    loot.second_item = operands.Half();
    loot.second_count = operands.Byte();
    host.PresentScriptedLoot(loot);
    host.SetMenuTransitionActive(true);
    return ScriptStep::kHold;
  }
  if (host.UpdateRewardSequence() == 0) {
    host.SetMenuTransitionActive(false);
    return ScriptStep::kAdvanceAndYield;
  }
  return ScriptStep::kHold;
}

ScriptStep CommandConsumeMiscItem(ScriptCommand command,
                                  ScriptActorHost& host) {
  Operands operands(command, host);
  host.ConsumeMiscItem(operands.Half());
  return ScriptStep::kAdvance;
}

ScriptStep CommandStoreActorDistance(ScriptCommand command,
                                     ScriptActorHost& host) {
  Operands operands(command, host);
  const std::uint16_t first = operands.Reference();
  const std::uint16_t second = operands.Reference();
  const int flag = operands.Half();
  const int distance = PlanarDistance(DeltaBetween(host, first, second));
  host.SetFlag(
      flag, static_cast<std::uint8_t>((distance / kDistanceUnit) & kByteMask));
  return ScriptStep::kAdvance;
}

}  // namespace VS::Event
