// Hand-maintained (tools/port/owned.csv).
#include "Actors/AnimationPose.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorMotion.hpp"
#include "Actors/ModelRendering.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
using Core::Math::Vec3;
using Core::Math::WrapSigned16;

constexpr int kBitsPerByte = 8;
constexpr std::size_t kMotionOffsetField = 0;
constexpr std::size_t kMapOffsetField = 4;
constexpr std::size_t kClipHeaderFixedBytes = 10;
constexpr std::size_t kClipHeaderBytesPerBone = 4;

// Clip header words and bytes.
constexpr std::size_t kEventsWord = 2;
constexpr std::size_t kRootRotationWord = 3;
constexpr std::size_t kRootScaleWord = 4;
constexpr std::size_t kFirstBoneWord = 5;
constexpr std::size_t kBaseClipByte = 2;
constexpr std::size_t kFlagsByte = 3;
constexpr std::uint8_t kHasScaleTracks = 1;

// Animation ids (FindActorAnimationHeader).
constexpr int kOwnedAnimation = 0;
constexpr int kSpecialAction = 0xFF;
constexpr int kSpecialModelAction = 0xFE;
constexpr int kFirstSharedAnimation = 0xDC;
constexpr int kFirstWorkAnimation = 0xCA;
constexpr int kFirstBattleAnimation = 100;
// The special bank holds three pitch clips per sequence model, then the
// per-model clips from entry 31.
constexpr int kSpecialClipsPerModel = 3;
constexpr int kSpecialModelClipBase = 31;
// The model whose special actions ignore its sequence model.
constexpr int kSpecialPitchModel = 2;

// Event cursors and initial keys.
constexpr std::uint8_t kLongFrame = 0xFF;
constexpr std::size_t kEncodedVectorBytes = 6;
constexpr std::size_t kRawScaleBytes = 3;
constexpr std::uint8_t kPackedScaleTag = 0xFE;
constexpr std::uint8_t kPackedScaleVersion = 1;
constexpr std::uint16_t kOwnBaseKeyCursor = 6;
constexpr std::uint16_t kOwnBasePackedScaleCursor = 8;
constexpr std::uint16_t kOwnBaseRawScaleCursor = 3;
constexpr std::uint16_t kNoEvent = 0xFFFF;

// Rotations are 4096 to a turn and keys hold two turns of range.
constexpr int kTurn = 4096;
constexpr int kKeyRange = kTurn * 2;

// Motion tweens and head look.
constexpr int kLocomotionStepping = 1;
constexpr int kLocomotionHeld = 2;
constexpr int kAnimationBankSpan = 100;
constexpr int kFirstJumpTrackAnimation = 31;
constexpr unsigned kJumpTrackAnimations = 8;
constexpr int kFirstLockedTrackAnimation = 8;
constexpr unsigned kLockedTrackAnimations = 27;
constexpr auto kFreeTrackAnimations = std::to_array({0, 1, 6, 13, 14, 17, 18});
constexpr int kLookForward = kTurn * 3 / 4;
constexpr int kQuarterTurn = kTurn / 4;
constexpr int kLookLimit = 800;

[[nodiscard]] bool Fits(std::span<const std::uint8_t> bytes, std::size_t offset,
                        std::size_t count) {
  return offset <= bytes.size() && count <= bytes.size() - offset;
}

[[nodiscard]] std::optional<std::uint16_t> ReadHalf(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
  if (!Fits(bytes, offset, sizeof(std::uint16_t))) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(bytes[offset] |
                                    (bytes[offset + 1] << kBitsPerByte));
}

[[nodiscard]] std::optional<std::size_t> ReadWord(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
  if (!Fits(bytes, offset, sizeof(std::uint32_t))) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  for (std::size_t i = sizeof(std::uint32_t); i-- > 0;) {
    value = (value << kBitsPerByte) | bytes[offset + i];
  }
  return value;
}

