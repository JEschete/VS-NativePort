#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/AnimationControl.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
}

namespace actors = VS::Actors;
using actors::ActionAnimationBusy;
using actors::AnimationActor;
using actors::AnimationGimmick;
using actors::AnimationLookup;
using actors::EventScriptHandle;
using actors::MotionVector;
using actors::PoseEventCursor;
using actors::SequenceHandle;
using actors::SoundEffectSlot;
using actors::SoundPanVolume;
using Calls = std::vector<std::string>;
using VS::Content::ActorAssetRequest;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

namespace {
constexpr int kActorSlots = 17;
constexpr int kBattleSlots = 16;
constexpr int kAshley = 0;
constexpr int kPartner = 1;
constexpr int kActor = 3;
constexpr int kOther = 4;
constexpr int kTickSpeed = 2;  // half a tick is one frame
constexpr int kEventScriptRunning = 4;
constexpr int kBank = 2;
constexpr int kBanks = 3;
constexpr int kReleaseStatus = 5;
constexpr std::int16_t kLength = 24;
constexpr int kRunSpeed = 8;
constexpr int kWalkSpeed = 4;
constexpr int kWalkingSpeed = 3;  // above 0 and below run_speed - 1
constexpr int kDrawnSoundBase = 0x20;
constexpr int kSheathedSoundBase = 0x40;
constexpr int kMoveBase = 0x42;
constexpr int kFastMoveBase = 0x46;
constexpr SequenceHandle kBankSequence{7};
constexpr SequenceHandle kOtherSequence{9};
constexpr SequenceHandle kFoundBank{11};
constexpr EventScriptHandle kScript{13};
constexpr std::uint16_t kEventFrame = 5;
constexpr std::uint8_t kNoSet = 0xFF;
constexpr std::uint8_t kNoRow = 0xFF;
constexpr std::uint8_t kRow = 3;
constexpr std::uint8_t kSkippedFrames = 2;
constexpr PoseEventCursor ClearedCursor() {
  return {.script = EventScriptHandle::kNone, .next_event_frame = 0xFFFF};
}
constexpr PoseEventCursor kClearedCursor = ClearedCursor();

// Animation ids (AnimationControl.cpp); 100 is added while the weapon is
// drawn.
constexpr int kDrawn = 100;
constexpr int kIdle = 1;
constexpr int kNeutral = 6;
constexpr int kLockedIdle = 8;
constexpr int kSheatheStep = 10;  // mode * 2 + 10
constexpr int kDrawStep = 9;      // mode * 2 + 9
constexpr int kWalk = 0xD;
constexpr int kBackWalk = 0xF;
constexpr int kTurnRight = 0x11;
constexpr int kTurnLeft = 0x12;
constexpr int kRun = 0x13;
constexpr int kRunLast = 0x14;
constexpr int kRunTurn = 0x19;
constexpr int kRunStraight = 0x1A;
constexpr int kReadyPose = 0x1C;
constexpr int kGuardPose = 0x1D;
constexpr int kBracePose = 0x1E;
constexpr int kJumpIdle = 31;
constexpr int kJumpWalk = 32;
constexpr int kAirborneJumpIdle = 35;
constexpr int kClimb = 36;
constexpr int kHang = 41;
constexpr int kAirborneIdle = 48;
constexpr int kBusy = 0x3B;
constexpr int kFirstBusy = 0x40;
constexpr int kStance = 0x4B;
constexpr int kFarReaction = 0x4F;
constexpr int kStanceWalk = 0x54;
constexpr int kStrike = 0x56;
constexpr int kStanceHold = 0x5F;
constexpr int kActionPoseBase = 0x5F;
constexpr int kTransition = 0x63;
constexpr int kDrawnTransition = 0xC7;
constexpr int kTurnLock = 0x75;
constexpr int kHeldPose = 0xC6;

// Blend frames.
constexpr int kBlend = 6;
constexpr int kDrawBlend = 16;
constexpr int kSheatheBlend = 5;
constexpr int kBasePoseBlend = 0x10;
constexpr int kTurnBlend = 0xA;
constexpr int kStanceWalkBlend = 4;
constexpr int kStanceIdleBlend = 10;
constexpr int kJumpBlend = 20;
constexpr int kJumpIdleBlend = 10;
constexpr int kRunBlend = 8;
constexpr int kQueuedStrideBlend = 4;
constexpr int kStopBlend = 10;

// Actor states.
constexpr std::uint8_t kAirborneMode = 3;
constexpr std::uint8_t kClimbing = 1;
constexpr std::uint8_t kHanging = 2;
constexpr std::uint8_t kTeleportReaction = 3;
constexpr std::uint8_t kSteeringRequest = 1;
constexpr std::uint8_t kTurnRequest = 6;
constexpr std::int16_t kBehind = 0x800;
constexpr int kQuarterTurn = 0x400;
constexpr int kFullTurn = 0x1000;
constexpr std::uint8_t kCounterLimit = 250;

// Records the host calls the controls make and answers with the configured
// banks, statuses and lookups.
class FakeAnimationHost final : public actors::AnimationControlHost {
 public:
  std::array<std::optional<AnimationActor>, kActorSlots> table{};
  std::array<bool, kBattleSlots> battle_actors{};
  std::optional<AnimationGimmick> gimmick;
  int bank_index = 0;
  int event_state = 0;
  int release_status = 0;
  int restart_status = 0;
  int window_frame = 0;
  std::set<int> cannot_unwind;
  // The animations the actor's banks lack.
  std::set<int> missing;
  SoundPanVolume pan_volume{};
  std::vector<ActorAssetRequest> requests;
  Calls calls;

  AnimationActor& Add(int id) {
    auto& slot = table.at(static_cast<std::size_t>(id));
    slot.emplace();
    slot->motion.slot = static_cast<std::uint8_t>(id);
    slot->motion.run_speed = kRunSpeed;
    slot->motion.walk_speed = kWalkSpeed;
    return *slot;
  }

  [[nodiscard]] bool Called(const std::string& call) const {
    return std::ranges::find(calls, call) != calls.end();
  }
  [[nodiscard]] std::size_t Count(std::string_view prefix) const {
    return static_cast<std::size_t>(
        std::ranges::count_if(calls, [prefix](const std::string& call) {
          return call.starts_with(prefix);
        }));
  }

