#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorMotion.hpp"
#include "Actors/MotionRequests.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// Finding, starting and blending an actor's SEQ animation pose (BATTLE.PRG
// 40564.c, 44F14.c and 49268.c): the bank and map entry an animation id
// names, the clip header a map entry selects, the keys a clip header starts a
// pose with, the halfway and branch-aligned blends between two poses, the
// per-frame blend clock, the motion tweens and head look that follow it, and
// the transform of a bone locked to a saved matrix. Every SEQ read is bounded
// by the bank's registered extent and every pose write by the pose's fixed
// key count (kPoseKeyCount); bank offsets replace the on-disc pointers the
// original added to the bank address.
namespace VS::Actors {
// A map entry that names no clip.
inline constexpr int kNoClip = 0xFF;
// The motion data and map offsets that precede a bank's clip headers.
inline constexpr std::size_t kSequenceBankHeaderBytes = 8;

// The bytes of one clip header for `bones` bones: the length, base clip and
// flags, event, root and scale words, then a rotation track offset and a
// scale track offset per bone.
[[nodiscard]] std::size_t ClipHeaderBytes(int bones);

// The SEQ bank an animation id selects.
enum class AnimationBank : std::uint8_t {
  kOwned,   // the actor's own clip (id 0)
  kCommon,  // the common bank (ids 1..99)
  kBattle,  // the battle bank (ids 100..201)
  kShared,  // a shared bank slot (ids 220.. and the special ids 254/255)
  kWork,    // the sequence work buffer (ids 202..219)
};

// The actor fields that bound each bank's map, select a special clip and size
// the clip headers.
struct AnimationBankLimits {
  // The actor's bone count, which sizes every clip header of its banks.
  std::uint8_t bones = 0;
  std::uint8_t slot = 0;
  std::uint16_t model_id = 0;
  std::uint8_t sequence_model_id = 0;
  // The map entries of the common, battle and special (shared slot 3) banks.
  std::uint8_t common_entries = 0;
  std::uint8_t battle_entries = 0;
  std::uint8_t special_entries = 0;
  // The special action's pitch (Battle_specialActionPitch).
  std::uint8_t special_pitch = 0;
  // The selected shared bank (Actors_sharedSequenceBankIndex).
  std::uint8_t shared_slot = 0;
};

// A bank and the map index an animation id names in it.
struct AnimationEntry {
  AnimationBank bank = AnimationBank::kOwned;
  int shared_slot = 0;
  int index = 0;

  friend bool operator==(const AnimationEntry&,
                         const AnimationEntry&) = default;
};

// The shared slot that holds the special action and break art clips.
inline constexpr int kSpecialSharedSlot = 3;

// The entry `animation_id` names, or nothing when the id lies past its
// bank's map entries, needs a sequence model the actor lacks, or is
// negative.
[[nodiscard]] std::optional<AnimationEntry> SelectAnimationEntry(
    int animation_id, const AnimationBankLimits& limits);

enum class ClipLookupStatus : std::uint8_t {
  kFound,
  // The map entry names no clip.
  kNoClip,
  // The bank is too short for its map offset or the map entry.
  kMapOutsideBank,
  // The clip's header runs past the bank.
  kHeaderOutsideBank,
};

struct ClipLookup {
  ClipLookupStatus status = ClipLookupStatus::kMapOutsideBank;
  // The map entry; kNoClip when the map named none. Valid unless the map
  // lies outside the bank.
  int clip = kNoClip;
  // The clip header's offset in the bank, when found.
  std::size_t header_offset = 0;
};

// Reads map entry `index` of `bank` and locates the clip header it names for
// `bones` bones.
[[nodiscard]] ClipLookup LookupAnimationClip(std::span<const std::uint8_t> bank,
                                             int index, int bones);

// A bank as its owner holds it: whether one is loaded, and its registered
// bytes (empty when loaded without a recorded extent).
struct AnimationBankView {
  bool loaded = false;
  std::span<const std::uint8_t> bytes;
};

class AnimationBankSource {
 public:
  AnimationBankSource() = default;
  AnimationBankSource(const AnimationBankSource&) = delete;
  AnimationBankSource& operator=(const AnimationBankSource&) = delete;
  AnimationBankSource(AnimationBankSource&&) = delete;
  AnimationBankSource& operator=(AnimationBankSource&&) = delete;
  virtual ~AnimationBankSource() = default;