// The header word at `index` of the clip header at `header`.
[[nodiscard]] std::optional<std::size_t> HeaderWord(
    std::span<const std::uint8_t> bank, std::size_t header, std::size_t index) {
  const auto word = ReadHalf(bank, header + index * sizeof(std::uint16_t));
  if (!word) {
    return std::nullopt;
  }
  return *word;
}

// The bank offset of the motion data a header word points into.
[[nodiscard]] std::optional<std::size_t> MotionData(
    std::span<const std::uint8_t> bank, std::size_t header, std::size_t index) {
  const auto motion = ReadWord(bank, kMotionOffsetField);
  const auto word = HeaderWord(bank, header, index);
  if (!motion || !word) {
    return std::nullopt;
  }
  return *motion + *word;
}

[[nodiscard]] std::int16_t DecodeComponent(std::uint8_t high,
                                           std::uint8_t low) {
  const auto value = static_cast<std::uint32_t>((high << kBitsPerByte) | low);
  return WrapSigned16(value * 2);
}

[[nodiscard]] std::optional<int> SpecialIndex(
    int animation_id, const AnimationBankLimits& limits) {
  int index = 0;
  if (animation_id == kSpecialAction) {
    if (limits.slot == 0 || limits.model_id == kSpecialPitchModel) {
      index = limits.special_pitch;
    } else if (limits.sequence_model_id == 0) {
      return std::nullopt;
    } else {
      index = (limits.sequence_model_id - 1) * kSpecialClipsPerModel +
              limits.special_pitch;
    }
  } else {
    if (limits.sequence_model_id == 0) {
      return std::nullopt;
    }
    index = (limits.sequence_model_id - 1) * kSpecialClipsPerModel +
            kSpecialModelClipBase;
  }
  if (index >= limits.special_entries) {
    return std::nullopt;
  }
  return index;
}

[[nodiscard]] AnimationHeaderStatus HeaderStatus(ClipLookupStatus status) {
  switch (status) {
    case ClipLookupStatus::kFound:
      return AnimationHeaderStatus::kFound;
    case ClipLookupStatus::kNoClip:
      return AnimationHeaderStatus::kNoClip;
    case ClipLookupStatus::kMapOutsideBank:
      return AnimationHeaderStatus::kMapOutsideBank;
    case ClipLookupStatus::kHeaderOutsideBank:
      break;
  }
  return AnimationHeaderStatus::kHeaderOutsideBank;
}

// The scale key a scale track starts with: a packed vector (FE 01 and a
// halved initial vector) or three raw bytes.
[[nodiscard]] std::optional<PoseVector> DecodeInitialScale(
    std::span<const std::uint8_t> bank, std::size_t offset, int base) {
  if (!Fits(bank, offset, kRawScaleBytes)) {
    return std::nullopt;
  }
  const bool own_base = base == kNoClip;
  if (bank[offset] == kPackedScaleTag &&
      bank[offset + 1] == kPackedScaleVersion) {
    auto key = DecodeInitialPoseVector(bank, offset + 2, base);
    if (!key) {
      return std::nullopt;
    }
    key->x = static_cast<std::int16_t>(key->x >> 1);
    key->y = static_cast<std::int16_t>(key->y >> 1);
    key->z = static_cast<std::int16_t>(key->z >> 1);
    key->word = own_base ? kOwnBasePackedScaleCursor : 0;
    return key;
  }
  return PoseVector{.x = bank[offset],
                    .y = bank[offset + 1],
                    .z = bank[offset + 2],
                    .word = static_cast<std::uint16_t>(
                        own_base ? kOwnBaseRawScaleCursor : 0)};
}

