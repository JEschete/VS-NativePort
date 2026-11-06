#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorFlags.hpp"
#include "Actors/ActorLifecycle.hpp"
#include "Actors/ActorMotion.hpp"
#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/BattleActorState.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.h"
#include "Actors/WeaponModel.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Field/Gimmicks.h"
}

namespace {
namespace actors = VS::Actors;
using actors::StateFlag;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

using Calls = std::vector<std::string>;

// The actor table, gimmicks, weapon models and systems of the state rules,
// logging every system call in order.
class FakeStateHost final : public actors::ActorStateHost {
 public:
  std::array<std::optional<actors::ActorStateRecord>, actors::kActorStateSlots>
      records{};
  std::map<int, actors::GimmickView> gimmicks;
  std::map<int, std::int16_t> gimmick_modes;
  std::map<int, bool> weapons;
  std::set<std::pair<int, int>> animations;
  Calls calls;
  bool texture_pages = false;
  int support_height = 0;
  int ceiling = 0;
  std::optional<VS::Field::PlatformContact> platform;

  actors::ActorStateRecord& Add(int id) {
    actors::ActorStateRecord& record =
        records.at(static_cast<std::size_t>(id)).emplace();
    record.motion.slot = static_cast<std::uint8_t>(id);
    return record;
  }
  actors::ActorStateRecord& At(int id) {
    std::optional<actors::ActorStateRecord>& record =
        records.at(static_cast<std::size_t>(id));
    if (!record.has_value()) {
      throw std::logic_error("the fake has no actor state record");
    }
    return *record;
  }

  actors::ActorStateRecord* Record(int id) override {
    if (id < 0 || id >= actors::kActorStateSlots) return nullptr;
    std::optional<actors::ActorStateRecord>& record =
        records.at(static_cast<std::size_t>(id));
    return record.has_value() ? &*record : nullptr;
  }
  std::optional<actors::GimmickView> Gimmick(int id) override {
    const auto found = gimmicks.find(id);
    if (found == gimmicks.end()) return std::nullopt;
    return found->second;
  }
  void SetGimmickDisplay(int id, bool skipped, bool visible) override {
    gimmicks.at(id).skipped = skipped;
    gimmicks.at(id).visible = visible;
    calls.push_back(std::format("gimmick-display {}", id));
  }
  void SetGimmickMode(int id, std::int16_t mode) override {
    gimmick_modes[id] = mode;
    calls.push_back(std::format("gimmick-mode {} {}", id, mode));
  }
  void ClearGimmickSupport(int id) override {
    calls.push_back(std::format("gimmick-support {}", id));
  }
  void PlayGimmickSound(int id, int sound, int variant) override {
    calls.push_back(std::format("gimmick-sound {} {} {}", id, sound, variant));
  }
  std::optional<bool> WeaponModelShown(int index) override {
    const auto found = weapons.find(index);
    if (found == weapons.end()) return std::nullopt;
    return found->second;
  }
  void SetWeaponModelShown(int index, bool shown) override {
    if (weapons.contains(index)) weapons[index] = shown;
    calls.push_back(std::format("weapon {} {}", index, shown));
  }
  bool SharedBankHasTexturePages() override { return texture_pages; }
  void ResetHeadTrackingSpeed(int id) override {
    calls.push_back(std::format("head {}", id));
  }
  void UploadTextureFrame(int id, int channel, int frame) override {
    calls.push_back(std::format("texture {} {} {}", id, channel, frame));
  }
  void ClearTint(int id) override {
    calls.push_back(std::format("tint {}", id));
  }
  void ClearSoundOverrides(int id) override {
    calls.push_back(std::format("sound {}", id));
  }
  void PlayAnimation(int id, int animation, int blend_frames) override {
    calls.push_back(std::format("play {} {} {}", id, animation, blend_frames));
  }
  bool HasAnimation(int id, int animation) override {
    return animations.contains({id, animation});
  }
  void ReleaseMovingPlatform(int platform_index) override {
    calls.push_back(std::format("release {}", platform_index));
  }
  int SupportHeight(int x, int z) override {
    calls.push_back(std::format("support {} {}", x, z));
    return support_height;
  }
  int NavigationCeilingHeight() override { return ceiling; }
  std::optional<VS::Field::PlatformContact> MovingPlatformContact(
      int support_layer) override {
    calls.push_back(std::format("platform {}", support_layer));
    return platform;
  }
  void SaveGroundedTransform(int id) override {
    calls.push_back(std::format("save {}", id));
  }
};

class FakeBattleHost final : public actors::BattleActorHost {
 public:
  std::array<std::optional<actors::BattleActorState>, actors::kBattleActorSlots>
      actors{};
  std::array<std::optional<actors::ActorVitals>, actors::kBattleActorSlots>
      vitals{};
  Calls calls;
  bool list_changes = true;

  actors::BattleActorState& Add(int id) {
    return actors.at(static_cast<std::size_t>(id)).emplace();
  }
  actors::BattleActorState& At(int id) {
    std::optional<actors::BattleActorState>& actor =
        actors.at(static_cast<std::size_t>(id));
    if (!actor.has_value()) {
      throw std::logic_error("the fake has no battle actor");
    }
    return *actor;
  }

  actors::BattleActorState* Actor(int id) override {
    if (!actors::IsBattleActorSlot(id)) return nullptr;
    std::optional<actors::BattleActorState>& actor =
        actors.at(static_cast<std::size_t>(id));
    return actor.has_value() ? &*actor : nullptr;
  }
  actors::ActorVitals* Vitals(int id) override {
    if (!actors::IsBattleActorSlot(id)) return nullptr;
    std::optional<actors::ActorVitals>& stats =
        vitals.at(static_cast<std::size_t>(id));
    return stats.has_value() ? &*stats : nullptr;
  }
  void ClearStatuses(int id) override {
    calls.push_back(std::format("statuses {}", id));
  }
  void SetMarker(int id, int flags) override {
    calls.push_back(std::format("marker {} {:#x}", id, flags));
  }
  bool UnlinkFromRoomList(int id) override {
    calls.push_back(std::format("unlink {}", id));
    return list_changes;
  }
  bool LinkAfterAshley(int id) override {
    calls.push_back(std::format("link {}", id));
    return list_changes;
  }
  void SetAnimationLocked(int id, bool locked) override {
    calls.push_back(std::format("lock {} {}", id, locked));
  }
  void ClearAiLinksAndPlan(int id) override {
    calls.push_back(std::format("clear-ai {}", id));
  }
  void ResolveAiLinkedActors() override { calls.emplace_back("resolve-ai"); }
  void RebuildActionPlans() override { calls.emplace_back("rebuild-plans"); }
};

class FakeToggleHost final : public actors::ActorModelToggleHost {
 public:
  std::array<bool, actors::kRosterSlots> present{};
  std::array<std::uint32_t, actors::kRosterSlots> flags{};
  std::vector<std::size_t> written;

  std::uint32_t StateFlags(std::size_t slot) const override {
    return flags.at(slot);
  }
  void SetDisplayFlags(std::size_t slot, std::uint32_t value) override {
    flags.at(slot) = value;
    written.push_back(slot);
  }
  bool HasBattleActor(std::size_t slot) override {
    return slot < present.size() && present.at(slot);
  }
};

// A record with no flag raised: shown, no parent, no motion.
actors::ActorStateRecord Quiet() {
  actors::ActorStateRecord record;
  record.skipped = true;
  return record;
}

std::uint32_t Flags(const actors::ActorStateRecord& record) {
  return actors::ComposeActorStateFlags(record, {});
}
}  // namespace

