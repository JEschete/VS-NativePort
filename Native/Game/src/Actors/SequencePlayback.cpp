// Hand-maintained (tools/port/owned.csv).
// Skeletal SEQ playback over typed poses and bounded SEQ bytes
// (BATTLE.PRG 44F14.c and 48C7C.s).
#include "Actors/SequencePlayback.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
using VS::Core::Math::WrapSigned16;

constexpr int kOne = 4096;
constexpr int kFixedFractionBits = 12;
constexpr std::uint16_t kRotationMask = 0x1FFF;
constexpr std::uint16_t kShadowLevelMask = 0xFFF;

constexpr std::size_t kRootRotationTrack = 3;
constexpr std::size_t kRootTranslationTrack = 4;
constexpr std::size_t kFirstBoneTrack = 5;
constexpr std::size_t kScaleFlagsByte = 3;
constexpr std::uint8_t kHasScaleTracks = 2;
constexpr int kRootTranslationDivisor = 4;
constexpr int kBitsPerByte = 8;
constexpr int kByteSignBit = 0x80;
constexpr int kByteRange = 0x100;

enum class Command : std::uint8_t { kApplied, kEnded, kOverrun };

// A cursor over one track's bytes that refuses to read past them.
class TrackReader {
 public:
  TrackReader(std::span<const std::uint8_t> bytes, std::size_t at)
      : bytes_(bytes), at_(at) {}

  [[nodiscard]] std::size_t At() const { return at_; }

  bool Byte(std::uint8_t& out) {
    if (at_ >= bytes_.size()) {
      return false;
    }
    out = bytes_[at_++];
    return true;
  }

  bool SignedByte(std::int16_t& out) {
    std::uint8_t value = 0;
    if (!Byte(value)) {
      return false;
    }
    const int byte = value;
    out = static_cast<std::int16_t>(
        (byte & kByteSignBit) != 0 ? byte - kByteRange : byte);
    return true;
  }

  // Track words are stored big-endian.
  bool Word(int& out) {
    std::uint8_t high = 0;
    std::uint8_t low = 0;
    if (at_ >= bytes_.size() || bytes_.size() - at_ < sizeof(std::int16_t)) {
      return false;
    }
    Byte(high);
    Byte(low);
    out =
        WrapSigned16((static_cast<std::uint32_t>(high) << kBitsPerByte) | low);
    return true;
  }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t at_;
};

// Decodes one keyframe command: its frame run and the delta axes it sets.
// The delta is committed only when the whole command lies inside the track.
Command DecodeCommand(TrackReader& reader, int& next_frame, PoseVector& delta) {
  enum : int {
    kByteDeltaX = 0x80,
    kByteDeltaY = 0x40,
    kByteDeltaZ = 0x20,
    kByteDeltaAxes = kByteDeltaX | kByteDeltaY | kByteDeltaZ,
    kLongRunMask = 0x1F,
    kShortRunMask = 3,
    kPackedAxisBits = 3,
    kPackedX = 4,
    kPackedY = 2,
    kPackedZ = 1
  };
  std::uint8_t first = 0;
  if (!reader.Byte(first)) {
    return Command::kOverrun;
  }
  int flags = first;
  if (flags == 0) {
    return Command::kEnded;
  }
  const int frame_mask =
      (flags & kByteDeltaAxes) != 0 ? kLongRunMask : kShortRunMask;
  const int run = flags & frame_mask;
  int frame = next_frame + run + 1;
  if (run == frame_mask) {
    std::uint8_t extra = 0;
    if (!reader.Byte(extra)) {
      return Command::kOverrun;
    }
    frame += extra;
  }
  PoseVector next = delta;
  if ((flags & kByteDeltaAxes) == 0) {
    flags <<= kPackedAxisBits;
    int packed = 0;
    if (!reader.Word(packed)) {
      return Command::kOverrun;
    }
    int value = packed >> kPackedAxisBits;
    if ((packed & kPackedX) != 0) {
      next.x = WrapSigned16(static_cast<std::uint32_t>(value));
      flags &= kByteDeltaY | kByteDeltaZ;
      if ((packed & (kPackedY | kPackedZ)) != 0 && !reader.Word(value)) {
        return Command::kOverrun;
      }
    }
    if ((packed & kPackedY) != 0) {
      next.y = WrapSigned16(static_cast<std::uint32_t>(value));
      flags &= kByteDeltaX | kByteDeltaZ;
      if ((packed & kPackedZ) != 0 && !reader.Word(value)) {
        return Command::kOverrun;
      }
    }
    if ((packed & kPackedZ) != 0) {
      next.z = WrapSigned16(static_cast<std::uint32_t>(value));
      flags &= kByteDeltaX | kByteDeltaY;
    }
  }
  if ((flags & kByteDeltaX) != 0 && !reader.SignedByte(next.x)) {
    return Command::kOverrun;
  }
  if ((flags & kByteDeltaY) != 0 && !reader.SignedByte(next.y)) {
    return Command::kOverrun;
  }
  if ((flags & kByteDeltaZ) != 0 && !reader.SignedByte(next.z)) {
    return Command::kOverrun;
  }
  delta = next;
  next_frame = frame;
  return Command::kApplied;
}