// Decodes the key at header word `index` into `key`, clearing the key's
// delta cursor as the original's paired store did.
[[nodiscard]] bool DecodeKey(std::span<const std::uint8_t> bank,
                             std::size_t header, std::size_t index, int base,
                             PoseVector& key, PoseVector& delta) {
  const auto data = MotionData(bank, header, index);
  const auto decoded =
      data ? DecodeInitialPoseVector(bank, *data, base) : std::nullopt;
  if (!decoded) {
    return false;
  }
  key = *decoded;
  delta.word = 0;
  return true;
}

[[nodiscard]] bool DecodeEvents(const AnimationClipRef& clip,
                                AnimationPoseState& state) {
  const ClipEvents events = FindClipEvents(clip);
  switch (events.status) {
    case ClipEventsStatus::kStarts:
      state.next_event_frame = events.cursor.frame;
      state.event_offset = events.cursor.offset;
      return true;
    case ClipEventsStatus::kNone:
      state.next_event_frame = kNoEvent;
      return true;
    case ClipEventsStatus::kOutOfBank:
      return false;
  }
  return false;
}

[[nodiscard]] bool DecodeScales(std::span<const std::uint8_t> bank,
                                std::size_t header, int bones, int base,
                                SkeletalPose& pose) {
  const std::size_t count = PoseBoneCount(bones);
  if (!Fits(bank, header + kFlagsByte, 1)) {
    return false;
  }
  const bool has_scales = (bank[header + kFlagsByte] & kHasScaleTracks) != 0;
  for (std::size_t i = 0; i < count; ++i) {
    if (has_scales) {
      const auto data = MotionData(
          bank, header, static_cast<std::size_t>(bones) + kFirstBoneWord + i);
      const auto key =
          data ? DecodeInitialScale(bank, *data, base) : std::nullopt;
      if (!key) {
        return false;
      }
      pose.keys.scales[i] = *key;
    } else {
      pose.keys.scales[i] = {.x = kUnitBoneScale,
                             .y = kUnitBoneScale,
                             .z = kUnitBoneScale,
                             .word = 0};
    }
    pose.scale_deltas[i].word = 0;
  }
  return true;
}

[[nodiscard]] bool DecodeClipStart(const AnimationClipRef& clip,
                                   AnimationPoseState& state) {
  const std::span<const std::uint8_t> bank = clip.bank;
  std::size_t header = clip.header_offset;
  SkeletalPose& pose = state.pose;
  if (!DecodeEvents(clip, state)) {
    return false;
  }
  const auto root_scale = HeaderWord(bank, header, kRootScaleWord);
  if (!root_scale ||
      (*root_scale != 0 && !DecodeKey(bank, header, kRootScaleWord, kNoClip,
                                      pose.keys.scales[kRootPoseKey],
                                      pose.scale_deltas[kRootPoseKey]))) {
    return false;
  }
  if (!Fits(bank, header + kBaseClipByte, 1)) {
    return false;
  }
  const int base = bank[header + kBaseClipByte];
  if (base != kNoClip) {
    header = kSequenceBankHeaderBytes +
             ClipHeaderBytes(clip.bones) * static_cast<std::size_t>(base);
  }
  if (!DecodeKey(bank, header, kRootRotationWord, base,
                 pose.keys.rotations[kRootPoseKey],
                 pose.rotation_deltas[kRootPoseKey])) {
    return false;
  }
  const std::size_t count = PoseBoneCount(clip.bones);
  for (std::size_t i = 0; i < count; ++i) {
    if (!DecodeKey(bank, header, kFirstBoneWord + i, base,
                   pose.keys.rotations[i], pose.rotation_deltas[i])) {
      return false;
    }
  }
  return DecodeScales(bank, header, clip.bones, base, pose);
}

[[nodiscard]] std::int16_t HalfwayRotation(std::int16_t destination,
                                           std::int16_t source) {
  int difference = source - destination;
  if (difference >= kTurn) {
    difference -= kKeyRange;
  } else if (difference < -kTurn) {
    difference += kKeyRange;
  }
  return WrapSigned16(
      static_cast<std::uint32_t>(destination + (difference >> 1)));
}