TEST_CASE("State flags compose a record's display and attachment state",
          "[actors-state]") {
  CHECK(Flags(Quiet()) == StateFlag::kActorRecord);

  actors::ActorStateRecord record = Quiet();
  record.skipped = false;
  record.visible = true;
  record.frozen = true;
  record.sound_enabled = true;
  record.pose_reached_end = true;
  record.attached_child_count = 2;
  record.attach_parent = 4;
  record.animation_locked = true;
  record.support_gimmick = 3;
  CHECK(actors::ComposeActorStateFlags(record, {.first = true}) ==
        (StateFlag::kActorRecord | StateFlag::kShown | StateFlag::kVisible |
         StateFlag::kFrozen | StateFlag::kSoundEnabled | StateFlag::kPoseEnded |
         StateFlag::kHasAttachedChildren | StateFlag::kAttachedToParent |
         StateFlag::kFirstWeaponShown | StateFlag::kAnimationLocked |
         StateFlag::kOnGimmick));

  record = Quiet();
  record.holding_last_frame = true;
  CHECK(Flags(record) == (StateFlag::kActorRecord | 0x08000080U));
  record = Quiet();
  record.animation_event = true;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kEventBit));
  record = Quiet();
  record.bypass_actor_collision = true;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kEventBit));
  // The support layer shares bit 19 with the action pose.
  record = Quiet();
  record.support_layer = 0xF;
  CHECK(Flags(record) == (StateFlag::kActorRecord | 0xF0000U));
  CHECK(actors::ComposeActorStateFlags(Quiet(), {.second = true}) ==
        (StateFlag::kActorRecord | StateFlag::kSecondWeaponShown));
}

TEST_CASE("State flags compose an actor's motion state", "[actors-state]") {
  actors::ActorStateRecord record = Quiet();
  // A route raises both the translation and the turn.
  record.motion.route_phase = 1;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kTranslating |
                          StateFlag::kTurning));
  record = Quiet();
  record.motion.move_frames = 3;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kTranslating));
  record = Quiet();
  record.motion.turn_frames = 3;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kTurning));

  record = Quiet();
  record.motion.scale_frames = 1;
  record.motion.movement_mode = 2;
  record.motion.motion_locked = true;
  record.motion.pose_state = 4;
  record.motion.movement_pending = true;
  CHECK(Flags(record) ==
        (StateFlag::kActorRecord | StateFlag::kScaling |
         StateFlag::kMovementActive | StateFlag::kMotionLocked |
         StateFlag::kPoseActive | StateFlag::kMovementPending));

  record = Quiet();
  record.motion.locomotion_state = 1;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kLocomotion |
                          StateFlag::kLocomotionStarting));
  record.motion.locomotion_state = 2;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kLocomotion));
}

TEST_CASE("State flags compose an actor's animation and reaction state",
          "[actors-state]") {
  actors::ActorStateRecord record = Quiet();
  record.motion.action_animation_state = 5;
  CHECK(Flags(record) ==
        (StateFlag::kActorRecord | StateFlag::kActionAnimating));
  record.motion.action_animation_state = 6;
  CHECK(Flags(record) == StateFlag::kActorRecord);

  record = Quiet();
  record.motion.action_pose_active = true;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kActionPose));
  for (const int animation : {0x47, 0x54}) {
    record = Quiet();
    record.animation_id = static_cast<std::uint8_t>(animation);
    CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kActionPose));
  }
  for (const int animation : {0x46, 0x55}) {
    record = Quiet();
    record.animation_id = static_cast<std::uint8_t>(animation);
    CHECK(Flags(record) == StateFlag::kActorRecord);
  }

  record = Quiet();
  record.reaction_state = 2;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kLightReaction));
  record.reaction_state = 3;
  CHECK(Flags(record) == (StateFlag::kActorRecord | StateFlag::kHeavyReaction));
}

TEST_CASE("State flags read the record's weapons, a gimmick or nothing",
          "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = host.Add(3);
  record.skipped = true;
  // The weapons follow the record's own slot, not the queried id.
  record.motion.slot = 5;
  host.weapons = {{6, true}, {10, true}, {11, false}};
  CHECK(actors::ActorStateFlags(host, 3) ==
        (StateFlag::kActorRecord | StateFlag::kFirstWeaponShown));

  host.gimmicks[4] = {.skipped = false, .visible = true};
  CHECK(actors::ActorStateFlags(host, 4) ==
        (StateFlag::kShown | StateFlag::kVisible));
  host.gimmicks[4] = {.skipped = true, .visible = false};
  CHECK(actors::ActorStateFlags(host, 4) == 0);
  CHECK(actors::ComposeGimmickStateFlags({}) == StateFlag::kShown);
  CHECK(actors::ActorStateFlags(host, 7) == 0);
  CHECK(actors::ActorStateFlags(host, 40) == 0);
}

TEST_CASE("Display flags decode the shown, visible, frozen and sound bits",
          "[actors-state]") {
  CHECK(actors::DecodeActorDisplayFlags(0) ==
        actors::ActorDisplay{.skipped = true});
  CHECK(actors::DecodeActorDisplayFlags(0x1847) ==
        actors::ActorDisplay{.skipped = false,
                             .visible = true,
                             .frozen = true,
                             .sound_enabled = true,
                             .first_weapon_shown = true,
                             .second_weapon_shown = true});
  // Only bits 0-2, 6, 11 and 12 are read.
  CHECK(actors::DecodeActorDisplayFlags(0xFFFFE7B8U) ==
        actors::ActorDisplay{.skipped = true});
}

TEST_CASE("Display flags follow the companion chain", "[actors-state]") {
  FakeStateHost host;
  host.Add(2).companion_slot = 6;
  // A marker companion (1) ends the chain.
  host.Add(6).companion_slot = 1;
  host.Add(1);
  host.weapons = {{4, false}, {12, false}};
  actors::SetActorDisplayFlags(host, 2,
                               StateFlag::kVisible | StateFlag::kSoundEnabled |
                                   StateFlag::kFirstWeaponShown);
  for (const int id : {2, 6}) {
    const actors::ActorStateRecord& record = host.At(id);
    CHECK(record.skipped);
    CHECK(record.visible);
    CHECK_FALSE(record.frozen);
    CHECK(record.sound_enabled);
  }
  CHECK_FALSE(host.At(1).visible);
  CHECK(host.calls == Calls{"weapon 4 true", "weapon 5 false", "weapon 12 true",
                            "weapon 13 false"});
  CHECK(host.weapons == std::map<int, bool>{{4, true}, {12, true}});
}

TEST_CASE("Display flags set the gimmick standing in an empty slot",
          "[actors-state]") {
  FakeStateHost host;
  host.gimmicks[5] = {.skipped = false, .visible = false};
  actors::SetActorDisplayFlags(host, 5, StateFlag::kVisible);
  CHECK(host.gimmicks.at(5) ==
        actors::GimmickView{.skipped = true, .visible = true});
  actors::SetActorDisplayFlags(host, 8, StateFlag::kShown);
  CHECK(host.calls == Calls{"gimmick-display 5"});
}

TEST_CASE("A repeating companion chain stops the display flags",
          "[actors-state]") {
  FakeStateHost host;
  host.Add(2).companion_slot = 3;
  host.Add(3).companion_slot = 2;
  const ChannelWarnings warnings("actors");
  actors::SetActorDisplayFlags(host, 2, StateFlag::kShown);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Display flags stop at a repeating companion chain from actor 2"});
  // One pass over the table, then the slot after it.
  CHECK(host.calls.size() == std::size_t{2} * (actors::kActorStateSlots + 1));
}

