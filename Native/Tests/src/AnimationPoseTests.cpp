#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorMotion.hpp"
#include "Actors/AnimationEvents.hpp"
#include "Actors/AnimationPose.hpp"
#include "Actors/ModelRendering.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace actors = VS::Actors;
using actors::AnimationBank;
using actors::AnimationEntry;
using actors::AnimationEventMode;
using actors::AnimationEventStatus;
using actors::AnimationHeaderStatus;
using actors::ClipEventsStatus;
using actors::ClipLookupStatus;
using actors::EventRestart;
using actors::kRootPoseKey;
using actors::PoseStart;
using Bytes = std::vector<std::uint8_t>;
using Calls = std::vector<std::string>;
using Key = std::array<int, 4>;
using VS::Core::Math::Mat3;
using VS::Core::Math::Transform3;
using VS::Core::Math::Vec3;

namespace {
// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

template <typename T>
T Required(std::optional<T>&& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *std::move(value);
}

void Put16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
  bytes.at(offset) = static_cast<std::uint8_t>(value & 0xFFU);
  bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

void Put32(Bytes& bytes, std::size_t offset, std::size_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    bytes.at(offset + i) =
        static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFU);
  }
}

Key KeyOf(const actors::PoseVector& key) {
  return {key.x, key.y, key.z, key.word};
}

// Slot 1, model 5 with sequence model 3, special pitch 2 and shared slot 1.
constexpr actors::AnimationBankLimits TestLimits() {
  return {.bones = 2,
          .slot = 1,
          .model_id = 5,
          .sequence_model_id = 3,
          .common_entries = 50,
          .battle_entries = 40,
          .special_entries = 40,
          .special_pitch = 2,
          .shared_slot = 1};
}
constexpr actors::AnimationBankLimits kLimits = TestLimits();

std::optional<AnimationEntry> Select(
    int id, const actors::AnimationBankLimits& limits = kLimits) {
  return actors::SelectAnimationEntry(id, limits);
}

constexpr AnimationEntry Entry(AnimationBank bank, int index,
                               int shared_slot = 0) {
  return {.bank = bank, .shared_slot = shared_slot, .index = index};
}

// Clip headers for `bones` bones from offset 8, then the map.
Bytes MapBank(int bones, std::size_t clips, const Bytes& map) {
  const std::size_t map_at = actors::kSequenceBankHeaderBytes +
                             (actors::ClipHeaderBytes(bones) * clips);
  Bytes bank(map_at + map.size());
  Put32(bank, 4, map_at);
  std::copy(map.begin(), map.end(),
            bank.begin() + static_cast<std::ptrdiff_t>(map_at));
  return bank;
}

class FakeBanks final : public actors::AnimationBankSource {
 public:
  actors::AnimationBankView Bank(const AnimationEntry& entry) override {
    asked.push_back(entry);
    return view;
  }

  actors::AnimationBankView view;
  std::vector<AnimationEntry> asked;
};

// One clip for one bone: its header at 8 (14 bytes), the map at 22 and the
// motion data at 32.
constexpr std::size_t kClipHeader = 8;
constexpr std::size_t kClipMap = 22;
constexpr std::size_t kClipMotion = 32;
constexpr int kOneBone = 1;
constexpr std::size_t kFirstEvent = kClipMotion + 3;

Bytes OneBoneClip(std::uint8_t base = 0xFF, std::uint8_t flags = 1,
                  std::uint16_t events = 2) {
  const Bytes motion{0,    0,    7,    0,  // +2: the first event, frame 7
                     0x00, 0x10, 0x00, 0x20, 0x00, 0x30,  // +4: root rotation
                     0x00, 0x08, 0x00, 0x08, 0x00, 0x08,  // +10: root scale
                     0x01, 0x00, 0x02, 0x00, 0x03, 0x00,  // +16: bone rotation
                     0xFE, 0x01, 0x00, 0x20, 0x00, 0x40,
                     0x00, 0x60};  // +22: packed scale
  Bytes bank(kClipMotion);
  Put32(bank, 0, kClipMotion);
  Put32(bank, 4, kClipMap);
  Put16(bank, kClipHeader, 20);
  bank.at(kClipHeader + 2) = base;
  bank.at(kClipHeader + 3) = flags;
  Put16(bank, kClipHeader + 4, events);
  Put16(bank, kClipHeader + 6, 4);
  Put16(bank, kClipHeader + 8, 10);
  Put16(bank, kClipHeader + 10, 16);
  Put16(bank, kClipHeader + 12, 22);
  bank.insert(bank.end(), motion.begin(), motion.end());
  return bank;
}

actors::AnimationClipRef ClipOf(std::span<const std::uint8_t> bank) {
  return {.bank = bank, .header_offset = kClipHeader, .bones = kOneBone};
}

actors::AnimationPoseState PlayingState() {
  actors::AnimationPoseState state;
  state.frame = 5;
  state.event_wait = 3;
  state.holding_last_frame = true;
  state.animation_id = 4;
  state.next_event_frame = 9;
  state.event_offset = 99;
  state.pose.keys.scales[kRootPoseKey] = {.x = 1, .y = 2, .z = 3, .word = 4};
  state.pose.scaleDeltas[kRootPoseKey].word = 5;
  state.pose.keys.rotations[0] = {.x = 7, .y = 7, .z = 7, .word = 7};
  state.pose.rotationDeltas[0].word = 11;
  state.pose.scaleDeltas[0].word = 12;
  return state;
}

// Only the clock and the root scale reset.
void CheckOnlyClockReset(const actors::AnimationPoseState& state) {
  CHECK(state.frame == 0);
  CHECK(state.event_wait == 0);
  CHECK(KeyOf(state.pose.keys.scales[kRootPoseKey]) == Key{});
  CHECK(state.pose.scaleDeltas[kRootPoseKey].word == 0);
  CHECK(state.holding_last_frame);
  CHECK(state.animation_id == 4);
  CHECK(state.next_event_frame == 9);
  CHECK(state.event_offset == std::optional<std::size_t>{99});
  CHECK(KeyOf(state.pose.keys.rotations[0]) == Key{7, 7, 7, 7});
  CHECK(state.pose.rotationDeltas[0].word == 11);
}

class FakeBlendHost final : public actors::AnimationBlendHost {
 public:
  void AdvanceAnimation() override { calls.emplace_back("advance"); }
  [[nodiscard]] actors::BlendClock Clock() const override { return clock; }
  void SetClock(actors::BlendClock next) override { clock = next; }
  [[nodiscard]] int FramesPerStep() const override { return frames_per_step; }
  void AlignBlendRotations() override { calls.emplace_back("align"); }
  void BlendTowardPose(actors::BlendClock at) override {
    calls.push_back("blend " + std::to_string(at.frame) + "/" +
                    std::to_string(at.frames));
  }
  void CopyPoseToBlend() override { calls.emplace_back("copy"); }
  void UpdateMotion() override { calls.emplace_back("motion"); }

  actors::BlendClock clock;
  int frames_per_step = 1;
  Calls calls;
};

Calls BlendStep(actors::BlendClock clock, int frames_per_step,
                actors::BlendClock& after) {
  FakeBlendHost host;
  host.clock = clock;
  host.frames_per_step = frames_per_step;
  actors::UpdateActorAnimationBlend(host);
  after = host.clock;
  return host.calls;
}

