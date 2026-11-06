#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/ActorCollisionLive.hpp"

extern "C" {
#include "Event/Triggers.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
}

namespace {

using SnapshotBytes = std::array<std::byte, sizeof(Field_GimmickSnapshot)>;

struct SnapshotFixture {
  std::array<Field_GimmickInstance*, 16> saved{};
  std::unique_ptr<Field_GimmickInstance> object =
      std::make_unique<Field_GimmickInstance>();

  SnapshotFixture() {
    std::ranges::copy(Field_gimmickInstances, saved.begin());
    std::ranges::fill(Field_gimmickInstances, nullptr);
    object->parent = 0xFF;
  }
  ~SnapshotFixture() { std::ranges::copy(saved, Field_gimmickInstances); }
};

struct TriggerFixture {
  SnapshotFixture snapshots;
  VS::Event::RoomEventLease room = VS::Event::LiveRoomScriptSections();
  VS::Event::PlaybackPhase phase = VS::Event::LiveScriptVm().Phase();
  VS::Event::EventSource source = VS::Event::LiveScriptVm().Source();
  std::int16_t event_id = VS::Event::LiveScriptVm().EventId();
  std::int16_t entry = Event_hasRoomEntryScript;
  char direct = Event_isDirectEventRequest;

  TriggerFixture() {
    VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
    Event_isDirectEventRequest = 0;
    Event_hasRoomEntryScript = 0;
  }
  ~TriggerFixture() {
    VS::Event::PublishLiveRoomScriptSections(room);
    VS::Event::LiveScriptVm().SetPhase(phase);
    VS::Event::LiveScriptVm().SetSource(source);
    VS::Event::LiveScriptVm().SetEventId(event_id);
    Event_hasRoomEntryScript = entry;
    Event_isDirectEventRequest = direct;
  }
};

void PublishTrigger(std::initializer_list<std::uint8_t> bytes) {
  const std::vector<std::uint8_t> data(bytes);
  const auto room = VS::Event::RoomEventSections::Copy({.triggers = data});
  REQUIRE(room);
  VS::Event::PublishLiveRoomScriptSections(*room);
}

Field_GimmickSnapshot SentinelSnapshot() {
  return {0xAA, 0xBB, 0xCC, 0xDD, 0x12345678, 0x7F7F7F7F};
}

std::uint8_t ExpectedHeight(const Field_GimmickInstance& object) {
  const auto support = VS::Field::LiveSampleSupportHeightAt(
      object.positionX, object.positionY, object.positionZ, 0, 0, 0);
  const auto difference = static_cast<std::int64_t>(support) - object.positionY;
  const auto wrapped =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
  return static_cast<std::uint8_t>(wrapped / 128);
}

void CheckValidSnapshot(int slot, const Field_GimmickInstance& object) {
  const auto expected_height = ExpectedHeight(object);
  auto output = SentinelSnapshot();
  REQUIRE(Field_GimmickPlacement(slot, &output) == 0);
  CHECK(output.placementByte0 == 5);
  CHECK(output.placementByte2 == 9);
  CHECK(output.orientation == 3);
  CHECK(output.heightAboveGround == expected_height);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) & 0xFFFF) == 0xFB2E);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) >> 16) ==
        static_cast<std::uint16_t>(object.positionY));
  CHECK(std::bit_cast<std::uint32_t>(output.positionZ) == 0x13570929);
}

}  // namespace

TEST_CASE("Gimmick snapshots refuse invalid and absent slots without writing",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int slot = -2; slot < 18; ++slot) {
    CHECK(Field_GimmickPlacement(slot, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
  Field_gimmickInstances[0] = fixture.object.get();
  CHECK(Field_GimmickPlacement(0, nullptr) == -1);
}

TEST_CASE("Gimmick snapshots reject attached falling and moving records",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  Field_gimmickInstances[0] = fixture.object.get();
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int refusal = 0; refusal < 3; ++refusal) {
    fixture.object->parent = refusal == 0 ? 0 : 0xFF;
    fixture.object->objectInfo.isFalling = refusal == 1;
    fixture.object->mode = static_cast<std::int16_t>(refusal == 2);
    CHECK(Field_GimmickPlacement(0, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
}

TEST_CASE("Gimmick snapshots preserve packed coordinates and height rounding",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto& object = *fixture.object;
  object.tileX = 5;
  object.tileZ = 9;
  object.objectInfo.material = 3;
  object.positionX = -1234;
  object.positionZ = 2345;
  object.reserved22 = 0x1357;
  for (const int slot : {0, 15}) {
    Field_gimmickInstances[slot] = &object;
    for (const int height : {-129, -128, -127, 0, 127, 128, 129}) {
      object.positionY = static_cast<std::int16_t>(height);
      CheckValidSnapshot(slot, object);
    }
    Field_gimmickInstances[slot] = nullptr;
  }
}

TEST_CASE(
    "Native room triggers consume the bounded stationary gimmick snapshot",
    "[gimmick-placement][room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  auto& object = *fixture.snapshots.object;
  object.tileX = 5;
  object.tileZ = 9;
  Field_gimmickInstances[0] = &object;
  PublishTrigger({4, 0, 255, 255, 2, 9, 0, 11, 0, 5, 9, 0, 0, 255});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kPlayerTile};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kStarted);
  CHECK(VS::Event::LiveScriptVm().EventId() == 9);
  CHECK(VS::Event::LiveScriptVm().Source() ==
        VS::Event::EventSource::kEventFile);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  object.mode = 1;
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
  Field_gimmickInstances[0] = nullptr;
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
}

TEST_CASE("Native room trigger contexts preserve signed reference semantics",
          "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  PublishTrigger({4, 0, 255, 255, 8, 7, 0, 6, 0, 128, 0, 32, 255});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kActorState,
      .context = VS::Event::TriggerActorPair{-32768, 8192}};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kStarted);
  CHECK(VS::Event::LiveScriptVm().EventId() == 7);
}

TEST_CASE("Native room trigger status queries require no section bytes",
          "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  VS::Event::PublishLiveRoomScriptSections({});
  const VS::Event::RoomTriggerQuery query{
      .request = VS::Event::RoomTriggerRequest::kPlaybackStatus};
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kNoMatch);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kRunning);
  CHECK(VS::Event::EvaluateLiveRoomEventTriggers(query) ==
        VS::Event::RoomTriggerResult::kBusy);
}

TEST_CASE(
    "C trigger entry preserves tile contexts and refuses missing contexts",
    "[room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  PublishTrigger({4, 0, 255, 255, 4, 12, 0, 8, 7, 9, 255});
  std::array<std::uint8_t, 4> tile{7, 0, 9, 0};
  CHECK(Event_EvaluateRoomEventTriggers(4, 0, tile.data(), 1) == 1);
  CHECK(VS::Event::LiveScriptVm().EventId() == 12);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  CHECK(Event_EvaluateRoomEventTriggers(4, 0, nullptr, 1) == 0);
}