TEST_CASE("The model switches toggle each present actor's shown flag",
          "[actors-state]") {
  FakeToggleHost host;
  host.present.at(0) = true;
  host.present.at(4) = true;
  host.flags.at(0) = StateFlag::kVisible;
  host.flags.at(4) = StateFlag::kShown | StateFlag::kFrozen;
  host.flags.at(9) = StateFlag::kVisible;

  actors::EnableAllActorModels(host);
  CHECK(host.written == std::vector<std::size_t>{0, 4});
  CHECK(host.flags.at(0) == (StateFlag::kVisible | StateFlag::kShown));
  CHECK(host.flags.at(9) == StateFlag::kVisible);

  host.written.clear();
  actors::DisableActorModel(host, 4);
  actors::DisableActorModel(host, 9);
  actors::DisableActorModel(host, 99);
  CHECK(host.written == std::vector<std::size_t>{4});
  CHECK(host.flags.at(4) == StateFlag::kFrozen);

  host.written.clear();
  actors::DisableAllActorModels(host);
  actors::EnableActorModel(host, 4);
  CHECK(host.written == std::vector<std::size_t>{0, 4, 4});
  CHECK(host.flags.at(0) == StateFlag::kVisible);
  CHECK(host.flags.at(4) == (StateFlag::kFrozen | StateFlag::kShown));
}

TEST_CASE("Battle actors turn hostile or peaceful", "[actors-state]") {
  FakeBattleHost host;
  host.Add(3).kind = 1;
  actors::SetActorHostile(host, 3, true);
  CHECK(host.At(3).kind == actors::kHostileActorKind);
  actors::SetActorHostile(host, 3, false);
  CHECK(host.At(3).kind == actors::kPeacefulActorKind);
  actors::SetActorHostile(host, 4, true);
  CHECK_FALSE(host.actors.at(4).has_value());

  const ChannelWarnings warnings("actors");
  actors::SetActorHostile(host, 42, true);
  actors::SetActorHostile(host, -1, true);
  CHECK(warnings.messages ==
        std::vector<std::string>{"Battle actor 42 is outside the actor table",
                                 "Battle actor -1 is outside the actor table"});
}

TEST_CASE("The HP percentage keeps a living actor at 1 or more",
          "[actors-state]") {
  CHECK(actors::HpPercentage(0, 100) == 0);
  CHECK(actors::HpPercentage(-5, 100) == 0);
  CHECK(actors::HpPercentage(50, 100) == 50);
  CHECK(actors::HpPercentage(1, 1000) == 1);
  CHECK(actors::HpPercentage(999, 1000) == 99);
  CHECK(actors::HpPercentage(150, 100) == 150);

  const ChannelWarnings warnings("actors");
  CHECK(actors::HpPercentage(5, 0) == 1);
  CHECK(actors::HpPercentage(0, 0) == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "An actor with 5 HP has no maximum HP; reporting 1 percent"});
}

TEST_CASE("An actor's HP percentage needs the actor and its stats",
          "[actors-state]") {
  FakeBattleHost host;
  host.Add(2);
  host.Add(3);
  host.vitals.at(3) = actors::ActorVitals{.current_hp = 30, .max_hp = 120};
  CHECK(actors::ActorHpPercentage(host, 3) == 25);
  CHECK(actors::ActorHpPercentage(host, 2) == 0);

  const ChannelWarnings warnings("actors");
  CHECK(actors::ActorHpPercentage(host, 5) == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{"No battle actor 5 for the HP percentage"});
}

TEST_CASE("Restoring vitals refills HP, MP and limbs and ends statuses",
          "[actors-state]") {
  FakeBattleHost host;
  host.Add(3);
  actors::ActorVitals vitals{.current_hp = 4,
                             .max_hp = 90,
                             .current_mp = 1,
                             .max_mp = 20,
                             .risk = 55,
                             .action_recovery_timer = 7};
  for (std::size_t limb = 0; limb < actors::kActorLimbs; ++limb) {
    vitals.limbs.at(limb) = {.hp = 1,
                             .max_hp = static_cast<std::int16_t>(10 + limb)};
  }
  host.vitals.at(3) = vitals;
  actors::RestoreActorVitals(host, 3);

  actors::ActorVitals restored = vitals;
  restored.current_hp = 90;
  restored.current_mp = 20;
  restored.risk = 0;
  restored.action_recovery_timer = 0;
  for (actors::LimbVitals& limb : restored.limbs) limb.hp = limb.max_hp;
  CHECK(host.vitals.at(3) == restored);
  CHECK(host.calls == Calls{"statuses 3"});

  host.calls.clear();
  host.Add(4);
  actors::RestoreActorVitals(host, 4);
  actors::RestoreActorVitals(host, 5);
  CHECK(host.calls.empty());
}

TEST_CASE("Room actor scans cover the first sixteen slots", "[actors-state]") {
  FakeBattleHost host;
  host.Add(2) = {.model_id = 0x40, .kind = actors::kHostileActorKind};
  host.Add(5) = {.model_id = 0x40, .kind = actors::kHostileActorKind};
  host.Add(9) = {.model_id = 0x41, .kind = actors::kPeacefulActorKind};
  host.Add(16) = {.model_id = 0x42, .kind = actors::kHostileActorKind};
  CHECK(actors::FindActorByModelId(host, 0x40) == 2);
  CHECK(actors::FindActorByModelId(host, 0x41) == 9);
  CHECK(actors::FindActorByModelId(host, 0x42) == actors::kModelNotFoundSlot);
  CHECK(actors::CountActorsByDisposition(host, actors::kHostileActorKind) == 2);
  CHECK(actors::CountActorsByDisposition(host, actors::kPeacefulActorKind) ==
        1);
  CHECK(actors::CountActorsByDisposition(host, 1) == 0);
}

TEST_CASE("Retention after defeat needs a known actor", "[actors-state]") {
  FakeBattleHost host;
  host.Add(3).retain_after_defeat = true;
  host.Add(4);
  CHECK(actors::IsActorRetainedAfterDefeat(host, 3));
  CHECK_FALSE(actors::IsActorRetainedAfterDefeat(host, 4));
  CHECK_FALSE(actors::IsActorRetainedAfterDefeat(host, 5));
  const ChannelWarnings warnings("actors");
  CHECK_FALSE(actors::IsActorRetainedAfterDefeat(host, 50));
  CHECK(warnings.messages.size() == 1);
}

TEST_CASE("Disabling logic suspends an actor and enabling resumes it",
          "[actors-state]") {
  FakeBattleHost host;
  host.Add(3) = {.command_state = 7, .kind = actors::kHostileActorKind};
  actors::SetActorLogicEnabled(host, 3, false);
  CHECK(host.actors.at(3) ==
        actors::BattleActorState{.command_state = actors::kLogicDisabledCommand,
                                 .kind = 0x8004});
  CHECK(host.calls == Calls{"marker 3 0x0", "marker 3 0x40"});

  actors::SetActorLogicEnabled(host, 3, true);
  CHECK(host.actors.at(3) ==
        actors::BattleActorState{.kind = actors::kHostileActorKind});
  // Only a suspended actor resumes.
  host.At(3).command_state = 9;
  host.At(3).kind = 0x8004;
  actors::SetActorLogicEnabled(host, 3, true);
  CHECK(host.actors.at(3) ==
        actors::BattleActorState{.command_state = 9, .kind = 0x8004});
}

TEST_CASE("Other actors' logic covers slots 1 to 15", "[actors-state]") {
  FakeBattleHost host;
  for (const int id : {0, 1, 15, 16}) host.Add(id);
  actors::SetOtherActorLogicEnabled(host, false);
  CHECK(host.At(0).command_state == 0);
  CHECK(host.At(1).command_state == actors::kLogicDisabledCommand);
  CHECK(host.At(15).command_state == actors::kLogicDisabledCommand);
  CHECK(host.At(16).command_state == 0);
  CHECK(host.calls == Calls{"marker 1 0x0", "marker 1 0x40", "marker 15 0x0",
                            "marker 15 0x40"});
}

