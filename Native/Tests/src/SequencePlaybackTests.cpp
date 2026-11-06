#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/SequencePlayback.hpp"
#include "Actors/SequencePlaybackLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
}  // extern "C"

namespace {

using VS::Actors::AnimationClip;
using VS::Actors::ApplyPoseTracks;
using VS::Actors::BlendActorPose;
using VS::Actors::DecodeAnimationTrack;
using VS::Actors::kMaxPoseBones;
using VS::Actors::kRootPoseKey;
using VS::Actors::PoseBlend;
using VS::Actors::PoseKeys;
using VS::Actors::PoseVector;
using VS::Actors::SkeletalPose;
using VS::Actors::TrackDecode;
using VS::Actors::TrackKind;

bool Same(const PoseVector& left, const PoseVector& right) {
  return left.x == right.x && left.y == right.y && left.z == right.z &&
         left.word == right.word;
}

// A clip header of little-endian words.
std::vector<std::uint8_t> Header(const std::vector<std::uint16_t>& words) {
  std::vector<std::uint8_t> bytes;
  for (const std::uint16_t word : words) {
    bytes.push_back(static_cast<std::uint8_t>(word & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(word >> 8));
  }
  return bytes;
}

}  // namespace

namespace {
const PoseVector kStartValue{5, 6, 7, 0};
const PoseVector kStartDelta{1, 1, 1, 0};

TrackDecode DecodeFirstFrame(std::span<const std::uint8_t> track,
                             PoseVector& value, PoseVector& delta) {
  return DecodeAnimationTrack(track, {.first = 1, .last = 1}, value, delta,
                              TrackKind::kLinear);
}
}  // namespace

TEST_CASE("SEQ commands cut short by the bank end are not applied",
          "[actors][sequence][bounds]") {
  const std::uint8_t track[] = {0xE0, 1};
  PoseVector value = kStartValue;
  PoseVector delta = kStartDelta;
  CHECK(DecodeFirstFrame(track, value, delta) == TrackDecode::kOverrun);
  CHECK(Same(value, kStartValue));
  CHECK(Same(delta, kStartDelta));
}

TEST_CASE("SEQ packed commands cut short by the bank end are not applied",
          "[actors][sequence][bounds]") {
  const std::uint8_t track[] = {0x07, 0x00, 0x0C, 0x00};
  PoseVector value = kStartValue;
  PoseVector delta = kStartDelta;
  CHECK(DecodeFirstFrame(track, value, delta) == TrackDecode::kOverrun);
  CHECK(Same(delta, kStartDelta));
}

TEST_CASE("SEQ track cursors past the bank end are not followed",
          "[actors][sequence][bounds]") {
  const std::uint8_t track[] = {0xE0, 1, 1, 1};
  PoseVector value{0, 0, 0, 200};
  PoseVector delta = kStartDelta;
  CHECK(DecodeFirstFrame(track, value, delta) == TrackDecode::kOverrun);
  CHECK(value.word == 200);
}

TEST_CASE("SEQ track terminators leave the cursors unchanged",
          "[actors][sequence]") {
  const std::uint8_t track[] = {0};
  PoseVector value = kStartValue;
  PoseVector delta = kStartDelta;
  CHECK(DecodeFirstFrame(track, value, delta) == TrackDecode::kEnded);
  CHECK(Same(value, kStartValue));
  CHECK(Same(delta, kStartDelta));
}

TEST_CASE("SEQ clips report header words and tracks outside their bank",
          "[actors][sequence][bounds]") {
  const std::uint8_t motion[] = {0xE0, 1, 2, 3};
  SkeletalPose pose;
  SECTION("a header too short for its track words") {
    const auto header = Header({0, 0, 0});
    const auto result = ApplyPoseTracks({.header = header, .motion = motion}, 1,
                                        {.first = 1, .last = 1}, pose);
    CHECK(result.overrun);
    CHECK_FALSE(result.moved);
  }
  SECTION("one track outside the motion data") {
    const auto header = Header({0, 0, 0, 0, 0, 90});
    const auto result = ApplyPoseTracks({.header = header, .motion = motion}, 1,
                                        {.first = 1, .last = 1}, pose);
    CHECK(result.overrun);
    CHECK(pose.keys.rotations[kRootPoseKey].x == 1);
    CHECK(pose.keys.rotations[kRootPoseKey].z == 3);
    CHECK(pose.keys.rotations[0].x == 0);
  }
}

TEST_CASE("SEQ root translation moves the actor by a quarter of its key",
          "[actors][sequence]") {
  const std::uint8_t motion[] = {0, 0xE0, 8, 12, 0xFC, 0};
  const auto header = Header({0, 0, 0, 0, 1});
  SkeletalPose pose;
  const auto result = ApplyPoseTracks({.header = header, .motion = motion}, 0,
                                      {.first = 1, .last = 1}, pose);
  CHECK_FALSE(result.overrun);
  CHECK(result.moved);
  CHECK(result.step.x == 2);
  CHECK(result.step.y == 3);
  CHECK(result.step.z == -1);
}

TEST_CASE("SEQ playback clamps header bone counts to the pose arrays",
          "[actors][sequence][bounds]") {
  constexpr int kHeaderBones = 200;
  std::vector<std::uint16_t> words(5 + kHeaderBones * 2, 0);
  const std::uint8_t motion[] = {0xE0, 1, 1, 1, 0};
  auto header = Header(words);
  header[3] = 2;
  SkeletalPose pose;
  const auto result =
      ApplyPoseTracks({.header = header, .motion = motion}, kHeaderBones,
                      {.first = 1, .last = 1}, pose);
  CHECK_FALSE(result.overrun);
  CHECK(pose.keys.rotations[kMaxPoseBones - 1].x == 1);
  CHECK(pose.keys.scales[kMaxPoseBones - 1].y == 1);
  CHECK(VS::Actors::PoseBoneCount(kHeaderBones) == kMaxPoseBones);
  CHECK(VS::Actors::PoseBoneCount(-3) == 0);
}

TEST_CASE("Pose blends never divide by a zero frame count",
          "[actors][sequence][bounds]") {
  PoseKeys destination;
  PoseKeys source;
  destination.rotations[0].x = 10;
  source.rotations[0].x = 50;
  CHECK(BlendActorPose(destination, source, 1, 0, 0) == PoseBlend::kCopySource);
  CHECK(BlendActorPose(destination, source, 1, 3, 0) == PoseBlend::kSkipped);
  CHECK(destination.rotations[0].x == 10);
  CHECK(BlendActorPose(destination, source, 1, 2, 4) == PoseBlend::kHalfway);
  CHECK(BlendActorPose(destination, source, 255, 1, 4) == PoseBlend::kWeighted);
  CHECK(destination.rotations[0].x == 20);
}

TEST_CASE("Shadow blends step their remaining deltas once per frame",
          "[actors][sequence]") {
  VS::Actors::ShadowBlend shadow{.level_frames = 2,
                                 .radius_frames = 1,
                                 .level = 0xFFE,
                                 .level_delta = 100,
                                 .radius_expand_rate = 10,
                                 .radius_shrink_rate = 20,
                                 .radius_expand_delta = 7,
                                 .radius_shrink_delta = -4};
  VS::Actors::AdvanceShadowBlend(shadow);
  CHECK(shadow.level_frames == 1);
  CHECK(shadow.level_delta == 50);
  CHECK(shadow.level == ((0xFFE + 50) & 0xFFF));
  CHECK(shadow.radius_frames == 0);
  CHECK(shadow.radius_expand_rate == 17);
  CHECK(shadow.radius_shrink_rate == 16);
  CHECK(shadow.radius_expand_delta == 0);
  CHECK(shadow.radius_shrink_delta == 0);
}

namespace {
// Memory a registry test registers banks in, forgotten when the test ends.
struct RegistryMemory {
  std::array<std::uint32_t, 64> words{};
  std::uint8_t* bytes = reinterpret_cast<std::uint8_t*>(words.data());