  AnimationActor* Actor(int id) override {
    if (id < 0 || id >= kActorSlots) return nullptr;
    auto& slot = table.at(static_cast<std::size_t>(id));
    return slot ? &*slot : nullptr;
  }
  bool HasBattleActor(int index) override {
    return index >= 0 && index < kBattleSlots &&
           battle_actors.at(static_cast<std::size_t>(index));
  }
  int TickSpeed() override { return kTickSpeed; }
  int EventPlaybackState() override { return event_state; }
  std::optional<AnimationGimmick> GimmickForActorSlot(int /*slot*/) override {
    return gimmick;
  }
  void SetGimmickMode(int slot, std::int16_t mode) override {
    Record("gimmick mode", slot, mode);
  }
  int DrawnWeaponSoundBase(int sequence_model) override {
    return kDrawnSoundBase + sequence_model;
  }
  int SheathedWeaponSoundBase(int sequence_model) override {
    return kSheathedSoundBase + sequence_model;
  }
  int QuadrantMoveAnimation(bool fast, int quadrant) override {
    return (fast ? kFastMoveBase : kMoveBase) + quadrant;
  }
  int SharedSequenceBankIndex() override { return bank_index; }
  void SetSharedSequenceBankIndex(int index) override { bank_index = index; }
  void SetTransitionControl(int control) override {
    Record("transition", control);
  }
  void QueueSequenceAsset(const ActorAssetRequest& request) override {
    requests.push_back(request);
  }
  int ReleaseSharedSequence(int bank) override {
    Record("release", bank);
    return release_status;
  }
  void ReleaseSequenceEffectTextures(int bank) override {
    Record("release textures", bank);
  }
  void BeginActorDeath(int id, int animate) override {
    Record("death", id, animate);
  }
  void SetWeaponModelsDrawn(int id, bool drawn) override {
    Record("models", id, drawn);
  }
  void PlaySoundPair(AnimationActor& actor, int first, int second) override {
    Record("sound", actor.motion.slot, first, second);
  }
  SoundPanVolume SoundPanAndVolume(int id) override {
    Record("pan volume", id);
    return pan_volume;
  }
  void CancelTeleportTransition(AnimationActor& actor) override {
    Record("cancel teleport", actor.motion.slot);
  }
  void ReleaseAiDestinationTile(int tile_x, int tile_z) override {
    Record("release tile", tile_x, tile_z);
  }
  void UploadTextureFrame(AnimationActor& actor, int channel,
                          int frame) override {
    Record("upload", actor.motion.slot, channel, frame);
  }
  void RestartTextureAnimation(int id) override {
    Record("texture restart", id);
  }
  AnimationLookup FindAnimationHeader(AnimationActor& /*actor*/,
                                      int animation) override {
    Record("find", animation);
    return {.bank = kFoundBank,
            .has_entry = !missing.contains(animation),
            .length = kLength,
            .resolved_set = static_cast<std::uint8_t>(animation)};
  }
  bool CanUnwindBoundAnimation(AnimationActor& actor) override {
    Record("can unwind", actor.motion.slot);
    return !cannot_unwind.contains(actor.motion.slot);
  }
  int RestartAnimationEvents(AnimationActor& actor) override {
    Record("restart events", actor.motion.slot);
    return restart_status;
  }
  void InitializePose(AnimationActor& actor) override {
    Record("initialize", actor.motion.slot);
  }
  void BeginShadowBlend(AnimationActor& actor, int blend_frames) override {
    Record("shadow blend", actor.motion.slot, blend_frames);
  }
  int RunBattleWindowEvents(AnimationActor& actor) override {
    Record("window events", actor.motion.slot);
    actor.pose = kClearedCursor;
    return window_frame;
  }
  void AdvanceSkeletalAnimation(AnimationActor& actor) override {
    Record("advance", actor.motion.slot, actor.frames_per_step);
  }
  void CaptureBoneAnchor(AnimationActor& /*actor*/, int bone,
                         int lock_index) override {
    Record("anchor", bone, lock_index);
  }
  void SetBonePoseLockSlot(AnimationActor& /*actor*/, int bone,
                           int lock_slot) override {
    Record("lock slot", bone, lock_slot);
  }

 private:
  template <typename... Values>
  void Record(std::string call, Values... values) {
    ((call += ' ' + std::to_string(values)), ...);
    calls.push_back(std::move(call));
  }
};

// An actor whose pose comes from shared bank kBank, with an override bound.
AnimationActor& PosedFromBank(FakeAnimationHost& host, int id) {
  AnimationActor& actor = host.Add(id);
  actor.shared_sequences.at(kBank - 1) = kBankSequence;
  actor.animation_header = kBankSequence;
  actor.pose_override_active = true;
  actor.animation_set = kRow;
  actor.pose = {.script = kScript, .next_event_frame = kEventFrame};
  actor.blend_pose = actor.pose;
  return actor;
}

// Places a zeroed actor record in the actor table for the test's lifetime.
class TableRecord {
 public:
  explicit TableRecord(int id) : id_(id), saved_(Actors_actorRecords[id]) {
    Actors_actorRecords[id_] = record_.get();
  }
  TableRecord(const TableRecord&) = delete;
  TableRecord& operator=(const TableRecord&) = delete;
  TableRecord(TableRecord&&) = delete;
  TableRecord& operator=(TableRecord&&) = delete;
  ~TableRecord() { Actors_actorRecords[id_] = saved_; }

  Actors_ActorRecord& operator*() const { return *record_; }
  Actors_ActorRecord* Get() const { return record_.get(); }