TEST_CASE("Detaching an actor locks it and refreshes the AI on a change",
          "[actors-state]") {
  FakeBattleHost host;
  host.Add(3);
  actors::SetActorDetached(host, 3, true);
  CHECK(host.calls ==
        Calls{"unlink 3", "lock 3 true", "clear-ai 3", "rebuild-plans"});
  host.calls.clear();
  actors::SetActorDetached(host, 3, false);
  CHECK(host.calls ==
        Calls{"link 3", "lock 3 false", "resolve-ai", "rebuild-plans"});

  host.calls.clear();
  host.list_changes = false;
  actors::SetActorDetached(host, 3, true);
  actors::SetActorDetached(host, 3, false);
  actors::SetActorDetached(host, 4, true);
  CHECK(host.calls ==
        Calls{"unlink 3", "lock 3 true", "link 3", "lock 3 false"});
}

namespace {
// A record with every field the motion reset clears set.
actors::ActorStateRecord Busy(int slot) {
  actors::ActorStateRecord record;
  actors::ActorMotion& motion = record.motion;
  motion.slot = static_cast<std::uint8_t>(slot);
  motion.scale = {.x = 1, .y = 2, .z = 3, .w = 4};
  motion.jump_countdown = 7;
  motion.scale_frames = 9;
  motion.turn_frames = 11;
  motion.rotation_x = 12;
  motion.facing = 13;
  motion.rotation_z = 14;
  motion.move_frames = 15;
  motion.movement_mode = 3;
  motion.route_phase = 2;
  motion.pose_state = 5;
  motion.locomotion_state = 2;
  motion.movement_pending = true;
  motion.action_pose_active = true;
  motion.action_animation_state = 4;
  motion.run_frames = 8;
  motion.is_running = true;
  motion.request_mode = 2;
  motion.request_direction = 3;
  motion.request_facing = 4;
  motion.request_distance = 5;
  motion.position = {.x = 100, .y = -50, .z = 200, .w = 9};
  record.rotation_reserved = 16;
  record.head_track_target = 5;
  record.look_pitch = 17;
  record.look_yaw = 18;
  record.reaction_state = 4;
  record.palette_fade_state = 3;
  record.texture_timelines = {5, 6};
  record.palette_fade_active = true;
  record.palette_fade_reverse = true;
  record.companion_slot = 1;
  record.collision_state = 1;
  record.reserved_17ff = 19;
  record.palette_pulse_kind = 20;
  record.reserved_1860 = 21;
  record.support_layer = 3;
  record.placement_layer = 4;
  record.queued_animation = 22;
  record.ledge_height = 23;
  record.platform_offset_x = 24;
  record.platform_offset_z = 25;
  record.animation_locked = true;
  record.shadow = {.level_frames = 1, .level = 2, .radius_expand_rate = 3};
  record.shadow_extras = {.reserved0 = 26, .reserved6 = 27, .reserved7 = 28};
  record.shadow_expand_rate = 0x30;
  record.shadow_shrink_rate = 0x31;
  record.request_extras = {.reserved = {1, 2, 3},
                           .reserved_c = 29,
                           .step = {.x = 1, .y = 2, .z = 3, .w = 4}};
  record.attach_parent = 6;
  record.companion = {.offset = {.x = 1, .y = 2, .z = 3, .depth_bias = 4},
                      .color = {5, 6, 7, 8}};
  return record;
}

// What the motion reset leaves of Busy.
actors::ActorStateRecord BusyReset(int slot) {
  actors::ActorStateRecord record = Busy(slot);
  actors::ActorMotion& motion = record.motion;
  motion.scale = {.x = 0x1000, .y = 0x1000, .z = 0x1000, .w = 4};
  motion.jump_countdown = 0;
  motion.scale_frames = 0;
  motion.turn_frames = 0;
  motion.rotation_x = 0;
  motion.facing = 0;
  motion.rotation_z = 0;
  motion.move_frames = 0;
  motion.movement_mode = 0;
  motion.route_phase = 0;
  motion.pose_state = 0;
  motion.locomotion_state = 0;
  motion.movement_pending = false;
  motion.action_pose_active = false;
  motion.action_animation_state = 0;
  motion.run_frames = 0;
  motion.is_running = false;
  motion.request_mode = 0;
  motion.request_direction = 0;
  motion.request_facing = 0;
  motion.request_distance = 0;
  record.rotation_reserved = 0;
  record.head_track_target = -2;
  record.look_pitch = 0;
  record.look_yaw = 0;
  record.reaction_state = 0;
  record.palette_fade_state = 0;
  record.texture_timelines = {};
  record.palette_fade_active = false;
  record.palette_fade_reverse = false;
  record.companion_slot = 0;
  record.sound_enabled = true;
  record.collision_state = 4;
  record.reserved_17ff = 0;
  record.palette_pulse_kind = 0;
  record.reserved_1860 = 0;
  record.support_layer = 0;
  record.placement_layer = 0;
  record.queued_animation = 0;
  record.ledge_height = 0;
  record.platform_offset_x = 0;
  record.platform_offset_z = 0;
  record.animation_locked = false;
  record.skipped = true;
  record.visible = true;
  record.frozen = true;
  record.shadow = {.radius_expand_rate = 0x30, .radius_shrink_rate = 0x31};
  record.shadow_extras = {};
  record.request_extras = {};
  return record;
}
}  // namespace

TEST_CASE("The motion reset returns an actor to its standing pose",
          "[actors-state]") {
  FakeStateHost host;
  host.records.at(3) = Busy(3);
  host.texture_pages = true;
  actors::ResetActorMotionState(host, 3);
  CHECK(host.At(3) == BusyReset(3));
  CHECK(host.calls == Calls{"head 3", "texture 3 0 0", "texture 3 1 0",
                            "tint 3", "sound 3", "play 3 98 0"});

  // Ashley stands with animation 1; a record companion stays.
  host.calls.clear();
  host.texture_pages = false;
  host.records.at(0) = Busy(0);
  host.At(0).companion_slot = 5;
  actors::ResetActorMotionState(host, 0);
  actors::ActorStateRecord expected = BusyReset(0);
  expected.companion_slot = 5;
  CHECK(host.At(0) == expected);
  CHECK(host.calls == Calls{"head 0", "tint 0", "sound 0", "play 0 1 0"});

  host.calls.clear();
  actors::ResetActorMotionState(host, 4);
  const ChannelWarnings warnings("actors");
  actors::ResetActorMotionState(host, 17);
  CHECK(host.calls.empty());
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Motion reset refused for actor 17 outside the actor table"});
}

TEST_CASE("The movement reset clears every actor's movement",
          "[actors-state]") {
  FakeStateHost host;
  for (const int id : {0, 16}) {
    host.records.at(static_cast<std::size_t>(id)) = Busy(id);
    host.At(id).motion.weapon_pose_active = true;
  }
  actors::ResetAllActorMovement(host);
  for (const int id : {0, 16}) {
    actors::ActorStateRecord expected = Busy(id);
    actors::ActorMotion& motion = expected.motion;
    motion.action_pose_active = false;
    motion.jump_countdown = 0;
    motion.weapon_pose_active = false;
    motion.locomotion_state = 0;
    motion.movement_pending = false;
    motion.is_running = false;
    motion.movement_mode = 0;
    motion.request_mode = 0;
    motion.request_direction = 0;
    motion.request_facing = 0;
    motion.request_distance = 0;
    expected.support_layer = 0;
    expected.placement_layer = 0;
    expected.request_extras = {};
    CHECK(host.At(id) == expected);
  }
  CHECK(host.calls.empty());
}

