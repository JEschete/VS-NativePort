// Hand-maintained (tools/port/owned.csv).
// Live binding for skeletal SEQ playback (SequencePlayback.cpp): actor records
// and their poses, and the extents of the SEQ banks the loader produced.
#include "Actors/SequencePlaybackLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

#include "Core/Math/ScalarMath.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Shadow.h"
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
}

namespace VS::Actors {
namespace {
using VS::Core::Math::WrapSigned16;

static_assert(std::extent_v<decltype(Actors_AnimationPose::boneKeys)> ==
              kPoseKeyCount);
static_assert(std::extent_v<decltype(Actors_AnimationPose::scaleKeys)> ==
              kPoseKeyCount);
static_assert(std::extent_v<decltype(Actors_AnimationPose::reserved2A0)> ==
              kPoseKeyCount);
static_assert(std::extent_v<decltype(Actors_AnimationPose::trackStates)> ==
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
  kHeaderOutsideBank,
  kMotionOutsideBank,
  kTrackOverrun,
};

const char* Describe(Problem problem) {
  switch (problem) {
    case Problem::kUnregisteredBank:
      return "has no recorded extent";
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
  Core_Errorf(kChannel, "SEQ bank %p animation %d %s; playback skipped it",
              bank, animation, Describe(problem));
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

PoseKeys LoadPoseKeys(const Actors_AnimationPose& pose) {
  PoseKeys keys;
  LoadKeys(pose.boneKeys, keys.rotations);
  LoadKeys(pose.scaleKeys, keys.scales);
  return keys;
}

// The byte offset of `address` within `bank`, if it lies inside it.
bool OffsetInBank(std::span<const std::uint8_t> bank, std::uintptr_t address,
                  std::size_t& offset) {
  const auto begin = reinterpret_cast<std::uintptr_t>(bank.data());
  if (address < begin || address - begin > bank.size()) {
    return false;
  }
  offset = address - begin;
  return true;
}

// Resolves the actor's current clip within its registered bank.
bool ResolveClip(Actors_ActorRecord& actor, AnimationClip& clip,
                 const void*& bank_address) {
  u_short* header = nullptr;
  const intptr_t found =
      Actors_FindActorAnimationHeader(&actor, actor.animationId, &header);
  bank_address = reinterpret_cast<const void*>(found);
  if (found == 0 || header == nullptr) {
    return false;
  }
  const std::span<const std::uint8_t> bank = SequenceBankBytes(bank_address);
  if (bank.size() < sizeof(Actors_SeqAnimationBank)) {
    Report(bank_address, actor.animationId, Problem::kUnregisteredBank);
    return false;
  }
  std::size_t header_offset = 0;
  if (!OffsetInBank(bank, reinterpret_cast<std::uintptr_t>(header),
                    header_offset)) {
    Report(bank_address, actor.animationId, Problem::kHeaderOutsideBank);
    return false;
  }
  const int motion =
      static_cast<const Actors_SeqAnimationBank*>(bank_address)->animationData;
  if (motion < 0 || static_cast<std::size_t>(motion) > bank.size()) {
    Report(bank_address, actor.animationId, Problem::kMotionOutsideBank);
    return false;
  }
  clip = {.header = bank.subspan(header_offset),
          .motion = bank.subspan(static_cast<std::size_t>(motion))};
  return true;
}

void ApplyLivePoseTracks(Actors_ActorRecord& actor, Actors_AnimationPose& pose,
                         TrackFrames frames) {
  AnimationClip clip;
  const void* bank = nullptr;
  if (!ResolveClip(actor, clip, bank)) {
    return;
  }
  SkeletalPose typed;
  typed.keys = LoadPoseKeys(pose);
  LoadKeys(pose.reserved2A0, typed.rotationDeltas);
  LoadKeys(pose.trackStates, typed.scaleDeltas);
  const PoseTrackResult result =
      ApplyPoseTracks(clip, actor.instance.boneCount, frames, typed);
  StoreKeys(typed.keys.rotations, pose.boneKeys);
  StoreKeys(typed.keys.scales, pose.scaleKeys);
  StoreKeys(typed.rotationDeltas, pose.reserved2A0);
  StoreKeys(typed.scaleDeltas, pose.trackStates);
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
    Report(bank, actor.animationId, Problem::kTrackOverrun);
  }
}

class LiveSkeletalAnimation final : public SkeletalAnimationHost {
 public:
  LiveSkeletalAnimation(Actors_ActorRecord& actor, int mode)
      : actor_(actor), mode_(mode) {}

  int FramesPerStep() const override { return actor_.framesPerStep; }
  int Frame() const override { return actor_.pose.frame; }
  void SetFrame(int frame) override {
    actor_.pose.frame = WrapSigned16(static_cast<std::uint32_t>(frame));
  }
  int AnimationLength() const override {
    return static_cast<u_short>(actor_.animationLength);
  }
  void AdvanceShadow() override {
    ShadowBlend shadow = LoadLiveShadowBlend(actor_.shadowBlend);
    AdvanceShadowBlend(shadow);
    StoreLiveShadowBlend(shadow, actor_.shadowBlend);
  }
  void SetReachedEnd(bool reached) override {
    actor_.pose.reachedEnd = reached ? -1 : 0;
  }
  bool PlayQueuedAnimation() override {
    if (actor_.queuedAnimation == 0) {
      return false;
    }
    Actors_PlayActorAnimationAtFrame(
        actor_.instance.slot, actor_.queuedAnimation, actor_.queuedParameterA,
        actor_.queuedParameterB);
    actor_.queuedAnimation = 0;
    return true;
  }
  bool EventsDue() const override {
    return actor_.instance.isFrozen &&
           actor_.pose.nextEventFrame == actor_.pose.frame;
  }
  void RunEvents() override {
    Actors_RunAnimationEvents(&actor_, &actor_.pose, mode_);
  }
  bool HoldingLastFrame() const override {
    return actor_.pose.holdingLastFrame != 0;
  }
  void HoldLastFrame() override { actor_.pose.holdingLastFrame = -1; }
  bool WaitingForEvent() const override { return actor_.pose.eventWait != 0; }
  void ApplyPoseTracks(TrackFrames frames) override {
    ApplyLivePoseTracks(actor_, actor_.pose, frames);
  }

 private:
  Actors_ActorRecord& actor_;
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
    Core_Errorf(kChannel,
                "SEQ bank copy %p came from %p, which has no recorded extent",
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

ShadowBlend LoadLiveShadowBlend(const Actors_ShadowBlend& live) {
  return {.levelFrames = live.levelFrames,
          .radiusFrames = live.radiusFrames,
          .level = live.level,
          .levelDelta = live.levelDelta,
          .radiusExpandRate = live.radiusExpandRate,
          .radiusShrinkRate = live.radiusShrinkRate,
          .radiusExpandDelta = live.radiusExpandDelta,
          .radiusShrinkDelta = live.radiusShrinkDelta};
}

void StoreLiveShadowBlend(const ShadowBlend& shadow, Actors_ShadowBlend& live) {
  live.levelFrames = shadow.levelFrames;
  live.radiusFrames = shadow.radiusFrames;
  live.level = shadow.level;
  live.levelDelta = shadow.levelDelta;
  live.radiusExpandRate = shadow.radiusExpandRate;
  live.radiusShrinkRate = shadow.radiusShrinkRate;
  live.radiusExpandDelta = shadow.radiusExpandDelta;
  live.radiusShrinkDelta = shadow.radiusShrinkDelta;
}

void AdvanceLiveSkeletalAnimation(Actors_ActorRecord& actor, int mode) {
  LiveSkeletalAnimation host(actor, mode);
  AdvanceSkeletalAnimation(host);
}

void BlendLiveActorPose(Actors_AnimationPose& destination,
                        Actors_AnimationPose& source, int bones, int frame,
                        int frames) {
  PoseKeys keys = LoadPoseKeys(destination);
  switch (BlendActorPose(keys, LoadPoseKeys(source), bones, frame, frames)) {
    case PoseBlend::CopySource:
      destination = source;
      return;
    case PoseBlend::Halfway:
      Actors_BlendActorPoseHalfway(&destination, &source,
                                   static_cast<int>(PoseBoneCount(bones)));
      return;
    case PoseBlend::Weighted:
      StoreKeys(keys.rotations, destination.boneKeys);
      StoreKeys(keys.scales, destination.scaleKeys);
      return;
    case PoseBlend::Skipped:
      Core_Errorf(kChannel,
                  "Pose blend at frame %d of zero frames skipped (#261)",
                  frame);
      return;
  }
}
}  // namespace VS::Actors