class FakeMotionLook final : public actors::MotionLookHost {
 public:
  [[nodiscard]] actors::ActorMotion LoadMotion() const override {
    return motion;
  }
  void StoreMotion(const actors::ActorMotion& next) override {
    motion = next;
    calls.emplace_back("store motion");
  }
  [[nodiscard]] Vec3 RootKey() const override { return root_key; }
  [[nodiscard]] std::int32_t ActorScale() const override { return actor_scale; }
  void SetRootOffset(Vec3 offset) override {
    root_offset = offset;
    calls.emplace_back("root offset");
  }
  [[nodiscard]] bool PreviewPose() const override { return preview; }
  void CenterPreviewPose() override { calls.emplace_back("center"); }
  void FinishLocomotion() override { calls.emplace_back("finish locomotion"); }
  void SettleOnTile(int tile_x, int tile_z) override {
    calls.push_back("settle " + std::to_string(tile_x) + "," +
                    std::to_string(tile_z));
  }
  void TransformRootOffset() override { calls.emplace_back("transform root"); }
  void SaveGroundedTransform() override { calls.emplace_back("save grounded"); }
  [[nodiscard]] actors::HeadLook LoadHeadLook() const override { return look; }
  void StoreHeadLook(const actors::HeadLook& next) override {
    look = next;
    calls.emplace_back("store look");
  }
  [[nodiscard]] actors::HeadLookContext LookContext(
      const actors::HeadLook& /*look*/) override {
    return context;
  }
  void SetLookAngles(actors::LookAngles next) override {
    angles = next;
    calls.emplace_back("look angles");
  }

  actors::ActorMotion motion;
  Vec3 root_key{};
  std::int32_t actor_scale = 4096;
  Vec3 root_offset{};
  bool preview = false;
  actors::HeadLook look;
  actors::HeadLookContext context;
  actors::LookAngles angles;
  Calls calls;
};

// The opcode lengths these tests use (D_800E9298 has 68 entries).
struct EventTables {
  std::array<std::uint8_t, 68> lengths{};
  std::array<std::uint8_t, 12> weapon_sounds{};
  std::array<std::array<std::uint16_t, 4>, 8> points{};

  EventTables() {
    for (const auto [op, length] : std::to_array<std::pair<int, int>>(
             {{1, 1},  {2, 1},  {4, 2},  {13, 1}, {19, 2}, {20, 2}, {21, 2},
              {22, 3}, {23, 1}, {24, 1}, {36, 3}, {37, 3}, {38, 3}, {39, 5},
              {40, 1}, {41, 1}, {54, 4}, {55, 2}, {56, 2}, {57, 2}, {58, 1},
              {59, 1}, {60, 2}, {61, 1}, {62, 1}})) {
      lengths.at(static_cast<std::size_t>(op)) =
          static_cast<std::uint8_t>(length);
    }
    weapon_sounds[0] = 0x30;
    weapon_sounds[3] = 0x33;
    points[1] = {300, 200, 100, 0};
  }

  [[nodiscard]] actors::AnimationEventTables View() const {
    return {
        .lengths = lengths, .weapon_sounds = weapon_sounds, .points = points};
  }
};

struct ReportedProblem {
  AnimationEventStatus status = AnimationEventStatus::kDone;
  std::uint8_t op = 0;

  friend bool operator==(const ReportedProblem&,
                         const ReportedProblem&) = default;
};

class FakeEventHost final : public actors::AnimationEventHost {
 public:
  explicit FakeEventHost(Bytes script_bytes) : bytes(std::move(script_bytes)) {
    script = actors::EventScriptPosition{.bank = bytes, .offset = 0};
  }

  [[nodiscard]] std::optional<actors::EventScriptPosition> Script()
      const override {
    return script;
  }
  void StoreCursor(const actors::EventScriptPosition& position,
                   std::uint16_t frame) override {
    script = position;
    next_event_frame = frame;
  }
  [[nodiscard]] std::uint16_t NextEventFrame() const override {
    return next_event_frame;
  }
  void SetNextEventFrame(std::uint16_t frame) override {
    next_event_frame = frame;
  }
  [[nodiscard]] std::int16_t Frame() const override { return frame; }
  void SetFrame(std::int16_t next) override { frame = next; }
  [[nodiscard]] std::uint8_t EventWait() const override { return event_wait; }
  void SetEventWait(std::uint8_t wait) override { event_wait = wait; }
  [[nodiscard]] int AnimationId() const override { return animation_id; }
  [[nodiscard]] std::uint16_t AnimationLength() const override {
    return animation_length;
  }
  [[nodiscard]] bool EventFlag() const override { return event_flag; }
  void SetEventFlag(bool flag) override {
    event_flag = flag;
    calls.push_back(flag ? "flag 1" : "flag 0");
  }
  [[nodiscard]] bool AdvanceRequested() const override {
    return advance_requested;
  }
  void ClearAdvanceRequest() override {
    advance_requested = false;
    calls.emplace_back("clear advance");
  }
  [[nodiscard]] bool WeaponDrawn() const override { return weapon_drawn; }
  [[nodiscard]] std::uint8_t SequenceModelId() const override {
    return sequence_model;
  }
  [[nodiscard]] bool EventPlaybackActive() const override { return false; }
  [[nodiscard]] bool EffectsBusy() const override { return effects_busy; }
  [[nodiscard]] actors::ActorMotion LoadMotion() const override {
    return motion;
  }
  void StoreMotion(const actors::ActorMotion& next) override { motion = next; }
  [[nodiscard]] std::uint8_t CompanionSlot() const override {
    return companion_slot;
  }
  bool FinishCompanionAction(std::uint8_t slot) override {
    calls.push_back("finish companion " + std::to_string(slot));
    return companion_found;
  }
  void SetFastAnimation(bool fast) override {
    calls.push_back(fast ? "fast 1" : "fast 0");
  }
  bool QueueImpactEffects() override {
    calls.emplace_back("impact");
    return impact_queued;
  }
  void PlayAnimation(int animation, int blend_frames) override {
    calls.push_back("play " + std::to_string(animation) + "/" +
                    std::to_string(blend_frames));
    Reload();
  }
  void InitializePose() override {
    calls.emplace_back("init pose");
    Reload();
  }
  [[nodiscard]] std::optional<VS::Audio::SfxMix> SoundMix() override {
    return mix;
  }
  void PlaySfx(std::int32_t group, int effect,
               const VS::Audio::SfxMix& played) override {
    calls.push_back("sfx " + std::to_string(group) + "/" +
                    std::to_string(effect) + " " + std::to_string(played.pan) +
                    "/" + std::to_string(played.volume));
  }
  void StopSfx(std::int32_t group, int effect) override {
    calls.push_back("stop sfx " + std::to_string(group) + "/" +
                    std::to_string(effect));
  }
  void PlaySoundPair(int sound) override {
    calls.push_back("pair " + std::to_string(sound));
  }
  void PlayModelSound(int sound) override {
    calls.push_back("model sound " + std::to_string(sound));
  }
  void PlaySurfaceContactSound(int contact) override {
    calls.push_back("contact " + std::to_string(contact));
  }
  void FadeAttachedWeapons(const actors::WeaponFade& /*fade*/,
                           bool show) override {
    calls.push_back(show ? "fade weapons in" : "fade weapons out");
  }
  void SetTextureCellSheet(int channel,
                           const actors::TextureCellSheet& /*sheet*/) override {
    calls.push_back("cell sheet " + std::to_string(channel));
  }
  void SetTextureAnimation(int channel, int timeline,
                           actors::TextureAnimationCommand command) override {
    calls.push_back("timeline " + std::to_string(channel) + "/" +
                    std::to_string(timeline) + "/" +
                    std::to_string(static_cast<int>(command)));
  }
  void UploadTextureOverlay(
      const actors::TextureOverlayCopy& /*copy*/) override {
    calls.emplace_back("overlay");
  }
  void StartPaletteFade() override { calls.emplace_back("palette fade"); }
  bool MoveModelVertex(std::size_t vertex,
                       const VS::Render::ModelVertex& delta) override {
    calls.push_back("move " + std::to_string(vertex) + " " +
                    std::to_string(delta.x) + "," + std::to_string(delta.y) +
                    "," + std::to_string(delta.z));
    return vertex_moves;
  }
  void MarkPoseOverridden() override { calls.emplace_back("overridden"); }
  [[nodiscard]] actors::ShadowBlend LoadShadow() const override {
    return shadow;
  }
  void StoreShadow(const actors::ShadowBlend& next) override { shadow = next; }
  void LockBonePose(int bone, bool locked) override {
    calls.push_back("lock " + std::to_string(bone) + (locked ? " 1" : " 0"));
  }
  void StartWeaponTrail(int trail) override {
    calls.push_back("trail " + std::to_string(trail));
  }
  void FadeWeaponTrails(int frames) override {
    calls.push_back("fade trails " + std::to_string(frames));
  }
  void StartEffectTrack(int track) override {
    calls.push_back("track " + std::to_string(track));
  }
  void SpawnSourceEffect() override { calls.emplace_back("source effect"); }
  void ApplyGimmickInteraction(int command) override {
    calls.push_back("gimmick " + std::to_string(command));
  }
  void ReportEventProblem(AnimationEventStatus status,
                          std::uint8_t op) override {
    problems.push_back({.status = status, .op = op});
  }