TEST_CASE("A dying actor drops and plays its death", "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = host.Add(4);
  record.motion.movement_mode = 1;
  record.support_layer = 3;
  record.motion.move_frames = 5;
  record.motion.root_motion = {.x = 5, .y = 7, .z = 9, .w = 1};
  record.motion.destination_mode = 2;
  // A moving actor never takes its ground death.
  host.animations.insert({4, 0xAB});
  actors::BeginActorDeath(host, 4, true);
  CHECK(host.calls == Calls{"release 1", "play 4 70 6", "head 4"});
  CHECK(record.support_layer == 0);
  CHECK(record.motion.move_frames == 0);
  CHECK(record.motion.root_motion ==
        actors::MotionVector{.x = 0, .y = -6, .z = 0, .w = 1});
  CHECK(record.motion.destination_mode == 0);
  CHECK(record.motion.movement_mode == 3);
  CHECK(record.motion.action_animation_state == 1);
}

TEST_CASE("A grounded dying actor falls or takes its action pose",
          "[actors-state]") {
  FakeStateHost host;
  // A grounded actor with the probe animation falls with 0x47.
  host.Add(5);
  host.animations.insert({5, 0xAB});
  actors::BeginActorDeath(host, 5, true);
  CHECK(host.calls == Calls{"play 5 71 6", "head 5"});

  // An action pose takes 0x3A.
  host.calls.clear();
  host.Add(6).motion.action_pose_active = true;
  actors::BeginActorDeath(host, 6, true);
  CHECK(host.calls == Calls{"play 6 58 6", "head 6"});
}

TEST_CASE("Airborne Ashley keeps her rising fall speed", "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& ashley = host.Add(0);
  ashley.motion.movement_mode = 3;
  ashley.motion.root_motion.y = 4;
  actors::BeginActorDeath(host, 0, false);
  CHECK(ashley.motion.root_motion.y == 4);
  CHECK(ashley.motion.action_animation_state == 2);
  ashley.motion.root_motion.y = 0;
  actors::BeginActorDeath(host, 0, false);
  CHECK(ashley.motion.root_motion.y == -6);

  actors::ActorStateRecord& locked = host.Add(2);
  locked.motion.motion_locked = true;
  locked.motion.root_motion.y = 4;
  actors::BeginActorDeath(host, 2, false);
  CHECK(locked.motion.root_motion.y == -6);
  CHECK(host.calls.empty());
}

TEST_CASE("Death passes to companions and breaks slot gimmicks",
          "[actors-state]") {
  FakeStateHost host;
  host.Add(2).companion_slot = 6;
  host.Add(6).companion_slot = 9;
  host.gimmicks[9] = {};
  actors::BeginActorDeath(host, 2, false);
  CHECK(host.At(2).motion.action_animation_state == 2);
  CHECK(host.At(6).motion.action_animation_state == 2);
  CHECK(host.gimmick_modes.at(9) == 0xF7);
  CHECK(host.calls == Calls{"gimmick-mode 9 247", "gimmick-support 9",
                            "gimmick-sound 9 50 0"});

  host.calls.clear();
  actors::BeginActorDeath(host, 10, true);
  CHECK(host.calls.empty());

  host.At(6).companion_slot = 2;
  const ChannelWarnings warnings("actors");
  actors::BeginActorDeath(host, 2, false);
  actors::BeginActorDeath(host, 20, false);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Death stops at a repeating companion chain from 2",
            "Death refused for actor 20 outside the actor table"});
}

TEST_CASE("A companion slot takes its style and slot 0 keeps it",
          "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = host.Add(3);
  const actors::CompanionStyle style{
      .offset = {.x = 2, .y = -2, .z = 2, .depth_bias = 16},
      .color = {1, 2, 3, 4}};
  actors::SetActorCompanion(host, 3, 7, style);
  CHECK(record.companion_slot == 7);
  CHECK(record.companion == style);
  actors::SetActorCompanion(host, 3, 0, std::nullopt);
  CHECK(record.companion_slot == 0);
  CHECK(record.companion == style);

  const ChannelWarnings warnings("actors");
  actors::SetActorCompanion(host, 3, 1, std::nullopt);
  CHECK(record.companion_slot == 1);
  CHECK(record.companion == style);
  actors::SetActorCompanion(host, 30, 1, style);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Companion 1 of actor 3 has no style; keeping it",
            "Companion refused for actor 30 outside the actor table"});
}

TEST_CASE("Attaching an actor to a parent bone clears its pose",
          "[actors-state]") {
  FakeStateHost host;
  host.Add(2).attached_child_count = 2;
  actors::ActorStateRecord& child = host.Add(5);
  child.attach_parent = 2;
  child.attach_bone = 1;
  child.motion.position = {.x = 1, .y = 2, .z = 3, .w = 4};
  child.motion.move_frames = 5;
  child.motion.rotation_x = 6;
  child.motion.facing = 7;
  child.motion.rotation_z = 8;
  child.rotation_reserved = 9;
  child.motion.turn_frames = 10;
  actors::ActorStateRecord expected = child;
  expected.attach_parent = 3;
  expected.attach_bone = 4;
  expected.motion.position = {};
  expected.motion.move_frames = 0;
  expected.motion.rotation_x = 0;
  expected.motion.facing = 0;
  expected.motion.rotation_z = 0;
  expected.rotation_reserved = 0;
  expected.motion.turn_frames = 0;
  expected.attached = true;
  actors::AttachActorToParentBone(host, 5,
                                  actors::AttachPoint{.parent = 3, .bone = 4});
  CHECK(host.At(2).attached_child_count == 1);
  CHECK(child == expected);

  // No attach point keeps the parent, and only the old parent's count drops.
  child.attached = false;
  actors::AttachActorToParentBone(host, 5, std::nullopt);
  CHECK(child.attach_parent == 3);
  CHECK(child.attached);
}

TEST_CASE("Attaching keeps a parent's zero child count and needs the actor",
          "[actors-state]") {
  FakeStateHost host;
  host.Add(2);
  host.Add(5).attach_parent = 2;
  const ChannelWarnings warnings("actors");
  actors::AttachActorToParentBone(host, 5,
                                  actors::AttachPoint{.parent = 1, .bone = 0});
  CHECK(host.At(2).attached_child_count == 0);
  CHECK(host.At(5).attach_parent == 1);
  actors::AttachActorToParentBone(host, 6, std::nullopt);
  actors::AttachActorToParentBone(host, 17, std::nullopt);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Actor 2 has no attached child to release", "No actor 6 to attach",
            "Attachment refused for actor 17 outside the actor table",
            "No actor 17 to attach"});
}

TEST_CASE("Releasing an attachment needs a parent with a skeleton",
          "[actors-state]") {
  using actors::AttachmentRelease;
  FakeStateHost host;
  actors::ActorStateRecord& child = host.Add(5);
  child.attached = true;
  CHECK(actors::ReleaseActorAttachmentLock(host, 5) ==
        AttachmentRelease::kNoParent);
  CHECK(actors::ReleaseActorAttachmentLock(host, 6) ==
        AttachmentRelease::kNoParent);
  child.attach_parent = 17;
  CHECK(actors::ReleaseActorAttachmentLock(host, 5) ==
        AttachmentRelease::kParentNotReady);
  child.attach_parent = 2;
  CHECK(actors::ReleaseActorAttachmentLock(host, 5) ==
        AttachmentRelease::kParentNotReady);
  host.Add(2);
  CHECK(actors::ReleaseActorAttachmentLock(host, 5) ==
        AttachmentRelease::kParentNotReady);
  CHECK(child.attached);
  host.At(2).bone_count = 12;
  CHECK(actors::ReleaseActorAttachmentLock(host, 5) ==
        AttachmentRelease::kReleased);
  CHECK_FALSE(child.attached);
}