  RegistryMemory() = default;
  RegistryMemory(const RegistryMemory&) = delete;
  RegistryMemory& operator=(const RegistryMemory&) = delete;
  ~RegistryMemory() { VS::Actors::ForgetSequenceBanks(bytes, sizeof words); }
};
}  // namespace

TEST_CASE("Registered SEQ banks report their extent",
          "[actors][sequence][bounds]") {
  RegistryMemory memory;
  VS::Actors::RegisterSequenceBank(memory.bytes, 128);
  CHECK(VS::Actors::SequenceBankBytes(memory.bytes).size() == 128);
  CHECK(VS::Actors::SequenceBankBytes(memory.bytes + 4).empty());
  VS::Actors::ForgetSequenceBanks(memory.bytes, 4);
  CHECK(VS::Actors::SequenceBankBytes(memory.bytes).empty());
}

TEST_CASE("An overlapping SEQ bank registration replaces the older bank",
          "[actors][sequence][bounds]") {
  RegistryMemory memory;
  VS::Actors::RegisterSequenceBank(memory.bytes, 128);
  VS::Actors::RegisterSequenceBank(memory.bytes + 64, 64);
  CHECK(VS::Actors::SequenceBankBytes(memory.bytes).empty());
  CHECK(VS::Actors::SequenceBankBytes(memory.bytes + 64).size() == 64);
}

TEST_CASE("SEQ bank copies keep the source extent within their capacity",
          "[actors][sequence][bounds]") {
  RegistryMemory memory;
  std::uint8_t* copy = memory.bytes + 220;
  VS::Actors::RegisterSequenceBank(memory.bytes, 200);
  VS::Actors::RegisterSequenceBankCopy(copy, memory.bytes, 16);
  CHECK(VS::Actors::SequenceBankBytes(copy).size() == 16);
  VS::Actors::ForgetSequenceBanks(memory.bytes, 200);
  CHECK(VS::Actors::SequenceBankBytes(copy).size() == 16);
  VS::Actors::ForgetSequenceBanks(copy, 16);
  VS::Actors::RegisterSequenceBankCopy(copy, memory.bytes, 16);
  CHECK(VS::Actors::SequenceBankBytes(copy).empty());
}

TEST_CASE("Embedded SEQ files carry their bank extent at file+4",
          "[actors][sequence][bounds]") {
  RegistryMemory memory;
  memory.words[1] = 96;
  VS::Actors::RegisterEmbeddedSequenceBank(memory.bytes);
  CHECK(VS::Actors::SequenceBankBytes(memory.bytes + 8).size() == 96);
}

namespace {

// One actor playing animation 0 from a 256-byte bank whose root rotation and
// bone 0 tracks start at motion bytes 4 and 8.
struct LivePlayback {
  std::array<std::uint32_t, 64> file{};
  Actors_ActorRecord actor{};
  char saved_resolved = Actors_lastResolvedSequenceIndex;