 private:
  int id_;
  Actors_ActorRecord* saved_;
  std::unique_ptr<Actors_ActorRecord> record_ =
      std::make_unique<Actors_ActorRecord>();
};

TEST_CASE("Shared bank retirement checks every posed actor before changing one",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  const AnimationActor& first = PosedFromBank(host, kAshley);
  const AnimationActor& second = PosedFromBank(host, kActor);
  AnimationActor& elsewhere = host.Add(kOther);
  elsewhere.shared_sequences.at(kBank - 1) = kBankSequence;
  elsewhere.animation_header = kOtherSequence;
  elsewhere.pose_override_active = true;
  host.cannot_unwind.insert(kActor);
  const AnimationActor first_before = first;
  const AnimationActor second_before = second;

  CHECK(actors::UnloadSharedSequence(host, kBank) == -1);
  CHECK(first == first_before);
  CHECK(second == second_before);
  CHECK(host.calls == Calls{"can unwind 0", "can unwind 3"});
}

// A posed actor whose shared bank was retired loses its pose and override.
void CheckRetired(const AnimationActor& actor) {
  CHECK_FALSE(actor.pose_override_active);
  CHECK(actor.animation_header == SequenceHandle::kNone);
  CHECK(actor.animation_set == kNoSet);
  CHECK(actor.pose == kClearedCursor);
  CHECK(actor.blend_pose == kClearedCursor);
}

TEST_CASE("Shared bank retirement unwinds and clears every posed actor",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  host.release_status = kReleaseStatus;
  const AnimationActor& bound = PosedFromBank(host, kActor);
  AnimationActor& plain = PosedFromBank(host, kOther);
  plain.pose_override_active = false;
  AnimationActor& elsewhere = host.Add(kAshley);
  elsewhere.animation_header = kOtherSequence;
  elsewhere.animation_set = kRow;

  CHECK(actors::UnloadSharedSequence(host, kBank) == kReleaseStatus);
  CheckRetired(bound);
  CheckRetired(plain);
  CHECK(elsewhere.animation_header == kOtherSequence);
  CHECK(elsewhere.animation_set == kRow);
  CHECK(host.calls == Calls{"can unwind 3", "restart events 3", "release 2"});
}

TEST_CASE("Shared bank retirement refuses invalid banks and failed restarts",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  PosedFromBank(host, kActor);
  CHECK(actors::UnloadSharedSequence(host, 0) == -1);
  CHECK(actors::UnloadSharedSequence(host, kBanks + 1) == -1);
  CHECK(host.calls.empty());

  host.restart_status = -1;
  CHECK(actors::UnloadSharedSequence(host, kBank) == -1);
  CHECK(host.calls == Calls{"can unwind 3", "restart events 3"});
}

TEST_CASE("Shared bank selection resets textures only for actors losing a row",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& losing = host.Add(kAshley);
  losing.texture_rows = {kRow, kNoRow, kNoRow};
  losing.texture_current_frame = {kSkippedFrames, 1};
  AnimationActor& keeping = host.Add(kActor);
  keeping.texture_rows = {kRow, kRow, kNoRow};
  keeping.texture_current_frame = {kSkippedFrames, kSkippedFrames};
  AnimationActor& rowless = host.Add(kOther);
  rowless.texture_rows = {kNoRow, kNoRow, kNoRow};
  rowless.texture_current_frame = {kSkippedFrames, kSkippedFrames};

  CHECK(actors::SelectSharedSequenceBank(host, kBank) == 0);
  CHECK(host.calls == Calls{"upload 0 0 0"});
  CHECK(host.bank_index == kBank - 1);
  CHECK(actors::SharedSequenceBankId(host) == kBank);
  CHECK(actors::SelectSharedSequenceBank(host, 0) == -1);
  CHECK(actors::SelectSharedSequenceBank(host, kBanks + 1) == -1);
  CHECK(host.bank_index == kBank - 1);
}

TEST_CASE("Shared bank texture resets refuse an out-of-range bank index",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  host.bank_index = kBanks;
  const ChannelWarnings warnings("actors");
  actors::SetSharedSequenceBank(host, 1);
  CHECK(warnings.messages == Calls{Refused("Shared bank texture reset",
                                           "bank index 3 is not 0 to 2")});
  CHECK(host.calls.empty());
  CHECK(host.bank_index == 0);
}

constexpr int kDrawMode = 1;
constexpr int kSequenceModel = 3;

AnimationActor& WeaponActor(FakeAnimationHost& host) {
  AnimationActor& actor = host.Add(kActor);
  actor.sequence_model_id = kSequenceModel;
  return actor;
}

TEST_CASE("Weapon draws play the draw animation and queue the idle",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  const AnimationActor& actor = WeaponActor(host);
  actors::SetActorWeaponDrawn(host, kActor, 1, kDrawMode);
  CHECK(actor.weapon_drawn);
  CHECK(actor.animation_id == kDrawn + kDrawMode * 2 + kDrawStep);
  CHECK(actor.blend_frames == kDrawBlend);
  CHECK(actor.motion.weapon_pose_active);
  CHECK(actor.queued_animation == kIdle);
  CHECK(actor.queued_blend_frames == kDrawBlend);
}

TEST_CASE("Weapon sheathes play the sheathe animation before the weapon goes",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = WeaponActor(host);
  actor.weapon_drawn = true;
  actors::SetActorWeaponDrawn(host, kActor, 0, 0);
  CHECK_FALSE(actor.weapon_drawn);
  CHECK(actor.animation_id == kDrawn + kSheatheStep);
  CHECK(actor.blend_frames == kSheatheBlend);
}

TEST_CASE("Weapon draw sounds come from model 0 and only on a change",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = WeaponActor(host);
  actors::SetActorWeaponDrawn(host, kActor, 1, kDrawMode);
  CHECK(host.Called("sound 3 " +
                    std::to_string(kDrawnSoundBase + kSequenceModel) + " 0"));
  actors::SetActorWeaponDrawn(host, kActor, 0, 0);
  CHECK(host.Called("sound 3 " +
                    std::to_string(kSheathedSoundBase + kSequenceModel + 1) +
                    " 0"));
  host.calls.clear();
  actors::SetActorWeaponDrawn(host, kActor, 0, 0);
  CHECK(host.calls.empty());

  actor.model_id = kSequenceModel;
  actors::SetActorWeaponDrawn(host, kActor, 1, kDrawMode);
  CHECK(actor.weapon_drawn);
  CHECK(host.Count("sound") == 0);
}

TEST_CASE("Weapon draws in an event or of the models alone skip the animation",
          "[actors][actors-animation-control]") {
  constexpr int kModelsOnly = 2;
  constexpr int kOddDrawn = 3;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);

  actors::SetActorWeaponDrawn(host, kActor, kOddDrawn, kModelsOnly);
  CHECK(actor.weapon_drawn);
  CHECK(host.calls == Calls{"models 3 1"});

  actor.weapon_drawn = false;
  host.calls.clear();
  host.event_state = kEventScriptRunning;
  actors::SetActorWeaponDrawn(host, kActor, 1, 1);
  CHECK(actor.weapon_drawn);
  CHECK(actor.animation_id == kDrawn + kIdle);
  CHECK(actor.blend_frames == kDrawBlend);
  CHECK(actor.queued_animation == 0);
  CHECK(host.Count("sound") == 0);

  actors::PlayActorWeaponDrawSound(host, kActor, kOddDrawn);
  CHECK(host.Called("sound 3 " + std::to_string(kDrawnSoundBase + kOddDrawn) +
                    " 0"));
}

struct StanceCase {
  bool locked = false;
  bool airborne = false;
  bool jumping = false;
  bool drawn = false;
  bool action_pose = false;
  int requested = 0;
  int played = 0;
};

constexpr std::array kStanceCases{
    StanceCase{.locked = true, .requested = kIdle, .played = kLockedIdle},
    StanceCase{.locked = true, .requested = kNeutral, .played = kLockedIdle},
    StanceCase{.locked = true,
               .drawn = true,
               .requested = kNeutral,
               .played = kDrawn + kLockedIdle},
    StanceCase{.airborne = true, .requested = kIdle, .played = kAirborneIdle},
    StanceCase{.airborne = true,
               .jumping = true,
               .requested = kIdle,
               .played = kAirborneJumpIdle},
    StanceCase{.jumping = true, .requested = kIdle, .played = kJumpIdle},
    StanceCase{.jumping = true, .requested = kWalk, .played = kJumpWalk},
    StanceCase{.drawn = true, .requested = kIdle, .played = kDrawn + kIdle},
    StanceCase{.action_pose = true, .requested = kIdle, .played = kStance},
    StanceCase{.action_pose = true, .requested = kWalk, .played = kStanceWalk},
    StanceCase{.requested = kWalk, .played = kWalk},
    StanceCase{
        .drawn = true, .requested = kDrawn + kWalk, .played = kDrawn + kWalk},
};

TEST_CASE("Animation starts substitute the actor's stance animation",
          "[actors][actors-animation-control]") {
  for (const StanceCase& stance : kStanceCases) {
    CAPTURE(stance.requested, stance.played);
    FakeAnimationHost host;
    AnimationActor& actor = host.Add(kActor);
    actor.motion.motion_locked = stance.locked;
    actor.motion.movement_mode = stance.airborne ? kAirborneMode : 0;
    actor.motion.is_jumping = stance.jumping;
    actor.weapon_drawn = stance.drawn;
    actor.motion.action_pose_active = stance.action_pose;
    CHECK(actors::PlayActorAnimation(host, kActor, stance.requested, kBlend) ==
          0);
    CHECK(actor.animation_id == stance.played);
  }

  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.motion.action_pose_active = true;
  actor.animation_id = kStanceWalk;
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK(host.Count("find") == 0);
  CHECK(host.Count("initialize") == 0);
}