TEST_CASE("Placement reads a record, then a slot gimmick", "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = host.Add(3);
  record.motion.current_tile = {.x = 7, .layer = 2, .z = 9, .spare = 4};
  record.motion.position = {.x = 10, .y = -20, .z = 30, .w = 99};
  record.motion.facing = 0x340;
  CHECK(actors::ActorPlacement(host, 3) ==
        actors::ActorPlacementView{
            .tile = {.x = 7, .layer = 2, .z = 9, .spare = 4},
            .position = {.x = 10, .y = -20, .z = 30, .w = 0x340}});

  host.gimmicks[4] = {.tile = {.x = 1, .layer = 0, .z = 2, .spare = 3},
                      .position = {.x = 4, .y = 5, .z = 6, .w = 7},
                      .facing = 0x8800};
  CHECK(actors::ActorPlacement(host, 4) ==
        actors::ActorPlacementView{
            .tile = {.x = 1, .layer = 0, .z = 2, .spare = 3},
            .position = {.x = 4, .y = 5, .z = 6, .w = -0x7800}});
  CHECK_FALSE(actors::ActorPlacement(host, 5).has_value());

  const ChannelWarnings warnings("actors");
  CHECK_FALSE(actors::ActorPlacement(host, 17).has_value());
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "No placement for actor 17 outside the actor table"});
}

namespace {
constexpr actors::PackedTile kRoomTile{.x = 5, .layer = 0, .z = 9, .spare = 12};
constexpr std::int16_t kTileX = 5 * 128 + 64;
constexpr std::int16_t kTileZ = 9 * 128 + 64;

actors::ActorStateRecord& Placeable(FakeStateHost& host, int id) {
  actors::ActorStateRecord& record = host.Add(id);
  record.motion.move_frames = 3;
  record.motion.facing_delta = 4;
  record.motion.turn_frames = 5;
  record.pending_target_x = 6;
  record.pending_target_z = 7;
  record.previous_target_x = 8;
  record.previous_target_z = 9;
  record.contact_kind = 2;
  record.motion.momentum_distance = 40;
  return record;
}

void CheckFinished(const actors::ActorStateRecord& record) {
  CHECK(record.motion.move_frames == 0);
  CHECK(record.motion.facing_delta == 0);
  CHECK(record.motion.turn_frames == 0);
  CHECK(record.pending_target_x == 0);
  CHECK(record.pending_target_z == 0);
  CHECK(record.previous_target_x == 0);
  CHECK(record.previous_target_z == 0);
  CHECK(record.motion.facing == 12 * 16);
  CHECK(record.contact_kind == 0);
}
}  // namespace

TEST_CASE("Room placement stands an actor on its tile's floor",
          "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = Placeable(host, 3);
  host.support_height = -100;
  actors::PlaceActorInRoom(host, 3, kRoomTile, -500, 0);
  CHECK(record.motion.current_tile == kRoomTile);
  CHECK(record.motion.position ==
        actors::MotionVector{.x = kTileX, .y = -100, .z = kTileZ});
  CHECK_FALSE(record.motion.is_jumping);
  CheckFinished(record);
  actors::ActorMotion momentum = record.motion;
  actors::ResetActorRunMomentum(momentum);
  CHECK(momentum == record.motion);
  CHECK(host.calls == Calls{std::format("support {} {}", kTileX, kTileZ),
                            "play 3 1 0", "save 3"});

  for (const int movement_class :
       {actors::kLeapingClass, actors::kBoundingClass}) {
    Placeable(host, 4);
    actors::PlaceActorInRoom(host, 4, kRoomTile, 0, movement_class);
    CHECK(host.At(4).motion.is_jumping);
  }
}

TEST_CASE("Room placement drops an actor on the drop layer", "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = Placeable(host, 3);
  record.motion.root_motion.y = 9;
  actors::PackedTile tile = kRoomTile;
  tile.layer = actors::kDropPlacementLayer;
  actors::PlaceActorInRoom(host, 3, tile, -500, 0);
  CHECK(record.motion.current_tile.layer == 0);
  CHECK(record.motion.position.y == -500);
  CHECK(record.motion.root_motion.y == 0);
  CHECK(record.motion.movement_mode == 3);
  CheckFinished(record);
  CHECK(host.calls ==
        Calls{std::format("support {} {}", kTileX, kTileZ), "save 3"});
}

TEST_CASE("Room placement rides a moving platform layer", "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = Placeable(host, 3);
  host.platform = VS::Field::PlatformContact{
      .x = 10, .height = -20, .z = 30, .platform_index = 1};
  actors::PackedTile tile = kRoomTile;
  tile.layer = 0x13;
  actors::PlaceActorInRoom(host, 3, tile, 0, 0);
  CHECK(record.motion.position ==
        actors::MotionVector{.x = 10, .y = -20, .z = 30, .w = 1});
  CHECK(record.platform_anchor ==
        actors::MotionVector{.x = 10, .y = -20, .z = 30, .w = 0x13});
  CHECK(record.support_layer == 3);
  CHECK(record.placement_layer == 3);
  CheckFinished(record);
  CHECK(host.calls == Calls{std::format("support {} {}", kTileX, kTileZ),
                            "platform 19", "play 3 1 0", "save 3"});

  // A missing platform keeps the floor placement.
  host.platform.reset();
  host.support_height = -64;
  Placeable(host, 4);
  const ChannelWarnings warnings("actors");
  actors::PlaceActorInRoom(host, 4, tile, 0, 0);
  CHECK(host.At(4).motion.position ==
        actors::MotionVector{.x = kTileX, .y = -64, .z = kTileZ});
  CHECK(host.At(4).support_layer == 0);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Actor 4 is placed on missing moving platform layer 19; keeping "
            "it on the floor"});
}

TEST_CASE("Room placement lifts a flyer below the ceiling", "[actors-state]") {
  FakeStateHost host;
  host.support_height = -100;
  for (const auto& [ceiling, height] :
       {std::pair{0, -100 - 0x180}, std::pair{-1000, -1000},
        std::pair{-150, -100 - 0x80}}) {
    host.calls.clear();
    host.ceiling = ceiling;
    actors::ActorStateRecord& record = Placeable(host, 3);
    actors::PlaceActorInRoom(host, 3, kRoomTile, 0, actors::kFlyingClass);
    CHECK(record.motion.position.y == height);
    CHECK(record.motion.motion_locked);
    CheckFinished(record);
    CHECK(host.calls == Calls{std::format("support {} {}", kTileX, kTileZ),
                              "play 3 8 0", "save 3"});
  }

  // A hovering actor stacks above an earlier flyer at the same spot.
  host.ceiling = 0;
  actors::PlaceActorInRoom(host, 3, kRoomTile, 0, actors::kFlyingClass);
  REQUIRE(host.At(3).motion.position.y == -100 - 0x180);
  Placeable(host, 4);
  actors::PlaceActorInRoom(host, 4, kRoomTile, 0, actors::kHoveringClass);
  CHECK(host.At(4).motion.position.y == -100 - 0x180 - 0xC0);
}

TEST_CASE("A flight position is held by an earlier locked flyer",
          "[actors-state]") {
  FakeStateHost host;
  const actors::MotionVector spot{.x = 1, .y = 2, .z = 3, .w = 4};
  host.Add(1).motion = {.motion_locked = true, .position = spot};
  host.Add(3).motion = {.position = spot};
  host.Add(5).motion = {.motion_locked = true,
                        .position = {.x = 1, .y = 2, .z = 3, .w = 9}};
  host.Add(6).motion = {.motion_locked = true,
                        .position = {.x = 1, .y = 2, .z = 4}};
  CHECK(actors::FindActorAtFlightPosition(host, 5, spot) == std::nullopt);
  CHECK(actors::FindActorAtFlightPosition(host, 6, spot) == 5);
  CHECK(actors::FindActorAtFlightPosition(host, 16, spot) == 5);

  const ChannelWarnings warnings("actors");
  CHECK(actors::FindActorAtFlightPosition(host, 30, spot) == 5);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Flight placement of actor 30 checks only the table"});
}

