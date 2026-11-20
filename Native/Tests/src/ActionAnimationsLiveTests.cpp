#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionAnimation.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Content/ModuleData.hpp"
#include "Render/ModelGeometryLive.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SVECTOR;

using VS::Battle::ActionState;

namespace {
namespace b = VS::Battle;
namespace a = VS::Actors;
constexpr int kSlot = 16;
constexpr int kAbility = 17;

struct Model {
  VS::Actors::ShpModel header{};
  VS::Actors::ShpBone bone{};
};

struct LiveFixture {
  decltype(VS::Actors::actorRecords) saved_records = VS::Actors::actorRecords;
  MATRIX saved_bone_camera = VS::Actors::boneMatrixScratch;
  void* saved_work_bank = VS::Actors::sequenceWorkBuffer;
  int saved_side = VS::Battle::chainAbilityAnimationSide;
  char saved_pitch = VS::Battle::specialActionPitch;
  char saved_clip = VS::Actors::lastResolvedSequenceIndex;
  ActionState* saved_action = VS::Battle::actionState;
  ActionState action{};
  std::uint8_t saved_group = VS::Battle::chainAbilityAnimationGroups[kAbility];
  Model model{};
  std::array<std::uint32_t, 1024> sequence{};
  VS::Actors::ActorRecord actor{};

  LiveFixture() {
    std::ranges::fill(VS::Actors::actorRecords, nullptr);
    VS::Actors::actorRecords[kSlot] = &actor;
    actor.instance.slot = kSlot;
    actor.instance.bone_count = 1;
    actor.instance.model = &model.header;
    actor.model_data_size = sizeof(model);
    VS::Render::RecordLiveModelExtent(&model, sizeof(model));
    actor.instance.turn_frames = 777;
    actor.instance.facing_delta = 333;
    actor.instance.jump_countdown = 99;
    actor.sequence_model_id = 1;
    // LiveAnimationBankLimits reads these legacy bytes as
    // common/battle/special.
    actor.sequence_flags = 100;
    actor.common_animation_count = 102;
    actor.battle_animation_count = 64;
    actor.collision_radius = 127;
    actor.model_id = 40;
    actor.head_tracking.pad = 71;
    actor.common_sequence = Bank();
    actor.battle_sequence = Bank();
    actor.owned_sequence = Bank();
    actor.shared_sequences[3] = Bank();
    auto* bank = Bank();
    bank->animation_map = 1024;
    bank->animation_data = 1536;
    bank->headers[0] = 24;
    bank->headers[1] = 0xFF;
    auto* motion = reinterpret_cast<std::uint8_t*>(bank) + bank->animation_data;
    const std::array<std::uint8_t, 6> rotation{0, 16, 0, 32, 0, 48};
    std::ranges::copy(rotation, motion);
    a::RegisterSequenceBank(bank, sizeof(sequence));
    VS::Actors::sequenceWorkBuffer = bank;
    VS::Actors::boneMatrixScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      VS::Actors::boneMatrixScratch.m[axis][axis] = 4096;
      actor.bone_matrices[0].m[axis][axis] = 4096;
    }
    VS::Battle::chainAbilityAnimationGroups[kAbility] = 2;
    VS::Battle::chainAbilityAnimationSide = 0;
    VS::Battle::specialActionPitch = 2;
    VS::Battle::actionState = &action;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  ~LiveFixture() {
    a::ForgetSequenceBanks(sequence.data(), sizeof(sequence));
    VS::Render::ForgetLiveModelExtent(&model);
    std::ranges::copy(saved_records, std::begin(VS::Actors::actorRecords));
    VS::Actors::boneMatrixScratch = saved_bone_camera;
    VS::Actors::sequenceWorkBuffer = saved_work_bank;
    VS::Actors::lastResolvedSequenceIndex = saved_clip;
    VS::Battle::chainAbilityAnimationGroups[kAbility] = saved_group;
    VS::Battle::chainAbilityAnimationSide = saved_side;
    VS::Battle::specialActionPitch = saved_pitch;
    VS::Battle::actionState = saved_action;
  }
  VS::Actors::SeqAnimationBank* Bank() {
    return reinterpret_cast<VS::Actors::SeqAnimationBank*>(sequence.data());
  }
  void Missing(int index) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(sequence.data());
    bytes[Bank()->animation_map + index] = 0xFF;
  }
};