[[nodiscard]] std::int16_t Average(std::int16_t first, std::int16_t second) {
  return static_cast<std::int16_t>((first + second) >> 1);
}

[[nodiscard]] int BranchDistance(std::int16_t destination,
                                 std::int16_t source) {
  int difference = source - destination;
  if (difference >= kTurn) {
    difference -= kKeyRange;
  } else if (difference < -kTurn) {
    difference += kKeyRange;
  }
  return difference < 0 ? -difference : difference;
}

// One frame of a value tweened over `frames`: the share of `remaining` it
// moves by.
void StepTween(std::int16_t& remaining, std::int16_t& value, int frames) {
  const int step = remaining / frames;
  remaining = static_cast<std::int16_t>(remaining - step);
  value = WrapSigned16(static_cast<std::uint32_t>(value + step));
}

void StepTurn(std::int16_t& remaining, std::int16_t& angle, int frames) {
  const int step = remaining / frames;
  remaining = static_cast<std::int16_t>(remaining - step);
  angle = static_cast<std::int16_t>((angle + step) & kAngleMask);
}

[[nodiscard]] std::uint8_t TileOf(std::int16_t coordinate) {
  return static_cast<std::uint8_t>(coordinate / kTileUnits);
}

[[nodiscard]] bool TracksHead(const HeadLookContext& context) {
  int animation = context.animation_id;
  if (animation >= kAnimationBankSpan) {
    animation -= kAnimationBankSpan;
  }
  if (context.is_jumping) {
    return static_cast<unsigned>(animation - kFirstJumpTrackAnimation) <
           kJumpTrackAnimations;
  }
  if (context.motion_locked) {
    return static_cast<unsigned>(animation - kFirstLockedTrackAnimation) <
           kLockedTrackAnimations;
  }
  for (const int tracked : kFreeTrackAnimations) {
    if (animation == tracked) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] int AbsoluteSquare(int value) {
  const std::int32_t square = Core::Math::WrappingMultiply(value, value);
  return square < 0 ? Core::Math::WrappingNegate(square) : square;
}

// The yaw and pitch towards the target before wrapping, as full words.
struct LookTurn {
  int yaw = 0;
  int pitch = 0;
};

// The yaw and pitch from the actor towards its target, before wrapping.
[[nodiscard]] std::optional<LookTurn> RawLookAngles(
    const HeadLook& look, const HeadLookContext& context) {
  if (look.target == kLookAbsolute) {
    return LookTurn{.yaw = look.tracking.y, .pitch = look.tracking.x};
  }
  if (look.target == kLookRelative) {
    return LookTurn{.yaw = look.tracking.y - context.facing,
                    .pitch = look.tracking.x - context.rotation_x};
  }
  std::optional<MotionVector> point;
  if (look.target == kLookAtPoint) {
    point = look.tracking;
  } else {
    point = context.target_position;
  }
  if (!point) {
    return std::nullopt;
  }
  const int dx = point->x - context.position.x;
  const int dz = point->z - context.position.z;
  const int dy = point->y - context.position.y;
  const int distance = Core::Math::SquareRootInteger(
      Core::Math::WrappingAdd(AbsoluteSquare(dx), AbsoluteSquare(dz)));
  return LookTurn{
      .yaw = ((kLookForward - Core::Math::Arctangent2(dz, dx)) & kAngleMask) -
             context.facing,
      .pitch =
          (Core::Math::Arctangent2(distance, dy) - kQuarterTurn) & kAngleMask};
}

[[nodiscard]] int WrapLook(int angle) {
  if (angle >= kHalfTurn) {
    angle -= kAngleOne;
  }
  if (angle < -kHalfTurn) {
    angle += kAngleOne;
  }
  if (angle > kLookLimit) {
    angle = kLookLimit;
  }
  if (angle < -kLookLimit) {
    angle = -kLookLimit;
  }
  return angle;
}

[[nodiscard]] int TurnLimited(int target, std::int16_t current, int limit) {
  int turn = target - current;
  if (turn > limit) {
    turn = limit;
  }
  if (turn < -limit) {
    turn = -limit;
  }
  return current + turn;
}

// Turns the look towards (yaw, pitch) by at most the tracking limit.
[[nodiscard]] LookAngles ApplyLook(HeadLook& look, int yaw, int pitch) {
  const int limit = look.tracking.w;
  look.yaw = WrapSigned16(
      static_cast<std::uint32_t>(TurnLimited(yaw, look.yaw, limit)));
  look.pitch = WrapSigned16(
      static_cast<std::uint32_t>(TurnLimited(pitch, look.pitch, limit)));
  return LookAngles{.yaw = look.yaw, .pitch = look.pitch};
}
}  // namespace