  [[nodiscard]] std::size_t Offset() const {
    return script ? script->offset : 0;
  }

  Bytes bytes;
  std::optional<actors::EventScriptPosition> script;
  // The clip an event's animation reloads the script from, and its first
  // event's frame.
  std::optional<actors::EventScriptPosition> reload;
  std::uint16_t reload_frame = actors::kNoEventFrame;
  std::uint16_t next_event_frame = 0;
  std::int16_t frame = 0;
  std::uint8_t event_wait = 0;
  int animation_id = 5;
  std::uint16_t animation_length = 0;
  bool event_flag = false;
  bool advance_requested = false;
  bool weapon_drawn = false;
  std::uint8_t sequence_model = 0;
  bool effects_busy = false;
  actors::ActorMotion motion;
  std::uint8_t companion_slot = 0;
  bool companion_found = true;
  bool impact_queued = false;
  std::optional<VS::Audio::SfxMix> mix = VS::Audio::SfxMix{};
  bool vertex_moves = true;
  actors::ShadowBlend shadow;
  Calls calls;
  std::vector<ReportedProblem> problems;

 private:
  void Reload() {
    script = reload;
    next_event_frame = reload_frame;
  }
};

actors::AnimationEventRun RunEvents(
    FakeEventHost& host, AnimationEventMode mode = AnimationEventMode::kPlay) {
  const EventTables tables;
  return actors::RunAnimationEvents(host, tables.View(), mode);
}

Calls PlayedEvents(const Bytes& script,
                   AnimationEventMode mode = AnimationEventMode::kPlay) {
  FakeEventHost host(script);
  const auto run = RunEvents(host, mode);
  CHECK(run.status == AnimationEventStatus::kDone);
  CHECK(host.problems.empty());
  return host.calls;
}

// A bone-less clip whose header at 8 locates its events at `events` in the
// motion data at 20.
constexpr std::size_t kEventClipMotion = 20;

Bytes EventClip(std::uint16_t events, const Bytes& motion) {
  Bytes bank(kEventClipMotion);
  Put32(bank, 0, kEventClipMotion);
  Put32(bank, 4, 18);
  Put16(bank, kClipHeader + 4, events);
  bank.insert(bank.end(), motion.begin(), motion.end());
  return bank;
}
}  // namespace

TEST_CASE("Animation ids select their owned, common or battle entry",
          "[actors][actors-animation-pose]") {
  CHECK_FALSE(Select(-1));
  CHECK(Select(0) == Entry(AnimationBank::kOwned, 0));
  CHECK(Select(49) == Entry(AnimationBank::kCommon, 49));
  CHECK_FALSE(Select(50));
  CHECK(Select(100) == Entry(AnimationBank::kBattle, 0));
  CHECK(Select(139) == Entry(AnimationBank::kBattle, 39));
  CHECK_FALSE(Select(140));
}

TEST_CASE("Animation ids select their work or shared entry",
          "[actors][actors-animation-pose]") {
  CHECK(Select(0xCA) == Entry(AnimationBank::kWork, 0));
  CHECK(Select(0xDB) == Entry(AnimationBank::kWork, 17));
  CHECK(Select(0xDC) == Entry(AnimationBank::kShared, 0, 1));
  CHECK(Select(0xFD) == Entry(AnimationBank::kShared, 33, 1));
}

TEST_CASE("Special action ids select shared slot 3 by sequence model and pitch",
          "[actors][actors-animation-pose]") {
  // (sequence model 3 - 1) * 3 + pitch 2.
  CHECK(Select(0xFF) == Entry(AnimationBank::kShared, 8, 3));
  auto leader = kLimits;
  leader.slot = 0;
  CHECK(Select(0xFF, leader) == Entry(AnimationBank::kShared, 2, 3));
  auto pitch_model = kLimits;
  pitch_model.model_id = 2;
  CHECK(Select(0xFF, pitch_model) == Entry(AnimationBank::kShared, 2, 3));
  // (sequence model 3 - 1) * 3 + 31.
  CHECK(Select(0xFE) == Entry(AnimationBank::kShared, 37, 3));
}

TEST_CASE("Special action ids need a sequence model and a long enough bank",
          "[actors][actors-animation-pose]") {
  auto no_model = kLimits;
  no_model.sequence_model_id = 0;
  CHECK_FALSE(Select(0xFF, no_model));
  CHECK_FALSE(Select(0xFE, no_model));
  auto short_bank = kLimits;
  short_bank.special_entries = 37;
  CHECK_FALSE(Select(0xFE, short_bank));
  CHECK(Select(0xFF, short_bank));
  short_bank.special_entries = 8;
  CHECK_FALSE(Select(0xFF, short_bank));
}

TEST_CASE("Clip lookups read the map entry's clip and header",
          "[actors][actors-animation-pose]") {
  CHECK(actors::ClipHeaderBytes(0) == 10);
  CHECK(actors::ClipHeaderBytes(2) == 18);
  CHECK(actors::ClipHeaderBytes(-3) == 10);

  const Bytes bank = MapBank(2, 2, {1, 0xFF, 5});
  const auto found = actors::LookupAnimationClip(bank, 0, 2);
  CHECK(found.status == ClipLookupStatus::kFound);
  CHECK(found.clip == 1);
  CHECK(found.header_offset == 26);

  const auto none = actors::LookupAnimationClip(bank, 1, 2);
  CHECK(none.status == ClipLookupStatus::kNoClip);
  CHECK(none.clip == actors::kNoClip);
}

TEST_CASE("Clip lookups bound the map and the header by the bank",
          "[actors][actors-animation-pose]") {
  const Bytes bank = MapBank(2, 2, {1, 0xFF, 5});
  const auto past = actors::LookupAnimationClip(bank, 2, 2);
  CHECK(past.status == ClipLookupStatus::kHeaderOutsideBank);
  CHECK(past.clip == 5);
  // Clip 1's header fits for two bones but not for four.
  CHECK(actors::LookupAnimationClip(bank, 0, 4).status ==
        ClipLookupStatus::kHeaderOutsideBank);

  CHECK(actors::LookupAnimationClip(bank, 3, 2).status ==
        ClipLookupStatus::kMapOutsideBank);
  CHECK(actors::LookupAnimationClip(bank, -1, 2).status ==
        ClipLookupStatus::kMapOutsideBank);
  CHECK(actors::LookupAnimationClip(std::span(bank).first(6), 0, 2).status ==
        ClipLookupStatus::kMapOutsideBank);
  Bytes far = bank;
  Put32(far, 4, 0x1000);
  CHECK(actors::LookupAnimationClip(far, 0, 2).status ==
        ClipLookupStatus::kMapOutsideBank);
}

