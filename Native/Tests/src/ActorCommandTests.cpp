#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
}

namespace {
struct CommandFixture {
  Battle_Ai_FrameContext saved_frame = *Battle_Ai_FrameState();
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  Battle_Actor* saved_actor = Battle_actors[2];
  Actors_ActorRecord* saved_target = Actors_actorRecords[3];
  Battle_Actor actor{};
  Actors_ModelInstance model{};
  Actors_ActorRecord target{};
  Battle_Ai_ActorAiState state{};
  Battle_Ai_ActionCandidate candidate{};
  u_short heights[32][32]{};
  u_short flags[32][32]{};

  CommandFixture() {
    Battle_actors[2] = &actor;
    Actors_actorRecords[3] = &target;
    actor.id = state.actorId = 2;
    actor.model = &model;
    actor.commandArg0 = 0x13572468;
    actor.commandArg1.word = 0x22334455;
    actor.commandArg2 = 0x66778899;
    state.actor = reinterpret_cast<Battle_Ai_EngagementData*>(&actor);
    state.movementTarget = 3;
    state.position.vx = state.position.vz = 320;
    state.tile.word = 0x00020002;
    state.nextTile.word = state.destination.word = 0x00030003;
    state.radius = 16;
    state.locomotionFlags = 0x42;
    state.walkStep = 8;
    state.runStep = 12;
    state.opaque13A = 32;
    state.facingDelay = 7;
    state.flightHeight = 16;
    state.moveX = 1234;
    state.moveZ = 2345;
    state.moveY = 3456;
    state.nextPosition.pad = 0x5555;
    const uint16_t threshold = 64;
    std::memcpy(state.step_height_bytes, &threshold, sizeof(threshold));
    state.currentCandidate = &candidate;
    candidate.row = 2;
    candidate.column = 1;
    candidate.targetId = 3;
    candidate.targetLimb = 4;
    candidate.opaqueCBit7 = 13;
    state.actionSlots[2][1].kind = 1;
    target.instance.position = {100, 200, 300, 0};
    auto* frame = Battle_Ai_FrameState();
    frame->roomData = nullptr;
    frame->heightMap = heights;
    frame->tileFlags = flags;
    const std::array<u_char, 8> directions{0x30, 0x33, 0x03, 0x13,
                                           0x10, 0x11, 0x01, 0x31};
    std::copy(directions.begin(), directions.end(), frame->directions);
    Battle_Ai_tileFlagMap = flags;
  }

  ~CommandFixture() {
    *Battle_Ai_FrameState() = saved_frame;
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_actors[2] = saved_actor;
    Actors_actorRecords[3] = saved_target;
  }
};
}  // namespace

TEST_CASE("AI dispatch covers all eighteen actor command intents",
          "[ai][actor-commands]") {
  const auto intent =
      GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
  const std::array<int, 18> opcodes{2,  1,  20, 3,  4,  9,  9,  9, 19,
                                    16, 17, 21, 22, 23, 11, 24, 6, 8};
  CommandFixture fixture;
  Battle_Ai_IssueActorCommand(&fixture.state, intent);
  CHECK(fixture.actor.commandState == 0x80);
  CHECK(fixture.actor.commandOpcode == opcodes[intent]);
  CHECK(fixture.actor.commandTarget == 3);
  CHECK(Battle_Ai_FrameState()->movementResult == intent);
}

TEST_CASE(
    "AI direct commands preserve the required payload and untouched halves",
    "[ai][actor-commands]") {
  const auto intent = GENERATE(3, 8, 9, 10, 16);
  CommandFixture fixture;
  fixture.state.heading = 0x2345;
  Battle_Ai_IssueActorCommand(&fixture.state, intent);
  if (intent == 3) {
    CHECK(fixture.actor.commandArg0 == 0x00070345);
  } else if (intent == 8) {
    CHECK(fixture.actor.commandArg0 == 1234);
    CHECK(fixture.actor.commandArg1.word == 2345);
    CHECK(fixture.actor.commandArg2 == 3456);
  } else if (intent == 16) {
    CHECK(fixture.actor.commandArg0 == 0x00202345);
  } else {
    CHECK(fixture.actor.commandArg0 == (3456 << 16 | 1234));
    CHECK(fixture.actor.commandArg1.word == (0x22330000 | 2345));
    CHECK(fixture.actor.commandArg2 == 0x66778899);
  }
}