TEST_CASE("Flight separation climbs over stacked flyers", "[actors-state]") {
  FakeStateHost host;
  host.Add(2).motion = {.motion_locked = true,
                        .position = {.x = 1, .y = -0xC0, .z = 3}};
  host.Add(3).motion = {.motion_locked = true,
                        .position = {.x = 1, .y = -0x180, .z = 3}};
  host.Add(4);
  actors::MotionVector position{.x = 1, .y = 0, .z = 3};
  actors::SeparateActorFlightPlacement(host, 4, position);
  CHECK(position.y == -0x240);

  // Under a ceiling the actor retries half a step once, then stays put.
  host.ceiling = -400;
  position = {.x = 50, .y = -250, .z = 3};
  actors::SeparateActorFlightPlacement(host, 4, position);
  CHECK(position.y == -250 - 0x60);
  host.ceiling = -300;
  position = {.x = 50, .y = -250, .z = 3};
  actors::SeparateActorFlightPlacement(host, 4, position);
  CHECK(position.y == -250);
  host.At(4).collision_height = 100;
  host.ceiling = -400;
  position = {.x = 50, .y = -250, .z = 3};
  actors::SeparateActorFlightPlacement(host, 4, position);
  CHECK(position.y == -250);

  const ChannelWarnings warnings("actors");
  actors::SeparateActorFlightPlacement(host, 5, position);
  actors::SeparateActorFlightPlacement(host, 40, position);
  CHECK(position.y == -250);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Flight separation refused for actor 40 outside the actor table"});
}

TEST_CASE("The saved placement carries the facing in sixteenths",
          "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& record = host.Add(3);
  record.saved_placement = {.x = 1, .layer = 2, .z = 3, .spare = 4};
  record.motion.current_tile = {.x = 5, .layer = 6, .z = 7, .spare = 8};
  record.motion.facing = 0x345;
  CHECK(actors::ActorSavedPlacement(host, 3, 0) ==
        actors::PackedTile{.x = 1, .layer = 2, .z = 3, .spare = 0x34});
  CHECK(actors::ActorSavedPlacement(host, 3, actors::kFlyingClass) ==
        actors::PackedTile{.x = 1, .layer = 2, .z = 3, .spare = 0x34});
  record.motion.motion_locked = true;
  record.motion.facing = -40;
  CHECK(actors::ActorSavedPlacement(host, 3, actors::kHoveringClass) ==
        actors::PackedTile{.x = 5, .layer = 6, .z = 7, .spare = 0xFE});
  CHECK(actors::ActorSavedPlacement(host, 3, actors::kLeapingClass) ==
        actors::PackedTile{.x = 1, .layer = 2, .z = 3, .spare = 0xFE});
  CHECK_FALSE(actors::ActorSavedPlacement(host, 4, 0).has_value());
  CHECK_FALSE(actors::ActorSavedPlacement(host, 40, 0).has_value());
}

TEST_CASE("The nearest actor ahead is the closest shown one in range",
          "[actors-state]") {
  FakeStateHost host;
  actors::ActorStateRecord& self = host.Add(2);
  const actors::MotionVector front{.x = 0, .y = 0, .z = 300};
  const actors::MotionVector back{.x = 0, .y = 0, .z = -300};
  // Pick whichever side the facing convention puts ahead.
  const bool front_ahead = std::abs(actors::ActorFacingDeltaToPoint(
                               self.motion, &front)) <= actors::kAngleOne / 4;
  const std::int16_t ahead = front_ahead ? 1 : -1;
  auto at = [&](int distance, int height = 0) {
    return actors::MotionVector{
        .x = 0,
        .y = static_cast<std::int16_t>(height),
        .z = static_cast<std::int16_t>(ahead * distance)};
  };
  host.Add(0).motion.position = at(500);
  host.Add(4).motion.position = at(300);
  host.Add(5).motion.position = at(-100);
  actors::ActorStateRecord& hidden = host.Add(6);
  hidden.motion.position = at(100);
  hidden.skipped = true;
  host.Add(7).motion.position = at(200, 80);
  CHECK(actors::FindNearestActorAhead(
            host, 2, {.horizontal = 1000, .vertical = 50}) == 4);
  CHECK(actors::FindNearestActorAhead(
            host, 2, {.horizontal = 1000, .vertical = 100}) == 7);
  CHECK(actors::FindNearestActorAhead(
            host, 2, {.horizontal = 250, .vertical = 50}) == std::nullopt);
  // Ranges past the original's 32-bit squares still reach.
  host.At(4).motion.position = at(30000);
  host.At(0).motion.position = at(31000);
  CHECK(actors::FindNearestActorAhead(
            host, 2, {.horizontal = 50000, .vertical = 50}) == 4);

  CHECK(actors::FindNearestActorAhead(
            host, 3, {.horizontal = 1000, .vertical = 50}) == std::nullopt);
  const ChannelWarnings warnings("actors");
  CHECK(actors::FindNearestActorAhead(
            host, -1, {.horizontal = 1000, .vertical = 50}) == std::nullopt);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Nearest actor refused for actor -1 outside the actor table"});
}

namespace {
// Saves the live tables the actor state bindings touch and clears them for
// the test.
struct LiveStateFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Actors_weaponModels)) saved_weapons =
      std::to_array(Actors_weaponModels);
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  decltype(std::to_array(Battle_actorMarkers)) saved_markers =
      std::to_array(Battle_actorMarkers);

  LiveStateFixture() {
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_records.size(), nullptr);
    std::fill_n(Actors_weaponModels, saved_weapons.size(), nullptr);
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    std::fill_n(Battle_actorMarkers, saved_markers.size(), 0);
  }
  LiveStateFixture(const LiveStateFixture&) = delete;
  LiveStateFixture& operator=(const LiveStateFixture&) = delete;
  LiveStateFixture(LiveStateFixture&&) = delete;
  LiveStateFixture& operator=(LiveStateFixture&&) = delete;
  ~LiveStateFixture() {
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_records, Actors_actorRecords);
    std::ranges::copy(saved_weapons, Actors_weaponModels);
    std::ranges::copy(saved_gimmicks, Field_gimmickInstances);
    std::ranges::copy(saved_markers, Battle_actorMarkers);
  }
};

std::unique_ptr<Actors_ActorRecord> LiveRecord(int slot) {
  auto record = std::make_unique<Actors_ActorRecord>();
  record->instance.slot = static_cast<u_char>(slot);
  record->instance.attachParent = 0xFF;
  Actors_actorRecords[slot] = record.get();
  return record;
}

// A placement's x, y, z and facing.
std::array<int, 4> Position(const SVECTOR& position) {
  return {position.vx, position.vy, position.vz, position.pad};
}

// A live record's companion offset (x, y, z, depth bias) and colour.
std::array<int, 8> LiveCompanionStyle(const Actors_ActorRecord& record) {
  const Actors_CompanionOffset& offset = record.companionState;
  const Render_ColorCode& color = record.companionColor;
  return {offset.offsetX, offset.offsetY, offset.offsetZ, offset.depthBias,
          color.red,      color.green,    color.blue,     color.code};
}

// A hostile battle actor at 30 of 120 HP with model 0x44, in slot 3.
void PlaceLiveBattleActor(Battle_Actor& actor, Battle_ActorStats& stats) {
  stats.currentHp = 30;
  stats.maxHp = 120;
  actor.stats = &stats;
  actor.kind = actors::kHostileActorKind;
  actor.modelId = 0x44;
  actor.retainAfterDefeat = 1;
  Battle_actors[3] = &actor;
}

// A battle actor's command state and kind.
std::array<int, 2> LogicState(const Battle_Actor& actor) {
  return {actor.commandState, actor.kind};
}
}  // namespace