std::size_t ClipHeaderBytes(int bones) {
  return kClipHeaderFixedBytes +
         (kClipHeaderBytesPerBone *
          static_cast<std::size_t>(bones < 0 ? 0 : bones));
}

std::optional<AnimationEntry> SelectAnimationEntry(
    int animation_id, const AnimationBankLimits& limits) {
  if (animation_id < 0) {
    return std::nullopt;
  }
  if (animation_id == kOwnedAnimation) {
    return AnimationEntry{.bank = AnimationBank::kOwned};
  }
  if (animation_id == kSpecialAction || animation_id == kSpecialModelAction) {
    const auto index = SpecialIndex(animation_id, limits);
    if (!index) {
      return std::nullopt;
    }
    return AnimationEntry{.bank = AnimationBank::kShared,
                          .shared_slot = kSpecialSharedSlot,
                          .index = *index};
  }
  if (animation_id >= kFirstSharedAnimation) {
    return AnimationEntry{.bank = AnimationBank::kShared,
                          .shared_slot = limits.shared_slot,
                          .index = animation_id - kFirstSharedAnimation};
  }
  if (animation_id >= kFirstWorkAnimation) {
    return AnimationEntry{.bank = AnimationBank::kWork,
                          .index = animation_id - kFirstWorkAnimation};
  }
  if (animation_id >= kFirstBattleAnimation) {
    const int index = animation_id - kFirstBattleAnimation;
    if (index >= limits.battle_entries) {
      return std::nullopt;
    }
    return AnimationEntry{.bank = AnimationBank::kBattle, .index = index};
  }
  if (animation_id >= limits.common_entries) {
    return std::nullopt;
  }
  return AnimationEntry{.bank = AnimationBank::kCommon, .index = animation_id};
}

ClipLookup LookupAnimationClip(std::span<const std::uint8_t> bank, int index,
                               int bones) {
  ClipLookup lookup{};
  const auto map = ReadWord(bank, kMapOffsetField);
  if (!map || index < 0 ||
      !Fits(bank, *map + static_cast<std::size_t>(index), 1)) {
    return lookup;
  }
  lookup.clip = bank[*map + static_cast<std::size_t>(index)];
  if (lookup.clip == kNoClip) {
    lookup.status = ClipLookupStatus::kNoClip;
    return lookup;
  }
  const std::size_t header_bytes = ClipHeaderBytes(bones);
  const std::size_t header =
      kSequenceBankHeaderBytes +
      header_bytes * static_cast<std::size_t>(lookup.clip);
  if (!Fits(bank, header, header_bytes)) {
    lookup.status = ClipLookupStatus::kHeaderOutsideBank;
    return lookup;
  }
  lookup.status = ClipLookupStatus::kFound;
  lookup.header_offset = header;
  return lookup;
}