  LivePlayback() {
    auto* bank = Bank();
    bank->animationData = 64;
    bank->animationMap = 48;
    bank->headers[3] = 4;
    bank->headers[5] = 8;
    const std::uint8_t tracks[] = {0xE0, 1, 2, 3, 0xE0, 1, 1, 1};
    std::memcpy(reinterpret_cast<std::uint8_t*>(file.data()) + 68, tracks,
                sizeof tracks);
    actor.ownedSequence = bank;
    actor.instance.boneCount = 1;
    actor.framesPerStep = 1;
    actor.animationLength = 10;
    actor.pose.frame = 1;
  }
  LivePlayback(const LivePlayback&) = delete;
  LivePlayback& operator=(const LivePlayback&) = delete;

  ~LivePlayback() {
    VS::Actors::ForgetSequenceBanks(file.data(), sizeof file);
    Actors_lastResolvedSequenceIndex = saved_resolved;
  }

  Actors_SeqAnimationBank* Bank() {
    return reinterpret_cast<Actors_SeqAnimationBank*>(file.data());
  }
};

}  // namespace

TEST_CASE("Live playback decodes registered banks",
          "[actors][sequence][bounds]") {
  LivePlayback playback;
  VS::Actors::RegisterSequenceBank(playback.Bank(), sizeof playback.file);
  VS::Actors::AdvanceLiveSkeletalAnimation(playback.actor, 0);
  CHECK(playback.actor.pose.frame == 2);
  CHECK(playback.actor.pose.boneKeys[kRootPoseKey].vx == 1);
  CHECK(playback.actor.pose.boneKeys[kRootPoseKey].vz == 3);
  CHECK(playback.actor.pose.boneKeys[0].vy == 1);
}

TEST_CASE("Live playback skips banks it cannot bound",
          "[actors][sequence][bounds]") {
  LivePlayback playback;
  SECTION("an unregistered bank") {}
  SECTION("a bank too short for its motion data") {
    VS::Actors::RegisterSequenceBank(playback.Bank(), 60);
  }
  SECTION("a bank that ends inside its tracks") {
    VS::Actors::RegisterSequenceBank(playback.Bank(), 70);
  }
  VS::Actors::AdvanceLiveSkeletalAnimation(playback.actor, 0);
  CHECK(playback.actor.pose.frame == 2);
  CHECK(playback.actor.pose.boneKeys[kRootPoseKey].vx == 0);
  CHECK(playback.actor.pose.boneKeys[0].vy == 0);
}

TEST_CASE("Live pose blends clamp bones and skip zero-length blends",
          "[actors][sequence][bounds]") {
  Actors_AnimationPose destination{};
  Actors_AnimationPose source{};
  destination.boneKeys[0].vx = 10;
  source.boneKeys[0].vx = 50;
  VS::Actors::BlendLiveActorPose(destination, source, 1, 5, 0);
  CHECK(destination.boneKeys[0].vx == 10);
  VS::Actors::BlendLiveActorPose(destination, source, 255, 1, 4);
  CHECK(destination.boneKeys[0].vx == 20);
  VS::Actors::BlendLiveActorPose(destination, source, 255, 4, 4);
  CHECK(std::memcmp(destination.boneKeys, source.boneKeys,
                    sizeof source.boneKeys) == 0);
  CHECK(std::memcmp(destination.scaleKeys, source.scaleKeys,
                    sizeof source.scaleKeys) == 0);
}
