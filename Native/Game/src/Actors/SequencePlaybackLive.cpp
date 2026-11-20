// Hand-maintained (tools/port/owned.csv).
// Live binding for skeletal SEQ playback (SequencePlayback.cpp): actor records
// and their poses, and the extents of the SEQ banks the loader produced.
#include "Actors/SequencePlaybackLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/Shadow.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Gte.hpp"
#include "Core/Math/ScalarMath.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Actors {
namespace {
using VS::Core::Math::WrapSigned16;

static_assert(std::extent_v<decltype(VS::Actors::AnimationPose::bone_keys)> ==
              kPoseKeyCount);
static_assert(std::extent_v<decltype(VS::Actors::AnimationPose::scale_keys)> ==
              kPoseKeyCount);
static_assert(
    std::extent_v<decltype(VS::Actors::AnimationPose::rotation_deltas)> ==
    kPoseKeyCount);
static_assert(
    std::extent_v<decltype(VS::Actors::AnimationPose::track_states)> ==
    kPoseKeyCount);

constexpr const char* kChannel = "actors.sequence";

struct BankExtent {
  std::uintptr_t begin = 0;
  std::size_t bytes = 0;
};

std::vector<BankExtent>& Banks() {
  static std::vector<BankExtent> banks;
  return banks;
}

enum class Problem : std::uint8_t {
  kUnregisteredBank,
  kMapOutsideBank,
  kHeaderOutsideBank,
  kMotionOutsideBank,
  kTrackOverrun,
};

const char* Describe(Problem problem) {
  switch (problem) {
    case Problem::kUnregisteredBank:
      return "has no recorded extent";
    case Problem::kMapOutsideBank:
      return "has a map entry outside its bank";
    case Problem::kHeaderOutsideBank:
      return "has a header outside its bank";
    case Problem::kMotionOutsideBank:
      return "has motion data outside its bank";
    case Problem::kTrackOverrun:
      return "has a track that runs past its bank";
  }
  return "is invalid";
}

// Reports a problem once until a different one occurs, since playback hits
// the same problem every frame.
void Report(const void* bank, int animation, Problem problem) {
  struct Last {
    const void* bank = nullptr;
    int animation = -1;
    Problem problem = Problem::kUnregisteredBank;
  };
  static Last last;
  if (last.bank == bank && last.animation == animation &&
      last.problem == problem) {
    return;
  }
  last = {bank, animation, problem};
  VS::Core::Errorf(kChannel, "SEQ bank %p animation %d %s; skipped", bank,
                   animation, Describe(problem));
}

PoseVector FromLive(const SVECTOR& vector) {
  return {vector.vx, vector.vy, vector.vz,
          static_cast<std::uint16_t>(vector.pad)};
}

void ToLive(const PoseVector& vector, SVECTOR& live) {
  live.vx = vector.x;
  live.vy = vector.y;
  live.vz = vector.z;
  live.pad = static_cast<std::int16_t>(vector.word);
}

void LoadKeys(std::span<const SVECTOR, kPoseKeyCount> live,
              PoseKeyArray& keys) {
  std::ranges::transform(live, keys.begin(), FromLive);
}

void StoreKeys(const PoseKeyArray& keys,
               std::span<SVECTOR, kPoseKeyCount> live) {
  for (std::size_t index = 0; index < kPoseKeyCount; ++index) {
    ToLive(keys[index], live[index]);
  }
}

// Resolves the actor's current clip within its registered bank.
bool ResolveClip(const VS::Actors::ActorRecord& actor, AnimationClip& clip,
                 const void*& bank_address) {
  const AnimationHeader header =
      FindLiveAnimationHeader(actor, actor.animation_id);
  bank_address = header.bank.data();
  if (header.status != AnimationHeaderStatus::kFound) {
    return false;
  }
  // A found header lies past the bank's motion and map offsets.
  std::int32_t motion = 0;
  std::memcpy(&motion, header.bank.data(), sizeof motion);
  if (motion < 0 || static_cast<std::size_t>(motion) > header.bank.size()) {
    Report(bank_address, actor.animation_id, Problem::kMotionOutsideBank);
    return false;
  }
  clip = {.header = header.bank.subspan(header.header_offset),
          .motion = header.bank.subspan(static_cast<std::size_t>(motion))};
  return true;
}

void ApplyLivePoseTracks(VS::Actors::ActorRecord& actor,
                         VS::Actors::AnimationPose& pose, TrackFrames frames) {
  AnimationClip clip;
  const void* bank = nullptr;
  if (!ResolveClip(actor, clip, bank)) {
    return;
  }
  SkeletalPose typed = LoadLiveSkeletalPose(pose);
  const PoseTrackResult result =
      ApplyPoseTracks(clip, actor.instance.bone_count, frames, typed);
  StoreLiveSkeletalPose(typed, pose);
  if (result.moved) {
    SVECTOR& position = actor.instance.position;
    position.vx = WrapSigned16(static_cast<std::uint32_t>(position.vx) +
                               static_cast<std::uint32_t>(result.step.x));
    position.vy = WrapSigned16(static_cast<std::uint32_t>(position.vy) +
                               static_cast<std::uint32_t>(result.step.y));
    position.vz = WrapSigned16(static_cast<std::uint32_t>(position.vz) +
                               static_cast<std::uint32_t>(result.step.z));
  }
  if (result.overrun) {
    Report(bank, actor.animation_id, Problem::kTrackOverrun);
  }
}

class LiveSkeletalAnimation final : public SkeletalAnimationHost {
 public:
  LiveSkeletalAnimation(VS::Actors::ActorRecord& actor, int mode)
      : actor_(actor), mode_(mode) {}

