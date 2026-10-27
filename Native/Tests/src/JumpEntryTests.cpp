#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/MotionRequests.h"
#include "Core/Log.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Event/ScriptVm.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
}

namespace {
using JumpEntry = void (*)(int, int*, int, int);
using ActorBytes = std::array<std::byte, sizeof(Actors_ActorRecord)>;
namespace Core = VS::Core;

struct JumpLogScope {
  Core::LogObserver saved = Core::SetLogObserver({});
  Core::LogLevel console = Core::SetConsoleLogLevel(Core::LogLevel::Off);
  ~JumpLogScope() {
    Core::SetLogObserver(std::move(saved));
    Core::SetConsoleLogLevel(console);
    Core::SetLogLevel(Core::LogLevel::Info);
    Core::SetLogChannels("*");
  }
};

ActorBytes Bytes(const Actors_ActorRecord& actor) {
  ActorBytes bytes;
  std::memcpy(bytes.data(), &actor, bytes.size());
  return bytes;
}

void CheckGroundedSnapshot(Actors_ActorRecord& actor) {
  Actors_ActorRecord expected;
  std::memcpy(&expected, &actor, sizeof(expected));
  const auto& instance = actor.instance;
  const auto tile =
      static_cast<std::uint32_t>(instance.currentTileX) |
      (static_cast<std::uint32_t>(instance.currentTileLayer) << 8) |
      (static_cast<std::uint32_t>(instance.currentTileZ) << 16) |
      (static_cast<std::uint32_t>(instance.reserved5F) << 24);
  expected.instance.previousTileX = instance.currentTileX;
  expected.instance.previousTileLayer = instance.currentTileLayer;
  expected.instance.previousTileZ = instance.currentTileZ;
  expected.instance.reserved63 = instance.reserved5F;
  const auto xy = static_cast<std::uint32_t>(
                      static_cast<std::uint16_t>(instance.position.vx)) |
                  (static_cast<std::uint32_t>(
                       static_cast<std::uint16_t>(instance.position.vy))
                   << 16);
  expected.instance.supportOffset = std::bit_cast<std::int32_t>(xy);
  expected.instance.reserved50 = instance.position.vz;
  expected.instance.previousFacing = instance.facing;
  if (instance.supportLayer == 0) {
    expected.savedPlacement = std::bit_cast<std::int32_t>(tile);
  }

  Actors_SaveActorGroundedTransform(&actor);
  CHECK(actor.savedPlacement == expected.savedPlacement);
  CHECK(actor.instance.supportOffset == expected.instance.supportOffset);
  CHECK(actor.instance.reserved50 == expected.instance.reserved50);
  CHECK(actor.instance.previousFacing == expected.instance.previousFacing);
  CHECK(Bytes(actor) == Bytes(expected));
}

struct JumpFixture {
  Actors_ActorRecord* saved_actor = Actors_actorRecords[0];
  Field_RoomRecord saved_room = Field_roomData;
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  char saved_sequence = Actors_lastResolvedSequenceIndex;
  char saved_phase = Event_playbackPhase;
  std::array<u_char, 6> saved_hang{};
  std::array<u_char, 6> saved_height{};
  Actors_ActorRecord actor{};
  struct {
    Field_MpdCollisionHeader header{32, 32, 0, 1024};
    std::array<std::uint16_t, 2048> descriptors{};
    std::array<u_char, 16> samples{};
  } collision;
  std::array<std::uint32_t, 128> sequence{};

