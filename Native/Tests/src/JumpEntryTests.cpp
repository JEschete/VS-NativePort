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

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/MotionRequestsLive.hpp"
#include "Actors/Movement.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Core/Log.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"

using VS::Core::Gte::SVECTOR;

namespace {
using JumpEntry = void (*)(int, int*, int, int);
// The packed-word entries the copied callers use (each also has a typed
// overload).
constexpr JumpEntry kTileJump = &VS::Actors::BeginLiveTileJump;
constexpr JumpEntry kAirborneTileJump = &VS::Actors::BeginLiveAirborneTileJump;
constexpr JumpEntry kNavigationJump = &VS::Actors::BeginLiveNavigationJump;
using ActorBytes = std::array<std::byte, sizeof(VS::Actors::ActorRecord)>;
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

ActorBytes Bytes(const VS::Actors::ActorRecord& actor) {
  ActorBytes bytes;
  std::memcpy(bytes.data(), &actor, bytes.size());
  return bytes;
}

void CheckGroundedSnapshot(VS::Actors::ActorRecord& actor) {
  VS::Actors::ActorRecord expected;
  std::memcpy(&expected, &actor, sizeof(expected));
  const auto& instance = actor.instance;
  const auto tile =
      static_cast<std::uint32_t>(instance.current_tile_x) |
      (static_cast<std::uint32_t>(instance.current_tile_layer) << 8) |
      (static_cast<std::uint32_t>(instance.current_tile_z) << 16) |
      (static_cast<std::uint32_t>(instance.reserved5f) << 24);
  expected.instance.previous_tile_x = instance.current_tile_x;
  expected.instance.previous_tile_layer = instance.current_tile_layer;
  expected.instance.previous_tile_z = instance.current_tile_z;
  expected.instance.reserved63 = instance.reserved5f;
  const auto xy = static_cast<std::uint32_t>(
                      static_cast<std::uint16_t>(instance.position.vx)) |
                  (static_cast<std::uint32_t>(
                       static_cast<std::uint16_t>(instance.position.vy))
                   << 16);
  expected.instance.support_offset = std::bit_cast<std::int32_t>(xy);
  expected.instance.reserved50 = instance.position.vz;
  expected.instance.previous_facing = instance.facing;
  if (instance.support_layer == 0) {
    expected.saved_placement = std::bit_cast<std::int32_t>(tile);
  }

  VS::Actors::SaveLiveActorGroundedTransform(&actor);
  CHECK(actor.saved_placement == expected.saved_placement);
  CHECK(actor.instance.support_offset == expected.instance.support_offset);
  CHECK(actor.instance.reserved50 == expected.instance.reserved50);
  CHECK(actor.instance.previous_facing == expected.instance.previous_facing);
  CHECK(Bytes(actor) == Bytes(expected));
}

struct JumpFixture {
  VS::Actors::ActorRecord* saved_actor = VS::Actors::actorRecords[0];
  VS::Field::RoomDataRecord saved_room = VS::Field::roomData;
  decltype(VS::Field::gimmickInstances) saved_gimmicks =
      VS::Field::gimmickInstances;
  char saved_sequence = VS::Actors::lastResolvedSequenceIndex;
  std::uint8_t saved_phase = VS::Event::playbackPhase;
  std::array<std::uint8_t, 6> saved_hang{};
  std::array<std::uint8_t, 6> saved_height{};
  VS::Actors::ActorRecord actor{};
  struct {
    VS::Field::MpdCollisionHeader header{32, 32, 0, 1024};
    std::array<std::uint16_t, 2048> descriptors{};
    std::array<std::uint8_t, 16> samples{};
  } collision;
  std::array<std::uint32_t, 128> sequence{};