TEST_CASE("Live display flags round-trip the record and weapon bits",
          "[actors-state]") {
  const LiveStateFixture fixture;
  const std::unique_ptr<Actors_ActorRecord> record = LiveRecord(3);
  record->instance.isFrozen = -1;
  Actors_WeaponModel first{};
  Actors_WeaponModel second{};
  Actors_weaponModels[6] = &first;
  Actors_weaponModels[7] = &second;
  second.reserved08Bit4 = 1;

  VS::Actors::SetLiveActorDisplayFlags(
      3, static_cast<int>(StateFlag::kShown | StateFlag::kVisible |
                          StateFlag::kSoundEnabled |
                          StateFlag::kFirstWeaponShown));
  CHECK(record->instance.isSkipped == 0);
  CHECK(record->instance.isVisible != 0);
  CHECK(record->instance.isFrozen == 0);
  CHECK(record->instance.soundEnabled == 1);
  CHECK(std::array<int, 2>{first.reserved08Bit4, second.reserved08Bit4} ==
        std::array{1, 0});
  CHECK(VS::Actors::LiveActorStateFlags(3) ==
        (StateFlag::kActorRecord | StateFlag::kShown | StateFlag::kVisible |
         StateFlag::kSoundEnabled | StateFlag::kFirstWeaponShown));
  // The rest of the record is left alone.
  CHECK(record->instance.attachParent == 0xFF);
  CHECK(record->instance.slot == 3);
}

TEST_CASE("Live model switches need a battle actor and a slot",
          "[actors-state]") {
  const LiveStateFixture fixture;
  const std::unique_ptr<Actors_ActorRecord> record = LiveRecord(2);
  Battle_Actor actor{};
  Battle_actors[2] = &actor;
  VS::Actors::DisableLiveActorModel(2);
  CHECK(record->instance.isSkipped != 0);
  VS::Actors::EnableAllLiveActorModels();
  CHECK(record->instance.isSkipped == 0);

  const ChannelWarnings warnings("actors");
  VS::Actors::EnableLiveActorModel(-1);
  VS::Actors::DisableLiveActorModel(-2);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("EnableActorModel", "the actor id is negative"),
            Refused("DisableActorModel", "the actor id is negative")});
}

TEST_CASE("Live placement writes the tile, position and facing",
          "[actors-state]") {
  const LiveStateFixture fixture;
  const std::unique_ptr<Actors_ActorRecord> record = LiveRecord(3);
  record->instance.currentTileX = 7;
  record->instance.currentTileLayer = 2;
  record->instance.currentTileZ = 9;
  record->instance.reserved5F = 4;
  record->instance.position = {10, -20, 30, 99};
  record->instance.facing = 0x340;
  Actors_ActorLocationQuery query{};
  query.actorIndex = 11;
  CHECK(VS::Actors::LiveActorPlacement(3, &query) == 0);
  CHECK(query.location.packedTile.word == 0x04090207);
  CHECK(Position(query.location.position) == std::array{10, -20, 30, 0x340});
  CHECK(query.actorIndex == 11);

  const ChannelWarnings warnings("actors");
  CHECK(VS::Actors::LiveActorPlacement(
            3, static_cast<Actors_ActorLocation*>(nullptr)) == -1);
  CHECK(warnings.messages == std::vector<std::string>{Refused(
                                 "ActorPlacement", "no placement output")});
}

TEST_CASE("Live placement falls back to a slot gimmick", "[actors-state]") {
  const LiveStateFixture fixture;
  Field_GimmickInstance gimmick{};
  gimmick.tileX = 1;
  gimmick.tileZ = 2;
  gimmick.positionX = 4;
  gimmick.positionY = 5;
  gimmick.positionZ = 6;
  gimmick.rotationY = 0x100;
  Field_gimmickInstances[4] = &gimmick;
  Actors_ActorLocation location{};
  CHECK(VS::Actors::LiveActorPlacement(4, &location) == 0);
  CHECK(location.packedTile.word == 0x00020001);
  CHECK(Position(location.position) == std::array{4, 5, 6, 0x100});
  CHECK(VS::Actors::LiveActorPlacement(5, &location) == -1);
}

TEST_CASE("Live saved placement writes only the packed tile",
          "[actors-state]") {
  const LiveStateFixture fixture;
  const std::unique_ptr<Actors_ActorRecord> record = LiveRecord(3);
  record->savedPlacement = 0x00030A07;
  record->instance.facing = 0x340;
  Actors_ActorLocationQuery query{};
  query.location.position.vx = 77;
  VS::Actors::LiveActorSavedPlacement(3, 0, &query);
  CHECK(query.location.packedTile.word == 0x34030A07);
  CHECK(query.location.position.vx == 77);
  query.location.packedTile.word = 5;
  VS::Actors::LiveActorSavedPlacement(4, 0, &query);
  CHECK(query.location.packedTile.word == 5);
}

TEST_CASE("Live battle actors answer the state queries", "[actors-state]") {
  const LiveStateFixture fixture;
  Battle_ActorStats stats{};
  Battle_Actor actor{};
  PlaceLiveBattleActor(actor, stats);

  CHECK(VS::Actors::LiveActorHpPercentage(3) == 25);
  CHECK(VS::Actors::FindLiveActorByModelId(0x44) == 3);
  CHECK(VS::Actors::CountLiveActorsByDisposition(actors::kHostileActorKind) ==
        1);
  CHECK(VS::Actors::IsLiveActorRetainedAfterDefeat(3) == 1);
  CHECK(VS::Actors::IsLiveActorRetainedAfterDefeat(4) == 0);
  CHECK(VS::Actors::LiveActorAtSlot(3) == &actor);

  const ChannelWarnings warnings("actors");
  CHECK(VS::Actors::LiveActorAtSlot(42) == nullptr);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("ActorAtSlot", "the actor id is outside Battle_actors")});
}

TEST_CASE("Live battle actors switch their logic and hostility",
          "[actors-state]") {
  const LiveStateFixture fixture;
  Battle_ActorStats stats{};
  Battle_Actor actor{};
  PlaceLiveBattleActor(actor, stats);

  Battle_actorMarkers[3] = static_cast<char>(0x85);
  CHECK(VS::Actors::SetLiveActorLogicEnabled(3, 0) == 0);
  CHECK(LogicState(actor) ==
        std::array<int, 2>{actors::kLogicDisabledCommand, 0x8004});
  CHECK(Battle_actorMarkers[3] == 0);
  CHECK(VS::Actors::SetLiveActorLogicEnabled(3, 1) == 1);
  CHECK(LogicState(actor) == std::array<int, 2>{0, actors::kHostileActorKind});
  VS::Actors::SetLiveActorHostile(3, 0);
  CHECK(actor.kind == actors::kPeacefulActorKind);
  // Only the written fields change.
  CHECK(std::array<int, 2>{actor.modelId, stats.currentHp} ==
        std::array{0x44, 30});
}

TEST_CASE("A live companion takes its offset and colour", "[actors-state]") {
  const LiveStateFixture fixture;
  const std::unique_ptr<Actors_ActorRecord> record = LiveRecord(3);
  const Render_ColorCode offset{2, static_cast<u_char>(-2), 2, 16};
  const Render_ColorCode color{10, 20, 30, 40};
  constexpr std::array kStyle{2, -2, 2, 16, 10, 20, 30, 40};
  VS::Actors::SetLiveActorCompanion(3, 5, &offset, &color);
  CHECK(record->companionSlot == 5);
  CHECK(LiveCompanionStyle(*record) == kStyle);
  VS::Actors::SetLiveActorCompanion(3, 0, nullptr, nullptr);
  CHECK(record->companionSlot == 0);
  CHECK(LiveCompanionStyle(*record) == kStyle);

  // A slot that does not fit the record's byte changes nothing.
  const ChannelWarnings warnings("actors");
  constexpr int kSlotPastByte = 256;
  VS::Actors::SetLiveActorCompanion(3, kSlotPastByte, &color, &offset);
  CHECK(record->companionSlot == 0);
  CHECK(LiveCompanionStyle(*record) == kStyle);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("SetActorCompanion", "the companion slot is not a byte")});
}