  JumpFixture() {
    Actors_actorRecords[0] = &actor;
    std::copy_n(Actors_kJumpHangFrames, saved_hang.size(), saved_hang.begin());
    std::copy_n(Actors_kJumpHeightAdjustments, saved_height.size(),
                saved_height.begin());
    std::fill_n(Actors_kJumpHangFrames, saved_hang.size(), 2);
    std::fill_n(Actors_kJumpHeightAdjustments, saved_height.size(), 0);
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    Field_roomData.collisionMap = &collision.header;
    Field_roomData.header.collisionMapLength = sizeof(collision);
    Event_playbackPhase = 0;
    actor.instance.position = {960, 128, 1216, 0x1234};
    actor.instance.rootMotion = {5, -6, 7, 0x5678};
    actor.instance.facing = 0xF00;
    actor.instance.movementMode = 1;
    actor.instance.locomotionState = 3;
    actor.instance.weaponDrawn = 1;
    actor.instance.movementPending = 1;
    actor.instance.soundEnabled = 1;
    actor.animationLocked = 1;
    actor.animationId = 77;
    actor.runSpeed = 32;
    actor.turnRate = 17;
    actor.jumpStarted = 1;
    actor.fastAnimation = 1;
    actor.previewPose = 1;
    actor.holdsTextureSlots = 1;
    actor.reserved5B0Bits6 = 3;
    actor.routeTarget = {11, 22, 33, 44};
  }

  ~JumpFixture() {
    Actors_actorRecords[0] = saved_actor;
    Field_roomData = saved_room;
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              Field_gimmickInstances);
    Actors_lastResolvedSequenceIndex = saved_sequence;
    Event_playbackPhase = saved_phase;
    std::copy(saved_hang.begin(), saved_hang.end(), Actors_kJumpHangFrames);
    std::copy(saved_height.begin(), saved_height.end(),
              Actors_kJumpHeightAdjustments);
  }

  void EnableAnimation() {
    // Synthetic four-header bank: navigation, rise, fall and ground jump.
    auto* bank = reinterpret_cast<Actors_SeqAnimationBank*>(sequence.data());
    bank->animationMap = 128;
    bank->animationData = 384;
    auto* map = reinterpret_cast<u_char*>(bank) + bank->animationMap;
    map[31] = 0;
    map[33] = 1;
    map[35] = 2;
    map[47] = 3;
    for (std::size_t index = 0; index < 4; ++index) {
      bank->headers[index * 5] = static_cast<u_short>(16 + index);
      bank->headers[index * 5 + 1] = 0xFF;
    }
    actor.commonSequence = bank;
    actor.battleSequence = bank;
    actor.ownedSequence = bank;
    actor.sequenceFlags = 100;
    actor.commonAnimationCount = 100;
    actor.instance.weaponDrawn = 0;
    actor.animationLocked = 0;
  }
};

bool Navigation(JumpEntry entry, bool jumping, bool locked) {
  return entry == Actors_BeginNavigationJump ||
         (entry == Actors_BeginTileJump && !jumping && locked);
}

void PrepareExpected(Actors_ActorRecord& actor, JumpEntry entry) {
  if (entry == Actors_BeginAirborneTileJump ||
      (entry == Actors_BeginTileJump && actor.instance.isJumping)) {
    actor.instance.motionLocked = 0;
    actor.instance.isJumping = 1;
  } else if (entry == Actors_BeginNavigationJump ||
             actor.instance.motionLocked) {
    actor.instance.motionLocked = 1;
    actor.instance.isJumping = 1;
  } else {
    actor.jumpProfile = 1;
  }
}

ActorBytes ConsumeExpected(const Actors_ActorRecord& actor, JumpEntry entry,
                           int* target, int speed, int facing) {
  Actors_ActorRecord expected;
  std::memcpy(&expected, &actor, sizeof(expected));
  const bool navigation =
      Navigation(entry, actor.instance.isJumping, actor.instance.motionLocked);
  PrepareExpected(expected, entry);
  auto* saved = Actors_actorRecords[0];
  Actors_actorRecords[0] = &expected;
  if (navigation) {
    Actors_PlayActorAnimation(0, 31, 0, 8);
  }
  const auto narrowed =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(facing));
  Actors_BeginActorJumpToTile(&expected, reinterpret_cast<u_char*>(target),
                              speed, narrowed);
  Actors_actorRecords[0] = saved;
  return Bytes(expected);
}