TEST_CASE("Animation starts fall back to animation 0 and restart changed poses",
          "[actors][actors-animation-control]") {
  constexpr int kMissing = 0x30;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  host.missing = {kMissing};
  actor.motion.is_running = true;

  CHECK(actors::PlayActorAnimation(host, kActor, kMissing, kBlend) == 0);
  CHECK(actor.animation_id == 0);
  CHECK_FALSE(actor.motion.is_running);
  CHECK(actor.animation_header == kFoundBank);
  CHECK(actor.animation_length == kLength);
  CHECK(host.Count("initialize") == 1);
  CHECK(host.Called("texture restart 3"));
}

TEST_CASE("Animation starts keep the pose when animation 0 is missing too",
          "[actors][actors-animation-control]") {
  constexpr int kMissing = 0x30;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kWalk;
  host.missing = {kMissing, 0};
  CHECK(actors::PlayActorAnimation(host, kActor, kMissing, kBlend) == 0);
  CHECK(actor.animation_id == kWalk);
  CHECK(host.Count("initialize") == 0);
}

TEST_CASE("Animation starts initialize only changed poses or event poses",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  host.Add(kActor);
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK(host.Count("initialize") == 1);
  host.event_state = kEventScriptRunning;
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK(host.Count("initialize") == 2);
}

TEST_CASE("Animation starts refuse missing actors and ignore locked actors",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == -1);
  CHECK(actors::PlayActorAnimation(host, kActor, 0, kBlend) == 0);
  host.Add(kActor).animation_locked = true;
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK(host.calls.empty());
}

TEST_CASE("Animation starts refuse while a pose override cannot restart",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.pose_override_active = true;
  host.restart_status = -1;
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == -1);
  CHECK(actor.pose_override_active);
  CHECK(host.calls == Calls{"restart events 3"});

  host.restart_status = 0;
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK_FALSE(actor.pose_override_active);
  CHECK(actor.animation_id == kWalk);
}

TEST_CASE("Animation starts double large actors' blends and skip short ones",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.collision_radius =
      static_cast<std::uint16_t>(actors::kLargeActorRadius);
  CHECK(actors::PlayActorAnimation(host, kActor, kWalk, kBlend) == 0);
  CHECK(actor.blend_frames == kBlend * 2);
  CHECK(actor.blend_frame == 1);
  CHECK(host.Called("shadow blend 3 " + std::to_string(kBlend)));

  actor.collision_radius = 0;
  CHECK(actors::PlayActorAnimation(host, kActor, kIdle, 1) == 0);
  CHECK(actor.blend_frames == 0);
  CHECK(actor.blend_frame == 0);
}

TEST_CASE("Animation starts enter climbs and hangs and settle without motion",
          "[actors][actors-animation-control]") {
  constexpr std::int16_t kFacing = 0x300;
  constexpr std::int16_t kFacingDelta = 0xE00;
  constexpr std::uint16_t kMoveFacing = 0x100;
  constexpr std::int16_t kStep = 4;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.motion.facing = kFacing;
  actor.motion.facing_delta = kFacingDelta;
  actors::PlayActorAnimation(host, kActor, kClimb, kBlend);
  CHECK(actor.motion.locomotion_state == kClimbing);
  CHECK(actor.move_facing == kMoveFacing);
  actors::PlayActorAnimation(host, kActor, kHang, kBlend);
  CHECK(actor.motion.locomotion_state == kHanging);

  actor.motion.root_motion.x = kStep;
  actors::PlayActorAnimation(host, kActor, kWalk, kBlend);
  CHECK(actor.motion.locomotion_state == kHanging);
  actor.motion.root_motion.x = 0;
  actor.motion.movement_pending = true;
  actors::PlayActorAnimation(host, kActor, kIdle, kBlend);
  CHECK(actor.motion.locomotion_state == 0);
  CHECK_FALSE(actor.motion.movement_pending);
}

TEST_CASE("Animation starts at a frame advance by the frame less half a tick",
          "[actors][actors-animation-control]") {
  constexpr std::uint8_t kFramesPerStep = 3;
  constexpr int kStartFrame = 5;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.frames_per_step = kFramesPerStep;

  CHECK(actors::PlayActorAnimationAtFrame(host, kActor, kWalk, kBlend, 0) == 0);
  CHECK(actor.frames_per_step == kFramesPerStep);
  actors::PlayActorAnimationAtFrame(host, kActor, kWalk, kBlend, kStartFrame);
  CHECK(host.Called("advance 3 " + std::to_string(kStartFrame - 1)));
  CHECK(actor.frames_per_step == 1);

  host.calls.clear();
  actor.frames_per_step = kFramesPerStep;
  actors::PlayActorAnimationAtFrame(host, kActor, kWalk, kBlend, 1);
  CHECK(host.Count("advance") == 0);
  CHECK(actor.frames_per_step == 1);
}

TEST_CASE("Animation starts at a frame refuse a missing actor",
          "[actors][actors-animation-control]") {
  constexpr int kStartFrame = 5;
  FakeAnimationHost host;
  CHECK(actors::PlayActorAnimationAtFrame(host, kOther, kWalk, kBlend,
                                          kStartFrame) == -1);
  const ChannelWarnings warnings("actors");
  CHECK(actors::PlayActorAnimationAtFrame(host, kOther, 0, kBlend,
                                          kStartFrame) == 0);
  CHECK(warnings.messages ==
        Calls{Refused("Animation start frame", "no actor record for actor 4")});
}

constexpr int kStrikeKind = 2;
constexpr int kBraceKind = 5;
constexpr int kReadyKind = 6;
constexpr int kGuardKind = 7;
constexpr int kUnknownKind = 8;

TEST_CASE("Action poses face their target and play the pose's animation",
          "[actors][actors-animation-control]") {
  constexpr std::uint16_t kActionTurnFrames = 0xA;
  constexpr std::uint8_t kRunFrames = 4;
  constexpr std::int16_t kDistance = 100;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  const MotionVector target{.x = -kDistance};
  const int facing_delta =
      actors::ActorFacingDeltaToPoint(actor.motion, &target);
  REQUIRE(facing_delta != 0);
  actor.motion.run_frames = kRunFrames;
  actor.queued_animation = kWalk;

  actors::PlayActorActionPose(host, kActor, &target, kStrikeKind);
  CHECK(actor.animation_id == kActionPoseBase + kStrikeKind);
  CHECK(actor.motion.facing_delta == facing_delta);
  CHECK(actor.motion.turn_frames == kActionTurnFrames);
  CHECK(actor.motion.run_frames == 0);
  CHECK(actor.queued_animation == 0);
  CHECK(actor.blend_frames == kBlend);
}

TEST_CASE("Action poses pick the brace, guard, ready and stance poses",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actors::PlayActorActionPose(host, kActor, nullptr, kBraceKind);
  CHECK(actor.animation_id == kBracePose);
  CHECK(actor.motion.facing_delta == 0);
  actors::PlayActorActionPose(host, kActor, nullptr, kGuardKind);
  CHECK(actor.animation_id == kGuardPose);
  actors::PlayActorActionPose(host, kActor, nullptr, kReadyKind);
  CHECK(actor.animation_id == kReadyPose);
  actor.motion.action_pose_active = true;
  actors::PlayActorActionPose(host, kActor, nullptr, kReadyKind);
  CHECK(actor.animation_id == kStance);
}