  JumpFixture() {
    VS::Actors::actorRecords[0] = &actor;
    std::copy_n(VS::Actors::jumpHangFrames.begin(), saved_hang.size(),
                saved_hang.begin());
    std::copy_n(VS::Actors::jumpHeightAdjustments.begin(), saved_height.size(),
                saved_height.begin());
    std::fill_n(VS::Actors::jumpHangFrames.begin(), saved_hang.size(), 2);
    std::fill_n(VS::Actors::jumpHeightAdjustments.begin(), saved_height.size(),
                0);
    std::fill_n(VS::Field::gimmickInstances.begin(), saved_gimmicks.size(),
                nullptr);
    VS::Field::roomData.collision_map = &collision.header;
    VS::Field::roomData.header.collision_map_length = sizeof(collision);
    VS::Event::playbackPhase = 0;
    actor.instance.position = {960, 128, 1216, 0x1234};
    actor.instance.root_motion = {5, -6, 7, 0x5678};
    actor.instance.facing = 0xF00;
    actor.instance.movement_mode = 1;
    actor.instance.locomotion_state = 3;
    actor.instance.weapon_drawn = 1;
    actor.instance.movement_pending = 1;
    actor.instance.sound_enabled = 1;
    actor.animation_locked = 1;
    actor.animation_id = 77;
    actor.run_speed = 32;
    actor.turn_rate = 17;
    actor.jump_started = 1;
    actor.fast_animation = 1;
    actor.preview_pose = 1;
    actor.holds_texture_slots = 1;
    actor.reserved5b0_bits6 = 3;
    actor.route_target = {11, 22, 33, 44};
  }

  ~JumpFixture() {
    VS::Actors::ForgetSequenceBanks(sequence.data(), sizeof sequence);
    VS::Actors::actorRecords[0] = saved_actor;
    VS::Field::roomData = saved_room;
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              VS::Field::gimmickInstances.begin());
    VS::Actors::lastResolvedSequenceIndex = saved_sequence;
    VS::Event::playbackPhase = saved_phase;
    std::copy(saved_hang.begin(), saved_hang.end(),
              VS::Actors::jumpHangFrames.begin());
    std::copy(saved_height.begin(), saved_height.end(),
              VS::Actors::jumpHeightAdjustments.begin());
  }

  void EnableAnimation() {
    // Synthetic four-header bank: navigation, rise, fall and ground jump.
    auto* bank =
        reinterpret_cast<VS::Actors::SeqAnimationBank*>(sequence.data());
    bank->animation_map = 128;
    bank->animation_data = 384;
    auto* map = reinterpret_cast<std::uint8_t*>(bank) + bank->animation_map;
    map[31] = 0;
    map[33] = 1;
    map[35] = 2;
    map[47] = 3;
    for (std::size_t index = 0; index < 4; ++index) {
      bank->headers[index * 5] = static_cast<std::uint16_t>(16 + index);
      bank->headers[index * 5 + 1] = 0xFF;
    }
    actor.common_sequence = bank;
    actor.battle_sequence = bank;
    actor.owned_sequence = bank;
    actor.sequence_flags = 100;
    actor.common_animation_count = 100;
    actor.instance.weapon_drawn = 0;
    actor.animation_locked = 0;
    VS::Actors::RegisterSequenceBank(bank, sizeof sequence);
  }
};

bool Navigation(JumpEntry entry, bool jumping, bool locked) {
  return entry == kNavigationJump || (entry == kTileJump && !jumping && locked);
}

void PrepareExpected(VS::Actors::ActorRecord& actor, JumpEntry entry) {
  if (entry == kAirborneTileJump ||
      (entry == kTileJump && actor.instance.is_jumping)) {
    actor.instance.motion_locked = 0;
    actor.instance.is_jumping = 1;
  } else if (entry == kNavigationJump || actor.instance.motion_locked) {
    actor.instance.motion_locked = 1;
    actor.instance.is_jumping = 1;
  } else {
    actor.jump_profile = 1;
  }
}

ActorBytes ConsumeExpected(const VS::Actors::ActorRecord& actor,
                           JumpEntry entry, int* target, int speed,
                           int facing) {
  VS::Actors::ActorRecord expected;
  std::memcpy(&expected, &actor, sizeof(expected));
  const bool navigation = Navigation(entry, actor.instance.is_jumping,
                                     actor.instance.motion_locked);
  PrepareExpected(expected, entry);
  auto* saved = VS::Actors::actorRecords[0];
  VS::Actors::actorRecords[0] = &expected;
  if (navigation) {
    VS::Actors::PlayLiveActorAnimation(0, 31, 0, 8);
  }
  const auto narrowed =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(facing));
  VS::Actors::BeginLiveActorJumpToTile(
      &expected, std::bit_cast<VS::Actors::PackedTile>(*target), speed,
      narrowed);
  VS::Actors::actorRecords[0] = saved;
  return Bytes(expected);
}

void CheckConsumerDestination(const VS::Actors::ActorRecord& actor,
                              int* target) {
  CHECK(std::memcmp(&actor.route_target, target, sizeof(*target)) == 0);
  CHECK(actor.route_target.vz == 33);
  CHECK(actor.route_target.pad == 44);
  CHECK(actor.destination_x == 1216);
  CHECK(actor.destination_z == 1472);
  CHECK(actor.destination_mode == 2);
}