void CheckConsumerDestination(const Actors_ActorRecord& actor, int* target) {
  CHECK(std::memcmp(&actor.routeTarget, target, sizeof(*target)) == 0);
  CHECK(actor.routeTarget.vz == 33);
  CHECK(actor.routeTarget.pad == 44);
  CHECK(actor.destinationX == 1216);
  CHECK(actor.destinationZ == 1472);
  CHECK(actor.destinationMode == 2);
}

void CheckConsumerArc(const Actors_ActorRecord& actor, int speed) {
  CHECK(actor.instance.movementMode == 2);
  CHECK(actor.jumpFrames > 0);
  CHECK(actor.instance.rootMotion.vx > 0);
  CHECK(actor.instance.rootMotion.vy < 0);
  CHECK(actor.instance.rootMotion.vz > 0);
  CHECK(actor.instance.jumpCountdown == (speed >= 32 ? 2 : 6));
  CHECK(actor.landingPending == (speed < 18));
}

void CheckConsumerFacing(const Actors_ActorRecord& actor, int facing) {
  const int expected_delta = facing == -1           ? 255
                             : facing == 0x10003    ? 259
                             : facing == 0x12348001 ? 24833
                                                    : 256;
  CHECK(actor.instance.facingDelta == expected_delta);
  CHECK(actor.instance.turnFrames == (expected_delta * 8 + 4095) / 4096);
  CHECK(actor.turnRate == 17);
}

void CheckStartedJumpLog(const std::vector<std::string>& messages) {
  REQUIRE(messages.size() == 2);
  CHECK(messages[0].find(
            "entry=plain target=[9,1,11,227] speed=40 facing=-32767") !=
        std::string::npos);
  CHECK(messages[0].find("position=[960,128,1216] phase=0") !=
        std::string::npos);
  CHECK(messages[1].find("phase=2 countdown=2 destination=[1216,1472]") !=
        std::string::npos);
  CHECK(
      messages[1].find("destination_mode=2 jump_frames=11 root=[23,-27,23]") !=
      std::string::npos);
}
}  // namespace

TEST_CASE("Grounded snapshots write only the actual actor history fields",
          "[grounded-transform]") {
  const auto layer =
      GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  const auto packed = GENERATE(0U, 0x00060004U, 0x80FE7FFFU, 0xFFFFFFFFU);
  constexpr std::array<SVECTOR, 3> kPositions{{
      {576, -448, 832, 0x1234},
      {-32768, 32767, -32768, -1},
      {32767, -32768, 32767, 0},
  }};
  const auto position = GENERATE(0, 1, 2);
  Actors_ActorRecord actor{};
  actor.instance.position = kPositions[position];
  actor.instance.currentTileX = static_cast<u_char>(packed);
  actor.instance.currentTileLayer = static_cast<u_char>(packed >> 8);
  actor.instance.currentTileZ = static_cast<u_char>(packed >> 16);
  actor.instance.reserved5F = static_cast<u_char>(packed >> 24);
  actor.instance.supportLayer = static_cast<u_char>(layer);
  actor.instance.placementLayer = static_cast<u_char>(15 - layer);
  actor.instance.facing = -12345;
  actor.instance.previousTileX = 0x11;
  actor.instance.previousTileLayer = 0x22;
  actor.instance.previousTileZ = 0x33;
  actor.instance.reserved63 = 0x44;
  actor.instance.supportOffset = 0x55667788;
  actor.instance.reserved50 = -77;
  actor.instance.previousFacing = 0x1234;
  actor.savedPlacement = 0x11223344;
  actor.runFrames = 0x55;
  actor.runTurns = 0x66;
  actor.opaque1860 = 0x77;
  actor.reserved1861 = 0x88;
  actor.reserved1862 = 0x99;
  actor.reserved1863 = 0xAA;
  CheckGroundedSnapshot(actor);
}