using ThreeArguments = void (*)(int, SVECTOR*, int);
static_assert(
    std::is_same_v<decltype(&b::SetLiveEnemyCombatPose), void (*)(int, int)>);
static_assert(std::is_same_v<decltype(&b::PlayLiveDefenseAbilityAnimation),
                             ThreeArguments>);
static_assert(std::is_same_v<decltype(&b::FinishLiveEnemyCombatPose),
                             void (*)(int, std::int16_t)>);
static_assert(
    std::is_same_v<decltype(&b::PlayLiveMeleeAttackAnimation), ThreeArguments>);
static_assert(std::is_same_v<decltype(&b::PlayLiveDefenseAnimation),
                             void (*)(int, SVECTOR*)>);
static_assert(
    std::is_same_v<decltype(&b::PlayLiveSpellCastAnimation), ThreeArguments>);
static_assert(std::is_same_v<decltype(&b::PlayLiveChainAbilityAnimation),
                             void (*)(int, SVECTOR*, int, int)>);
static_assert(std::is_same_v<decltype(&b::PlayLiveSpecialActionAnimation),
                             ThreeArguments>);
static_assert(
    std::is_same_v<decltype(&b::PlayLiveBreakArtAnimation), ThreeArguments>);

TEST_CASE("Action animation module relocation binds all six original ABIs",
          "[action-animations][redirect-binding]") {
  const std::array<std::uint32_t, 6> addresses{
      0x8009DD00, 0x8009DDF4, 0x8009E2E0, 0x8009EC9C, 0x8009EFEC, 0x8009F298};
  const auto image =
      std::bit_cast<std::array<unsigned char, sizeof(addresses)>>(addresses);
  const std::array<void*, 6> expected{
      reinterpret_cast<void*>(&b::PlayLiveMeleeAttackAnimation),
      reinterpret_cast<void*>(&b::PlayLiveDefenseAnimation),
      reinterpret_cast<void*>(&b::PlayLiveSpellCastAnimation),
      reinterpret_cast<void*>(&b::PlayLiveChainAbilityAnimation),
      reinterpret_cast<void*>(&b::PlayLiveSpecialActionAnimation),
      reinterpret_cast<void*>(&b::PlayLiveBreakArtAnimation)};
  std::array<void*, 6> relocated{};
  const VS::Content::ModulePointerTable table{"BATTLE/BATTLE.PRG", 0, 6,
                                              relocated.data(),    6, nullptr};
  REQUIRE(VS::Content::RelocateModulePointerTable(
      table, image, VS::Content::ModuleSymbols()));
  CHECK(relocated == expected);
}

TEST_CASE(
    "F00817 real bone height and playback initialize the selected melee pose",
    "[action-animations][F00817][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  actor.collision_radius = 129;
  actor.instance.position.vy = -2000;
  actor.bone_matrices[0].t[1] = 800;
  SVECTOR point{1024, 400, 0, 77};
  b::PlayLiveMeleeAttackAnimation(kSlot, &point, 0);
  CHECK(actor.animation_id == 0x48);
  CHECK(actor.instance.facing_delta == -1024);
  CHECK(actor.instance.turn_frames == 20);
  CHECK(actor.head_tracking.pad == 20);
  CHECK(actor.blend_frames == 12);
  CHECK(actor.pose.bone_keys[0].vx == 32);
  CHECK(actor.animation_length == 24);
}

TEST_CASE("F00818 actual slot zero cleanup preserves the unrelated live flags",
          "[action-animations][F00818][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  VS::Actors::actorRecords[0] = &actor;
  actor.instance.slot = 0;
  actor.instance.weapon_drawn = 1;
  actor.instance.motion_locked = 1;
  actor.instance.is_jumping = 1;
  actor.instance.reaction_state = 13;
  actor.instance.movement_mode = 2;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveDefenseAnimation(kSlot, &point);
  CHECK(actor.animation_id == 0x50 + 100);
  CHECK(actor.instance.movement_mode == 0);
  CHECK(actor.instance.jump_countdown == 0);
  CHECK(actor.instance.motion_locked == 1);
  CHECK(actor.instance.is_jumping == 1);
  CHECK(actor.instance.reaction_state == 13);
  CHECK(actor.instance.turn_frames == 4);
  CHECK(actor.blend_frames == 8);
}