TEST_CASE("Animation headers need an entry and a registered, loaded bank",
          "[actors][actors-animation-pose]") {
  const Bytes bank = MapBank(2, 2, {1, 0xFF, 5});
  FakeBanks banks;
  const auto missing = actors::FindActorAnimationHeader(kLimits, banks, 50);
  CHECK(missing.status == AnimationHeaderStatus::kNoEntry);
  CHECK(banks.asked.empty());

  banks.view = {.loaded = false, .bytes = bank};
  const auto unloaded = actors::FindActorAnimationHeader(kLimits, banks, 1);
  CHECK(unloaded.status == AnimationHeaderStatus::kNoBank);
  CHECK(unloaded.entry == Entry(AnimationBank::kCommon, 1));
  CHECK(banks.asked == std::vector{Entry(AnimationBank::kCommon, 1)});

  banks.view = {.loaded = true, .bytes = {}};
  CHECK(actors::FindActorAnimationHeader(kLimits, banks, 1).status ==
        AnimationHeaderStatus::kUnregisteredBank);
}

TEST_CASE("Animation headers come from the selected bank's registered extent",
          "[actors][actors-animation-pose]") {
  const Bytes bank = MapBank(2, 2, {1, 0xFF, 5});
  FakeBanks banks;
  banks.view = {.loaded = true, .bytes = bank};
  const auto found = actors::FindActorAnimationHeader(kLimits, banks, 100);
  CHECK(found.status == AnimationHeaderStatus::kFound);
  CHECK(found.entry == Entry(AnimationBank::kBattle, 0));
  CHECK(found.bank.data() == bank.data());
  CHECK(found.bank.size() == bank.size());
  CHECK(found.clip == 1);
  CHECK(found.header_offset == 26);
}

TEST_CASE("Animation headers report a missing clip or a short bank",
          "[actors][actors-animation-pose]") {
  const Bytes bank = MapBank(2, 2, {1, 0xFF, 5});
  FakeBanks banks;
  banks.view = {.loaded = true, .bytes = bank};
  const auto none = actors::FindActorAnimationHeader(kLimits, banks, 101);
  CHECK(none.status == AnimationHeaderStatus::kNoClip);
  CHECK(none.clip == actors::kNoClip);
  CHECK(actors::FindActorAnimationHeader(kLimits, banks, 102).status ==
        AnimationHeaderStatus::kHeaderOutsideBank);
  CHECK(actors::FindActorAnimationHeader(kLimits, banks, 103).status ==
        AnimationHeaderStatus::kMapOutsideBank);
}

TEST_CASE("Event cursors read a frame byte or a long frame",
          "[actors][actors-animation-pose]") {
  const Bytes bytes{5, 0xFF, 3, 0xFF};
  const auto first = Required(actors::ReadEventCursor(bytes, 0));
  CHECK(first.frame == 5);
  CHECK(first.offset == 1);
  const auto long_frame = Required(actors::ReadEventCursor(bytes, 1));
  CHECK(long_frame.frame == 0x102);
  CHECK(long_frame.offset == 3);
  CHECK_FALSE(actors::ReadEventCursor(bytes, 3));
  CHECK_FALSE(actors::ReadEventCursor(bytes, 4));
  CHECK_FALSE(actors::ReadEventCursor(bytes, 99));
}

TEST_CASE("Initial pose vectors are big-endian and doubled",
          "[actors][actors-animation-pose]") {
  const Bytes bytes{0x01, 0x02, 0xC0, 0x00, 0x7F, 0xFF};
  const auto own =
      Required(actors::DecodeInitialPoseVector(bytes, 0, actors::kNoClip));
  CHECK(KeyOf(own) == Key{0x204, -0x8000, -2, 6});
  const auto based = Required(actors::DecodeInitialPoseVector(bytes, 0, 3));
  CHECK(based.word == 0);
  CHECK_FALSE(actors::DecodeInitialPoseVector(bytes, 1, actors::kNoClip));
}

TEST_CASE("Clip events locate the first cursor in the motion data",
          "[actors][actors-animation-pose]") {
  Bytes bank = OneBoneClip();
  const auto events = actors::FindClipEvents(ClipOf(bank));
  CHECK(events.status == ClipEventsStatus::kStarts);
  CHECK(events.cursor.frame == 7);
  CHECK(events.cursor.offset == kFirstEvent);

  bank.at(kClipMotion + 2) = 0xFF;
  bank.at(kClipMotion + 3) = 1;
  const auto long_frame = actors::FindClipEvents(ClipOf(bank));
  CHECK(long_frame.cursor.frame == 0x100);
  CHECK(long_frame.cursor.offset == kFirstEvent + 1);

  Put16(bank, kClipHeader + 4, 0);
  CHECK(actors::FindClipEvents(ClipOf(bank)).status == ClipEventsStatus::kNone);
  Put16(bank, kClipHeader + 4, 200);
  CHECK(actors::FindClipEvents(ClipOf(bank)).status ==
        ClipEventsStatus::kOutOfBank);
  CHECK(actors::FindClipEvents(
            {.bank = bank, .header_offset = bank.size() - 4, .bones = kOneBone})
            .status == ClipEventsStatus::kOutOfBank);
}

TEST_CASE("A clip start resets the clock and finds the first event",
          "[actors][actors-animation-pose]") {
  const Bytes bank = OneBoneClip();
  auto state = PlayingState();
  CHECK(actors::InitializeActorAnimationPose(ClipOf(bank), 9, state) ==
        PoseStart::kInitialized);
  CHECK(state.frame == 0);
  CHECK(state.event_wait == 0);
  CHECK_FALSE(state.holding_last_frame);
  CHECK(state.animation_id == 9);
  CHECK(state.next_event_frame == 7);
  CHECK(state.event_offset == std::optional<std::size_t>{kFirstEvent});
}

TEST_CASE("A clip start decodes the clip's first keys",
          "[actors][actors-animation-pose]") {
  const Bytes bank = OneBoneClip();
  auto state = PlayingState();
  REQUIRE(actors::InitializeActorAnimationPose(ClipOf(bank), 9, state) ==
          PoseStart::kInitialized);
  const auto& keys = state.pose.keys;
  CHECK(KeyOf(keys.rotations[kRootPoseKey]) == Key{0x20, 0x40, 0x60, 6});
  CHECK(KeyOf(keys.scales[kRootPoseKey]) == Key{0x10, 0x10, 0x10, 6});
  CHECK(KeyOf(keys.rotations[0]) == Key{0x200, 0x400, 0x600, 6});
  // FE 01 marks a packed scale: the decoded vector halved.
  CHECK(KeyOf(keys.scales[0]) == Key{0x20, 0x40, 0x60, 8});
  CHECK(state.pose.rotationDeltas[0].word == 0);
  CHECK(state.pose.scaleDeltas[0].word == 0);
}

TEST_CASE("A clip based on another starts its track cursors at zero",
          "[actors][actors-animation-pose]") {
  // Base clip 0 is this clip's own header; flags 0 has no scale tracks.
  const Bytes bank = OneBoneClip(0, 0);
  auto state = PlayingState();
  REQUIRE(actors::InitializeActorAnimationPose(ClipOf(bank), 9, state) ==
          PoseStart::kInitialized);
  const auto& keys = state.pose.keys;
  CHECK(KeyOf(keys.rotations[kRootPoseKey]) == Key{0x20, 0x40, 0x60, 0});
  CHECK(KeyOf(keys.rotations[0]) == Key{0x200, 0x400, 0x600, 0});
  // The root scale is always the clip's own.
  CHECK(KeyOf(keys.scales[kRootPoseKey]) == Key{0x10, 0x10, 0x10, 6});
  CHECK(KeyOf(keys.scales[0]) == Key{0x40, 0x40, 0x40, 0});
}