TEST_CASE("Grounded history preserves its anchor on supports and refreshes",
          "[grounded-transform]") {
  Actors_ActorRecord actor{};
  actor.instance.currentTileX = 4;
  actor.instance.currentTileZ = 6;
  actor.instance.position = {576, -448, 832, 0x1234};
  actor.instance.facing = -2345;
  actor.savedPlacement = 0x11223344;
  CheckGroundedSnapshot(actor);
  CHECK(actor.savedPlacement == 0x00060004);

  actor.instance.supportLayer = 2;
  actor.instance.currentTileX = 20;
  actor.instance.currentTileLayer = 3;
  actor.instance.currentTileZ = 30;
  actor.instance.reserved5F = 0xFF;
  actor.instance.position = {-128, 256, -384, -1};
  actor.instance.facing = 0x4321;
  CheckGroundedSnapshot(actor);
  CHECK(actor.savedPlacement == 0x00060004);

  actor.instance.supportLayer = 0;
  CheckGroundedSnapshot(actor);
  CHECK(actor.savedPlacement ==
        std::bit_cast<std::int32_t>(std::uint32_t{0xFF1E0314}));
}

TEST_CASE(
    "Jump entries preserve every unrelated byte and jumping wins priority",
    "[actors][jump-entry]") {
  const auto entry =
      GENERATE(&Actors_BeginTileJump, &Actors_BeginAirborneTileJump,
               &Actors_BeginNavigationJump);
  const bool jumping = GENERATE(false, true);
  const bool locked = GENERATE(false, true);
  const auto profile = GENERATE(0, 1, 2, 3);
  JumpFixture fixture;
  fixture.actor.instance.isJumping = jumping;
  fixture.actor.instance.motionLocked = locked;
  fixture.actor.jumpProfile = static_cast<u_char>(profile);
  Actors_ActorRecord expected;
  std::memcpy(&expected, &fixture.actor, sizeof(expected));
  PrepareExpected(expected, entry);
  int target = std::bit_cast<int>(0xE30B0109u);
  entry(0, &target, 40, 0x18001);
  CHECK(Bytes(fixture.actor) == Bytes(expected));
  CHECK(std::bit_cast<unsigned>(target) == 0xE30B0109u);
}

TEST_CASE(
    "Only navigation entries request animation 31 with eight blend frames",
    "[actors][jump-entry]") {
  const auto entry =
      GENERATE(&Actors_BeginTileJump, &Actors_BeginAirborneTileJump,
               &Actors_BeginNavigationJump);
  const bool jumping = GENERATE(false, true);
  const bool locked = GENERATE(false, true);
  JumpFixture fixture;
  fixture.EnableAnimation();
  fixture.actor.instance.isJumping = jumping;
  fixture.actor.instance.motionLocked = locked;
  int target = 0x000B0009;
  entry(0, &target, 40, 0);
  const bool navigation = Navigation(entry, jumping, locked);
  CHECK(fixture.actor.animationId == (navigation ? 31 : 77));
  CHECK(fixture.actor.blendFrames == (navigation ? 8 : 0));
  CHECK(fixture.actor.instance.movementMode == 1);
  CHECK(fixture.actor.instance.locomotionState == 3);
}