void CheckConsumerArc(const VS::Actors::ActorRecord& actor, int speed) {
  CHECK(actor.instance.movement_mode == 2);
  CHECK(actor.jump_frames > 0);
  CHECK(actor.instance.root_motion.vx > 0);
  CHECK(actor.instance.root_motion.vy < 0);
  CHECK(actor.instance.root_motion.vz > 0);
  CHECK(actor.instance.jump_countdown == (speed >= 32 ? 2 : 6));
  CHECK(actor.landing_pending == (speed < 18));
}

void CheckConsumerFacing(const VS::Actors::ActorRecord& actor, int facing) {
  const int expected_delta = facing == -1           ? 255
                             : facing == 0x10003    ? 259
                             : facing == 0x12348001 ? 24833
                                                    : 256;
  CHECK(actor.instance.facing_delta == expected_delta);
  CHECK(actor.instance.turn_frames == (expected_delta * 8 + 4095) / 4096);
  CHECK(actor.turn_rate == 17);
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
  VS::Actors::ActorRecord actor{};
  actor.instance.position = kPositions[position];
  actor.instance.current_tile_x = static_cast<std::uint8_t>(packed);
  actor.instance.current_tile_layer = static_cast<std::uint8_t>(packed >> 8);
  actor.instance.current_tile_z = static_cast<std::uint8_t>(packed >> 16);
  actor.instance.reserved5f = static_cast<std::uint8_t>(packed >> 24);
  actor.instance.support_layer = static_cast<std::uint8_t>(layer);
  actor.instance.placement_layer = static_cast<std::uint8_t>(15 - layer);
  actor.instance.facing = -12345;
  actor.instance.previous_tile_x = 0x11;
  actor.instance.previous_tile_layer = 0x22;
  actor.instance.previous_tile_z = 0x33;
  actor.instance.reserved63 = 0x44;
  actor.instance.support_offset = 0x55667788;
  actor.instance.reserved50 = -77;
  actor.instance.previous_facing = 0x1234;
  actor.saved_placement = 0x11223344;
  actor.run_frames = 0x55;
  actor.run_turns = 0x66;
  actor.opaque1860 = 0x77;
  actor.reserved1861 = 0x88;
  actor.reserved1862 = 0x99;
  actor.reserved1863 = 0xAA;
  CheckGroundedSnapshot(actor);
}

TEST_CASE("Grounded history preserves its anchor on supports and refreshes",
          "[grounded-transform]") {
  VS::Actors::ActorRecord actor{};
  actor.instance.current_tile_x = 4;
  actor.instance.current_tile_z = 6;
  actor.instance.position = {576, -448, 832, 0x1234};
  actor.instance.facing = -2345;
  actor.saved_placement = 0x11223344;
  CheckGroundedSnapshot(actor);
  CHECK(actor.saved_placement == 0x00060004);

  actor.instance.support_layer = 2;
  actor.instance.current_tile_x = 20;
  actor.instance.current_tile_layer = 3;
  actor.instance.current_tile_z = 30;
  actor.instance.reserved5f = 0xFF;
  actor.instance.position = {-128, 256, -384, -1};
  actor.instance.facing = 0x4321;
  CheckGroundedSnapshot(actor);
  CHECK(actor.saved_placement == 0x00060004);

  actor.instance.support_layer = 0;
  CheckGroundedSnapshot(actor);
  CHECK(actor.saved_placement ==
        std::bit_cast<std::int32_t>(std::uint32_t{0xFF1E0314}));
}

TEST_CASE(
    "Jump entries preserve every unrelated byte and jumping wins priority",
    "[actors][jump-entry]") {
  const auto entry = GENERATE(kTileJump, kAirborneTileJump, kNavigationJump);
  const bool jumping = GENERATE(false, true);
  const bool locked = GENERATE(false, true);
  const auto profile = GENERATE(0, 1, 2, 3);
  JumpFixture fixture;
  fixture.actor.instance.is_jumping = jumping;
  fixture.actor.instance.motion_locked = locked;
  fixture.actor.jump_profile = static_cast<std::uint8_t>(profile);
  VS::Actors::ActorRecord expected;
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
  const auto entry = GENERATE(kTileJump, kAirborneTileJump, kNavigationJump);
  const bool jumping = GENERATE(false, true);
  const bool locked = GENERATE(false, true);
  JumpFixture fixture;
  fixture.EnableAnimation();
  fixture.actor.instance.is_jumping = jumping;
  fixture.actor.instance.motion_locked = locked;
  int target = 0x000B0009;
  entry(0, &target, 40, 0);
  const bool navigation = Navigation(entry, jumping, locked);
  CHECK(fixture.actor.animation_id == (navigation ? 31 : 77));
  CHECK(fixture.actor.blend_frames == (navigation ? 8 : 0));
  CHECK(fixture.actor.instance.movement_mode == 1);
  CHECK(fixture.actor.instance.locomotion_state == 3);
}