TEST_CASE("Action poses skip hanging actors and unknown pose kinds",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.motion.locomotion_state = kHanging;
  actors::PlayActorActionPose(host, kActor, nullptr, kReadyKind);
  actor.motion.locomotion_state = 0;
  actors::PlayActorActionPose(host, kActor, nullptr, kUnknownKind);
  CHECK(host.calls.empty());
  CHECK(actor.motion.turn_frames == 0);
  CHECK(actor.animation_id == 0);
}

TEST_CASE("Animation controls that read a record refuse a missing actor",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  const ChannelWarnings warnings("actors");
  actors::PlayActorActionPose(host, kOther, nullptr, 1);
  actors::PlayActorActionPose(host, kOther, nullptr, kUnknownKind);
  CHECK_FALSE(actors::IsAshleyTransitionPose(host));
  actors::PlayActorTurnAnimation(host, kOther, kQuarterTurn);
  CHECK(actors::AnimationBattleWindowFrame(host, kOther) == 0);
  const std::string missing = "no actor record for actor 4";
  CHECK(warnings.messages ==
        Calls{Refused("Action pose", missing),
              Refused("Ashley transition pose", "no actor record for actor 0"),
              Refused("Turn animation", missing),
              Refused("Battle window frame", missing)});
  CHECK(host.calls.empty());
}

TEST_CASE("Ashley's transition pose blocks the idle restore",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& ashley = host.Add(kAshley);
  for (const int animation : {kTransition, kDrawnTransition}) {
    ashley.animation_id = static_cast<std::uint8_t>(animation);
    CHECK(actors::IsAshleyTransitionPose(host));
    actors::RestoreActorIdlePose(host, kAshley);
    CHECK(ashley.animation_id == animation);
  }
  CHECK(host.calls.empty());
  ashley.animation_id = kWalk;
  CHECK_FALSE(actors::IsAshleyTransitionPose(host));
  actors::ReturnActorToIdle(host, kAshley);
  CHECK(ashley.animation_id == kIdle);
  CHECK(ashley.blend_frames == kBasePoseBlend);
}

struct BusyCase {
  std::uint8_t state = 0;
  bool holding = false;
  int animation = 0;
  ActionAnimationBusy busy = ActionAnimationBusy::kIdle;
};

constexpr std::uint8_t kActingState = 3;
constexpr std::uint8_t kGuardState = 5;
constexpr std::uint8_t kRecoverState = 11;
constexpr std::array kBusyCases{
    BusyCase{.animation = kIdle},
    BusyCase{.animation = kBusy, .busy = ActionAnimationBusy::kBusy},
    BusyCase{.animation = kReadyPose, .busy = ActionAnimationBusy::kBusy},
    BusyCase{.animation = kStrike, .busy = ActionAnimationBusy::kBusy},
    BusyCase{.state = kActingState,
             .animation = kIdle,
             .busy = ActionAnimationBusy::kBusy},
    BusyCase{.state = kGuardState, .animation = kIdle},
    BusyCase{.state = kRecoverState, .holding = true, .animation = kBusy},
    BusyCase{.animation = kDrawn + kSheatheStep,
             .busy = ActionAnimationBusy::kBusy},
    BusyCase{.animation = kDrawn + kTransition},
    BusyCase{.animation = kDrawn + kFirstBusy,
             .busy = ActionAnimationBusy::kBusy},
    BusyCase{.animation = kDrawn + kIdle},
};

TEST_CASE("Action animation busy tells idle, busy and missing actors apart",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  CHECK(actors::IsActorActionAnimationBusy(host, kActor) ==
        ActionAnimationBusy::kNoActor);
  host.gimmick = AnimationGimmick{};
  CHECK(actors::IsActorActionAnimationBusy(host, kActor) ==
        ActionAnimationBusy::kIdle);
  AnimationActor& actor = host.Add(kActor);
  for (const BusyCase& busy : kBusyCases) {
    CAPTURE(busy.state, busy.animation);
    actor.motion.action_animation_state = busy.state;
    actor.holding_last_frame = busy.holding;
    actor.animation_id = static_cast<std::uint8_t>(busy.animation);
    CHECK(actors::IsActorActionAnimationBusy(host, kActor) == busy.busy);
  }
}

TEST_CASE("Base pose restores settle a waiting gimmick by its full mode word",
          "[actors][actors-animation-control]") {
  constexpr std::int16_t kSettling = 0xF9;
  constexpr std::int16_t kSettlingAlt = 0xFF;
  constexpr std::int16_t kWideMode = 0x1F9;
  FakeAnimationHost host;
  for (const std::int16_t mode : {kSettling, kSettlingAlt}) {
    host.gimmick = AnimationGimmick{.mode = mode};
    host.calls.clear();
    actors::RestoreActorBasePose(host, kOther, kIdle);
    CHECK(host.calls == Calls{"gimmick mode 4 0"});
  }
  host.calls.clear();
  host.gimmick = AnimationGimmick{.mode = kWideMode};
  actors::RestoreActorBasePose(host, kOther, kIdle);
  host.gimmick = AnimationGimmick{.mode = kSettling, .support_index = 1};
  actors::RestoreActorNeutralPose(host, kOther);
  host.gimmick.reset();
  actors::RestoreActorIdlePose(host, kOther);
  CHECK(host.calls.empty());
}

// A base pose restore stops every pending motion of the actor.
void CheckStopped(const AnimationActor& actor) {
  CHECK(actor.motion.run_frames == 0);
  CHECK(actor.motion.locomotion_state == 0);
  CHECK(actor.motion.turn_frames == 0);
  CHECK(actor.motion.move_frames == 0);
  CHECK(actor.motion.scale_frames == 0);
  CHECK_FALSE(actor.motion.movement_pending);
}

constexpr std::uint16_t kRestoreFrames = 5;

TEST_CASE("Base pose restores stop the actor and play the base pose",
          "[actors][actors-animation-control]") {
  constexpr std::uint8_t kRunFrames = 4;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kWalk;
  actor.motion.turn_frames = kRestoreFrames;
  actor.motion.move_frames = kRestoreFrames;
  actor.motion.scale_frames = kRestoreFrames;
  actor.motion.movement_pending = true;
  actor.motion.locomotion_state = kClimbing;
  actor.motion.run_frames = kRunFrames;

  actors::RestoreActorNeutralPose(host, kActor);
  CHECK(actor.animation_id == kNeutral);
  CHECK(actor.blend_frames == kBasePoseBlend);
  CheckStopped(actor);
}

TEST_CASE("Idle pose restores keep held poses and cancel teleports",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kHeldPose;
  actor.motion.turn_frames = kRestoreFrames;
  actors::RestoreActorIdlePose(host, kActor);
  CHECK(actor.motion.turn_frames == kRestoreFrames);

  actor.animation_id = kWalk;
  actor.reaction_state = kTeleportReaction;
  actors::RestoreActorIdlePose(host, kActor);
  CHECK(host.Called("cancel teleport 3"));
  CHECK(actor.animation_id == kIdle);

  actor.animation_id = kWalk;
  actor.reaction_state = 1;
  actor.motion.turn_frames = kRestoreFrames;
  actors::RestoreActorIdlePose(host, kActor);
  CHECK(actor.animation_id == kWalk);
  CHECK(actor.motion.turn_frames == 0);
}