  [[nodiscard]] virtual AnimationBankView Bank(const AnimationEntry& entry) = 0;
};

enum class AnimationHeaderStatus : std::uint8_t {
  kFound,
  kNoClip,
  kNoEntry,
  kNoBank,
  kUnregisteredBank,
  kMapOutsideBank,
  kHeaderOutsideBank,
};

struct AnimationHeader {
  AnimationHeaderStatus status = AnimationHeaderStatus::kNoEntry;
  AnimationEntry entry{};
  std::span<const std::uint8_t> bank;
  // The map entry read, when the map lay inside the bank.
  int clip = kNoClip;
  std::size_t header_offset = 0;
};

// decomp: BATTLE.PRG func_800AD494. Resolves an animation id to its bank and
// clip header.
[[nodiscard]] AnimationHeader FindActorAnimationHeader(
    const AnimationBankLimits& limits, AnimationBankSource& banks,
    int animation_id);

// An event script position: the frame of its next event and the offset of
// that event's opcode.
struct EventCursor {
  std::uint16_t frame = 0;
  std::size_t offset = 0;
};

// The cursor at `offset`: one frame byte, or 0xFF and a byte the frame
// exceeds 0xFF by. Nothing when the bytes run past `bytes`.
[[nodiscard]] std::optional<EventCursor> ReadEventCursor(
    std::span<const std::uint8_t> bytes, std::size_t offset);

// decomp: BATTLE.PRG func_800ACFA0. The key a six-byte big-endian vector at
// `offset` starts with: each component doubled, and a cursor word of 6 for a
// clip that is its own base (`base` == kNoClip). Nothing when the vector runs
// past `bank`.
[[nodiscard]] std::optional<PoseVector> DecodeInitialPoseVector(
    std::span<const std::uint8_t> bank, std::size_t offset, int base);

// A found clip: its bank's registered bytes, its header offset and the
// actor's bone count, which sizes every header of the bank.
struct AnimationClipRef {
  std::span<const std::uint8_t> bank;
  std::size_t header_offset = 0;
  int bones = 0;
};

enum class ClipEventsStatus : std::uint8_t {
  // The clip has an event script; the cursor is its first event.
  kStarts,
  // The clip's event word is 0: it has no events.
  kNone,
  // The event word or the first cursor lies outside the bank.
  kOutOfBank,
};

struct ClipEvents {
  ClipEventsStatus status = ClipEventsStatus::kOutOfBank;
  EventCursor cursor{};
};

// The first event of `clip`'s event script, which its header's event word
// locates in the motion data.
[[nodiscard]] ClipEvents FindClipEvents(const AnimationClipRef& clip);

// The pose fields a clip start resets and decodes.
struct AnimationPoseState {
  SkeletalPose pose{};
  std::int16_t frame = 0;
  std::uint16_t next_event_frame = 0;
  // The event script's offset in the clip's bank; nothing leaves the script
  // where it was, as a clip without events does.
  std::optional<std::size_t> event_offset;
  std::uint8_t event_wait = 0;
  bool holding_last_frame = false;
  std::uint8_t animation_id = 0;
};

enum class PoseStart : std::uint8_t {
  kInitialized,
  // No clip was found; only the clock and root scale were reset.
  kNoClip,
  // A header word or keyframe of the clip lies outside its bank; only the
  // clock and root scale were reset.
  kOutOfBank,
};

// The no-motion bone scale a clip without scale tracks starts with.
inline constexpr std::int16_t kUnitBoneScale = 0x40;

// decomp: BATTLE.PRG func_800AD008. Resets the pose clock and root scale,
// then starts the pose at `clip`'s first keys and events. The decoded keys
// are committed only when every read lay inside the bank.
PoseStart InitializeActorAnimationPose(
    const std::optional<AnimationClipRef>& clip, std::uint8_t animation_id,
    AnimationPoseState& state);

// decomp: BATTLE.PRG func_800AE980. Moves the root, the first `bones` bone
// rotations (along the shorter way round) and their scales halfway towards
// `source`.
void BlendActorPoseHalfway(PoseKeys& destination, const PoseKeys& source,
                           int bones);

// decomp: BATTLE.PRG func_800AF844. Flips each of the first `bones` rotations
// to its equivalent Euler branch when its x and z are a half turn or more
// from `source`, so a blend takes the short path.
void AlignPoseRotationBranches(PoseKeyArray& destination,
                               const PoseKeyArray& source, int bones);

// The blend between the displayed (blend) pose and the playing pose.
struct BlendClock {
  std::int8_t frame = 0;
  std::int8_t frames = 0;

  friend bool operator==(const BlendClock&, const BlendClock&) = default;
};

class AnimationBlendHost {
 public:
  AnimationBlendHost() = default;
  AnimationBlendHost(const AnimationBlendHost&) = delete;
  AnimationBlendHost& operator=(const AnimationBlendHost&) = delete;
  AnimationBlendHost(AnimationBlendHost&&) = delete;
  AnimationBlendHost& operator=(AnimationBlendHost&&) = delete;
  virtual ~AnimationBlendHost() = default;