// decomp: BATTLE.PRG func_800AD494 (src/BATTLE/BATTLE.PRG/40564.c:2719)
// The bank comes from its owner's registered extent instead of a pointer, and
// a map entry or header past that extent is refused instead of read.
AnimationHeader FindActorAnimationHeader(const AnimationBankLimits& limits,
                                         AnimationBankSource& banks,
                                         int animation_id) {
  AnimationHeader header{};
  const auto entry = SelectAnimationEntry(animation_id, limits);
  if (!entry) {
    return header;
  }
  header.entry = *entry;
  const AnimationBankView view = banks.Bank(*entry);
  if (!view.loaded) {
    header.status = AnimationHeaderStatus::kNoBank;
    return header;
  }
  if (view.bytes.empty()) {
    header.status = AnimationHeaderStatus::kUnregisteredBank;
    return header;
  }
  header.bank = view.bytes;
  const ClipLookup lookup =
      LookupAnimationClip(view.bytes, entry->index, limits.bones);
  header.status = HeaderStatus(lookup.status);
  header.clip = lookup.clip;
  header.header_offset = lookup.header_offset;
  return header;
}

std::optional<EventCursor> ReadEventCursor(std::span<const std::uint8_t> bytes,
                                           std::size_t offset) {
  if (!Fits(bytes, offset, 1)) {
    return std::nullopt;
  }
  if (bytes[offset] != kLongFrame) {
    return EventCursor{.frame = bytes[offset], .offset = offset + 1};
  }
  if (!Fits(bytes, offset + 1, 1)) {
    return std::nullopt;
  }
  return EventCursor{
      .frame = static_cast<std::uint16_t>(bytes[offset + 1] + kLongFrame),
      .offset = offset + 2};
}

// The event word and first cursor read by BATTLE.PRG func_800AD008
// (40564.c:2623) and func_800AE47C (44F14.c:610).
ClipEvents FindClipEvents(const AnimationClipRef& clip) {
  const auto events = HeaderWord(clip.bank, clip.header_offset, kEventsWord);
  if (!events) {
    return {};
  }
  if (*events == 0) {
    return {.status = ClipEventsStatus::kNone};
  }
  const auto data = MotionData(clip.bank, clip.header_offset, kEventsWord);
  const auto cursor = data ? ReadEventCursor(clip.bank, *data) : std::nullopt;
  if (!cursor) {
    return {};
  }
  return {.status = ClipEventsStatus::kStarts, .cursor = *cursor};
}

// decomp: BATTLE.PRG func_800ACFA0 (src/BATTLE/BATTLE.PRG/40564.c:2561)
// The original also cleared the key's delta cursor 339 halfwords on; callers
// clear the paired delta themselves.
std::optional<PoseVector> DecodeInitialPoseVector(
    std::span<const std::uint8_t> bank, std::size_t offset, int base) {
  if (!Fits(bank, offset, kEncodedVectorBytes)) {
    return std::nullopt;
  }
  const auto component = [&](std::size_t index) {
    const std::size_t at = offset + index * sizeof(std::uint16_t);
    return DecodeComponent(bank[at], bank[at + 1]);
  };
  return PoseVector{
      .x = component(0),
      .y = component(1),
      .z = component(2),
      .word = base == kNoClip ? kOwnBaseKeyCursor : std::uint16_t{0}};
}

// decomp: BATTLE.PRG func_800AD008 (src/BATTLE/BATTLE.PRG/40564.c:2623)
// The pose-override unwind and the header lookup stay with the caller. Keys
// decode into a copy that is committed only when the whole clip start lay
// inside its bank, and pose writes stop at the pose's bone capacity.
PoseStart InitializeActorAnimationPose(
    const std::optional<AnimationClipRef>& clip, std::uint8_t animation_id,
    AnimationPoseState& state) {
  state.frame = 0;
  state.event_wait = 0;
  state.pose.keys.scales[kRootPoseKey] = {};
  state.pose.scale_deltas[kRootPoseKey].word = 0;
  if (!clip) {
    return PoseStart::kNoClip;
  }
  AnimationPoseState next = state;
  next.holding_last_frame = false;
  next.animation_id = animation_id;
  if (!DecodeClipStart(*clip, next)) {
    return PoseStart::kOutOfBank;
  }
  state = next;
  return PoseStart::kInitialized;
}