TEST_CASE("F00818 wide production defense keeps the preexisting turn",
          "[action-animations][F00818][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  actor.collision_radius = 128;
  actor.instance.weapon_drawn = 1;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveDefenseAnimation(kSlot, &point);
  CHECK(actor.animation_id == 0x50 + 100);
  CHECK(actor.instance.turn_frames == 777);
  CHECK(actor.instance.facing_delta == 333);
  CHECK(actor.blend_frames == 8);
  CHECK(actor.head_tracking.pad == 30);
}

TEST_CASE("F00823 actual missing bank-map entry selects the initial fallback",
          "[action-animations][F00823][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->Missing(0x55);
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveSpellCastAnimation(kSlot, &point, 0);
  CHECK(fixture->actor.animation_id == 0x57);
  CHECK(fixture->actor.instance.facing_delta == -1024);
  CHECK(fixture->actor.instance.turn_frames == 10);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
  CHECK(fixture->actor.head_tracking.pad == 30);
}

TEST_CASE("F00823 drawn phase one queries the real battle bank without turning",
          "[action-animations][F00823][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.instance.weapon_drawn = 1;
  fixture->Missing(0x58);
  b::PlayLiveSpellCastAnimation(kSlot, nullptr, 1);
  CHECK(fixture->actor.animation_id == 0x59 + 100);
  CHECK(fixture->actor.instance.facing_delta == 333);
  CHECK(fixture->actor.instance.turn_frames == 777);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
  CHECK(fixture->actor.animation_length == 24);
}

TEST_CASE(
    "F00831 full-word side chooses a real work-bank clip without resetting the "
    "head",
    "[action-animations][F00831][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  VS::Battle::chainAbilityAnimationSide = -2;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveChainAbilityAnimation(kSlot, &point, kAbility, 0x10001);
  CHECK(VS::Battle::chainAbilityAnimationSide == -1);
  CHECK(fixture->actor.animation_id == 215);
  CHECK(fixture->actor.instance.facing_delta == -1024);
  CHECK(fixture->actor.instance.turn_frames == 10);
  CHECK(fixture->actor.head_tracking.pad == 71);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
}

TEST_CASE(
    "F00833 mode zero uses real BF availability and its distinct radius "
    "boundary",
    "[action-animations][F00833][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.collision_radius = 128;
  fixture->Missing(0xBF - 100);
  b::PlayLiveSpecialActionAnimation(kSlot, nullptr, 0);
  CHECK(fixture->actor.animation_id == 0xC1);
  CHECK(fixture->actor.instance.turn_frames == 20);
  CHECK(fixture->actor.instance.facing_delta == 0);
  CHECK(fixture->actor.head_tracking.pad == 30);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
}

TEST_CASE(
    "F00833 real FF lookup stores the bone-relative pitch before pose playback",
    "[action-animations][F00833][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.bone_matrices[0].t[1] = 800;
  SVECTOR point{1024, 400, 0, 77};
  b::PlayLiveSpecialActionAnimation(kSlot, &point, 1);
  CHECK(VS::Battle::specialActionPitch == 0);
  CHECK(fixture->actor.animation_id == 0xFF);
  CHECK(fixture->actor.instance.turn_frames == 777);
  CHECK(fixture->actor.instance.facing_delta == 333);
  CHECK(fixture->actor.head_tracking.pad == 71);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
}

TEST_CASE(
    "F00833 real absent FF falls back without the model40 special melee "
    "override",
    "[action-animations][F00833][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.battle_animation_count = 0;
  fixture->actor.collision_radius = 129;
  SVECTOR point{1024, 5000, 0, 77};
  b::PlayLiveSpecialActionAnimation(kSlot, &point, 1);
  CHECK(fixture->actor.animation_id == 0x4A);
  CHECK(fixture->actor.instance.turn_frames == 20);
  CHECK(fixture->actor.head_tracking.pad == 20);
  CHECK(VS::Battle::specialActionPitch == 2);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
}

TEST_CASE("F00834 actual special playback precedes real FE queue fields",
          "[action-animations][F00834][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  b::PlayLiveBreakArtAnimation(kSlot, nullptr, 1);
  CHECK(fixture->actor.animation_id == 0xFF);
  CHECK(fixture->actor.queued_animation == 0xFE);
  CHECK(fixture->actor.queued_parameter_a == 8);
  CHECK(fixture->actor.queued_parameter_b == 0);
  CHECK(fixture->actor.head_tracking.pad == 71);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
}