TEST_CASE("A clip without events leaves the script where it was",
          "[actors][actors-animation-pose]") {
  const Bytes bank = OneBoneClip(0xFF, 1, 0);
  auto state = PlayingState();
  REQUIRE(actors::InitializeActorAnimationPose(ClipOf(bank), 9, state) ==
          PoseStart::kInitialized);
  CHECK(state.next_event_frame == actors::kNoEventFrame);
  CHECK(state.event_offset == std::optional<std::size_t>{99});
}

TEST_CASE("A clip start that leaves its bank commits no keys",
          "[actors][actors-animation-pose]") {
  const Bytes bank = OneBoneClip();
  auto cut = PlayingState();
  CHECK(actors::InitializeActorAnimationPose(
            ClipOf(std::span(bank).first(bank.size() - 1)), 9, cut) ==
        PoseStart::kOutOfBank);
  CheckOnlyClockReset(cut);

  auto missing = PlayingState();
  CHECK(actors::InitializeActorAnimationPose(std::nullopt, 9, missing) ==
        PoseStart::kNoClip);
  CheckOnlyClockReset(missing);
}

TEST_CASE("Halfway blends take the shorter way round each rotation",
          "[actors][actors-animation-pose]") {
  actors::PoseKeys destination;
  actors::PoseKeys source;
  destination.rotations[kRootPoseKey] = {.x = 10, .y = 20, .z = -30};
  source.rotations[kRootPoseKey] = {.x = 20, .y = 40, .z = -31};
  destination.rotations[0] = {.x = 4000, .y = -4000, .z = 100};
  source.rotations[0] = {.x = -4000, .y = 4000, .z = 300};
  destination.scales[0] = {.x = 64, .y = 64, .z = 64};
  source.scales[0] = {.x = 128, .y = 0, .z = 65};
  destination.rotations[1] = {.x = 7, .y = 7, .z = 7};
  source.rotations[1] = {.x = 100, .y = 100, .z = 100};
  destination.scales[kRootPoseKey] = {.x = 9, .y = 9, .z = 9};

  actors::BlendActorPoseHalfway(destination, source, 1);
  CHECK(KeyOf(destination.rotations[kRootPoseKey]) == Key{15, 30, -31, 0});
  // 8000 apart one way is 192 the other: halfway lies 96 past each end.
  CHECK(KeyOf(destination.rotations[0]) == Key{4096, -4096, 200, 0});
  CHECK(KeyOf(destination.scales[0]) == Key{96, 32, 64, 0});
  // Only `bones` bones blend, and the root scale does not.
  CHECK(KeyOf(destination.rotations[1]) == Key{7, 7, 7, 0});
  CHECK(KeyOf(destination.scales[kRootPoseKey]) == Key{9, 9, 9, 0});
}

TEST_CASE("Branch alignment flips rotations a half turn or more away",
          "[actors][actors-animation-pose]") {
  actors::PoseKeyArray destination{};
  actors::PoseKeyArray source{};
  destination[0] = {.x = 3000, .y = 100, .z = 3000};
  source[0] = {.x = -1000, .y = 0, .z = -1000};
  destination[1] = {.x = 100, .y = 50, .z = 100};
  source[1] = {.x = 200, .y = 0, .z = 200};
  // x is 3192 away the short way round and z 1000.
  destination[2] = {.x = -5000, .y = 10, .z = 1000};
  source[2] = {.x = 0, .y = 0, .z = 0};

  actors::AlignPoseRotationBranches(destination, source, 2);
  CHECK(KeyOf(destination[0]) == Key{7096, 3996, 7096, 0});
  CHECK(KeyOf(destination[1]) == Key{100, 50, 100, 0});
  // Bone 2 lies past `bones`.
  CHECK(KeyOf(destination[2]) == Key{-5000, 10, 1000, 0});

  actors::AlignPoseRotationBranches(destination, source, 3);
  // C's truncating remainder keeps the negative sum's sign.
  CHECK(KeyOf(destination[2]) == Key{-904, 4086, 5096, 0});
  CHECK(KeyOf(destination[0]) == Key{7096, 3996, 7096, 0});
}

TEST_CASE("The blend clock blends towards the playing pose until it ends",
          "[actors][actors-animation-pose]") {
  actors::BlendClock after;
  CHECK(BlendStep({}, 1, after) == Calls{"advance", "copy", "motion"});
  CHECK(after == actors::BlendClock{});

  CHECK(BlendStep({.frame = 1, .frames = 4}, 1, after) ==
        Calls{"advance", "align", "blend 1/4", "motion"});
  CHECK(after == actors::BlendClock{.frame = 2, .frames = 4});

  CHECK(BlendStep({.frame = 3, .frames = 4}, 1, after) ==
        Calls{"advance", "blend 3/4", "motion"});
  CHECK(after == actors::BlendClock{});
}

TEST_CASE("The blend clock moves on a frame first at two frames a step",
          "[actors][actors-animation-pose]") {
  actors::BlendClock after;
  CHECK(BlendStep({.frame = 2, .frames = 8}, 2, after) ==
        Calls{"advance", "blend 3/8", "motion"});
  CHECK(after == actors::BlendClock{.frame = 4, .frames = 8});

  CHECK(BlendStep({.frame = 7, .frames = 8}, 2, after) ==
        Calls{"advance", "copy", "motion"});
  CHECK(after == actors::BlendClock{});
}

namespace {
// Scale, turn and move tweens all one step from finishing a walk.
actors::ActorMotion FinishingTweens() {
  actors::ActorMotion motion;
  motion.scale_frames = 2;
  motion.scale_delta = {.x = 10, .y = -10, .z = 5};
  motion.scale = {.x = 100, .y = 100, .z = 100};
  motion.turn_frames = 1;
  motion.rotation_step_x = 100;
  motion.rotation_x = 4090;
  motion.facing_delta = -200;
  motion.facing = 100;
  motion.move_frames = 1;
  motion.root_motion = {.x = 256, .y = 0, .z = -128};
  motion.position = {.x = 0, .y = 0, .z = 300};
  motion.locomotion_state = 1;
  motion.movement_pending = true;
  return motion;
}
}  // namespace

TEST_CASE("Motion tweens step scale, turn and translation each frame",
          "[actors][actors-animation-pose]") {
  actors::ActorMotion motion = FinishingTweens();
  static_cast<void>(actors::StepActorMotionTweens(motion));
  CHECK(motion.scale == actors::MotionVector{.x = 105, .y = 95, .z = 102});
  CHECK(motion.scale_delta == actors::MotionVector{.x = 5, .y = -5, .z = 3});
  CHECK(motion.scale_frames == 1);
  CHECK(motion.rotation_x == 94);
  CHECK(motion.facing == 3996);
  CHECK(motion.turn_frames == 0);
  CHECK(motion.position == actors::MotionVector{.x = 256, .y = 0, .z = 172});
  CHECK(motion.move_frames == 0);
}

TEST_CASE("The last move frame finishes the walk and settles the tile",
          "[actors][actors-animation-pose]") {
  actors::ActorMotion motion = FinishingTweens();
  const auto step = actors::StepActorMotionTweens(motion);
  CHECK(step.locomotion_finished);
  CHECK(motion.locomotion_state == 0);
  CHECK_FALSE(motion.movement_pending);
  CHECK(step.tile_settled);
  CHECK(motion.current_tile.x == 2);
  CHECK(motion.current_tile.z == 1);
}

TEST_CASE("Held or unfinished moves neither finish the walk nor settle",
          "[actors][actors-animation-pose]") {
  actors::ActorMotion held;
  held.move_frames = 1;
  held.root_motion = {.x = 256};
  held.locomotion_state = 2;
  const auto held_step = actors::StepActorMotionTweens(held);
  CHECK_FALSE(held_step.locomotion_finished);
  CHECK_FALSE(held_step.tile_settled);
  CHECK(held.locomotion_state == 2);

  actors::ActorMotion moving;
  moving.move_frames = 2;
  moving.root_motion = {.x = 256};
  moving.locomotion_state = 1;
  const auto moving_step = actors::StepActorMotionTweens(moving);
  CHECK_FALSE(moving_step.locomotion_finished);
  CHECK_FALSE(moving_step.tile_settled);
  CHECK(moving.position.x == 128);
}