TEST_CASE("Base pose restores finish a hang's step",
          "[actors][actors-animation-control]") {
  constexpr MotionVector kPosition{.x = 10, .y = 20, .z = 30};
  constexpr MotionVector kRootMotion{.x = 4, .y = 5, .z = 6};
  constexpr MotionVector kSettled{.x = 14, .y = 25, .z = 36};
  constexpr std::int16_t kFacing = 0x100;
  constexpr std::int16_t kFacingDelta = 0x10;
  constexpr std::uint16_t kFrames = 3;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kHang;
  actor.motion.locomotion_state = kHanging;
  actor.motion.move_frames = kFrames;
  actor.motion.position = kPosition;
  actor.motion.root_motion = kRootMotion;
  actor.motion.facing = kFacing;
  actor.motion.facing_delta = kFacingDelta;
  actors::RestoreActorIdlePose(host, kActor);
  CHECK(actor.motion.position == kSettled);
  CHECK(actor.motion.facing == kFacing + kFacingDelta);
  CHECK(actor.motion.move_frames == 0);
  CHECK(actor.motion.locomotion_state == kHanging);
  CHECK(actor.animation_id == kHang);
}

TEST_CASE("Idle pose restores free the tile a far reaction reserved",
          "[actors][actors-animation-control]") {
  constexpr std::uint8_t kTileX = 5;
  constexpr std::uint8_t kTileZ = 7;
  FakeAnimationHost host;
  AnimationActor& ashley = host.Add(kAshley);
  ashley.motion.current_tile = {.x = kTileX, .z = kTileZ};
  ashley.animation_id = kFarReaction;
  actors::RestoreActorIdlePose(host, kAshley);
  CHECK(host.Called("release tile 5 5"));
  ashley.animation_id = kFarReaction;
  ashley.motion.facing = kBehind;
  actors::RestoreActorIdlePose(host, kAshley);
  CHECK(host.Called("release tile 5 9"));
  CHECK(ashley.animation_id == kIdle);
}

TEST_CASE("Battle window frames keep the pose's event cursor",
          "[actors][actors-animation-control]") {
  constexpr int kWindowFrame = 12;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  CHECK(actors::AnimationBattleWindowFrame(host, kActor) == 0);
  CHECK(host.calls.empty());

  const PoseEventCursor cursor{.script = kScript,
                               .next_event_frame = kEventFrame};
  actor.pose = cursor;
  host.window_frame = kWindowFrame;
  CHECK(actors::AnimationBattleWindowFrame(host, kActor) == kWindowFrame);
  CHECK(host.calls == Calls{"window events 3"});
  CHECK(actor.pose == cursor);
}

constexpr std::uint8_t kPositional = 1;
constexpr std::uint8_t kFixed = 2;
constexpr std::uint8_t kPan = 0x30;
constexpr std::uint8_t kVolume = 0x50;
constexpr std::uint8_t kLevel = 0x60;
constexpr std::uint8_t kFlags = 0x70;
constexpr std::uint8_t kParameter = 9;
constexpr std::uint8_t kFixedLevel = 0x80;
constexpr SoundEffectSlot TargetedSound() {
  return {.kind = kPositional,
          .level = kLevel,
          .flags = kFlags,
          .parameter = kParameter};
}
constexpr SoundEffectSlot kTargeted = TargetedSound();

TEST_CASE("Sound overrides take positional levels from the current effect",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  host.pan_volume = {.pan = kPan, .volume = kVolume};

  actors::ApplyActorSoundOverrides(host, kActor, kTargeted);
  CHECK(actor.sound_effect == SoundEffectSlot{.kind = kPositional,
                                              .level = kPan,
                                              .flags = kVolume,
                                              .parameter = kParameter});
  CHECK(actor.sound_effect_target == kTargeted);

  actor.sound_effect = {.kind = kPositional, .level = kLevel};
  actors::ApplyActorSoundOverrides(host, kActor, kTargeted);
  CHECK(actor.sound_effect == SoundEffectSlot{.kind = kPositional,
                                              .level = kLevel,
                                              .flags = kVolume,
                                              .parameter = kParameter});

  actor.sound_effect = {.kind = kFixed};
  actors::ApplyActorSoundOverrides(host, kActor, kTargeted);
  CHECK(actor.sound_effect == SoundEffectSlot{.kind = kPositional,
                                              .level = kFixedLevel,
                                              .flags = kPositional,
                                              .parameter = kParameter});

  actor.sound_effect = {};
  SoundEffectSlot quiet = kTargeted;
  quiet.level = 0;
  actors::ApplyActorSoundOverrides(host, kActor, quiet);
  CHECK(actor.sound_effect == SoundEffectSlot{.kind = kPositional,
                                              .flags = kVolume,
                                              .parameter = kParameter});
}

TEST_CASE("Sound overrides copy, silence and reach every actor",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  AnimationActor& other = host.Add(kOther);
  SoundEffectSlot direct = kTargeted;
  direct.parameter = 0;
  actors::ApplyActorSoundOverrides(host, kActor, direct);
  CHECK(actor.sound_effect == direct);
  CHECK(host.calls.empty());

  actors::SetActorSoundOverrides(host, kActor, SoundEffectSlot{});
  CHECK(actor.sound_effect == SoundEffectSlot{});
  CHECK(other.sound_effect == SoundEffectSlot{});

  actors::SetActorSoundOverrides(host, -1, SoundEffectSlot{.kind = kFixed});
  CHECK(actor.sound_effect.kind == kFixed);
  CHECK(other.sound_effect.kind == kFixed);
  actors::ApplyActorSoundOverrides(host, kAshley, kTargeted);
  CHECK(host.calls.empty());
}

TEST_CASE("Travel animations default to the walk and refuse missing actors",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  CHECK(actors::SetActorTravelAnimation(host, kActor, kRun) == -1);
  CHECK_FALSE(actors::ActorTravelAnimation(host, kActor));
  const AnimationActor& actor = host.Add(kActor);
  CHECK(actors::SetActorTravelAnimation(host, kActor, -1) == 0);
  CHECK(actors::ActorTravelAnimation(host, kActor) == kWalk);
  CHECK(actors::SetActorTravelAnimation(host, kActor, kRun) == 0);
  CHECK(actor.travel_animation == kRun);
}

TEST_CASE("Queued animations store the animation, blend and start frame",
          "[actors][actors-animation-control]") {
  constexpr int kStartFrame = 3;
  FakeAnimationHost host;
  CHECK(actors::QueueActorAnimation(host, kActor, kRun, kBlend, 0) == -1);
  const AnimationActor& actor = host.Add(kActor);
  CHECK(actors::QueueActorAnimation(host, kActor, kRun, kBlend, kStartFrame) ==
        0);
  CHECK(actor.queued_animation == kRun);
  CHECK(actor.queued_blend_frames == kBlend);
  CHECK(actor.queued_start_frame == kStartFrame);
}