std::int16_t AddWrapped(std::int16_t value, std::int16_t delta) {
  return WrapSigned16(static_cast<std::uint32_t>(value) +
                      static_cast<std::uint32_t>(delta));
}

std::int16_t MaskRotation(std::int16_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value) &
                                   kRotationMask);
}

// The header word at `index`, stored in the bank's little-endian layout.
bool HeaderWord(std::span<const std::uint8_t> header, std::size_t index,
                std::size_t& out) {
  const std::size_t at = index * sizeof(std::uint16_t);
  if (header.size() < sizeof(std::uint16_t) ||
      at > header.size() - sizeof(std::uint16_t)) {
    return false;
  }
  out = static_cast<std::size_t>(header[at]) |
        (static_cast<std::size_t>(header[at + 1]) << kBitsPerByte);
  return true;
}

// Decodes the track the header word at `index` points to. Returns false when
// the word or its track lies past the clip.
bool DecodeClipTrack(const AnimationClip& clip, std::size_t index,
                     TrackFrames frames, PoseVector& value, PoseVector& delta,
                     TrackKind kind) {
  std::size_t offset = 0;
  if (!HeaderWord(clip.header, index, offset) || offset > clip.motion.size()) {
    return false;
  }
  return DecodeAnimationTrack(clip.motion.subspan(offset), frames, value, delta,
                              kind) != TrackDecode::Overrun;
}

std::int16_t BlendComponent(std::int16_t from, std::int16_t to, int weight,
                            TrackKind kind) {
  int delta = static_cast<int>(to) - from;
  if (kind == TrackKind::Rotation) {
    if (delta >= kOne) {
      delta -= kOne * 2;
    } else if (delta < -kOne) {
      delta += kOne * 2;
    }
  }
  // GPF loads a signed 16-bit IR component, then shifts its product
  // arithmetically by 12.
  return static_cast<std::int16_t>(
      from + ((static_cast<int>(static_cast<std::int16_t>(delta)) * weight) >>
              kFixedFractionBits));
}

void BlendVector(PoseVector& destination, const PoseVector& source, int weight,
                 TrackKind kind) {
  destination.x = BlendComponent(destination.x, source.x, weight, kind);
  destination.y = BlendComponent(destination.y, source.y, weight, kind);
  destination.z = BlendComponent(destination.z, source.z, weight, kind);
}

std::uint16_t AddRate(std::uint16_t rate, int step) {
  return static_cast<std::uint16_t>(rate + step);
}

std::int16_t SubtractStep(std::int16_t remaining, int step) {
  return WrapSigned16(static_cast<std::uint32_t>(remaining) -
                      static_cast<std::uint32_t>(step));
}
}  // namespace

std::size_t PoseBoneCount(int bones) {
  return static_cast<std::size_t>(
      std::clamp(bones, 0, static_cast<int>(kMaxPoseBones)));
}

// decomp: BATTLE.PRG func_800AFDE8 (src/BATTLE/BATTLE.PRG/44F14.c:1373)
// The original delta was value + 84 SVECTORs; callers now pass the matching
// pose key explicitly, and the track is bounded by its bank (#261).
TrackDecode DecodeAnimationTrack(std::span<const std::uint8_t> track,
                                 TrackFrames frames, PoseVector& value,
                                 PoseVector& delta, TrackKind kind) {
  TrackReader reader(track, value.word);
  int next_frame = delta.word;
  for (int frame = frames.first; frame <= frames.last; ++frame) {
    while (next_frame < frame) {
      const Command command = DecodeCommand(reader, next_frame, delta);
      if (command == Command::kEnded) {
        return TrackDecode::Ended;
      }
      if (command == Command::kOverrun) {
        return TrackDecode::Overrun;
      }
    }
    value.x = AddWrapped(value.x, delta.x);
    value.y = AddWrapped(value.y, delta.y);
    value.z = AddWrapped(value.z, delta.z);
    if (kind == TrackKind::Rotation) {
      value.x = MaskRotation(value.x);
      value.y = MaskRotation(value.y);
      value.z = MaskRotation(value.z);
    }
  }
  value.word = static_cast<std::uint16_t>(reader.At());
  delta.word = static_cast<std::uint16_t>(next_frame);
  return TrackDecode::Complete;
}