  int FramesPerStep() const override { return actor_.frames_per_step; }
  int Frame() const override { return actor_.pose.frame; }
  void SetFrame(int frame) override {
    actor_.pose.frame = WrapSigned16(static_cast<std::uint32_t>(frame));
  }
  int AnimationLength() const override {
    return static_cast<std::uint16_t>(actor_.animation_length);
  }
  void AdvanceShadow() override {
    ShadowBlend shadow = LoadLiveShadowBlend(actor_.shadow_blend);
    AdvanceShadowBlend(shadow);
    StoreLiveShadowBlend(shadow, actor_.shadow_blend);
  }
  void SetReachedEnd(bool reached) override {
    actor_.pose.reached_end = reached ? -1 : 0;
  }
  bool PlayQueuedAnimation() override {
    if (actor_.queued_animation == 0) {
      return false;
    }
    PlayLiveActorAnimationAtFrame(actor_.instance.slot, actor_.queued_animation,
                                  actor_.queued_parameter_a,
                                  actor_.queued_parameter_b);
    actor_.queued_animation = 0;
    return true;
  }
  bool EventsDue() const override {
    return actor_.instance.is_frozen &&
           actor_.pose.next_event_frame == actor_.pose.frame;
  }
  void RunEvents() override {
    static_cast<void>(RunLiveAnimationEvents(actor_, actor_.pose, mode_));
  }
  bool HoldingLastFrame() const override {
    return actor_.pose.holding_last_frame != 0;
  }
  void HoldLastFrame() override { actor_.pose.holding_last_frame = -1; }
  bool WaitingForEvent() const override { return actor_.pose.event_wait != 0; }
  void ApplyPoseTracks(TrackFrames frames) override {
    ApplyLivePoseTracks(actor_, actor_.pose, frames);
  }

 private:
  VS::Actors::ActorRecord& actor_;
  int mode_;
};
}  // namespace

void RegisterSequenceBank(const void* bank, std::size_t bytes) {
  if (bank == nullptr) {
    return;
  }
  ForgetSequenceBanks(bank, bytes);
  Banks().push_back({reinterpret_cast<std::uintptr_t>(bank), bytes});
}

void ForgetSequenceBanks(const void* memory, std::size_t bytes) {
  const auto begin = reinterpret_cast<std::uintptr_t>(memory);
  const std::uintptr_t end = begin + bytes;
  std::erase_if(Banks(), [&](const BankExtent& known) {
    return known.begin == begin ||
           (known.begin < end && begin < known.begin + known.bytes);
  });
}

void RegisterSequenceBankCopy(const void* destination, const void* source,
                              std::size_t capacity) {
  const std::span<const std::uint8_t> copied = SequenceBankBytes(source);
  if (copied.empty()) {
    VS::Core::Errorf(
        kChannel, "SEQ bank copy %p came from %p, which has no recorded extent",
        destination, source);
    return;
  }
  RegisterSequenceBank(destination, std::min(copied.size(), capacity));
}

void RegisterEmbeddedSequenceBank(const void* file) {
  constexpr std::size_t kSizeWord = 4;
  constexpr std::size_t kBankStart = 8;
  if (file == nullptr) {
    return;
  }
  const auto* bytes = static_cast<const std::uint8_t*>(file);
  std::uint32_t size = 0;
  std::memcpy(&size, bytes + kSizeWord, sizeof size);
  RegisterSequenceBank(bytes + kBankStart, size);
}

std::span<const std::uint8_t> SequenceBankBytes(const void* bank) {
  const auto begin = reinterpret_cast<std::uintptr_t>(bank);
  const auto found = std::ranges::find(Banks(), begin, &BankExtent::begin);
  if (bank == nullptr || found == Banks().end()) {
    return {};
  }
  return {static_cast<const std::uint8_t*>(bank), found->bytes};
}