TEST_CASE("Head look turns towards its target within the turn limit",
          "[actors][actors-animation-pose]") {
  actors::HeadLookContext playing{.event_playback_active = true};

  actors::HeadLook rest;
  CHECK(actors::UpdateHeadLook(rest, playing) == actors::LookAngles{});

  actors::HeadLook returning{.target = actors::kNoHeadTrack,
                             .tracking = {.w = 100},
                             .yaw = 300,
                             .pitch = -50};
  CHECK(actors::UpdateHeadLook(returning, playing) ==
        actors::LookAngles{.yaw = 200, .pitch = 0});
  CHECK(returning.yaw == 200);

  actors::HeadLook absolute{.target = actors::kLookAbsolute,
                            .tracking = {.x = 300, .y = 900, .w = 1000}};
  CHECK(actors::UpdateHeadLook(absolute, playing) ==
        actors::LookAngles{.yaw = 800, .pitch = 300});

  actors::HeadLook relative{.target = actors::kLookRelative,
                            .tracking = {.x = 0, .y = 500, .w = 1000}};
  auto facing = playing;
  facing.facing = 100;
  facing.rotation_x = 50;
  CHECK(actors::UpdateHeadLook(relative, facing) ==
        actors::LookAngles{.yaw = 400, .pitch = -50});

  // Target straight down +z from an actor facing 2348: 300 to its right.
  actors::HeadLook point{.target = actors::kLookAtPoint,
                         .tracking = {.x = 0, .y = 0, .z = 100, .w = 1000}};
  auto turned = playing;
  turned.facing = 2348;
  CHECK(actors::UpdateHeadLook(point, turned) ==
        actors::LookAngles{.yaw = -300, .pitch = 0});

  // A slot target without a record turns back to rest.
  actors::HeadLook lost{.target = 3, .tracking = {.w = 1000}, .yaw = 40};
  CHECK(actors::UpdateHeadLook(lost, playing) == actors::LookAngles{});
}

TEST_CASE("Head look outside event playback follows the animation",
          "[actors][actors-animation-pose]") {
  const actors::HeadLook looking{.target = actors::kLookAbsolute,
                                 .tracking = {.x = 0, .y = 400, .w = 1000},
                                 .yaw = 100};
  auto drawn_look = looking;
  CHECK_FALSE(actors::UpdateHeadLook(drawn_look, {.was_drawn = true}));
  CHECK(drawn_look == looking);

  // Animation 2 does not track: the head turns back.
  auto still = looking;
  CHECK(actors::UpdateHeadLook(still, {.animation_id = 2}) ==
        actors::LookAngles{});
  // Drawn-weapon animation 106 is free-tracking animation 6.
  auto tracking = looking;
  CHECK(actors::UpdateHeadLook(tracking, {.animation_id = 106}) ==
        actors::LookAngles{.yaw = 400, .pitch = 0});
  auto jumping = looking;
  constexpr actors::LookAngles kUnchanged{.yaw = -1};
  CHECK(
      actors::UpdateHeadLook(jumping, {.animation_id = 31, .is_jumping = true})
          .value_or(kUnchanged)
          .yaw == 400);
  auto landing = looking;
  CHECK(actors::UpdateHeadLook(landing, {.animation_id = 6, .is_jumping = true})
            .value_or(kUnchanged)
            .yaw == 0);
  auto locked = looking;
  CHECK(actors::UpdateHeadLook(locked,
                               {.animation_id = 34, .motion_locked = true})
            .value_or(kUnchanged)
            .yaw == 400);
}

TEST_CASE("Motion and look-at places the root, steps tweens and turns the head",
          "[actors][actors-animation-pose]") {
  FakeMotionLook host;
  host.root_key = {4096, 4096, 4096};
  host.motion.scale = {.x = 8192, .y = 4096, .z = 2048};
  host.motion.move_frames = 1;
  host.motion.root_motion = {.x = 128};
  host.motion.locomotion_state = 1;
  host.preview = true;
  actors::UpdateMotionAndLookAt(host, false);
  CHECK(host.calls == Calls{"root offset", "store motion", "center",
                            "finish locomotion", "settle 1,0", "transform root",
                            "save grounded", "store look", "look angles"});
  // The instance scales are passed as (y, z, x).
  const Vec3 expected =
      actors::ActorRootOffset(host.root_key, {4096, 2048, 8192}, 4096);
  CHECK(host.root_offset == expected);
  CHECK(expected !=
        actors::ActorRootOffset(host.root_key, {8192, 4096, 2048}, 4096));

  FakeMotionLook skipped;
  skipped.motion.movement_mode = 2;
  skipped.motion.move_frames = 1;
  actors::UpdateMotionAndLookAt(skipped, true);
  CHECK(skipped.calls ==
        Calls{"root offset", "transform root", "store look", "look angles"});
  CHECK(skipped.motion.move_frames == 1);

  FakeMotionLook drawn;
  drawn.motion.motion_locked = true;
  drawn.look = {.target = actors::kLookAbsolute};
  drawn.context = {.was_drawn = true};
  actors::UpdateMotionAndLookAt(drawn, true);
  CHECK(drawn.calls == Calls{"root offset", "transform root"});
}

TEST_CASE("A locked bone takes its saved transform under the parent",
          "[actors][actors-animation-pose]") {
  // A quarter turn about z for the parent and about x for the locked bone;
  // the two do not commute.
  const Mat3 about_z{{{{0, -4096, 0}, {4096, 0, 0}, {0, 0, 4096}}}};
  const Mat3 about_x{{{{4096, 0, 0}, {0, 0, -4096}, {0, 4096, 0}}}};
  const Transform3 parent{.rotation = about_z, .translation = {10, 20, 30}};
  const Transform3 locked{.rotation = about_x, .translation = {100, 0, 0}};
  const Transform3 world = actors::ApplyLockedBoneTransform(parent, locked);
  CHECK(world.rotation == Mat3{{{{0, 0, 4096}, {4096, 0, 0}, {0, 4096, 0}}}});
  CHECK(world.translation == Vec3{10, 120, 30});
}

TEST_CASE("Event runs play the due events and stop at a later frame",
          "[actors][actors-animation-events]") {
  FakeEventHost host({62, 0, 61, 5, 62, 0, 0});
  host.frame = 2;
  CHECK(RunEvents(host).status == AnimationEventStatus::kDone);
  CHECK(host.calls == Calls{"fast 1", "fast 0"});
  CHECK(host.next_event_frame == 5);
  CHECK(host.Offset() == 4);

  host.frame = 5;
  CHECK(RunEvents(host).status == AnimationEventStatus::kDone);
  CHECK(host.calls == Calls{"fast 1", "fast 0", "fast 1"});
  CHECK(host.next_event_frame == actors::kNoEventFrame);
  CHECK(host.problems.empty());
}

TEST_CASE("Terminating opcodes end the script in every mode",
          "[actors][actors-animation-events]") {
  for (const auto mode :
       {AnimationEventMode::kPlay, AnimationEventMode::kPresentation,
        AnimationEventMode::kUnwind, AnimationEventMode::kFindImpact}) {
    for (const std::uint8_t op : std::to_array<std::uint8_t>(
             {0, 3, 16, 18, 42, 45, 50, 65, 67, 0xFF})) {
      FakeEventHost host({61, 0, op, 61, 0});
      host.animation_length = 10;
      const auto run = RunEvents(host, mode);
      CHECK(run.status == AnimationEventStatus::kDone);
      CHECK(host.next_event_frame == actors::kNoEventFrame);
      CHECK(host.Offset() == 2);
      CHECK(host.problems.empty());
    }
  }
}