TEST_CASE("Animation locks and advances store flags and pass host requests on",
          "[actors][actors-animation-control]") {
  constexpr int kOddLock = 3;
  constexpr int kControl = 5;
  FakeAnimationHost host;
  const AnimationActor& actor = host.Add(kActor);
  actors::SetActorAnimationLocked(host, kActor, kOddLock);
  CHECK(actor.animation_locked);
  actors::SetActorAnimationLocked(host, kActor, 2);
  CHECK_FALSE(actor.animation_locked);
  actors::RequestActorAnimationAdvance(host, kActor);
  CHECK(actor.advance_requested);

  actors::SetActorTransitionMode(host, kControl);
  actors::InterruptActorAction(host, kActor, 1);
  actors::ReleaseSequenceEffectTextures(host, kBank);
  actors::SharedSequenceBank(host);
  CHECK(host.calls == Calls{"transition 5", "death 3 1", "release textures 2"});
}

TEST_CASE("Sequence asset requests name the kind, slot and model",
          "[actors][actors-animation-control]") {
  constexpr std::int8_t kIndex = 5;
  constexpr std::int16_t kWeaponId = 0x21;
  constexpr int kCategory = 6;
  constexpr int kArmedSpawn = 1;
  constexpr int kUnarmedSpawn = 2;
  constexpr std::uint8_t kTextureSlot = 3;
  constexpr std::uint8_t kSequence = 7;
  constexpr std::uint8_t kEffectTextures = 9;
  FakeAnimationHost host;
  actors::QueueActorBaseSequenceLoad(host, kActor);
  actors::QueueActorWeaponSequenceLoad(host, kCategory, kAshley, 0, 0);
  actors::QueueActorWeaponSequenceLoad(host, kCategory, kPartner, 1, 0);
  actors::QueueActorWeaponSequenceLoad(host, kCategory, kActor, 0, kArmedSpawn);
  actors::QueueActorWeaponSequenceLoad(host, kCategory, kActor, 0,
                                       kUnarmedSpawn);
  actors::QueueSharedSequenceLoad(host, kIndex, kWeaponId);
  actors::QueueSharedSequenceWithTextures(host, kIndex, kWeaponId,
                                          kTextureSlot);
  actors::QueueSequenceEffectTextures(host, kIndex, kWeaponId, kTextureSlot);

  const std::vector<ActorAssetRequest> expected{
      {.kind = kSequence, .target_slot = kActor},
      {.kind = kSequence, .target_slot = kPartner, .model_id = kCategory},
      {.kind = kSequence, .target_slot = kActor, .model_id = kCategory},
      {.kind = kSequence,
       .target_slot = kIndex,
       .model_id = kWeaponId,
       .sub_kind = 1},
      {.kind = kSequence,
       .target_slot = kIndex,
       .model_id = kWeaponId,
       .sub_kind = 2,
       .texture_slot = kTextureSlot},
      {.kind = kEffectTextures,
       .target_slot = kIndex,
       .model_id = kWeaponId,
       .texture_slot = kTextureSlot},
  };
  CHECK(host.requests == expected);

  const ChannelWarnings warnings("actors");
  actors::QueueActorWeaponSequenceLoad(host, std::nullopt, kActor, 0,
                                       kArmedSpawn);
  CHECK(warnings.messages ==
        Calls{Refused("Weapon sequence load", "no battle actor for actor 3")});
  CHECK(host.requests.size() == expected.size());
}

TEST_CASE("Returning every actor to idle skips absent battle actors",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& present = host.Add(kActor);
  present.animation_id = kWalk;
  AnimationActor& absent = host.Add(kOther);
  absent.animation_id = kWalk;
  host.battle_actors.at(kActor) = true;
  actors::ReturnAllActorsToIdle(host);
  CHECK(present.animation_id == kIdle);
  CHECK(absent.animation_id == kWalk);
}

TEST_CASE("Turn animations pick the side, or the jump idle while jumping",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actors::PlayActorTurnAnimation(host, kActor, kQuarterTurn);
  CHECK(actor.motion.request_mode == kTurnRequest);
  CHECK(actor.animation_id == kTurnRight);
  CHECK(actor.blend_frames == kTurnBlend);
  actors::PlayActorTurnAnimation(host, kActor, kFullTurn - kQuarterTurn);
  CHECK(actor.animation_id == kTurnLeft);
  actor.motion.is_jumping = true;
  actors::PlayActorTurnAnimation(host, kActor, kQuarterTurn);
  CHECK(actor.animation_id == kJumpIdle);
}

TEST_CASE("Turn animations skip level turns, turn locks and locked actors",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actors::PlayActorTurnAnimation(host, kActor, kFullTurn);
  CHECK(actor.motion.request_mode == 0);
  actor.animation_id = kTurnLock;
  actors::PlayActorTurnAnimation(host, kActor, kQuarterTurn);
  CHECK(actor.motion.request_mode == kTurnRequest);
  CHECK(actor.animation_id == kTurnLock);

  actor.animation_id = kIdle;
  actor.motion.request_mode = 0;
  actor.motion.motion_locked = true;
  actors::PlayActorTurnAnimation(host, kActor, kQuarterTurn);
  CHECK(actor.motion.request_mode == 0);
  CHECK(actor.animation_id == kIdle);
}

TEST_CASE("Locomotion holds the action stance while walking or standing",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.motion.action_pose_active = true;
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(actor.animation_id == kStanceWalk);
  CHECK(actor.blend_frames == kStanceWalkBlend);
  actors::UpdateActorLocomotionAnimation(host, actor, 0);
  CHECK(actor.animation_id == kStance);
  CHECK(actor.blend_frames == kStanceIdleBlend);
  actor.animation_id = kStanceHold;
  host.calls.clear();
  actors::UpdateActorLocomotionAnimation(host, actor, 0);
  CHECK(host.calls.empty());
}

TEST_CASE("Locomotion plays jump moves by speed and heading",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.motion.is_jumping = true;
  actors::UpdateActorLocomotionAnimation(host, actor, kRunSpeed);
  CHECK(actor.animation_id == kFastMoveBase);
  CHECK(actor.blend_frames == kJumpBlend);
  actor.motion.request_mode = kSteeringRequest;
  actor.motion.request_facing = kBehind;
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(actor.animation_id == kMoveBase + 2);
  actors::UpdateActorLocomotionAnimation(host, actor, 0);
  CHECK(actor.animation_id == kJumpIdle);
  CHECK(actor.blend_frames == kJumpIdleBlend);
}

TEST_CASE("Locomotion keeps a run going until its animation is missing",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.motion.is_running = true;
  actor.animation_id = kRun;
  actors::UpdateActorLocomotionAnimation(host, actor, kRunSpeed);
  CHECK(actor.animation_id == kRunStraight);
  CHECK(actor.blend_frames == kRunBlend);
  CHECK(actor.motion.is_running);
  actor.animation_id = kRunLast;
  actors::UpdateActorLocomotionAnimation(host, actor, kRunSpeed);
  CHECK(actor.animation_id == kRunTurn);
  host.calls.clear();
  actors::UpdateActorLocomotionAnimation(host, actor, kRunSpeed);
  CHECK(host.calls.empty());
  host.missing = {kRunStraight};
  actor.animation_id = kTurnLeft;
  actors::UpdateActorLocomotionAnimation(host, actor, kRunSpeed);
  CHECK(actor.animation_id == kNeutral);
  CHECK_FALSE(actor.motion.is_running);
}