// decomp: BATTLE.PRG func_800AE980 (src/BATTLE/BATTLE.PRG/44F14.c:781)
void BlendActorPoseHalfway(PoseKeys& destination, const PoseKeys& source,
                           int bones) {
  PoseVector& root = destination.rotations[kRootPoseKey];
  const PoseVector& source_root = source.rotations[kRootPoseKey];
  root.x = Average(root.x, source_root.x);
  root.y = Average(root.y, source_root.y);
  root.z = Average(root.z, source_root.z);
  const std::size_t count = PoseBoneCount(bones);
  for (std::size_t i = 0; i < count; ++i) {
    PoseVector& rotation = destination.rotations[i];
    rotation.x = HalfwayRotation(rotation.x, source.rotations[i].x);
    rotation.y = HalfwayRotation(rotation.y, source.rotations[i].y);
    rotation.z = HalfwayRotation(rotation.z, source.rotations[i].z);
    PoseVector& scale = destination.scales[i];
    scale.x = Average(scale.x, source.scales[i].x);
    scale.y = Average(scale.y, source.scales[i].y);
    scale.z = Average(scale.z, source.scales[i].z);
  }
}

// decomp: BATTLE.PRG func_800AF844 (src/BATTLE/BATTLE.PRG/44F14.c:1192)
void AlignPoseRotationBranches(PoseKeyArray& destination,
                               const PoseKeyArray& source, int bones) {
  const std::size_t count = PoseBoneCount(bones);
  for (std::size_t i = 0; i < count; ++i) {
    PoseVector& key = destination[i];
    if (BranchDistance(key.x, source[i].x) +
            BranchDistance(key.z, source[i].z) <
        kTurn) {
      continue;
    }
    key.x = static_cast<std::int16_t>((key.x + kTurn) % kKeyRange);
    key.z = static_cast<std::int16_t>((key.z + kTurn) % kKeyRange);
    key.y = static_cast<std::int16_t>((kTurn - key.y) % kKeyRange);
  }
}

// decomp: BATTLE.PRG func_800AF6E8 (src/BATTLE/BATTLE.PRG/44F14.c:1146)
void UpdateActorAnimationBlend(AnimationBlendHost& host) {
  host.AdvanceAnimation();
  BlendClock clock = host.Clock();
  if (clock.frame != 0) {
    if (clock.frame == 1) {
      host.AlignBlendRotations();
    }
    const int frames_per_step = host.FramesPerStep();
    if (frames_per_step != 1) {
      // The original added 0xFF and the step to the signed byte: a step of
      // frames_per_step - 1 frames.
      clock.frame = static_cast<std::int8_t>(clock.frame - 1 + frames_per_step);
      host.SetClock(clock);
      if (clock.frame >= clock.frames) {
        host.SetClock({});
        host.CopyPoseToBlend();
        host.UpdateMotion();
        return;
      }
    }
    host.BlendTowardPose(clock);
    host.UpdateMotion();
    clock = host.Clock();
    clock.frame = static_cast<std::int8_t>(clock.frame + 1);
    host.SetClock(clock.frame >= clock.frames ? BlendClock{} : clock);
    return;
  }
  host.CopyPoseToBlend();
  host.UpdateMotion();
}