PoseTrackResult ApplyPoseTracks(const AnimationClip& clip, int bones,
                                TrackFrames frames, SkeletalPose& pose) {
  PoseTrackResult result;
  auto decode = [&](std::size_t index, PoseVector& value, PoseVector& delta,
                    TrackKind kind) {
    if (!DecodeClipTrack(clip, index, frames, value, delta, kind)) {
      result.overrun = true;
    }
  };
  std::size_t translation = 0;
  if (!HeaderWord(clip.header, kRootTranslationTrack, translation)) {
    result.overrun = true;
    return result;
  }
  if (translation != 0) {
    PoseVector& root = pose.keys.scales[kRootPoseKey];
    decode(kRootTranslationTrack, root, pose.scaleDeltas[kRootPoseKey],
           TrackKind::Linear);
    result.moved = true;
    result.step = {root.x / kRootTranslationDivisor,
                   root.y / kRootTranslationDivisor,
                   root.z / kRootTranslationDivisor};
  }
  decode(kRootRotationTrack, pose.keys.rotations[kRootPoseKey],
         pose.rotationDeltas[kRootPoseKey], TrackKind::Linear);
  const bool scaled = clip.header.size() > kScaleFlagsByte &&
                      (clip.header[kScaleFlagsByte] & kHasScaleTracks) != 0;
  // Scale track words follow every bone the header describes, even ones
  // past the pose's capacity.
  const std::size_t header_bones = static_cast<std::size_t>(std::max(bones, 0));
  const std::size_t pose_bones = PoseBoneCount(bones);
  for (std::size_t bone = 0; bone < pose_bones; ++bone) {
    decode(kFirstBoneTrack + bone, pose.keys.rotations[bone],
           pose.rotationDeltas[bone], TrackKind::Rotation);
    if (scaled) {
      decode(kFirstBoneTrack + header_bones + bone, pose.keys.scales[bone],
             pose.scaleDeltas[bone], TrackKind::Linear);
    }
  }
  return result;
}

void AdvanceShadowBlend(ShadowBlend& shadow) {
  if (shadow.levelFrames != 0) {
    const int delta = shadow.levelDelta / shadow.levelFrames;
    shadow.levelDelta = SubtractStep(shadow.levelDelta, delta);
    shadow.level = AddRate(shadow.level, delta);
    --shadow.levelFrames;
  }
  shadow.level &= kShadowLevelMask;
  if (shadow.radiusFrames != 0) {
    const int expand = shadow.radiusExpandDelta / shadow.radiusFrames;
    shadow.radiusExpandDelta = SubtractStep(shadow.radiusExpandDelta, expand);
    shadow.radiusExpandRate = AddRate(shadow.radiusExpandRate, expand);
    const int shrink = shadow.radiusShrinkDelta / shadow.radiusFrames;
    shadow.radiusShrinkDelta = SubtractStep(shadow.radiusShrinkDelta, shrink);
    shadow.radiusShrinkRate = AddRate(shadow.radiusShrinkRate, shrink);
    --shadow.radiusFrames;
  }
}

// decomp: BATTLE.PRG func_800AFA28 (src/BATTLE/BATTLE.PRG/44F14.c:1252)
// Native frame ranges are local; no pointer-bearing context is placed at the
// PS1 scratchpad's end.
void AdvanceSkeletalAnimation(SkeletalAnimationHost& host) {
  host.SetReachedEnd(false);
  int first_frame = host.Frame();
  for (int step = 0; step < host.FramesPerStep(); ++step) {
    host.AdvanceShadow();
    if (host.Frame() >= host.AnimationLength()) {
      host.SetReachedEnd(true);
      if (host.PlayQueuedAnimation()) {
        return;
      }
    }
    if (host.EventsDue()) {
      host.RunEvents();
    }
    if (host.Frame() >= host.AnimationLength()) {
      host.SetFrame(host.Frame() - 1);
      host.HoldLastFrame();
    }
    host.SetFrame(host.Frame() + 1);
  }
  if (host.Frame() == 1 || host.HoldingLastFrame() || host.WaitingForEvent()) {
    return;
  }
  const int last_frame = host.Frame() - 1;
  if (last_frame < first_frame || first_frame == 0) {
    first_frame = 1;
  }
  host.ApplyPoseTracks({.first = first_frame, .last = last_frame});
}

// decomp: BATTLE.PRG func_800B147C (src/BATTLE/BATTLE.PRG/48C7C.s:10-184).
// Full copies use the native pose extent, not the assembly's fixed byte count.
// A zero-length blend no longer divides by zero (#261).
PoseBlend BlendActorPose(PoseKeys& destination, const PoseKeys& source,
                         int bones, int frame, int frames) {
  if (frame == frames) {
    return PoseBlend::CopySource;
  }
  if (frame == (frames >> 1)) {
    return PoseBlend::Halfway;
  }
  if (frames == 0) {
    return PoseBlend::Skipped;
  }
  const int weight = (frame * kOne) / frames;
  BlendVector(destination.rotations[kRootPoseKey],
              source.rotations[kRootPoseKey], weight, TrackKind::Linear);
  const std::size_t count = PoseBoneCount(bones);
  for (std::size_t bone = 0; bone < count; ++bone) {
    BlendVector(destination.rotations[bone], source.rotations[bone], weight,
                TrackKind::Rotation);
    BlendVector(destination.scales[bone], source.scales[bone], weight,
                TrackKind::Linear);
  }
  return PoseBlend::Weighted;
}
}  // namespace VS::Actors