  // Steps the playing pose's clock and tracks.
  virtual void AdvanceAnimation() = 0;
  [[nodiscard]] virtual BlendClock Clock() const = 0;
  virtual void SetClock(BlendClock clock) = 0;
  [[nodiscard]] virtual int FramesPerStep() const = 0;
  // AlignPoseRotationBranches of the blend pose towards the playing pose.
  virtual void AlignBlendRotations() = 0;
  // Blends the displayed pose towards the playing pose by `clock` and copies
  // the playing pose's end flags.
  virtual void BlendTowardPose(BlendClock clock) = 0;
  // Displays the playing pose as it is.
  virtual void CopyPoseToBlend() = 0;
  // UpdateMotionAndLookAt without skipping the tweens.
  virtual void UpdateMotion() = 0;
};

// decomp: BATTLE.PRG func_800AF6E8. Advances the playing pose, then steps the
// blend clock and blends the displayed pose, or copies the playing pose once
// no blend is running.
void UpdateActorAnimationBlend(AnimationBlendHost& host);

struct MotionTweenStep {
  // A locomotion step's translation finished: the host reinitializes the
  // animation set.
  bool locomotion_finished = false;
  // The translation finished outside locomotion state 2: current_tile x and
  // z hold the actor's new tile and the host settles it on the tile's
  // gimmick.
  bool tile_settled = false;
};

// Plays one frame of the actor's scale, turn and translation tweens.
[[nodiscard]] MotionTweenStep StepActorMotionTweens(ActorMotion& motion);

// Head tracking targets besides an actor slot (0..16).
inline constexpr int kLookAtPoint = -1;
inline constexpr int kNoHeadTrack = -2;
inline constexpr int kLookRelative = -3;
inline constexpr int kLookAbsolute = -4;

// The record's head tracking: the target, its point or angles (w is the
// per-frame turn limit), and the current look yaw and pitch.
struct HeadLook {
  std::int8_t target = kNoHeadTrack;
  MotionVector tracking{};
  std::int16_t yaw = 0;
  std::int16_t pitch = 0;

  friend bool operator==(const HeadLook&, const HeadLook&) = default;
};

// What the head look reads from the actor and its target.
struct HeadLookContext {
  bool event_playback_active = false;
  bool was_drawn = false;
  int animation_id = 0;
  bool is_jumping = false;
  bool motion_locked = false;
  MotionVector position{};
  std::int16_t rotation_x = 0;
  std::int16_t facing = 0;
  // The tracked actor's position, for a slot target that has a record.
  std::optional<MotionVector> target_position{};
};

struct LookAngles {
  std::int16_t yaw = 0;
  std::int16_t pitch = 0;

  friend bool operator==(const LookAngles&, const LookAngles&) = default;
};

// Turns the head towards its target by at most the turn limit, or back to
// rest when the animation does not track. Returns the look angles for the
// model workspace, or nothing when a drawn actor outside event playback
// keeps its look as it is.
[[nodiscard]] std::optional<LookAngles> UpdateHeadLook(
    HeadLook& look, const HeadLookContext& context);

class MotionLookHost {
 public:
  MotionLookHost() = default;
  MotionLookHost(const MotionLookHost&) = delete;
  MotionLookHost& operator=(const MotionLookHost&) = delete;
  MotionLookHost(MotionLookHost&&) = delete;
  MotionLookHost& operator=(MotionLookHost&&) = delete;
  virtual ~MotionLookHost() = default;

  [[nodiscard]] virtual ActorMotion LoadMotion() const = 0;
  virtual void StoreMotion(const ActorMotion& motion) = 0;
  // The displayed pose's root key and the actor's Q12 scale.
  [[nodiscard]] virtual Core::Math::Vec3 RootKey() const = 0;
  [[nodiscard]] virtual std::int32_t ActorScale() const = 0;
  virtual void SetRootOffset(Core::Math::Vec3 offset) = 0;
  [[nodiscard]] virtual bool PreviewPose() const = 0;
  // Faces both poses' first bone forward for the preview model.
  virtual void CenterPreviewPose() = 0;
  virtual void FinishLocomotion() = 0;
  virtual void SettleOnTile(int tile_x, int tile_z) = 0;
  // Rotates the root offset into the workspace's root transform.
  virtual void TransformRootOffset() = 0;
  virtual void SaveGroundedTransform() = 0;
  [[nodiscard]] virtual HeadLook LoadHeadLook() const = 0;
  virtual void StoreHeadLook(const HeadLook& look) = 0;
  [[nodiscard]] virtual HeadLookContext LookContext(const HeadLook& look) = 0;
  virtual void SetLookAngles(LookAngles angles) = 0;
};

// decomp: BATTLE.PRG func_800B002C. Scales the root offset, plays the motion
// tweens unless `skip_tweens`, places the root and turns the head.
void UpdateMotionAndLookAt(MotionLookHost& host, bool skip_tweens);

// decomp: BATTLE.PRG func_800B26B8. The world transform of a bone locked to
// the saved transform `locked`, under its parent's transform.
[[nodiscard]] Core::Math::Transform3 ApplyLockedBoneTransform(
    const Core::Math::Transform3& parent, const Core::Math::Transform3& locked);
}  // namespace VS::Actors