TEST_CASE(
    "Jump entries use the actual arc consumer with full targets and facing",
    "[actors][jump-entry]") {
  const auto entry = GENERATE(kTileJump, kAirborneTileJump, kNavigationJump);
  const auto speed = GENERATE(-10, 17, 18, 31, 32, 40, 0x10028);
  const auto facing = GENERATE(-1, 0x10003, 0x12348001, -0x7FFFFFFF - 1);
  const bool aliased = GENERATE(false, true);
  const auto start_y = GENERATE(-128, 128);
  JumpFixture fixture;
  fixture.EnableAnimation();
  fixture.actor.instance.movement_mode = 0;
  fixture.actor.instance.position.vy = static_cast<std::int16_t>(start_y);
  const SVECTOR original_position = fixture.actor.instance.position;
  int target = std::bit_cast<int>(0xE30B0109u);
  if (aliased) {
    std::memcpy(&fixture.actor.route_target, &target, sizeof(target));
  }
  const auto expected =
      ConsumeExpected(fixture.actor, entry, &target, speed, facing);
  auto* input =
      aliased ? reinterpret_cast<int*>(&fixture.actor.route_target) : &target;
  REQUIRE(VS::Field::LiveActorSupportHeight(1216, 1472) == 0);
  entry(0, input, speed, facing);
  CHECK(Bytes(fixture.actor) == expected);
  CheckConsumerDestination(fixture.actor, &target);
  CheckConsumerArc(fixture.actor, speed);
  const int animation = entry == kTileJump ? 47 : (start_y < 0 ? 35 : 33);
  CHECK(fixture.actor.animation_id == animation);
  CHECK(fixture.actor.pose.animation_id == fixture.actor.animation_id);
  CHECK(fixture.actor.blend_frames == 4);
  CheckConsumerFacing(fixture.actor, facing);
  CHECK(std::memcmp(&fixture.actor.instance.position, &original_position,
                    sizeof(original_position)) == 0);
}

TEST_CASE("Jump reentry honors the active arc gate and production motion reset",
          "[actors][jump-entry]") {
  const auto entry = GENERATE(kTileJump, kAirborneTileJump, kNavigationJump);
  JumpFixture fixture;
  fixture.actor.instance.movement_mode = 0;
  int target = 0x000B0009;
  entry(0, &target, 40, -1);
  VS::Actors::ActorRecord expected;
  std::memcpy(&expected, &fixture.actor, sizeof(expected));
  PrepareExpected(expected, entry);
  target = 0x7F0D010B;
  entry(0, &target, 18, 0x10003);
  CHECK(Bytes(fixture.actor) == Bytes(expected));
  VS::Actors::StopLiveActorMotion(0);
  REQUIRE(fixture.actor.instance.movement_mode == 0);
  REQUIRE(fixture.actor.instance.jump_countdown == 0);
  entry(0, &target, 18, 0x10003);
  CHECK(fixture.actor.instance.movement_mode == 2);
  CHECK(fixture.actor.destination_x == 1472);
  CHECK(fixture.actor.destination_z == 1728);
  CHECK(fixture.actor.instance.jump_countdown == 6);
  CHECK(std::memcmp(&fixture.actor.route_target, &target, sizeof(target)) == 0);
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
  fixture.actor.instance.movement_mode = 0;
  int target = std::bit_cast<int>(0xE30B0109u);
  if (aliased) {
    std::memcpy(&fixture.actor.route_target, &target, sizeof(target));
  }
  auto* input =
      aliased ? reinterpret_cast<int*>(&fixture.actor.route_target) : &target;
  VS::Actors::BeginLiveTileJump(0, input, 40, 0x18001);
  CheckStartedJumpLog(messages);
  VS::Actors::BeginLiveTileJump(0, input, 18, 0);
  REQUIRE(messages.size() == 4);
  CHECK(messages[2].find("phase=2") != std::string::npos);
  CHECK(messages[3] == messages[1]);
}