TEST_CASE("Locomotion strides into fast moves or queues the next stride",
          "[actors][actors-animation-control]") {
  constexpr int kFastSpeed = kRunSpeed - 1;
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kIdle;
  actors::UpdateActorLocomotionAnimation(host, actor, kFastSpeed);
  CHECK(actor.animation_id == kRun);
  actor.animation_id = kNeutral;
  actors::UpdateActorLocomotionAnimation(host, actor, kFastSpeed);
  CHECK(actor.animation_id == kRunLast);
  actor.animation_id = kWalk;
  actors::UpdateActorLocomotionAnimation(host, actor, kFastSpeed);
  CHECK(actor.queued_animation == kRunLast);
  CHECK(actor.queued_blend_frames == kQueuedStrideBlend);
  actor.animation_id = kBusy;
  actors::UpdateActorLocomotionAnimation(host, actor, kFastSpeed);
  CHECK(actor.queued_animation == kRun);
}

TEST_CASE("Locomotion strides walks by heading",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kIdle;
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(actor.animation_id == kWalk);
  host.calls.clear();
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(host.calls.empty());
  actor.motion.request_mode = kSteeringRequest;
  actor.motion.request_facing = kBehind;
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(actor.animation_id == kBackWalk + 1);
  actor.animation_id = kBusy;
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(actor.queued_animation == kBackWalk);
  actor.motion.request_mode = 0;
  actor.animation_id = kRun;
  actors::UpdateActorLocomotionAnimation(host, actor, kWalkingSpeed);
  CHECK(actor.queued_animation == kWalk + 1);
}

TEST_CASE("Locomotion stops a walk and settles a busy pose to idle",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.animation_id = kWalk;
  actors::UpdateActorLocomotionAnimation(host, actor, 0);
  CHECK(actor.animation_id == kNeutral);
  CHECK(actor.queued_animation == kIdle);
  CHECK(actor.queued_blend_frames == kStopBlend);
  actor.animation_id = kBusy;
  actor.locomotion_counter = kCounterLimit;
  actors::UpdateActorLocomotionAnimation(host, actor, 0);
  CHECK(actor.animation_id == kIdle);
  CHECK(actor.locomotion_counter == kTickSpeed / 2);
  actor.locomotion_counter = kCounterLimit;
  actors::UpdateActorLocomotionAnimation(host, actor, 0);
  CHECK(actor.locomotion_counter == kCounterLimit);
}

constexpr std::uint8_t kModelBones = 4;
constexpr int kLockedBone = 3;

TEST_CASE("Bone pose locks refuse bones outside the actor's model",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  const ChannelWarnings warnings("actors");
  actors::SetActorBonePoseLock(host, actor, 0, 1);
  actor.has_model = true;
  actor.bone_count = kModelBones;
  actors::SetActorBonePoseLock(host, actor, kModelBones, 1);
  actors::SetActorBonePoseLock(host, actor, -1, 1);
  const std::string model = " bones of actor 3's model";
  CHECK(warnings.messages ==
        Calls{Refused("Bone pose lock", "bone 0 is outside the 0" + model),
              Refused("Bone pose lock", "bone 4 is outside the 4" + model),
              Refused("Bone pose lock", "bone -1 is outside the 4" + model)});
  CHECK(host.calls.empty());
}

TEST_CASE("Bone pose locks fill two slots and release a locked bone",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.has_model = true;
  actor.bone_count = kModelBones;
  actors::SetActorBonePoseLock(host, actor, kLockedBone, 1);
  actors::SetActorBonePoseLock(host, actor, 1, 1);
  actors::SetActorBonePoseLock(host, actor, 2, 1);
  CHECK(actor.locked_bones == std::array<std::uint8_t, 2>{kLockedBone, 1});
  CHECK(host.calls ==
        Calls{"anchor 3 0", "lock slot 3 1", "anchor 1 1", "lock slot 1 2"});
  host.calls.clear();
  actors::SetActorBonePoseLock(host, actor, kLockedBone, 0);
  CHECK(actor.locked_bones == std::array<std::uint8_t, 2>{0, 1});
  CHECK(host.calls == Calls{"lock slot 3 0"});
}

TEST_CASE("Bone pose lock clears unlock every bone of a modelled actor",
          "[actors][actors-animation-control]") {
  FakeAnimationHost host;
  AnimationActor& actor = host.Add(kActor);
  actor.has_model = true;
  actor.bone_count = kModelBones;
  actor.locked_bones = {kLockedBone, 1};
  actors::ClearActorBonePoseLocks(host, actor);
  CHECK(actor.locked_bones == std::array<std::uint8_t, 2>{});
  CHECK(host.calls == Calls{"lock slot 0 0", "lock slot 1 0", "lock slot 2 0",
                            "lock slot 3 0"});
  actor.has_model = false;
  actor.locked_bones = {1, 2};
  const ChannelWarnings warnings("actors");
  actors::ClearActorBonePoseLocks(host, actor);
  CHECK(warnings.messages ==
        Calls{Refused("Bone pose lock clear",
                      "actor 3 has 4 bones and no model")});
  CHECK(actor.locked_bones == std::array<std::uint8_t, 2>{1, 2});
}

TEST_CASE("Live animation bindings refuse ids outside the table and nulls",
          "[actors][actors-animation-control]") {
  const ChannelWarnings warnings("actors");
  CHECK(actors::SetLiveActorTravelAnimation(kActorSlots, kWalk) == -1);
  CHECK(actors::SetLiveActorTravelAnimation(-1, kWalk) == -1);
  CHECK(actors::LiveActorTravelAnimation(kActor, nullptr) == -1);
  actors::SetLiveActorBonePoseLock(nullptr, 0, 1);
  actors::ClearLiveActorBonePoseLocks(nullptr);
  actors::ApplyLiveActorSoundOverrides(kActor, nullptr);
  CHECK(actors::UpdateLiveActorLocomotionAnimation(nullptr, 0, 0) == 0);
  CHECK(warnings.messages ==
        Calls{"Actor animation refused actor id 17",
              "Actor animation refused actor id -1",
              Refused("Travel animation", "null output"),
              Refused("Bone pose lock", "null actor"),
              Refused("Bone pose lock clear", "null actor"),
              Refused("Sound override apply", "null override"),
              Refused("Locomotion animation", "null actor")});
}

TEST_CASE("Live animation bindings read and write the table's record",
          "[actors][actors-animation-control]") {
  const TableRecord record(kActor);
  CHECK(actors::SetLiveActorTravelAnimation(kActor, -1) == 0);
  CHECK((*record).travelAnimation == kWalk);
  int animation = 0;
  CHECK(actors::LiveActorTravelAnimation(kActor, &animation) == 0);
  CHECK(animation == kWalk);
  CHECK(actors::QueueLiveActorAnimation(kActor, kRun, kBlend, 0) == 0);
  CHECK((*record).queuedAnimation == kRun);

  const ChannelWarnings warnings("actors");
  actors::SetLiveActorBonePoseLock(record.Get(), 0, 1);
  CHECK(warnings.messages ==
        Calls{Refused("Bone pose lock",
                      "bone 0 is outside the 0 bones of actor 0's model")});
}
}  // namespace