MotionTweenStep StepActorMotionTweens(ActorMotion& motion) {
  MotionTweenStep step{};
  if (motion.scale_frames != 0) {
    const int frames = motion.scale_frames;
    StepTween(motion.scale_delta.x, motion.scale.x, frames);
    StepTween(motion.scale_delta.y, motion.scale.y, frames);
    StepTween(motion.scale_delta.z, motion.scale.z, frames);
    --motion.scale_frames;
  }
  if (motion.turn_frames != 0) {
    const int frames = motion.turn_frames;
    StepTurn(motion.rotation_step_x, motion.rotation_x, frames);
    StepTurn(motion.facing_delta, motion.facing, frames);
    StepTurn(motion.rotation_step_z, motion.rotation_z, frames);
    --motion.turn_frames;
  }
  if (motion.move_frames == 0) {
    return step;
  }
  const int frames = motion.move_frames;
  StepTween(motion.root_motion.x, motion.position.x, frames);
  StepTween(motion.root_motion.y, motion.position.y, frames);
  StepTween(motion.root_motion.z, motion.position.z, frames);
  if (--motion.move_frames != 0) {
    return step;
  }
  if (motion.locomotion_state == kLocomotionStepping) {
    motion.locomotion_state = 0;
    motion.movement_pending = false;
    step.locomotion_finished = true;
  }
  if (motion.locomotion_state != kLocomotionHeld) {
    motion.current_tile.x = TileOf(motion.position.x);
    motion.current_tile.z = TileOf(motion.position.z);
    step.tile_settled = true;
  }
  return step;
}

std::optional<LookAngles> UpdateHeadLook(HeadLook& look,
                                         const HeadLookContext& context) {
  if (look.target == kNoHeadTrack) {
    if (look.yaw == 0 && look.pitch == 0) {
      return LookAngles{};
    }
    return ApplyLook(look, 0, 0);
  }
  if (!context.event_playback_active) {
    if (context.was_drawn) {
      return std::nullopt;
    }
    if (!TracksHead(context)) {
      return ApplyLook(look, 0, 0);
    }
  }
  const auto target = RawLookAngles(look, context);
  if (!target) {
    return ApplyLook(look, 0, 0);
  }
  return ApplyLook(look, WrapLook(target->yaw), WrapLook(target->pitch));
}

// decomp: BATTLE.PRG func_800B002C (src/BATTLE/BATTLE.PRG/44F14.c:1486)
// The root offset and look angles go to the host's model workspace; a slot
// target without a record turns the head back to rest instead of reading a
// null record.
void UpdateMotionAndLookAt(MotionLookHost& host, bool skip_tweens) {
  ActorMotion motion = host.LoadMotion();
  host.SetRootOffset(ActorRootOffset(
      host.RootKey(), Vec3{motion.scale.y, motion.scale.z, motion.scale.x},
      host.ActorScale()));
  if (!skip_tweens) {
    const MotionTweenStep step = StepActorMotionTweens(motion);
    host.StoreMotion(motion);
    if (host.PreviewPose()) {
      host.CenterPreviewPose();
    }
    if (step.locomotion_finished) {
      host.FinishLocomotion();
    }
    if (step.tile_settled) {
      host.SettleOnTile(motion.current_tile.x, motion.current_tile.z);
    }
  }
  host.TransformRootOffset();
  motion = host.LoadMotion();
  if (motion.movement_mode == 0 && motion.locomotion_state == 0 &&
      !motion.motion_locked) {
    host.SaveGroundedTransform();
  }
  HeadLook look = host.LoadHeadLook();
  const auto angles = UpdateHeadLook(look, host.LookContext(look));
  if (!angles) {
    return;
  }
  host.StoreHeadLook(look);
  host.SetLookAngles(*angles);
}

// decomp: BATTLE.PRG func_800B26B8 (src/BATTLE/BATTLE.PRG/49268.c:246)
// Returns the transform instead of leaving it in the model workspace and the
// GTE registers, which nothing read afterwards.
Core::Math::Transform3 ApplyLockedBoneTransform(
    const Core::Math::Transform3& parent,
    const Core::Math::Transform3& locked) {
  return Core::Math::Transform3{
      .rotation = Core::Math::PremultiplyRotationMatrix(parent.rotation,
                                                        locked.rotation),
      .translation = Core::Math::RotateTranslate(parent, locked.translation)};
}
}  // namespace VS::Actors