TEST_CASE("Event modes choose the events they dispatch",
          "[actors][actors-animation-events]") {
  const Bytes script{62, 0, 57, 4, 0, 36, 0x06, 0x05, 0, 0};
  CHECK(PlayedEvents(script) ==
        Calls{"fast 1", "lock 4 1", "move 5 3,0,0", "overridden"});
  CHECK(PlayedEvents(script, AnimationEventMode::kPresentation) ==
        Calls{"lock 4 1", "move 5 3,0,0", "overridden"});
  CHECK(PlayedEvents(script, AnimationEventMode::kUnwind) ==
        Calls{"move 5 -3,0,0", "overridden"});
  CHECK(PlayedEvents(script, AnimationEventMode::kFindImpact).empty());
}

TEST_CASE("Finding the impact returns its frame without playing it",
          "[actors][actors-animation-events]") {
  FakeEventHost host({61, 3, 2, 9, 62, 0, 0});
  host.animation_length = 20;
  const auto run = RunEvents(host, AnimationEventMode::kFindImpact);
  CHECK(run.status == AnimationEventStatus::kDone);
  CHECK(run.value == 3);
  CHECK(host.calls.empty());

  // An impact past the clip's length is not found.
  FakeEventHost short_clip({61, 3, 2, 9, 62, 0, 0});
  short_clip.animation_length = 2;
  CHECK(RunEvents(short_clip, AnimationEventMode::kFindImpact).value == 0);
}

TEST_CASE("Vertex events move by the packed delta and unwind in reverse",
          "[actors][actors-animation-events]") {
  // 0xFC07: vertex 7 and a delta of -2 in the top seven bits.
  const Bytes whole{39, 0xFC, 0x07, 4, 0xFF, 0, 0};
  CHECK(PlayedEvents(whole) == Calls{"move 7 -2,4,-1", "overridden"});
  CHECK(PlayedEvents(whole, AnimationEventMode::kUnwind) ==
        Calls{"move 7 2,-4,1", "overridden"});
  const Bytes components{37, 0x06, 0x05, 0, 38, 0x06, 0x05, 0, 0};
  CHECK(PlayedEvents(components) ==
        Calls{"move 5 0,3,0", "overridden", "move 5 0,0,3", "overridden"});
}

TEST_CASE("A vertex past the model skips its event and the run goes on",
          "[actors][actors-animation-events]") {
  FakeEventHost host({36, 0x06, 0x05, 0, 62, 0, 0});
  host.vertex_moves = false;
  const auto run = RunEvents(host);
  CHECK(run.status == AnimationEventStatus::kVertexOutOfRange);
  CHECK_FALSE(actors::StopsAnimationEvents(run.status));
  CHECK(host.calls == Calls{"move 5 3,0,0", "fast 1"});
  CHECK(host.problems ==
        std::vector<ReportedProblem>{
            {.status = AnimationEventStatus::kVertexOutOfRange, .op = 36}});
  CHECK(host.next_event_frame == actors::kNoEventFrame);
}

TEST_CASE("Wait events hold the frame for their count",
          "[actors][actors-animation-events]") {
  FakeEventHost host({55, 2, 0, 62, 0, 0});
  host.frame = 4;
  RunEvents(host);
  CHECK(host.event_wait == 2);
  CHECK(host.frame == 3);
  CHECK(host.Offset() == 0);
  CHECK(host.calls.empty());

  host.frame = 4;
  RunEvents(host);
  CHECK(host.event_wait == 1);
  CHECK(host.frame == 3);
  CHECK(host.calls.empty());
}

TEST_CASE("Wait events run on once their count is spent",
          "[actors][actors-animation-events]") {
  FakeEventHost host({55, 2, 0, 62, 0, 0});
  for (int frame = 0; frame < 2; ++frame) {
    host.frame = 4;
    RunEvents(host);
  }
  REQUIRE(host.event_wait == 1);
  host.frame = 4;
  RunEvents(host);
  CHECK(host.event_wait == 0);
  CHECK(host.frame == 4);
  CHECK(host.calls == Calls{"fast 1"});
}

TEST_CASE("Animation events play their clip and reload the script",
          "[actors][actors-animation-events]") {
  FakeEventHost host({62, 0, 21, 30});
  host.frame = 7;
  CHECK(RunEvents(host).status == AnimationEventStatus::kDone);
  CHECK(host.calls == Calls{"fast 1", "play 30/0", "flag 0"});
  CHECK(host.frame == 0);
  CHECK_FALSE(host.script);

  CHECK(PlayedEvents({22, 30, 6}) == Calls{"play 30/6", "flag 0"});
  CHECK(PlayedEvents({19, 5}) == Calls{"play 13/5"});
  CHECK(PlayedEvents({20, 5}) == Calls{"play 1/5"});
  CHECK(PlayedEvents({1}) == Calls{"init pose", "flag 1"});
}

TEST_CASE("Next and previous animation events step the animation id",
          "[actors][actors-animation-events]") {
  FakeEventHost next({24});
  next.animation_id = 99;
  RunEvents(next);
  CHECK(next.calls == Calls{"play 0/0"});
  FakeEventHost previous({23});
  RunEvents(previous);
  CHECK(previous.calls == Calls{"play 4/0"});
}

TEST_CASE("Impact events fall back to animation 1 from the reached frame",
          "[actors][actors-animation-events]") {
  // The first event's reached frame is 0 (#297).
  CHECK(PlayedEvents({2}) == Calls{"impact", "play 1/0"});

  FakeEventHost later({62, 6, 2});
  later.frame = 6;
  RunEvents(later);
  CHECK(later.calls == Calls{"fast 1", "impact", "play 1/6"});

  FakeEventHost queued({2, 0, 0});
  queued.impact_queued = true;
  RunEvents(queued);
  CHECK(queued.calls == Calls{"impact"});
}

TEST_CASE("Event 13 restarts the pose unless an advance was requested",
          "[actors][actors-animation-events]") {
  FakeEventHost busy({13, 0, 0});
  busy.effects_busy = true;
  RunEvents(busy);
  CHECK(busy.calls == Calls{"init pose", "flag 1"});

  FakeEventHost advanced({13, 0, 0});
  advanced.effects_busy = true;
  advanced.advance_requested = true;
  RunEvents(advanced);
  CHECK(advanced.calls == Calls{"clear advance"});
  CHECK_FALSE(advanced.advance_requested);

  CHECK(PlayedEvents({13, 0, 0}) == Calls{"clear advance"});
}

TEST_CASE("Reload and dispatch budgets stop runaway scripts",
          "[actors][actors-animation-events]") {
  FakeEventHost looping({24});
  looping.reload = looping.script;
  looping.reload_frame = 0;
  const auto reloaded = RunEvents(looping);
  CHECK(reloaded.status == AnimationEventStatus::kReloadLimit);
  CHECK(actors::StopsAnimationEvents(reloaded.status));
  CHECK(looping.calls.size() ==
        static_cast<std::size_t>(actors::kAnimationEventReloadLimit) + 1);
  CHECK(looping.problems ==
        std::vector<ReportedProblem>{
            {.status = AnimationEventStatus::kReloadLimit, .op = 24}});
  CHECK(looping.next_event_frame == actors::kNoEventFrame);

  Bytes long_script;
  for (int i = 0; i < actors::kAnimationEventDispatchLimit + 10; ++i) {
    long_script.insert(long_script.end(), {40, 0});
  }
  FakeEventHost dispatching(long_script);
  const auto dispatched = RunEvents(dispatching);
  CHECK(dispatched.status == AnimationEventStatus::kDispatchLimit);
  CHECK(dispatching.Offset() ==
        static_cast<std::size_t>(actors::kAnimationEventDispatchLimit) * 2);
  CHECK(dispatching.next_event_frame == actors::kNoEventFrame);
}

