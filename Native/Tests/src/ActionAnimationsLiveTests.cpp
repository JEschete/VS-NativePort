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

#include "Actors/SequencePlaybackLive.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Content/ModuleData.h"
#include "Render/ModelGeometryLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
}
namespace {
namespace b = VS::Battle;
namespace a = VS::Actors;
constexpr int kSlot = 16;
constexpr int kAbility = 17;

struct Model {
  Actors_ShpModel header{};
  Actors_ShpBone bone{};
};

struct LiveFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  MATRIX saved_bone_camera = Actors_boneMatrixScratch;
  void* saved_work_bank = Actors_sequenceWorkBuffer;
  int saved_side = Battle_chainAbilityAnimationSide;
  char saved_pitch = Battle_specialActionPitch;
  char saved_clip = Actors_lastResolvedSequenceIndex;
  Battle_ActionState* saved_action = Battle_actionState;
  Battle_ActionState action{};
  u_char saved_group = Battle_chainAbilityAnimationGroups[kAbility];
  Model model{};
  std::array<std::uint32_t, 1024> sequence{};
  Actors_ActorRecord actor{};

  LiveFixture() {
    std::ranges::fill(Actors_actorRecords, nullptr);
    Actors_actorRecords[kSlot] = &actor;
    actor.instance.slot = kSlot;
    actor.instance.boneCount = 1;
    actor.instance.model = &model.header;
    actor.modelDataSize = sizeof(model);
    VS::Render::RecordLiveModelExtent(&model, sizeof(model));
    actor.instance.turnFrames = 777;
    actor.instance.facingDelta = 333;
    actor.instance.jumpCountdown = 99;
    actor.sequenceModelId = 1;
    // LiveAnimationBankLimits reads these legacy bytes as
    // common/battle/special.
    actor.sequenceFlags = 100;
    actor.commonAnimationCount = 102;
    actor.battleAnimationCount = 64;
    actor.collisionRadius = 127;
    actor.modelId = 40;
    actor.headTracking.pad = 71;
    actor.commonSequence = Bank();
    actor.battleSequence = Bank();
    actor.ownedSequence = Bank();
    actor.sharedSequences[3] = Bank();
    auto* bank = Bank();
    bank->animationMap = 1024;
    bank->animationData = 1536;
    bank->headers[0] = 24;
    bank->headers[1] = 0xFF;
    auto* motion = reinterpret_cast<u_char*>(bank) + bank->animationData;
    const std::array<u_char, 6> rotation{0, 16, 0, 32, 0, 48};
    std::ranges::copy(rotation, motion);
    a::RegisterSequenceBank(bank, sizeof(sequence));
    Actors_sequenceWorkBuffer = bank;
    Actors_boneMatrixScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      Actors_boneMatrixScratch.m[axis][axis] = 4096;
      actor.boneMatrices[0].m[axis][axis] = 4096;
    }
    Battle_chainAbilityAnimationGroups[kAbility] = 2;
    Battle_chainAbilityAnimationSide = 0;
    Battle_specialActionPitch = 2;
    Battle_actionState = &action;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  ~LiveFixture() {
    a::ForgetSequenceBanks(sequence.data(), sizeof(sequence));
    VS::Render::ForgetLiveModelExtent(&model);
    std::ranges::copy(saved_records, std::begin(Actors_actorRecords));
    Actors_boneMatrixScratch = saved_bone_camera;
    Actors_sequenceWorkBuffer = saved_work_bank;
    Actors_lastResolvedSequenceIndex = saved_clip;
    Battle_chainAbilityAnimationGroups[kAbility] = saved_group;
    Battle_chainAbilityAnimationSide = saved_side;
    Battle_specialActionPitch = saved_pitch;
    Battle_actionState = saved_action;
  }
  Actors_SeqAnimationBank* Bank() {
    return reinterpret_cast<Actors_SeqAnimationBank*>(sequence.data());
  }
  void Missing(int index) {
    auto* bytes = reinterpret_cast<u_char*>(sequence.data());
    bytes[Bank()->animationMap + index] = 0xFF;
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
  const Content_ModulePointerTable table{"BATTLE/BATTLE.PRG", 0, 6,
                                         relocated.data(),    6, nullptr};
  REQUIRE(VS::Content::RelocateModulePointerTable(
      table, image,
      std::span(Content_moduleSymbols, Content_moduleSymbolCount)));
  CHECK(relocated == expected);
}

TEST_CASE(
    "F00817 real bone height and playback initialize the selected melee pose",
    "[action-animations][F00817][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  actor.collisionRadius = 129;
  actor.instance.position.vy = -2000;
  actor.boneMatrices[0].t[1] = 800;
  SVECTOR point{1024, 400, 0, 77};
  b::PlayLiveMeleeAttackAnimation(kSlot, &point, 0);
  CHECK(actor.animationId == 0x48);
  CHECK(actor.instance.facingDelta == -1024);
  CHECK(actor.instance.turnFrames == 20);
  CHECK(actor.headTracking.pad == 20);
  CHECK(actor.blendFrames == 12);
  CHECK(actor.pose.boneKeys[0].vx == 32);
  CHECK(actor.animationLength == 24);
}

TEST_CASE("F00818 actual slot zero cleanup preserves the unrelated live flags",
          "[action-animations][F00818][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  Actors_actorRecords[0] = &actor;
  actor.instance.slot = 0;
  actor.instance.weaponDrawn = 1;
  actor.instance.motionLocked = 1;
  actor.instance.isJumping = 1;
  actor.instance.reactionState = 13;
  actor.instance.movementMode = 2;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveDefenseAnimation(kSlot, &point);
  CHECK(actor.animationId == 0x50 + 100);
  CHECK(actor.instance.movementMode == 0);
  CHECK(actor.instance.jumpCountdown == 0);
  CHECK(actor.instance.motionLocked == 1);
  CHECK(actor.instance.isJumping == 1);
  CHECK(actor.instance.reactionState == 13);
  CHECK(actor.instance.turnFrames == 4);
  CHECK(actor.blendFrames == 8);
}

TEST_CASE("F00818 wide production defense keeps the preexisting turn",
          "[action-animations][F00818][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  actor.collisionRadius = 128;
  actor.instance.weaponDrawn = 1;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveDefenseAnimation(kSlot, &point);
  CHECK(actor.animationId == 0x50 + 100);
  CHECK(actor.instance.turnFrames == 777);
  CHECK(actor.instance.facingDelta == 333);
  CHECK(actor.blendFrames == 8);
  CHECK(actor.headTracking.pad == 30);
}

TEST_CASE("F00823 actual missing bank-map entry selects the initial fallback",
          "[action-animations][F00823][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->Missing(0x55);
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveSpellCastAnimation(kSlot, &point, 0);
  CHECK(fixture->actor.animationId == 0x57);
  CHECK(fixture->actor.instance.facingDelta == -1024);
  CHECK(fixture->actor.instance.turnFrames == 10);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
  CHECK(fixture->actor.headTracking.pad == 30);
}

TEST_CASE("F00823 drawn phase one queries the real battle bank without turning",
          "[action-animations][F00823][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.instance.weaponDrawn = 1;
  fixture->Missing(0x58);
  b::PlayLiveSpellCastAnimation(kSlot, nullptr, 1);
  CHECK(fixture->actor.animationId == 0x59 + 100);
  CHECK(fixture->actor.instance.facingDelta == 333);
  CHECK(fixture->actor.instance.turnFrames == 777);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
  CHECK(fixture->actor.animationLength == 24);
}

TEST_CASE(
    "F00831 full-word side chooses a real work-bank clip without resetting the "
    "head",
    "[action-animations][F00831][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  Battle_chainAbilityAnimationSide = -2;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveChainAbilityAnimation(kSlot, &point, kAbility, 0x10001);
  CHECK(Battle_chainAbilityAnimationSide == -1);
  CHECK(fixture->actor.animationId == 215);
  CHECK(fixture->actor.instance.facingDelta == -1024);
  CHECK(fixture->actor.instance.turnFrames == 10);
  CHECK(fixture->actor.headTracking.pad == 71);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
}

TEST_CASE(
    "F00833 mode zero uses real BF availability and its distinct radius "
    "boundary",
    "[action-animations][F00833][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.collisionRadius = 128;
  fixture->Missing(0xBF - 100);
  b::PlayLiveSpecialActionAnimation(kSlot, nullptr, 0);
  CHECK(fixture->actor.animationId == 0xC1);
  CHECK(fixture->actor.instance.turnFrames == 20);
  CHECK(fixture->actor.instance.facingDelta == 0);
  CHECK(fixture->actor.headTracking.pad == 30);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
}

TEST_CASE(
    "F00833 real FF lookup stores the bone-relative pitch before pose playback",
    "[action-animations][F00833][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.boneMatrices[0].t[1] = 800;
  SVECTOR point{1024, 400, 0, 77};
  b::PlayLiveSpecialActionAnimation(kSlot, &point, 1);
  CHECK(Battle_specialActionPitch == 0);
  CHECK(fixture->actor.animationId == 0xFF);
  CHECK(fixture->actor.instance.turnFrames == 777);
  CHECK(fixture->actor.instance.facingDelta == 333);
  CHECK(fixture->actor.headTracking.pad == 71);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
}

TEST_CASE(
    "F00833 real absent FF falls back without the model40 special melee "
    "override",
    "[action-animations][F00833][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.battleAnimationCount = 0;
  fixture->actor.collisionRadius = 129;
  SVECTOR point{1024, 5000, 0, 77};
  b::PlayLiveSpecialActionAnimation(kSlot, &point, 1);
  CHECK(fixture->actor.animationId == 0x4A);
  CHECK(fixture->actor.instance.turnFrames == 20);
  CHECK(fixture->actor.headTracking.pad == 20);
  CHECK(Battle_specialActionPitch == 2);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
}

TEST_CASE("F00834 actual special playback precedes real FE queue fields",
          "[action-animations][F00834][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  b::PlayLiveBreakArtAnimation(kSlot, nullptr, 1);
  CHECK(fixture->actor.animationId == 0xFF);
  CHECK(fixture->actor.queuedAnimation == 0xFE);
  CHECK(fixture->actor.queuedParameterA == 8);
  CHECK(fixture->actor.queuedParameterB == 0);
  CHECK(fixture->actor.headTracking.pad == 71);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
}

TEST_CASE("F00834 production mode zero never queues the available FE clip",
          "[action-animations][F00834][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  b::PlayLiveBreakArtAnimation(kSlot, nullptr, 0);
  CHECK(fixture->actor.animationId == 0xBF);
  CHECK(fixture->actor.queuedAnimation == 0);
  CHECK(fixture->actor.headTracking.pad == 30);
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
  const Content_ModulePointerTable table{"BATTLE/BATTLE.PRG", 0, 3,
                                         relocated.data(),    3, nullptr};
  REQUIRE(VS::Content::RelocateModulePointerTable(
      table, image,
      std::span(Content_moduleSymbols, Content_moduleSymbolCount)));
  CHECK(relocated == expected);
}

TEST_CASE(
    "F00819 real playback copies the complete native pose including padding",
    "[action-reactions][F00819][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  std::ranges::fill(std::as_writable_bytes(std::span(&actor.pose, 1)),
                    std::byte{0x5A});
  actor.pose.eventScript = nullptr;
  actor.pose.nextEventFrame = 0;
  actor.pose.eventWait = 0;
  actor.pose.frame = 0;
  actor.pose.holdingLastFrame = 0;
  actor.pose.reachedEnd = 0;
  using PoseBytes = std::array<std::byte, sizeof(Actors_AnimationPose)>;
  b::SetLiveEnemyCombatPose(kSlot, 1);
  CHECK(actor.animationId == 0xC6);
  CHECK(actor.pose.boneKeys[0].vx == 32);
  CHECK(std::bit_cast<PoseBytes>(actor.blendPose) ==
        std::bit_cast<PoseBytes>(actor.pose));
  CHECK(actor.pose.reserved54C == 0x5A5A5A5A);
  CHECK(actor.pose.reserved2A0[41].pad == 0);
}