TEST_CASE("AI turning keeps strict rate boundaries and upper-body selection",
          "[ai][actor-commands]") {
  const auto target = GENERATE(0, 31, 32, 64, 4095);
  const auto upper_body = GENERATE(0, 1);
  CommandFixture fixture;
  fixture.state.angle = static_cast<int16_t>(target);
  fixture.state.hasTaggedBone3 = static_cast<char>(upper_body);
  Battle_Ai_IssueActorCommand(&fixture.state, 4);
  const bool reached = target == 0 || target == 31 || target == 4095;
  CHECK(fixture.actor.commandOpcode == (reached ? 4 : (upper_body ? 7 : 3)));
  CHECK((static_cast<uint32_t>(fixture.actor.commandArg0) >> 16) == 32);
  CHECK((fixture.actor.commandArg0 & 0xFFFF) == (reached ? target : 32));
}

TEST_CASE(
    "AI tile commands stage native positions and reserve only their target",
    "[ai][actor-commands]") {
  CommandFixture fixture;
  Battle_Ai_IssueActorCommand(&fixture.state, 13);
  CHECK(fixture.state.nextPosition.vx == 448);
  CHECK(fixture.state.nextPosition.vy == 0);
  CHECK(fixture.state.nextPosition.vz == 448);
  CHECK(fixture.state.nextPosition.pad == 0x5555);
  CHECK(fixture.actor.commandArg0 == 0x00030003);
  CHECK(fixture.flags[3][3] == 0x80);
  Battle_Ai_IssueActorCommand(&fixture.state, 12);
  CHECK(fixture.actor.commandArg0 == 0x001001C0);
  CHECK((fixture.actor.commandArg1.word & 0xFFFF) == 448);
}

TEST_CASE(
    "AI step commands choose walking, jumping and dropping from height flags",
    "[ai][actor-commands]") {
  const auto flags = GENERATE(0, 8, 0x30, 0xC0);
  CommandFixture fixture;
  fixture.heights[3][3] = 1;
  const uint16_t threshold = 10;
  std::memcpy(fixture.state.step_height_bytes, &threshold, sizeof(threshold));
  fixture.state.movementFlags = static_cast<u_short>(flags);
  Battle_Ai_IssueActorCommand(&fixture.state, 14);
  const auto expected =
      flags == 8 ? 23 : (flags == 0x30 ? 18 : (flags == 0xC0 ? 14 : 11));
  CHECK(fixture.actor.commandOpcode == expected);
  CHECK(fixture.state.nextPosition.vy == 64);
  CHECK(fixture.flags[3][3] == 0x80);
}

TEST_CASE(
    "AI attacks encode limb targets or positional targets without guest "
    "pointers",
    "[ai][actor-commands]") {
  const auto kind = GENERATE(0, 1, 2, 3);
  CommandFixture fixture;
  fixture.state.actionSlots[2][1].kind = kind;
  Battle_Ai_IssueActorCommand(&fixture.state, 15);
  CHECK(fixture.actor.commandOpcode == 24);
  CHECK(fixture.actor.commandArg0 == 0x00010002);
  if (kind == 1 || kind == 2) {
    CHECK(fixture.actor.commandArg1.word == 0x00040003);
    CHECK(fixture.actor.commandArg2 == 0x66778899);
  } else {
    CHECK(fixture.actor.commandArg1.word == (187 << 16 | 100));
    CHECK(fixture.actor.commandArg2 == 0x6677012C);
  }
}

TEST_CASE(
    "AI blocked movement slides once per tile using fixed-point command "
    "payloads",
    "[ai][actor-commands]") {
  CommandFixture fixture;
  fixture.state.position.vx = 264;
  fixture.state.heading = 1024;
  fixture.heights[2][1] = 1;
  Battle_Ai_IssueActorCommand(&fixture.state, 7);
  CHECK(fixture.actor.commandOpcode == 13);
  CHECK(fixture.actor.commandArg0 == 32768);
  CHECK(fixture.actor.commandArg1.word == 192512);
  Battle_Ai_IssueActorCommand(&fixture.state, 7);
  CHECK(fixture.actor.commandOpcode == 9);
}
