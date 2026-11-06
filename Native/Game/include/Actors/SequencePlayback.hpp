#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// Skeletal SEQ playback: stepping an actor's animation clock, decoding the
// packed keyframe tracks of a SEQ bank into a pose, and blending two poses.
// Every SEQ read is bounded by an explicit byte extent and every pose write by
// the pose's fixed key count (#261).
namespace VS::Actors {
// Pose keys per array; the last key is the root.
inline constexpr std::size_t kPoseKeyCount = 42;
inline constexpr std::size_t kRootPoseKey = kPoseKeyCount - 1;
// The bones a pose can animate besides its root.
inline constexpr std::size_t kMaxPoseBones = kRootPoseKey;

// One pose key. `word` is the track cursor: the byte offset of the next
// command for a value key, or the frame the current delta lasts until for a
// delta key.
struct PoseVector {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::uint16_t word = 0;
};

using PoseKeyArray = std::array<PoseVector, kPoseKeyCount>;

// The rotation and scale keys blended between two poses.
struct PoseKeys {
  PoseKeyArray rotations{};
  PoseKeyArray scales{};
};

// A pose's keys and the per-key deltas its tracks are decoding.
struct SkeletalPose {
  PoseKeys keys{};
  PoseKeyArray rotation_deltas{};
  PoseKeyArray scale_deltas{};
};

// The inclusive frame range a decode advances through.
struct TrackFrames {
  int first = 0;
  int last = 0;
};

enum class TrackKind : std::uint8_t { kLinear, kRotation };

enum class TrackDecode : std::uint8_t {
  // Every frame in range was applied and the cursors were stored.
  kComplete,
  // The track's terminator was reached; the cursors were left unchanged.
  kEnded,
  // A cursor or command would read past the track; decoding stopped there
  // and the cursors were left unchanged.
  kOverrun,
};

// Advances `value` through `frames` by the deltas decoded from `track`, the
// bytes from the track's start to the end of its SEQ bank.
TrackDecode DecodeAnimationTrack(std::span<const std::uint8_t> track,
                                 TrackFrames frames, PoseVector& value,
                                 PoseVector& delta, TrackKind kind);

// One clip of a SEQ bank: its header words (from the header to the bank end)
// and its packed motion data (from the bank's motion offset to the bank end).
struct AnimationClip {
  std::span<const std::uint8_t> header;
  std::span<const std::uint8_t> motion;
};

// The root translation decoded for a frame range, already scaled to world
// units.
struct RootStep {
  int x = 0;
  int y = 0;
  int z = 0;
};

struct PoseTrackResult {
  bool moved = false;
  RootStep step{};
  // A header word or track crossed its extent. That track was left
  // unchanged; the other tracks were still decoded.
  bool overrun = false;
};

// Decodes the root, bone rotation and, when the clip has them, bone scale
// tracks of `clip` for `bones` bones (clamped to kMaxPoseBones).
PoseTrackResult ApplyPoseTracks(const AnimationClip& clip, int bones,
                                TrackFrames frames, SkeletalPose& pose);

// The animated bones a pose of `bones` bones can hold.
std::size_t PoseBoneCount(int bones);

// An actor's shadow fade and radius blend, stepped once per animation frame.
struct ShadowBlend {
  std::uint8_t level_frames = 0;
  std::uint8_t radius_frames = 0;
  std::uint16_t level = 0;
  std::int16_t level_delta = 0;
  std::uint16_t radius_expand_rate = 0;
  std::uint16_t radius_shrink_rate = 0;
  std::int16_t radius_expand_delta = 0;
  std::int16_t radius_shrink_delta = 0;

  friend bool operator==(const ShadowBlend&, const ShadowBlend&) = default;
};

void AdvanceShadowBlend(ShadowBlend& shadow);

// The actor and pose state the animation clock steps through. Events and a
// queued animation can change any of it, so it is read through the host on
// every step instead of copied up front.
class SkeletalAnimationHost {
 public:
  virtual ~SkeletalAnimationHost() = default;
  virtual int FramesPerStep() const = 0;
  virtual int Frame() const = 0;
  virtual void SetFrame(int frame) = 0;
  virtual int AnimationLength() const = 0;
  virtual void AdvanceShadow() = 0;
  virtual void SetReachedEnd(bool reached) = 0;
  // Starts the queued animation, if any, and reports whether one started.
  virtual bool PlayQueuedAnimation() = 0;
  virtual bool EventsDue() const = 0;
  virtual void RunEvents() = 0;
  virtual bool HoldingLastFrame() const = 0;
  virtual void HoldLastFrame() = 0;
  virtual bool WaitingForEvent() const = 0;
  virtual void ApplyPoseTracks(TrackFrames frames) = 0;
};

// Steps the animation clock by the actor's frames per step, then applies the
// tracks for the frames it passed.
void AdvanceSkeletalAnimation(SkeletalAnimationHost& host);

enum class PoseBlend : std::uint8_t {
  // The blend is complete: the caller copies the whole source pose.
  kCopySource,
  // The blend is at its midpoint: the caller averages the two poses.
  kHalfway,
  // The destination keys were blended towards the source keys.
  kWeighted,
  // A zero-length blend past its first frame; nothing was blended.
  kSkipped,
};

// Blends `destination` towards `source` by `frame` of `frames` for `bones`
// bones (clamped to kMaxPoseBones) and the root.
PoseBlend BlendActorPose(PoseKeys& destination, const PoseKeys& source,
                         int bones, int frame, int frames);
}  // namespace VS::Actors