TEST_CASE("F00819 real missing finish header falls back to the idle clip",
          "[action-reactions][F00819][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.animationId = 0xC6;
  fixture->Missing(0xC7 - 100);
  b::SetLiveEnemyCombatPose(kSlot, 3);
  CHECK(fixture->actor.animationId == 6);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
  CHECK(fixture->actor.blendFrames == 16);
  CHECK(fixture->actor.instance.turnFrames == 777);
}

TEST_CASE(
    "F00832 reuses production defense playback before the real empty tail",
    "[action-reactions][F00832][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->actor.instance.weaponDrawn = 1;
  SVECTOR point{1024, 0, 0, 77};
  b::PlayLiveDefenseAbilityAnimation(kSlot, &point, 0x10000);
  CHECK(fixture->actor.animationId == 0x50 + 100);
  CHECK(fixture->actor.pose.boneKeys[0].vx == 32);
  CHECK(fixture->actor.instance.turnFrames == 4);
  CHECK(fixture->actor.headTracking.pad == 30);
  CHECK(fixture->actor.instance.jumpCountdown == 99);
}

TEST_CASE(
    "F00891 uses the production turn service and actual finish-pose playback",
    "[action-reactions][F00891][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& actor = fixture->actor;
  actor.animationId = 0xC8;
  actor.turnRate = 99;
  actor.instance.wasDrawn = 1;
  actor.instance.reactionState = 13;
  actor.instance.poseState = 1;
  b::FinishLiveEnemyCombatPose(kSlot, 1024);
  CHECK(actor.instance.facingDelta == 1024);
  CHECK(actor.instance.turnFrames == 2);
  CHECK(actor.turnRate == 99);
  CHECK(actor.animationId == 0xC9);
  CHECK(actor.pose.boneKeys[0].vx == 32);
  CHECK(actor.instance.poseState == 6);
  CHECK(actor.instance.wasDrawn == 1);
  CHECK(actor.instance.reactionState == 13);
}
}  // namespace