TEST_CASE(
    "Jump entries use the actual arc consumer with full targets and facing",
    "[actors][jump-entry]") {
  const auto entry =
      GENERATE(&Actors_BeginTileJump, &Actors_BeginAirborneTileJump,
               &Actors_BeginNavigationJump);
  const auto speed = GENERATE(-10, 17, 18, 31, 32, 40, 0x10028);
  const auto facing = GENERATE(-1, 0x10003, 0x12348001, -0x7FFFFFFF - 1);
  const bool aliased = GENERATE(false, true);
  const auto start_y = GENERATE(-128, 128);
  JumpFixture fixture;
  fixture.EnableAnimation();
  fixture.actor.instance.movementMode = 0;
  fixture.actor.instance.position.vy = static_cast<std::int16_t>(start_y);
  const SVECTOR original_position = fixture.actor.instance.position;
  int target = std::bit_cast<int>(0xE30B0109u);
  if (aliased) {
    std::memcpy(&fixture.actor.routeTarget, &target, sizeof(target));
  }
  const auto expected =
      ConsumeExpected(fixture.actor, entry, &target, speed, facing);
  auto* input =
      aliased ? reinterpret_cast<int*>(&fixture.actor.routeTarget) : &target;
  REQUIRE(Field_ActorSupportHeight(1216, 1472) == 0);
  entry(0, input, speed, facing);
  CHECK(Bytes(fixture.actor) == expected);
  CheckConsumerDestination(fixture.actor, &target);
  CheckConsumerArc(fixture.actor, speed);
  const int animation =
      entry == Actors_BeginTileJump ? 47 : (start_y < 0 ? 35 : 33);
  CHECK(fixture.actor.animationId == animation);
  CHECK(fixture.actor.pose.animationId == fixture.actor.animationId);
  CHECK(fixture.actor.blendFrames == 4);
  CheckConsumerFacing(fixture.actor, facing);
  CHECK(std::memcmp(&fixture.actor.instance.position, &original_position,
                    sizeof(original_position)) == 0);
}

TEST_CASE("Jump reentry honors the active arc gate and production motion reset",
          "[actors][jump-entry]") {
  const auto entry =
      GENERATE(&Actors_BeginTileJump, &Actors_BeginAirborneTileJump,
               &Actors_BeginNavigationJump);
  JumpFixture fixture;
  fixture.actor.instance.movementMode = 0;
  int target = 0x000B0009;
  entry(0, &target, 40, -1);
  Actors_ActorRecord expected;
  std::memcpy(&expected, &fixture.actor, sizeof(expected));
  PrepareExpected(expected, entry);
  target = 0x7F0D010B;
  entry(0, &target, 18, 0x10003);
  CHECK(Bytes(fixture.actor) == Bytes(expected));
  Actors_StopActorMotion(0);
  REQUIRE(fixture.actor.instance.movementMode == 0);
  REQUIRE(fixture.actor.instance.jumpCountdown == 0);
  entry(0, &target, 18, 0x10003);
  CHECK(fixture.actor.instance.movementMode == 2);
  CHECK(fixture.actor.destinationX == 1472);
  CHECK(fixture.actor.destinationZ == 1728);
  CHECK(fixture.actor.instance.jumpCountdown == 6);
  CHECK(std::memcmp(&fixture.actor.routeTarget, &target, sizeof(target)) == 0);
}

TEST_CASE("Jump diagnostics retain packed requests and the actual consumer arc",
          "[actors][jump-entry][diagnostics]") {
  const bool aliased = GENERATE(false, true);
  std::vector<std::string> messages;
  JumpLogScope logs;
  Core::SetLogLevel(Core::LogLevel::Debug);
  REQUIRE(Core::SetLogChannels("actors.jump"));
  Core::SetLogObserver([&messages](const Core::LogRecord& record) {
    if (record.channel == "actors.jump") messages.emplace_back(record.message);
  });
  JumpFixture fixture;
  fixture.actor.instance.movementMode = 0;
  int target = std::bit_cast<int>(0xE30B0109u);
  if (aliased) {
    std::memcpy(&fixture.actor.routeTarget, &target, sizeof(target));
  }
  auto* input =
      aliased ? reinterpret_cast<int*>(&fixture.actor.routeTarget) : &target;
  Actors_BeginTileJump(0, input, 40, 0x18001);
  CheckStartedJumpLog(messages);
  Actors_BeginTileJump(0, input, 18, 0);
  REQUIRE(messages.size() == 4);
  CHECK(messages[2].find("phase=2") != std::string::npos);
  CHECK(messages[3] == messages[1]);
}