TEST_CASE("F00834 production mode zero never queues the available FE clip",
          "[action-animations][F00834][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  b::PlayLiveBreakArtAnimation(kSlot, nullptr, 0);
  CHECK(fixture->actor.animation_id == 0xBF);
  CHECK(fixture->actor.queued_animation == 0);
  CHECK(fixture->actor.head_tracking.pad == 30);
}

TEST_CASE("Reaction module relocation binds the original VOID2 and VOID3 ABIs",
          "[action-reactions][redirect-binding]") {
  const std::array<std::uint32_t, 3> addresses{0x8009DF3C, 0x8009EE9C,
                                               0x800A2574};
  const auto image =
      std::bit_cast<std::array<unsigned char, sizeof(addresses)>>(addresses);
  const std::array<void*, 3> expected{
      reinterpret_cast<void*>(&b::SetLiveEnemyCombatPose),
      reinterpret_cast<void*>(&b::PlayLiveDefenseAbilityAnimation),
      reinterpret_cast<void*>(&b::FinishLiveEnemyCombatPose)};
  std::array<void*, 3> relocated{};
  const VS::Content::ModulePointerTable table{"BATTLE/BATTLE.PRG", 0, 3,
                                              relocated.data(),    3, nullptr};
  REQUIRE(VS::Content::RelocateModulePointerTable(
      table, image, VS::Content::ModuleSymbols()));
  CHECK(relocated == expected);
}

TEST_CASE(
    "F00819 real playback copies the complete native pose including padding",
    "[action-reactions][F00819][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  std::ranges::fill(std::as_writable_bytes(std::span(&actor.pose, 1)),
                    std::byte{0x5A});
  actor.pose.event_script = nullptr;
  actor.pose.next_event_frame = 0;
  actor.pose.event_wait = 0;
  actor.pose.frame = 0;
  actor.pose.holding_last_frame = 0;
  actor.pose.reached_end = 0;
  using PoseBytes = std::array<std::byte, sizeof(VS::Actors::AnimationPose)>;
  b::SetLiveEnemyCombatPose(kSlot, 1);
  CHECK(actor.animation_id == 0xC6);
  CHECK(actor.pose.bone_keys[0].vx == 32);
  CHECK(std::bit_cast<PoseBytes>(actor.blend_pose) ==
        std::bit_cast<PoseBytes>(actor.pose));
  CHECK(actor.pose.reserved54c == 0x5A5A5A5A);
  CHECK(actor.pose.rotation_deltas[41].pad == 0);
}

TEST_CASE("F00819 real missing finish header falls back to the idle clip",
          "[action-reactions][F00819][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.animation_id = 0xC6;
  fixture->Missing(0xC7 - 100);
  b::SetLiveEnemyCombatPose(kSlot, 3);
  CHECK(fixture->actor.animation_id == 6);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
  CHECK(fixture->actor.blend_frames == 16);
  CHECK(fixture->actor.instance.turn_frames == 777);
}

TEST_CASE(
    "F00832 reuses production defense playback before the real empty tail",
    "[action-reactions][F00832][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.instance.weapon_drawn = 1;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveDefenseAbilityAnimation(kSlot, &point, 0x10000);
  CHECK(fixture->actor.animation_id == 0x50 + 100);
  CHECK(fixture->actor.pose.bone_keys[0].vx == 32);
  CHECK(fixture->actor.instance.turn_frames == 4);
  CHECK(fixture->actor.head_tracking.pad == 30);
  CHECK(fixture->actor.instance.jump_countdown == 99);
}

TEST_CASE(
    "F00891 uses the production turn service and actual finish-pose playback",
    "[action-reactions][F00891][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  actor.animation_id = 0xC8;
  actor.turn_rate = 99;
  actor.instance.was_drawn = 1;
  actor.instance.reaction_state = 13;
  actor.instance.pose_state = 1;
  b::FinishLiveEnemyCombatPose(kSlot, 1024);
  CHECK(actor.instance.facing_delta == 1024);
  CHECK(actor.instance.turn_frames == 2);
  CHECK(actor.turn_rate == 99);
  CHECK(actor.animation_id == 0xC9);
  CHECK(actor.pose.bone_keys[0].vx == 32);
  CHECK(actor.instance.pose_state == 6);
  CHECK(actor.instance.was_drawn == 1);
  CHECK(actor.instance.reaction_state == 13);
}
}  // namespace