namespace {
// Runs `host` to a stop that reports `status` once for opcode `op`.
void ExpectStop(FakeEventHost& host, AnimationEventStatus status,
                std::uint8_t op) {
  CHECK(RunEvents(host).status == status);
  CHECK(actors::StopsAnimationEvents(status));
  CHECK(host.problems ==
        std::vector<ReportedProblem>{{.status = status, .op = op}});
  CHECK(host.next_event_frame == actors::kNoEventFrame);
}
}  // namespace

TEST_CASE("Scripts with no script or short operands stop the run",
          "[actors][actors-animation-events]") {
  FakeEventHost no_script(Bytes{});
  no_script.script.reset();
  ExpectStop(no_script, AnimationEventStatus::kNoScript, 0);

  FakeEventHost short_operands({36, 0x06});
  ExpectStop(short_operands, AnimationEventStatus::kScriptOverrun, 36);
  CHECK(short_operands.calls.empty());

  FakeEventHost no_cursor({61});
  ExpectStop(no_cursor, AnimationEventStatus::kScriptOverrun, 61);
  CHECK(no_cursor.calls == Calls{"fast 0"});

  FakeEventHost cut_frame({61, 0xFF});
  ExpectStop(cut_frame, AnimationEventStatus::kScriptOverrun, 61);

  FakeEventHost past_bank({61, 0, 0});
  Required(past_bank.script).offset = 5;
  ExpectStop(past_bank, AnimationEventStatus::kScriptOverrun, 0);
}

TEST_CASE("Opcodes past the length table stop the run",
          "[actors][actors-animation-events]") {
  FakeEventHost past_table({61, 0, 0});
  EventTables tables;
  auto view = tables.View();
  view.lengths = view.lengths.first(40);
  CHECK(actors::RunAnimationEvents(past_table, view, AnimationEventMode::kPlay)
            .status == AnimationEventStatus::kTableOutOfRange);
  CHECK(past_table.problems ==
        std::vector<ReportedProblem>{
            {.status = AnimationEventStatus::kTableOutOfRange, .op = 61}});
  CHECK(past_table.calls.empty());
}

TEST_CASE("Sound pairs pick the drawn weapon's sound below the height limit",
          "[actors][actors-animation-events]") {
  const auto play_pair = [](std::uint8_t sound, bool drawn, std::uint8_t model,
                            std::int16_t height) {
    FakeEventHost host({4, sound, 0, 0});
    host.weapon_drawn = drawn;
    host.sequence_model = model;
    host.motion.position.y = height;
    const auto run = RunEvents(host);
    return std::pair{run.status, host.calls};
  };
  CHECK(play_pair(0x58, true, 3, 0).second == Calls{"pair 51"});
  CHECK(play_pair(0x58, false, 3, 0).second == Calls{"pair 48"});
  CHECK(play_pair(0x58, true, 0, 0).second == Calls{"pair 48"});
  CHECK(play_pair(9, true, 3, 0).second == Calls{"pair 9"});
  CHECK(play_pair(9, true, 3, 3000).second.empty());

  const auto [status, calls] = play_pair(0x58, true, 12, 0);
  CHECK(status == AnimationEventStatus::kSoundOutOfRange);
  CHECK(calls.empty());
}

TEST_CASE("Shadow points blend the radius and level the short way round",
          "[actors][actors-animation-events]") {
  FakeEventHost point({60, 1, 0, 0});
  point.shadow = {
      .level = 4000, .radiusExpandRate = 100, .radiusShrinkRate = 50};
  CHECK(RunEvents(point).status == AnimationEventStatus::kDone);
  CHECK(point.shadow.radiusExpandRate == 300);
  CHECK(point.shadow.radiusShrinkRate == 200);
  // 100 lies 196 past 4000 the short way round.
  CHECK(point.shadow.levelDelta == 196);
  CHECK(point.shadow.level == 100);

  FakeEventHost missing({60, 8, 0, 0});
  missing.shadow = point.shadow;
  CHECK(RunEvents(missing).status == AnimationEventStatus::kPointOutOfRange);
  CHECK(missing.shadow == point.shadow);
}

TEST_CASE("Shadow level events blend over their frames",
          "[actors][actors-animation-events]") {
  FakeEventHost blending({54, 0x00, 0x64, 5, 0, 0});
  blending.shadow.level = 4000;
  RunEvents(blending);
  CHECK(blending.shadow.level == 4000);
  CHECK(blending.shadow.levelDelta == 196);
  CHECK(blending.shadow.levelFrames == 5);
}

TEST_CASE("Action events finish the action and reset movement",
          "[actors][actors-animation-events]") {
  FakeEventHost companion({58, 0, 0});
  companion.companion_slot = 2;
  companion.companion_found = false;
  CHECK(RunEvents(companion).status == AnimationEventStatus::kNoCompanion);
  CHECK(companion.motion.action_animation_state == 2);
  CHECK(companion.calls == Calls{"finish companion 2"});

  FakeEventHost leader({58, 0, 0});
  leader.companion_slot = 1;
  RunEvents(leader);
  CHECK(leader.calls.empty());

  FakeEventHost moving({59, 0, 0});
  moving.motion.movement_pending = true;
  moving.motion.locomotion_state = 1;
  RunEvents(moving);
  CHECK(moving.motion.root_motion.y == 3);
  CHECK(moving.motion.movement_mode == 3);
  CHECK(moving.motion.locomotion_state == 0);
  CHECK_FALSE(moving.motion.movement_pending);
}

TEST_CASE("Restarting a clip's events unwinds its vertex moves",
          "[actors][actors-animation-events]") {
  const EventTables tables;
  // The events start one byte into the motion data, at frame 0.
  const Bytes bank = EventClip(1, {0, 0, 36, 0x06, 0x05, 0, 0});
  const actors::AnimationClipRef clip{
      .bank = bank, .header_offset = kClipHeader, .bones = 0};
  FakeEventHost host(Bytes{});
  host.script.reset();
  CHECK(actors::RestartActorAnimationEvents(host, tables.View(), clip, false) ==
        EventRestart::kUnwound);
  CHECK(host.calls == Calls{"move 5 -3,0,0", "overridden"});
  CHECK(Required(host.script).bank.data() == bank.data());
  CHECK(host.next_event_frame == actors::kNoEventFrame);
}

namespace {
// Restarts `clip_bank`'s events on a host with no script, which a restart
// that unwinds nothing leaves untouched.
EventRestart RestartIdle(const EventTables& tables, const Bytes& clip_bank,
                         bool overridden) {
  FakeEventHost idle(Bytes{});
  idle.script.reset();
  const auto result = actors::RestartActorAnimationEvents(
      idle, tables.View(),
      actors::AnimationClipRef{
          .bank = clip_bank, .header_offset = kClipHeader, .bones = 0},
      overridden);
  CHECK(idle.calls.empty());
  CHECK_FALSE(idle.script);
  return result;
}
}  // namespace

TEST_CASE("Restarts without events or a clip unwind nothing",
          "[actors][actors-animation-events]") {
  const EventTables tables;
  const Bytes no_events = EventClip(0, {});
  CHECK(RestartIdle(tables, no_events, false) == EventRestart::kNoEvents);
  CHECK(RestartIdle(tables, no_events, true) == EventRestart::kUnavailable);
  CHECK(RestartIdle(tables, EventClip(200, {}), false) ==
        EventRestart::kUnavailable);

  FakeEventHost unbound(Bytes{});
  CHECK(actors::RestartActorAnimationEvents(unbound, tables.View(),
                                            std::nullopt, false) ==
        EventRestart::kUnavailable);
}