std::optional<SequenceBankLocation> LocateInSequenceBank(const void* address) {
  if (address == nullptr) {
    return std::nullopt;
  }
  const auto at = reinterpret_cast<std::uintptr_t>(address);
  // A cursor at one bank's end can be the next bank's start; the bank that
  // holds a byte there wins.
  const BankExtent* holder = nullptr;
  for (const BankExtent& known : Banks()) {
    if (at < known.begin || at - known.begin > known.bytes) {
      continue;
    }
    if (at - known.begin < known.bytes) {
      holder = &known;
      break;
    }
    holder = holder == nullptr ? &known : holder;
  }
  if (holder == nullptr) {
    return std::nullopt;
  }
  const std::size_t offset = at - holder->begin;
  return SequenceBankLocation{
      .bank = {static_cast<const std::uint8_t*>(address) - offset,
               holder->bytes},
      .offset = offset};
}

void ReportLiveAnimationHeader(const void* bank, int animation,
                               AnimationHeaderStatus status) {
  switch (status) {
    case AnimationHeaderStatus::kUnregisteredBank:
      Report(bank, animation, Problem::kUnregisteredBank);
      return;
    case AnimationHeaderStatus::kMapOutsideBank:
      Report(bank, animation, Problem::kMapOutsideBank);
      return;
    case AnimationHeaderStatus::kHeaderOutsideBank:
      Report(bank, animation, Problem::kHeaderOutsideBank);
      return;
    case AnimationHeaderStatus::kFound:
    case AnimationHeaderStatus::kNoClip:
    case AnimationHeaderStatus::kNoEntry:
    case AnimationHeaderStatus::kNoBank:
      return;
  }
}

PoseKeys LoadLivePoseKeys(const VS::Actors::AnimationPose& pose) {
  PoseKeys keys;
  LoadKeys(pose.bone_keys, keys.rotations);
  LoadKeys(pose.scale_keys, keys.scales);
  return keys;
}

void StoreLivePoseKeys(const PoseKeys& keys, VS::Actors::AnimationPose& pose) {
  StoreKeys(keys.rotations, pose.bone_keys);
  StoreKeys(keys.scales, pose.scale_keys);
}

SkeletalPose LoadLiveSkeletalPose(const VS::Actors::AnimationPose& pose) {
  SkeletalPose typed;
  typed.keys = LoadLivePoseKeys(pose);
  LoadKeys(pose.rotation_deltas, typed.rotation_deltas);
  LoadKeys(pose.track_states, typed.scale_deltas);
  return typed;
}

void StoreLiveSkeletalPose(const SkeletalPose& typed,
                           VS::Actors::AnimationPose& pose) {
  StoreLivePoseKeys(typed.keys, pose);
  StoreKeys(typed.rotation_deltas, pose.rotation_deltas);
  StoreKeys(typed.scale_deltas, pose.track_states);
}

ShadowBlend LoadLiveShadowBlend(const ShadowBlendRecord& live) {
  return {.level_frames = live.level_frames,
          .radius_frames = live.radius_frames,
          .level = live.level,
          .level_delta = live.level_delta,
          .radius_expand_rate = live.radius_expand_rate,
          .radius_shrink_rate = live.radius_shrink_rate,
          .radius_expand_delta = live.radius_expand_delta,
          .radius_shrink_delta = live.radius_shrink_delta};
}

void StoreLiveShadowBlend(const ShadowBlend& shadow, ShadowBlendRecord& live) {
  live.level_frames = shadow.level_frames;
  live.radius_frames = shadow.radius_frames;
  live.level = shadow.level;
  live.level_delta = shadow.level_delta;
  live.radius_expand_rate = shadow.radius_expand_rate;
  live.radius_shrink_rate = shadow.radius_shrink_rate;
  live.radius_expand_delta = shadow.radius_expand_delta;
  live.radius_shrink_delta = shadow.radius_shrink_delta;
}

void AdvanceLiveSkeletalAnimation(VS::Actors::ActorRecord& actor, int mode) {
  LiveSkeletalAnimation host(actor, mode);
  AdvanceSkeletalAnimation(host);
}

void BlendLiveActorPose(VS::Actors::AnimationPose& destination,
                        VS::Actors::AnimationPose& source, int bones, int frame,
                        int frames) {
  PoseKeys keys = LoadLivePoseKeys(destination);
  const PoseKeys from = LoadLivePoseKeys(source);
  switch (BlendActorPose(keys, from, bones, frame, frames)) {
    case PoseBlend::kCopySource:
      destination = source;
      return;
    case PoseBlend::kHalfway:
      keys = LoadLivePoseKeys(destination);
      BlendActorPoseHalfway(keys, from, bones);
      StoreLivePoseKeys(keys, destination);
      return;
    case PoseBlend::kWeighted:
      StoreLivePoseKeys(keys, destination);
      return;
    case PoseBlend::kSkipped:
      VS::Core::Errorf(kChannel,
                       "Pose blend at frame %d of zero frames skipped (#261)",
                       frame);
      return;
  }
}
}  // namespace VS::Actors
