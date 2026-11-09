#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/SkeletonLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Event/RoomTriggerEntryLive.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/TriggersLive.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/ChestInteractions.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickAnimationQueue.hpp"
#include "Field/GimmickAnimationQueueLive.h"
#include "Field/GimmickDynamics.hpp"
#include "Field/GimmickDynamicsLive.hpp"
#include "Field/GimmickPlacement.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/GimmickReactions.hpp"
#include "Field/GimmickReactionsLive.hpp"
#include "Field/GimmickResources.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/InteractionTarget.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomSwitches.hpp"
#include "Field/RoomSwitchesLive.h"
#include "Field/TransitionsLive.hpp"
#include "Field/Traps.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Random.h"
#include "Effects/Runtime.h"
#include "Event/Triggers.h"
#include "Field/Doors.h"
#include "Field/GimmickDynamicsLive.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/GimmickReactionsLive.h"
#include "Field/GimmickResourcesLive.h"
#include "Field/Gimmicks.h"
#include "Field/InteractionTargetLive.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
}

TEST_CASE(
    "Gimmick sharing keeps first slot signed exclusion and three-bit identity",
    "[gimmick-resource-core][field]") {
  std::array<VS::Field::GimmickResourceSlot, 16> slots{};
  slots[1] = {true, 0x81};
  slots[5] = {true, 1};
  CHECK(VS::Field::FindSharedGimmickModel(slots, -1, 1) == 1);
  CHECK(VS::Field::FindSharedGimmickModel(slots, 1, 1) == 5);
  CHECK(VS::Field::FindSharedGimmickModel(slots, 5, 1) == 1);
  CHECK_FALSE(VS::Field::FindSharedGimmickModel(slots, -1, 0x81).has_value());
}

TEST_CASE("Gimmick sharing preserves every signed request-slot byte",
          "[gimmick-resource-core][field]") {
  std::array<VS::Field::GimmickResourceSlot, 16> slots{};
  slots.fill({true, 3});
  for (std::uint32_t bits = 0; bits <= 255; ++bits) {
    const auto excluded =
        std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(bits));
    CHECK(VS::Field::FindSharedGimmickModel(slots, excluded, 3) ==
          (excluded == 0 ? 1 : 0));
  }
}

TEST_CASE("Gimmick sharing scans only the original sixteen live slots",
          "[gimmick-resource-core][field]") {
  std::array<VS::Field::GimmickResourceSlot, 17> slots{};
  slots.back() = {true, 3};
  CHECK_FALSE(VS::Field::FindSharedGimmickModel(slots, -1, 3).has_value());
  CHECK_FALSE(VS::Field::FindSharedGimmickModel({}, -1, 3).has_value());
}

TEST_CASE("Gimmick palette table copies exactly the original complete bytes",
          "[gimmick-resource-core][field]") {
  VS::Field::GimmickPaletteTable table{};
  std::array<std::byte, 0xE1> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::byte>(index);
  }
  REQUIRE(VS::Field::SetGimmickPaletteTable(table, bytes));
  CHECK(std::memcmp(table.data(), bytes.data(), sizeof(table)) == 0);
  const auto saved = table;
  CHECK_FALSE(
      VS::Field::SetGimmickPaletteTable(table, std::span(bytes).first(0xDF)));
  CHECK(table == saved);
}

TEST_CASE("Gimmick palette table supports self and overlapping input",
          "[gimmick-resource-core][field]") {
  VS::Field::GimmickPaletteTable table{};
  for (std::size_t index = 0; index < table.size(); ++index) {
    table[index] = static_cast<std::uint16_t>(index + 1);
  }
  const auto saved = table;
  REQUIRE(VS::Field::SetGimmickPaletteTable(table,
                                            std::as_bytes(std::span(table))));
  CHECK(table == saved);
}

TEST_CASE("Gimmick palette table stages a partially overlapping source",
          "[gimmick-resource-core][field]") {
  struct PaletteStorage {
    VS::Field::GimmickPaletteTable table{};
    std::uint16_t tail = 0x1234;
  } storage;
  for (std::size_t index = 0; index < storage.table.size(); ++index) {
    storage.table[index] = static_cast<std::uint16_t>(index + 1);
  }
  auto expected = storage.table;
  std::move(expected.begin() + 1, expected.end(), expected.begin());
  expected.back() = storage.tail;
  const auto bytes = std::as_bytes(std::span(&storage, 1));
  REQUIRE(VS::Field::SetGimmickPaletteTable(
      storage.table, bytes.subspan(2, sizeof(storage.table))));
  CHECK(storage.table == expected);
  CHECK(storage.tail == 0x1234);
}

namespace {
struct RecordingGimmickReleaseHost final : VS::Field::GimmickReleaseHost {
  std::vector<int> calls;
  int refuse = -1;
  bool FreeModel(std::size_t) override {
    calls.push_back(0);
    return refuse != 0;
  }
  bool RemoveOccupancy(std::size_t) override {
    calls.push_back(1);
    return refuse != 1;
  }
  bool ClearSlot(std::size_t) override {
    calls.push_back(2);
    return refuse != 2;
  }
};
}  // namespace

TEST_CASE(
    "Gimmick release frees unshared model before occupancy and slot clear",
    "[gimmick-release-core][field]") {
  RecordingGimmickReleaseHost host;
  std::array<VS::Field::GimmickResourceSlot, 16> slots{};
  slots[3] = {true, 5, 1};
  CHECK(VS::Field::ReleaseGimmickInstance(3, slots, host) == 0);
  CHECK(host.calls == std::vector<int>{0, 1, 2});
}

TEST_CASE(
    "Gimmick release retains models shared by any other live same-kind slot",
    "[gimmick-release-core][field]") {
  RecordingGimmickReleaseHost host;
  std::array<VS::Field::GimmickResourceSlot, 16> slots{};
  slots[3] = {true, 5, 1};
  slots[15] = {true, 0x85, 0};
  CHECK(VS::Field::ReleaseGimmickInstance(3, slots, host) == 0);
  CHECK(host.calls == std::vector<int>{1, 2});
}

TEST_CASE("Gimmick release rejects missing bone-zero and out-of-range slots",
          "[gimmick-release-core][field]") {
  RecordingGimmickReleaseHost host;
  std::array<VS::Field::GimmickResourceSlot, 16> slots{};
  CHECK(VS::Field::ReleaseGimmickInstance(-1, slots, host) == -1);
  CHECK(VS::Field::ReleaseGimmickInstance(16, slots, host) == -1);
  CHECK(VS::Field::ReleaseGimmickInstance(0, slots, host) == -1);
  slots[0] = {true, 1, 0};
  CHECK(VS::Field::ReleaseGimmickInstance(0, slots, host) == -1);
  CHECK(host.calls.empty());
}

TEST_CASE("Gimmick release stops before slot clear on earlier host refusal",
          "[gimmick-release-core][field]") {
  for (const int refusal : {0, 1, 2}) {
    RecordingGimmickReleaseHost host;
    host.refuse = refusal;
    std::array<VS::Field::GimmickResourceSlot, 16> slots{};
    slots[1] = {true, 3, 255};
    CHECK(VS::Field::ReleaseGimmickInstance(1, slots, host) == -1);
    CHECK(host.calls.size() == static_cast<std::size_t>(refusal + 1));
  }
}

namespace {
struct GimmickResourcesLiveFixture {
  decltype(std::to_array(Field_gimmickInstances)) saved_instances =
      std::to_array(Field_gimmickInstances);
  decltype(std::to_array(Field_gimmickPaletteColors)) saved_palettes =
      std::to_array(Field_gimmickPaletteColors);
  std::array<Field_GimmickInstance, 2> instances{};
  GimmickResourcesLiveFixture() {
    std::fill_n(Field_gimmickInstances, saved_instances.size(), nullptr);
    Field_gimmickInstances[1] = &instances[0];
    Field_gimmickInstances[5] = &instances[1];
    instances[0].objectInfo.objectId = 3;
    instances[1].objectInfo.objectId = 3;
    std::fill_n(Field_gimmickPaletteColors, saved_palettes.size(), 0x35);
  }

  ~GimmickResourcesLiveFixture() {
    std::copy(saved_instances.begin(), saved_instances.end(),
              Field_gimmickInstances);
    std::copy(saved_palettes.begin(), saved_palettes.end(),
              Field_gimmickPaletteColors);
  }
};

struct GimmickReleaseLiveFixture : GimmickResourcesLiveFixture {
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  decltype(Battle_Ai_specialTerrainHeights) saved_heights =
      Battle_Ai_specialTerrainHeights;
  GimmickReleaseLiveFixture() {
    Battle_Ai_tileFlagMap = nullptr;
    Battle_Ai_specialTerrainHeights = nullptr;
    instances[0].boneCount = 1;
  }
  ~GimmickReleaseLiveFixture() {
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_Ai_specialTerrainHeights = saved_heights;
  }
};

struct GimmickReactionLiveFixture : GimmickReleaseLiveFixture {
  std::array<std::span<std::uint8_t>, 14> tables{
      std::span(Field_kGimmickSpinHeightSteps).first(16),
      std::span(Field_kGimmickSpinPitchSteps).first(16),
      std::span(Field_kGimmickSpinYawSteps).first(16),
      std::span(Field_kGimmickSpinRollSteps).first(16),
      std::span(Field_kGimmickTiltDepthSteps).first(12),
      std::span(Field_kGimmickTiltAngleSteps).first(12),
      std::span(Field_kGimmickBounceHeightSteps).first(4),
      std::span(Field_kGimmickRollHeightSteps).first(16),
      std::span(Field_kGimmickRollDisplacementSteps).first(16),
      std::span(Field_kGimmickRollAngleSteps).first(16),
      std::span(Field_kGimmickSquashHeightSteps).first(16),
      std::span(Field_kGimmickSquashScaleX).first(16),
      std::span(Field_kGimmickSquashScaleY).first(16),
      std::span(Field_kGimmickSquashScaleZ).first(16)};
  std::array<std::array<std::uint8_t, 16>, 14> saved_tables{};
  GimmickReactionLiveFixture() {
    instances[0].slot = 1;
    for (std::size_t index = 0; index < tables.size(); ++index) {
      std::copy(tables[index].begin(), tables[index].end(),
                saved_tables[index].begin());
      std::fill(tables[index].begin(), tables[index].end(), 0);
    }
  }
  ~GimmickReactionLiveFixture() {
    for (std::size_t index = 0; index < tables.size(); ++index) {
      std::copy_n(saved_tables[index].begin(), tables[index].size(),
                  tables[index].begin());
    }
  }
};
}  // namespace

TEST_CASE(
    "Native gimmick reaction applies signed samples and preserves terminal "
    "cursor",
    "[gimmick-reaction-live][field]") {
  GimmickReactionLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 253;
  record.positionY = INT16_MIN;
  Field_kGimmickSpinHeightSteps[1] = 255;
  Field_kGimmickSpinPitchSteps[1] = 128;
  REQUIRE(VS::Field::UpdateLiveGimmickReactionAnimation(&record) ==
          VS::Field::GimmickReactionResult::kAdvanced);
  CHECK(record.positionY == INT16_MAX);
  CHECK(record.rotationX == -128);
  CHECK(record.objectInfo.supportIndex == 2);
  record.objectInfo.supportIndex = 14;
  REQUIRE(VS::Field::UpdateLiveGimmickReactionAnimation(&record) ==
          VS::Field::GimmickReactionResult::kComplete);
  CHECK(record.mode == 0);
  CHECK(record.objectInfo.supportIndex == 14);
}

TEST_CASE("Native gimmick reaction preserves unsigned roll angle bytes",
          "[gimmick-reaction-live][field]") {
  GimmickReactionLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 249;
  record.orientation = 1;
  Field_kGimmickRollDisplacementSteps[1] = 128;
  Field_kGimmickRollAngleSteps[1] = 255;
  REQUIRE(VS::Field::UpdateLiveGimmickReactionAnimation(&record) ==
          VS::Field::GimmickReactionResult::kAdvanced);
  CHECK(record.positionX == -128);
  CHECK(VS::Core::Math::WrapSigned16(record.rotationZ) == -255);
}

TEST_CASE(
    "Native gimmick reaction restores exact palette and motion nibble at break "
    "frame nine",
    "[gimmick-reaction-live][field]") {
  GimmickReactionLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 248;
  record.objectInfo.supportIndex = 9;
  record.isVisible = -1;
  record.reserved9Bits4 = 15;
  for (std::size_t color = 0; color < std::size(record.basePalette); ++color) {
    record.basePalette[color] = static_cast<std::uint16_t>(color + 17);
    record.pulsedPalette[color] = 65535;
  }
  REQUIRE(VS::Field::UpdateLiveGimmickReactionAnimation(&record) ==
          VS::Field::GimmickReactionResult::kAdvanced);
  CHECK(std::equal(std::begin(record.basePalette), std::end(record.basePalette),
                   std::begin(record.pulsedPalette)));
  CHECK(record.positionY == 3);
  CHECK(record.objectInfo.supportIndex == 10);
  CHECK(record.motionState == 12);
  CHECK(record.isVisible != 0);
  CHECK(record.reserved9Bits4 == 15);
}

TEST_CASE(
    "Native gimmick reaction roll completion reuses owned orientation reset",
    "[gimmick-reaction-live][field]") {
  GimmickReactionLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 249;
  record.objectInfo.objectId = 0;
  record.objectInfo.material = 0;
  record.objectInfo.supportIndex = 14;
  record.rotationTicks = 7;
  record.rotationX = 11;
  record.reserved2A = 99;
  REQUIRE(VS::Field::UpdateLiveGimmickReactionAnimation(&record) ==
          VS::Field::GimmickReactionResult::kComplete);
  CHECK(record.mode == 0);
  CHECK(record.objectInfo.supportIndex == 14);
  CHECK(record.rotationTicks == 0);
  CHECK(record.rotationX == 0);
  CHECK(record.reserved2A == 0);
}

TEST_CASE(
    "Native gimmick reaction break release frees real model before completing",
    "[gimmick-reaction-live][field]") {
  GimmickReactionLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  void* model = VS::Core::Memory::AllocateLiveFromLowEnd(128);
  REQUIRE(model != nullptr);
  auto& record = fixture.instances[0];
  record.model = static_cast<Actors_ShpModel*>(model);
  record.mode = 248;
  record.objectInfo.supportIndex = 40;
  Field_gimmickInstances[5] = nullptr;
  VS::Render::RecordLiveModelExtent(model, 128);
  REQUIRE(VS::Field::UpdateLiveGimmickReactionAnimation(&record) ==
          VS::Field::GimmickReactionResult::kComplete);
  CHECK(Field_gimmickInstances[1] == nullptr);
  CHECK(record.positionY == 3);
  CHECK(record.mode == 0);
  CHECK(record.objectInfo.supportIndex == 40);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(model).has_value());
  CHECK(VS::Render::LiveModelBytes(model).empty());
}

TEST_CASE(
    "Native gimmick reaction refuses null short and malformed frames without "
    "completion",
    "[gimmick-reaction-live][field]") {
  GimmickReactionLiveFixture fixture;
  CHECK(VS::Field::UpdateLiveGimmickReactionAnimation(nullptr) ==
        VS::Field::GimmickReactionResult::kRefused);
  fixture.instances[0].mode = 253;
  fixture.instances[0].objectInfo.supportIndex = 255;
  CHECK(VS::Field::UpdateLiveGimmickReactionAnimation(&fixture.instances[0]) ==
        VS::Field::GimmickReactionResult::kRefused);
  CHECK(fixture.instances[0].objectInfo.supportIndex == 255);
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  void* short_record = VS::Core::Memory::AllocateLiveFromLowEnd(32);
  REQUIRE(short_record != nullptr);
  CHECK(VS::Field::UpdateLiveGimmickReactionAnimation(
            static_cast<Field_GimmickInstance*>(short_record)) ==
        VS::Field::GimmickReactionResult::kRefused);
  CHECK(Field_UpdateGimmickReactionAnimationEntry(nullptr) == 1);
}

TEST_CASE("Gimmick reaction retains its original module address",
          "[gimmick-reaction-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B101C) ==
        reinterpret_cast<const void*>(
            &Field_UpdateGimmickReactionAnimationEntry));
}

TEST_CASE(
    "Native gimmick release frees an unshared heap model and forgets extent",
    "[gimmick-release-live][field]") {
  GimmickReleaseLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  void* model = VS::Core::Memory::AllocateLiveFromLowEnd(128);
  REQUIRE(model != nullptr);
  fixture.instances[0].model = static_cast<Actors_ShpModel*>(model);
  Field_gimmickInstances[5] = nullptr;
  VS::Render::RecordLiveModelExtent(model, 128);
  REQUIRE(VS::Field::ReleaseLiveGimmickInstance(1) == 0);
  CHECK(Field_gimmickInstances[1] == nullptr);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(model).has_value());
  CHECK(VS::Render::LiveModelBytes(model).empty());
}

TEST_CASE(
    "Native gimmick release retains a shared heap model until its last owner",
    "[gimmick-release-live][field]") {
  GimmickReleaseLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  void* model = VS::Core::Memory::AllocateLiveFromLowEnd(128);
  REQUIRE(model != nullptr);
  fixture.instances[0].model = fixture.instances[1].model =
      static_cast<Actors_ShpModel*>(model);
  fixture.instances[1].boneCount = 1;
  VS::Render::RecordLiveModelExtent(model, 128);
  REQUIRE(VS::Field::ReleaseLiveGimmickInstance(1) == 0);
  CHECK(VS::Core::Memory::LivePayloadCapacity(model).has_value());
  CHECK(VS::Render::LiveModelBytes(model).size() == 128);
  REQUIRE(VS::Field::ReleaseLiveGimmickInstance(5) == 0);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(model).has_value());
  CHECK(VS::Render::LiveModelBytes(model).empty());
}

TEST_CASE("Native gimmick release refuses invalid and bone-zero slots",
          "[gimmick-release-live][field]") {
  GimmickReleaseLiveFixture fixture;
  fixture.instances[0].boneCount = 0;
  CHECK(VS::Field::ReleaseLiveGimmickInstance(-1) == -1);
  CHECK(VS::Field::ReleaseLiveGimmickInstance(16) == -1);
  CHECK(VS::Field::ReleaseLiveGimmickInstance(0) == -1);
  CHECK(VS::Field::ReleaseLiveGimmickInstance(1) == -1);
  CHECK(Field_gimmickInstances[1] == &fixture.instances[0]);
}

TEST_CASE("Gimmick release retains its original module address",
          "[gimmick-release-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80099D6C) ==
        reinterpret_cast<const void*>(&Field_ReleaseGimmickInstanceEntry));
}

TEST_CASE(
    "Native gimmick sharing writes only the selected request data address",
    "[gimmick-resource-live][field]") {
  GimmickResourcesLiveFixture fixture;
  Content_ActorAssetRequest request{};
  request.targetSlot = 1;
  request.modelId = -7;
  request.dataAddress = 0x1234;
  request.subKind = 3;
  auto expected = request;
  expected.dataAddress = 5;
  REQUIRE(Field_FindSharedGimmickModelEntry(&request) == 1);
  using Bytes = std::array<std::byte, sizeof(request)>;
  CHECK(std::bit_cast<Bytes>(request) == std::bit_cast<Bytes>(expected));
  CHECK(fixture.instances[0].objectInfo.objectId == 3);
}

TEST_CASE(
    "Native gimmick sharing supports signed exclusion and unaligned requests",
    "[gimmick-resource-live][field]") {
  GimmickResourcesLiveFixture fixture;
  Content_ActorAssetRequest request{};
  request.targetSlot = -1;
  request.subKind = 3;
  std::array<std::byte, sizeof(request) + 1> bytes{};
  std::memcpy(bytes.data() + 1, &request, sizeof(request));
  REQUIRE(Field_FindSharedGimmickModelEntry(
              reinterpret_cast<Content_ActorAssetRequest*>(bytes.data() + 1)) ==
          1);
  std::memcpy(&request, bytes.data() + 1, sizeof(request));
  CHECK(request.dataAddress == 1);
  CHECK(request.targetSlot == -1);
}

TEST_CASE("Native gimmick sharing preserves failed and absent requests",
          "[gimmick-resource-live][field]") {
  GimmickResourcesLiveFixture fixture;
  Content_ActorAssetRequest request{};
  request.subKind = 255;
  request.dataAddress = 0x1234;
  REQUIRE(Field_FindSharedGimmickModelEntry(&request) == 0);
  CHECK(request.dataAddress == 0x1234);
  CHECK(Field_FindSharedGimmickModelEntry(nullptr) == 0);
}

TEST_CASE("Native gimmick palettes preserve the compatibility tail",
          "[gimmick-resource-live][field]") {
  GimmickResourcesLiveFixture fixture;
  VS::Field::GimmickPaletteTable palette{};
  for (std::size_t index = 0; index < palette.size(); ++index) {
    palette[index] = static_cast<std::uint16_t>(index * 17);
  }
  Field_SetGimmickPaletteTableEntry(palette.data());
  CHECK(std::memcmp(Field_gimmickPaletteColors, palette.data(),
                    sizeof(palette)) == 0);
  CHECK(std::all_of(std::begin(Field_gimmickPaletteColors) + sizeof(palette),
                    std::end(Field_gimmickPaletteColors),
                    [](char value) { return value == 0x35; }));
}

TEST_CASE("Native gimmick palettes reject short input without mutation",
          "[gimmick-resource-live][field]") {
  GimmickResourcesLiveFixture fixture;
  const auto saved = std::to_array(Field_gimmickPaletteColors);
  std::array<std::byte, 0xDF> short_palette{};
  CHECK_FALSE(VS::Field::SetLiveGimmickPaletteTable(short_palette));
  Field_SetGimmickPaletteTableEntry(nullptr);
  CHECK(std::ranges::equal(saved, Field_gimmickPaletteColors));
}

TEST_CASE("Gimmick resource entries retain their original module addresses",
          "[gimmick-resource-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80099900) ==
        reinterpret_cast<const void*>(&Field_FindSharedGimmickModelEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80099960) ==
        reinterpret_cast<const void*>(&Field_SetGimmickPaletteTableEntry));
}

namespace {

struct RecordingAshleyFallHost final : VS::Field::AshleyFallHost {
  std::optional<VS::Field::ScriptedFallDestination> destination;
  std::optional<std::int16_t> hp = 21;
  VS::Field::DoorRecord reload{.destination_zone_id = 9,
                               .destination_room_id = 4};
  std::optional<VS::Field::DoorRecord> transition;
  std::optional<VS::Field::DoorRecord> entrance;
  VS::Field::AshleyTransitionRoute route =
      VS::Field::AshleyTransitionRoute::kRoom;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::ScriptedFallDestination> ScriptedDestination()
      override {
    calls.push_back(0);
    return destination;
  }
  std::optional<std::int16_t> CurrentHp(std::int32_t) override {
    calls.push_back(1);
    return hp;
  }
  void ShowFallDamage(std::int32_t) override { calls.push_back(2); }
  void SetHp(std::int32_t, std::int16_t value) override {
    calls.push_back(3);
    hp = value;
  }
  void BeginDefeat(std::int32_t) override { calls.push_back(4); }
  VS::Field::DoorRecord StoreEntranceForReload() override {
    calls.push_back(5);
    return reload;
  }
  std::uint8_t CurrentZoneId() const override { return 9; }
  void BeginTransition(const VS::Field::DoorRecord& door,
                       VS::Field::AshleyTransitionRoute value) override {
    calls.push_back(6);
    transition = door;
    route = value;
  }
  void ResetRunMomentum(std::int32_t) override { calls.push_back(7); }
  void SetEntrance(const std::optional<VS::Field::DoorRecord>& door) override {
    calls.push_back(8);
    entrance = door;
  }
};

struct RecordingGimmickCreationHost final : VS::Field::GimmickCreationHost {
  std::array<std::optional<VS::Field::GimmickCreationState>, 16> states{};
  std::array<VS::Field::GimmickResourceSlot, 16> slots{};
  mutable std::vector<std::string> calls;
  bool destination = true;
  bool model = true;
  std::int32_t height = 512;
  bool Occupied(std::size_t slot) const override {
    calls.emplace_back("occupied");
    return states[slot].has_value();
  }
  bool PublishDestination(std::size_t slot) override {
    calls.emplace_back("publish");
    if (destination) states[slot] = VS::Field::GimmickCreationState{};
    return destination;
  }
  bool ClearPrefix(std::size_t slot) override {
    calls.emplace_back("clear");
    states[slot] = VS::Field::GimmickCreationState{};
    return true;
  }
  std::span<const VS::Field::GimmickResourceSlot> Slots() override {
    calls.emplace_back("slots");
    for (std::size_t index = 0; index < states.size(); ++index) {
      const auto& state = states[index];
      if (state.has_value()) {
        slots[index] = {true, state->object_id, 1};
      } else {
        slots[index] = {false, 0, 1};
      }
    }
    return slots;
  }
  VS::Field::GimmickCreationState& State(std::size_t slot) {
    auto& state = states[slot];
    if (!state) {
      FAIL("Missing creation fixture state");
      throw std::bad_optional_access{};
    }
    return *state;
  }
  bool CopySharedPrefix(std::size_t slot, std::size_t source) override {
    calls.push_back("copy" + std::to_string(source));
    states[slot] = states[source];
    return true;
  }
  bool CreateModel(std::size_t, std::uint8_t kind,
                   std::uint8_t uv_kind) override {
    calls.push_back("model" + std::to_string(kind) + ":" +
                    std::to_string(uv_kind));
    return model;
  }
  std::optional<VS::Field::GimmickCreationState> ReadState(
      std::size_t slot) override {
    return states[slot];
  }
  bool StoreState(std::size_t slot,
                  const VS::Field::GimmickCreationState& state) override {
    states[slot] = state;
    calls.emplace_back("store");
    return true;
  }
  bool CopyPalette(std::size_t, std::uint8_t kind) override {
    calls.push_back("palette" + std::to_string(kind));
    return true;
  }
  bool UploadPalette(std::size_t) override {
    calls.emplace_back("upload");
    return true;
  }
  std::optional<std::int32_t> SupportHeight(
      const VS::Field::GimmickCreationState&) override {
    calls.emplace_back("height");
    return height;
  }
  bool Orient(std::size_t, std::uint8_t material) override {
    calls.push_back("orient" + std::to_string(material));
    return true;
  }
  bool AddOccupancy(std::size_t) override {
    calls.emplace_back("occupancy");
    return true;
  }
};

struct RecordingAshleyTileHost final : VS::Field::AshleyTileTriggerHost {
  RecordingAshleyFallHost fall;
  std::uint32_t flags = 0;
  VS::Field::AshleyTileTriggerCell cell{};
  std::optional<VS::Actors::PackedTile> last{VS::Actors::PackedTile{}};
  VS::Actors::PackedTile collision_tile{};
  VS::Actors::PackedTile doors_tile{};
  bool transitioning = false;
  bool event_control = false;
  bool event_started = false;
  bool stored_location = false;
  std::optional<VS::Field::ActorTileTrap> trap;
  std::vector<VS::Field::DoorRecord> doors;
  std::uint8_t flag_value = 0;

  std::uint32_t StateFlags(std::int32_t) override {
    fall.calls.push_back(10);
    return flags;
  }
  VS::Field::AshleyTileTriggerCell Collision(
      const VS::Actors::PackedTile& tile) override {
    fall.calls.push_back(11);
    collision_tile = tile;
    return cell;
  }
  bool TransitionInProgress() override {
    fall.calls.push_back(12);
    return transitioning;
  }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(std::int32_t) override {
    fall.calls.push_back(13);
    return last;
  }
  bool EventOwnsControl() override {
    fall.calls.push_back(14);
    return event_control;
  }
  bool TriggerPlayerTileEvent() override {
    fall.calls.push_back(15);
    return event_started;
  }
  void BeginQueuedCutscene() override { fall.calls.push_back(16); }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile&) override {
    fall.calls.push_back(17);
    return trap;
  }
  void SpawnTrapTileEffect(const VS::Actors::ActorPlacementView&) override {
    fall.calls.push_back(18);
  }
  void BeginTrap(VS::Field::TrapTarget, const VS::Actors::ActorPlacementView&,
                 std::int32_t) override {
    fall.calls.push_back(19);
  }
  void StoreTriggerTile(std::int32_t, const VS::Actors::PackedTile& tile,
                        std::uint8_t mode, bool store_location) override {
    fall.calls.push_back(20);
    stored_location = store_location;
    auto next = store_location ? tile : last.value_or(VS::Actors::PackedTile{});
    next.spare = mode;
    last = next;
  }
  std::vector<VS::Field::DoorRecord> DoorsAt(
      const VS::Actors::PackedTile& tile) override {
    fall.calls.push_back(21);
    doors_tile = tile;
    return doors;
  }
  std::uint8_t ScriptFlag(std::uint16_t) override {
    fall.calls.push_back(22);
    return flag_value;
  }
  VS::Field::AshleyFallHost& FallHost() override { return fall; }
};

struct RecordingActorTileTriggerHost final : VS::Field::ActorTileTriggerHost {
  std::uint32_t flags = 0;
  VS::Field::ActorTileTriggerCell cell{};
  bool transition = false;
  bool event_control = false;
  bool defeat = true;
  bool fall_event = false;
  std::optional<VS::Actors::PackedTile> last{VS::Actors::PackedTile{}};
  std::optional<VS::Field::ActorTileTrap> trap;
  VS::Actors::PackedTile collision_tile{};
  std::vector<std::int32_t> calls;

  std::uint32_t StateFlags(std::int32_t) override {
    calls.push_back(0);
    return flags;
  }
  VS::Field::ActorTileTriggerCell Collision(
      const VS::Actors::PackedTile& tile) override {
    calls.push_back(1);
    collision_tile = tile;
    return cell;
  }
  bool TransitionInProgress() override {
    calls.push_back(2);
    return transition;
  }
  std::optional<VS::Actors::PackedTile> LastTriggerTile(std::int32_t) override {
    calls.push_back(3);
    return last;
  }
  bool EventOwnsControl() override {
    calls.push_back(4);
    return event_control;
  }
  std::optional<VS::Field::ActorTileTrap> Trap(
      const VS::Actors::PackedTile&) override {
    calls.push_back(5);
    return trap;
  }
  void BeginTrap(VS::Field::TrapTarget, const VS::Actors::ActorPlacementView&,
                 std::int32_t) override {
    calls.push_back(6);
  }
  void StoreTriggerTile(std::int32_t, const VS::Actors::PackedTile& tile,
                        std::uint8_t mode, bool store_location) override {
    calls.push_back(7);
    if (!last) return;
    if (store_location) *last = tile;
    last->spare = mode;
  }
  bool DefeatForFall(std::int32_t) override {
    calls.push_back(8);
    return defeat;
  }
  bool TriggerFallEvent() override {
    calls.push_back(9);
    return fall_event;
  }
};

struct RecordingChestInteractionHost final : VS::Field::ChestInteractionHost {
  std::optional<VS::Field::ChestLockState> chest{VS::Field::ChestLockState{}};
  bool flag = false;
  bool item = false;
  std::vector<std::int32_t> calls;
  std::optional<VS::Field::ChestTarget> selected;
  bool notification_pending = false;
  VS::Field::ChestSelectionResult contents_result =
      VS::Field::ChestSelectionResult::kPending;

  std::optional<VS::Field::ChestLockState> Chest(
      VS::Field::ChestTarget) const override {
    return chest;
  }
  bool ScriptFlag(std::int32_t) override { return flag; }
  bool HasItem(std::int16_t) override { return item; }
  void SetScriptFlag(std::int32_t value) override {
    calls.push_back(value);
    flag = true;
  }
  void ConsumeItem(std::int16_t value) override { calls.push_back(value); }
  bool Unlock(VS::Field::ChestTarget) override {
    if (!chest) {
      return false;
    }
    chest->lock_level = 0;
    calls.push_back(1000);
    return true;
  }
  void ShowUnlocked(std::int16_t) override { calls.push_back(1001); }
  void PlayPose(const VS::Field::ChestLockState&, std::uint8_t pose) override {
    calls.push_back(pose);
  }
  bool StartAnimation(VS::Field::ChestTarget) override {
    calls.push_back(1002);
    return true;
  }
  bool SelectChest(VS::Field::ChestTarget target, std::int32_t) override {
    selected = target;
    return true;
  }
  std::optional<VS::Field::ChestTarget> SelectedChest() const override {
    return selected;
  }
  void BeginChestFlow() override { calls.push_back(1010); }
  void FinishChestFlow() override { calls.push_back(1011); }
  void RestoreIdlePoses() override { calls.push_back(1012); }
  void ResetActionSelection() override { calls.push_back(1013); }
  void ReleaseNotification() override { calls.push_back(1014); }
  void ShowLocked(std::int32_t kind, std::int16_t) override {
    calls.push_back(kind);
  }
  void OpenChestLoot(VS::Field::ChestTarget) override { calls.push_back(1015); }
  void OpenChestStorage(std::int32_t storage) override {
    calls.push_back(storage);
  }
  void PauseForDialog() override { calls.push_back(1016); }
  void PauseForMenu() override { calls.push_back(1017); }
  bool NotificationPending() override { return notification_pending; }
  VS::Field::ChestSelectionResult UpdateContentsSelection() override {
    return contents_result;
  }
  void ReleaseContentsRows() override { calls.push_back(1018); }
  void ResumeField() override { calls.push_back(1019); }
};

struct RecordingInteractionDispatchHost final
    : VS::Field::InteractionDispatchHost {
  VS::Field::InteractionTarget target;
  bool trap_mode = false;
  std::vector<int> calls;
  VS::Field::InteractionTarget Target() const override { return target; }
  bool TrapMode() const override { return trap_mode; }
  void BeginDispatch() override { calls.push_back(0); }
  bool DispatchTrap(VS::Field::TrapTarget) override {
    calls.push_back(1);
    return true;
  }
  bool DispatchChest(VS::Field::ChestTarget) override {
    calls.push_back(2);
    return true;
  }
};

struct RecordingTrapInteractionHost final : VS::Field::TrapSelectionHost,
                                            VS::Field::TrapFinishHost {
  bool valid = true;
  bool disable_succeeds = true;
  std::optional<VS::Field::TrapTarget> selected;
  std::optional<VS::Field::TrapTarget> disabled;
  std::int32_t selected_actor = -1;
  std::vector<std::int32_t> calls;

  bool Valid(VS::Field::TrapTarget, std::int32_t) const override {
    return valid;
  }
  void BeginFlow() override { calls.push_back(1000); }
  void Select(VS::Field::TrapTarget target, std::int32_t actor) override {
    selected = target;
    selected_actor = actor;
    calls.push_back(1001);
  }
  void RestoreIdle(std::int32_t actor) override { calls.push_back(actor); }
  void ShowInformation(VS::Field::TrapTarget) override {
    calls.push_back(1002);
  }
  void ResetActionSelection() override { calls.push_back(1003); }
  std::optional<VS::Field::TrapTarget> Target() const override {
    return selected;
  }
  void FinishFlow() override { calls.push_back(1004); }
  void ReleaseNotification() override { calls.push_back(1005); }
  bool DisableTrap(VS::Field::TrapTarget target) override {
    disabled = target;
    calls.push_back(1006);
    return disable_succeeds;
  }
  void ClearTarget() override {
    selected.reset();
    calls.push_back(1007);
  }
};

using SnapshotBytes = std::array<std::byte, sizeof(Field_GimmickSnapshot)>;

struct RecordingTrapActionHost final : VS::Field::TrapActionHost {
  bool action_state = true;
  bool stage_succeeds = true;
  bool resolve_succeeds = true;
  bool remove_during_resolve = false;
  bool change_during_resolve = false;
  mutable std::optional<std::int16_t> kind = 0;
  std::optional<VS::Field::TrapActionInput> staged;
  std::optional<VS::Field::TrapEffectRequest> effect;
  std::vector<std::int32_t> calls;

  bool HasActionState(const VS::Field::TrapActionInput&) const override {
    return action_state;
  }
  std::optional<std::int16_t> Kind(VS::Field::TrapTarget) const override {
    return kind;
  }
  bool StageVictim(const VS::Field::TrapActionInput& input) override {
    calls.push_back(1000);
    staged = input;
    return stage_succeeds;
  }
  bool ResolveAction(std::uint16_t action_id) override {
    calls.push_back(action_id);
    if (remove_during_resolve) {
      kind.reset();
    } else if (change_during_resolve) {
      kind = 1;
    }
    return resolve_succeeds;
  }
  bool BeginEffect(const VS::Field::TrapEffectRequest& request) override {
    calls.push_back(1001);
    effect = request;
    return true;
  }
};

struct RecordingInteractionCameraHost final : VS::Field::InteractionCameraHost {
  std::optional<VS::Field::TrapPosition> focus{
      VS::Field::TrapPosition{1, 2, 3}};
  std::optional<VS::Field::TrapPosition> ashley{
      VS::Field::TrapPosition{4, 5, 6}};
  std::int32_t speed = 0;
  std::uint32_t flags = 0;
  bool pending = false;
  bool transition = false;
  std::vector<std::int32_t> calls;
  mutable std::size_t flag_reads = 0;
  VS::Field::TrapPosition stepped{};
  std::int32_t mode = -1;
  std::int32_t requested_speed = 0;

  std::optional<VS::Field::TrapPosition> Focus() const override {
    return focus;
  }
  std::optional<VS::Field::TrapPosition> AshleyPosition() const override {
    return ashley;
  }
  std::int32_t StepLookAt(const VS::Field::TrapPosition& point,
                          std::int32_t step_mode,
                          std::int32_t step_speed) override {
    stepped = point;
    mode = step_mode;
    requested_speed = step_speed;
    calls.push_back(0);
    return speed;
  }
  std::uint32_t AshleyStateFlags() const override {
    ++flag_reads;
    return flags;
  }
  bool NotificationPending() override {
    calls.push_back(1);
    return pending;
  }
  bool AshleyTransitionPose() override {
    calls.push_back(2);
    return transition;
  }
};

struct RecordingFacingEventHost final : VS::Field::FacingEventHost {
  std::optional<VS::Actors::ActorPlacementView> placement{
      VS::Actors::ActorPlacementView{}};
  mutable std::int32_t observed_actor = -1;
  mutable std::size_t reads = 0;
  bool result = false;
  std::optional<VS::Field::FacingTile> evaluated;

  std::optional<VS::Actors::ActorPlacementView> Placement(
      std::int32_t actor) const override {
    ++reads;
    observed_actor = actor;
    return placement;
  }
  bool EvaluateInteraction(const VS::Field::FacingTile& tile) override {
    evaluated = tile;
    return result;
  }
};

struct RecordingFacingResourceHost final : VS::Field::FacingResourceHost {
  std::optional<VS::Actors::ActorPlacementView> placement{
      VS::Actors::ActorPlacementView{}};
  std::optional<VS::Field::RoomObjectTarget> object{
      VS::Field::RoomObjectTarget{2, 7}};
  std::optional<VS::Field::FacingChestState> chest{
      VS::Field::FacingChestState{.target = {3, 7}, .opened_flag = 9}};
  std::int32_t floor = 0;
  bool collected = false;
  std::vector<std::int32_t> calls;
  std::int32_t observed_flag = -1;

  std::optional<VS::Actors::ActorPlacementView> Placement(
      std::int32_t) const override {
    return placement;
  }
  std::optional<VS::Field::RoomObjectTarget> RoomObject(
      const VS::Field::FacingTile&) override {
    calls.push_back(0);
    return object;
  }
  std::optional<VS::Field::FacingChestState> Chest(
      const VS::Field::FacingTile&) override {
    calls.push_back(1);
    return chest;
  }
  std::int32_t FloorHeight(const VS::Field::FacingTile&) override {
    calls.push_back(2);
    return floor;
  }
  bool ScriptFlag(std::int32_t flag) override {
    calls.push_back(3);
    observed_flag = flag;
    return collected;
  }
};

struct RecordingObjectInteractionWaitHost final
    : VS::Field::ObjectInteractionWaitHost {
  VS::Field::FieldControlMode mode = VS::Field::FieldControlMode::kNone;
  std::vector<int> calls;

  void SetControlMode(VS::Field::FieldControlMode value) override {
    mode = value;
    calls.push_back(0);
  }
  void PauseField() override { calls.push_back(1); }
  void ReleaseNotification() override { calls.push_back(2); }
  void ResumeField() override { calls.push_back(3); }
  void EnterExploration() override {
    mode = VS::Field::FieldControlMode::kExploration;
    calls.push_back(4);
  }
};

struct RecordingUnlockEffectHost final : VS::Field::UnlockEffectHost {
  std::optional<VS::Field::UnlockEffectState> state{
      VS::Field::UnlockEffectState{.lock_level = 2}};
  bool opened = false;
  bool roll = true;
  bool clear = true;
  mutable std::optional<std::int16_t> flag;
  mutable std::vector<std::int32_t> calls;
  std::optional<VS::Field::TrapPosition> label;
  std::optional<VS::Field::UnlockEffectState> State(
      const VS::Field::UnlockEffectTarget&) const override {
    calls.push_back(0);
    return state;
  }
  bool ChestOpened(std::int16_t value) const override {
    calls.push_back(1);
    flag = value;
    return opened;
  }
  bool RollLockBreak() override {
    calls.push_back(2);
    return roll;
  }
  bool ClearLock(const VS::Field::UnlockEffectTarget&) override {
    calls.push_back(3);
    return clear;
  }
  void PlaySound(std::int32_t sound) override { calls.push_back(sound); }
  void ShowLockedLabel(const VS::Field::TrapPosition& point) override {
    calls.push_back(4);
    label = point;
  }
};

struct RecordingRoomObjectInteractionHost final
    : VS::Field::RoomObjectInteractionHost {
  std::optional<VS::Field::RoomObjectLockState> state{
      VS::Field::RoomObjectLockState{}};
  bool has_item = false;
  bool previous_flag = false;
  bool unlock_succeeds = true;
  std::int16_t flag_value = 0;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::RoomObjectLockState> State() const override {
    return state;
  }
  bool HasItem(std::int16_t) override {
    calls.push_back(0);
    return has_item;
  }
  bool PreviousUnlockFlag(std::int32_t) override {
    calls.push_back(1);
    return previous_flag;
  }
  void SetUnlockFlag(std::int32_t flag, std::int16_t value) override {
    calls.push_back(flag);
    flag_value = value;
  }
  void ConsumeItem(std::int16_t item) override { calls.push_back(item); }
  bool Unlock() override {
    calls.push_back(1000);
    if (unlock_succeeds && state) {
      state->lock_state = 0;
    }
    return unlock_succeeds;
  }
  void ShowNotification(std::int32_t kind, std::int16_t) override {
    calls.push_back(kind);
  }
  void PlayPose(const VS::Field::RoomObjectLockState&,
                std::uint8_t pose) override {
    calls.push_back(pose);
  }
  void BeginToggle() override { calls.push_back(1001); }
  void PlaySound(std::int32_t sound) override { calls.push_back(sound); }
  void RestoreIdle(std::int32_t actor) override {
    CHECK(actor == 0);
    calls.push_back(1002);
  }
};

struct RoomObjectRecordFixture {
  Field_RoomRecord saved = Field_roomData;
  std::array<Field_MpdLockTrigger, 2> locks{};

  RoomObjectRecordFixture() {
    Field_roomData.lockTriggers = locks.data();
    Field_roomData.header.lockTriggersLength = 0x28;
  }
  explicit RoomObjectRecordFixture(std::span<Field_MpdLockTrigger> records) {
    Field_roomData.lockTriggers = records.data();
    Field_roomData.header.lockTriggersLength =
        static_cast<std::uint32_t>(records.size_bytes());
  }
  ~RoomObjectRecordFixture() {
    Field_roomData = saved;
    VS::Field::InvalidateLiveInteractionTargets();
  }
  void CheckEightTickDoorCompletion() {
    for (std::size_t tick = 0; tick < 7; ++tick) {
      Field_UpdateGimmickAnimationQueueEntry();
      CHECK(locks[0].isOpen == 0);
    }
    Field_UpdateGimmickAnimationQueueEntry();
    Field_UpdateGimmickAnimationQueueEntry();
    CHECK(locks[0].isOpen == 1);
    CHECK(locks[0].isTriggered == 0);
    CHECK(Field_pendingGimmickAnimationCount == 0);
  }
};

struct UnlockEffectLiveFixture {
  RoomObjectRecordFixture room;
  decltype(Battle_characterState) saved_character = Battle_characterState;
  decltype(Battle_floatingLabels) saved_labels = Battle_floatingLabels;
  decltype(Battle_floatingLabelCount) saved_count = Battle_floatingLabelCount;
  decltype(Audio_Seq_unusedStateFlags) saved_flags = Audio_Seq_unusedStateFlags;
  VS::Core::Rand15 saved_rand = VS::Core::BiosRand();
  std::array<std::int32_t, VS::Core::Random::TableSize> saved_shuffle{};
  decltype(Core_randomShuffleIndex) saved_index = Core_randomShuffleIndex;
  Battle_Actor character{};
  Battle_ActorStats stats{};
  std::array<Items_TreasureChest, 1> chests{};
  std::remove_pointer_t<decltype(Battle_floatingLabels)> labels{};

  UnlockEffectLiveFixture() {
    std::ranges::copy(Core_randomShuffleTable, saved_shuffle.begin());
    Battle_characterState = &character;
    character.stats = &stats;
    stats.risk = std::numeric_limits<decltype(stats.risk)>::max();
    Battle_floatingLabels = &labels;
    Battle_floatingLabelCount = 0;
    Field_roomData.containers = chests.data();
    Field_roomData.header.containersLength = sizeof chests;
    std::memset(&Audio_Seq_unusedStateFlags, 0,
                sizeof(Audio_Seq_unusedStateFlags));
    room.locks[0].trigger.lockState.lock.state = 2;
    chests[0].lockLevel = 3;
  }
  ~UnlockEffectLiveFixture() {
    Battle_characterState = saved_character;
    Battle_floatingLabels = saved_labels;
    Battle_floatingLabelCount = saved_count;
    Audio_Seq_unusedStateFlags = saved_flags;
    VS::Core::BiosRand() = saved_rand;
    std::ranges::copy(saved_shuffle, std::begin(Core_randomShuffleTable));
    Core_randomShuffleIndex = saved_index;
  }
};

struct SigilDoorLiveFixture {
  decltype(Field_roomData) saved_room = Field_roomData;
  decltype(Audio_Seq_unusedStateFlags) saved_flags = Audio_Seq_unusedStateFlags;
  std::array<std::int16_t, std::size(Field_lockedDoorIndexRanges)>
      saved_ranges{};
  using Pairs =
      std::array<std::array<char, 4>, std::size(Field_lockedDoorRoomPairs)>;
  Pairs saved_pairs{};
  std::array<std::uint32_t, 4> scene{1, 1, 0, 9 | (247u << 16)};

  SigilDoorLiveFixture() {
    std::ranges::copy(Field_lockedDoorIndexRanges, saved_ranges.begin());
    std::ranges::fill(Field_lockedDoorIndexRanges, 0);
    for (std::size_t index = 0; index < saved_pairs.size(); ++index) {
      std::ranges::copy(Field_lockedDoorRoomPairs[index],
                        saved_pairs[index].begin());
      std::ranges::fill(Field_lockedDoorRoomPairs[index], 0);
    }
    std::ranges::fill(Audio_Seq_unusedStateFlags.opaque186, 0);
    Field_lockedDoorIndexRanges[1] = 2;
    for (std::size_t index = 0; index < 2; ++index) {
      Field_lockedDoorRoomPairs[index][0] = 9;
      Field_lockedDoorRoomPairs[index][1] =
          std::bit_cast<char>(std::uint8_t{247});
      Field_lockedDoorRoomPairs[index][2] = static_cast<char>(10 + index);
      Field_lockedDoorRoomPairs[index][3] = static_cast<char>(1 + index);
    }
    Field_roomData = {};
    Field_roomData.minimapScene =
        reinterpret_cast<decltype(Field_roomData.minimapScene)>(scene.data());
    Field_roomData.header.minimapSceneLength = sizeof scene;
  }

  ~SigilDoorLiveFixture() {
    std::ranges::copy(saved_ranges, std::begin(Field_lockedDoorIndexRanges));
    for (std::size_t index = 0; index < saved_pairs.size(); ++index) {
      std::ranges::copy(saved_pairs[index],
                        std::begin(Field_lockedDoorRoomPairs[index]));
    }
    Audio_Seq_unusedStateFlags = saved_flags;
    Field_roomData = saved_room;
  }
};

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

struct ActorTileTriggerLiveFixture {
  Battle_Actor* saved_actor = Battle_actors[2];
  Actors_ActorRecord* saved_record = Actors_actorRecords[2];
  decltype(Field_roomData) saved_room = Field_roomData;
  decltype(Field_controlMode) saved_control = Field_controlMode;
  std::unique_ptr<Battle_Actor> actor = std::make_unique<Battle_Actor>();
  std::unique_ptr<Actors_ActorRecord> record =
      std::make_unique<Actors_ActorRecord>();

  ActorTileTriggerLiveFixture() {
    Battle_actors[2] = actor.get();
    Actors_actorRecords[2] = record.get();
    Field_roomData = {};
    Field_controlMode = 3;
  }
  ~ActorTileTriggerLiveFixture() {
    Battle_actors[2] = saved_actor;
    Actors_actorRecords[2] = saved_record;
    Field_roomData = saved_room;
    Field_controlMode = saved_control;
  }
};

struct ScriptedFallDoorFixture {
  ActorTileTriggerLiveFixture actors;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{256} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<Battle_Actor*, std::size(Battle_actors)> saved_actors{};
  std::array<Actors_ActorRecord*, std::size(Actors_actorRecords)>
      saved_records{};
  VS::Field::RoomTransitionState saved_transition =
      VS::Field::LoadLiveRoomTransition();
  Field_MpdDoorRecord saved_entrance = Field_entranceDoorRecord;
  decltype(Field_zoneContext) saved_zone = Field_zoneContext;
  Field_MpdCollisionHeader collision{
      .width = 2, .height = 2, .heightOrigin = 0, .cellCount = 4};
  std::array<Field_MpdTileCell, 4> cells{};
  Field_MpdEnvironment environment{};

  ScriptedFallDoorFixture() {
    REQUIRE(saved_zone.zndData == nullptr);
    REQUIRE(saved_zone.zndDiscRequest == nullptr);
    std::ranges::copy(Battle_actors, saved_actors.begin());
    std::ranges::copy(Actors_actorRecords, saved_records.begin());
    std::ranges::fill(Battle_actors, nullptr);
    std::ranges::fill(Actors_actorRecords, nullptr);
    Battle_actors[2] = actors.actor.get();
    Actors_actorRecords[2] = actors.record.get();
    VS::Field::StoreLiveRoomTransition({});
    Field_zoneContext = {};
    Field_zoneContext.zndId = 9;
    cells[3].isPit = 1;
    environment.fallDestinationZone = 9;
    environment.fallDestinationRoom = 3;
    Field_roomData.collisionMap = &collision;
    Field_roomData.tileCells = cells.data();
    Field_roomData.environment = &environment;
    Field_roomData.header.collisionMapLength = sizeof collision;
    Field_roomData.header.tileCellsLength = sizeof cells;
    Field_roomData.header.environmentLength = sizeof environment;
  }
  ~ScriptedFallDoorFixture() {
    if (Field_zoneContext.zndDiscRequest != nullptr) {
      VS::Content::ReleaseLiveDiscRequest(Field_zoneContext.zndDiscRequest);
    }
    if (Field_zoneContext.zndData != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(Field_zoneContext.zndData);
    }
    VS::Field::StoreLiveRoomTransition(saved_transition);
    Field_entranceDoorRecord = saved_entrance;
    Field_zoneContext = saved_zone;
    Field_roomData = actors.saved_room;
    std::ranges::copy(saved_actors, Battle_actors);
    std::ranges::copy(saved_records, Actors_actorRecords);
  }
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
  REQUIRE(Field_GimmickPlacementEntry(slot, &output) == 0);
  CHECK(output.placementByte0 == 5);
  CHECK(output.placementByte2 == 9);
  CHECK(output.orientation == 3);
  CHECK(output.heightAboveGround == expected_height);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) & 0xFFFF) == 0xFB2E);
  CHECK((std::bit_cast<std::uint32_t>(output.positionXY) >> 16) ==
        static_cast<std::uint16_t>(object.positionY));
  CHECK(std::bit_cast<std::uint32_t>(output.positionZ) == 0x13570929);
}

struct RecordingGimmickHost final : VS::Field::GimmickPlacementHost {
  static constexpr std::size_t kSlots = 16;
  std::array<std::optional<VS::Field::GimmickPlacementState>, kSlots> states{};
  mutable std::size_t reads = 0;
  std::size_t support_calls = 0;
  std::int32_t support_height = 0;

  std::size_t GimmickSlots() const override { return states.size(); }
  std::optional<VS::Field::GimmickPlacementState> Gimmick(
      std::size_t slot) const override {
    ++reads;
    return states.at(slot);
  }
  std::int32_t SupportHeight(const VS::Field::GimmickPlacementState&) override {
    ++support_calls;
    return support_height;
  }
};

std::uint8_t OriginalGimmickHeight(std::int32_t support_height,
                                   std::int16_t position_y) {
  // CPU 800A00BC subu, 800A00C0 bgez, 800A00C8 addiu127, 800A00CC sra7.
  const auto difference =
      static_cast<std::int64_t>(support_height) - position_y;
  const auto wrapped =
      std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
  const auto rounded =
      wrapped < 0 ? static_cast<std::int64_t>(wrapped) + 127 : wrapped;
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(rounded) >> 7);
}

void CheckTypedGimmickPlacement(RecordingGimmickHost& host,
                                const VS::Field::GimmickPlacementState& state) {
  const auto snapshot = VS::Field::GimmickPlacement(0, host);
  if (!snapshot) {
    FAIL("Stationary gimmick placement must produce a snapshot");
    return;
  }
  REQUIRE(snapshot);
  CHECK(snapshot->tile_x == 0xFF);
  CHECK(snapshot->tile_z == 9);
  CHECK(snapshot->orientation == 0x81);
  CHECK(snapshot->height_above_ground ==
        OriginalGimmickHeight(host.support_height, state.position_y));
  CHECK(
      std::bit_cast<std::uint32_t>(snapshot->position_xy) ==
      (0x0000FB2E |
       (static_cast<std::uint32_t>(static_cast<std::uint16_t>(state.position_y))
        << 16)));
  CHECK(std::bit_cast<std::uint32_t>(snapshot->position_z_tail) == 0xFEDC0929);
}

}  // namespace

namespace {

struct RecordingAnimationQueueHost final
    : VS::Field::GimmickAnimationQueueHost {
  bool target = true;
  bool refuse_store = false;
  VS::Field::GimmickAnimationOccupancy slots{};
  mutable std::size_t reads = 0;
  std::size_t stores = 0;
  std::size_t count_writes = 0;
  std::size_t pending = 0;
  std::optional<std::size_t> stored_slot;

  bool HasRequestedTarget() const override { return target; }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    ++reads;
    return slots;
  }
  bool StoreRequestedJob(std::size_t slot,
                         VS::Field::GimmickAnimationKind) override {
    if (refuse_store) {
      return false;
    }
    ++stores;
    stored_slot = slot;
    slots.at(slot) = true;
    return true;
  }
  void SetPendingCount(std::size_t count) override {
    ++count_writes;
    pending = count;
  }
};

struct RecordingAnimationDispatchHost final
    : VS::Field::GimmickAnimationDispatchHost {
  std::array<std::optional<VS::Field::GimmickAnimationJobSnapshot>,
             VS::Field::kGimmickAnimationSlots>
      jobs{};
  std::vector<std::int32_t> calls;
  bool finish_ready = true;
  bool replace_during_finish = false;
  bool reset_during_finish = false;

  std::int32_t PendingCount() const override {
    return static_cast<std::int32_t>(std::ranges::count_if(
        jobs, [](const auto& job) { return job.has_value(); }));
  }
  VS::Field::GimmickAnimationOccupancy Occupancy() const override {
    VS::Field::GimmickAnimationOccupancy occupied{};
    for (std::size_t slot = 0; slot < jobs.size(); ++slot) {
      occupied[slot] = jobs[slot].has_value();
    }
    return occupied;
  }
  std::optional<VS::Field::GimmickAnimationJobSnapshot> Job(
      std::size_t slot) const override {
    return jobs.at(slot);
  }
  bool Matches(std::size_t slot,
               const VS::Field::GimmickAnimationJobSnapshot& expected) const {
    const auto& job = jobs.at(slot);
    return job && job->serial == expected.serial;
  }
  bool StoreCountdown(std::size_t slot,
                      const VS::Field::GimmickAnimationJobSnapshot& expected,
                      std::uint8_t countdown) override {
    if (!Matches(slot, expected)) {
      return false;
    }
    if (auto& job = jobs.at(slot)) {
      job->countdown = countdown;
    }
    return true;
  }
  bool FinishDoor(std::size_t slot,
                  const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(1);
    if (reset_during_finish) {
      for (auto& job : jobs) {
        job.reset();
      }
    }
    if (replace_during_finish) {
      if (auto& job = jobs.at(slot)) {
        ++job->serial;
      }
    }
    return finish_ready;
  }
  void ToggleSwitch(std::size_t,
                    const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(2);
  }
  void StartDoor(std::size_t,
                 const VS::Field::GimmickAnimationJobSnapshot&) override {
    calls.push_back(3);
  }
  bool RetireJob(
      std::size_t slot,
      const VS::Field::GimmickAnimationJobSnapshot& expected) override {
    if (!Matches(slot, expected)) {
      return false;
    }
    calls.push_back(4);
    jobs.at(slot).reset();
    return true;
  }
};

struct RecordingAnimationResetHost final
    : VS::Field::GimmickAnimationResetHost {
  std::vector<std::size_t> calls;
  void ClearPendingCount() override { calls.push_back(8); }
  void ClearMarkerTicks() override { calls.push_back(9); }
  void ClearJob(std::size_t slot) override { calls.push_back(slot); }
};

struct AnimationQueueFixture {
  static constexpr std::size_t kSlots = 8;
  std::array<Field_GimmickAnimationJob, kSlots> saved{};
  Field_RoomRecord saved_room = Field_roomData;
  std::int16_t pending = Field_pendingGimmickAnimationCount;
  std::uint16_t marker_ticks = Field_triggerMarkerFrameTicks;

  AnimationQueueFixture() {
    std::ranges::copy(Field_gimmickAnimationQueue, saved.begin());
    Field_ResetGimmickAnimationQueueEntry();
  }
  ~AnimationQueueFixture() {
    Field_roomData = saved_room;
    VS::Field::InvalidateLiveInteractionTargets();
    std::ranges::copy(saved, Field_gimmickAnimationQueue);
    Field_pendingGimmickAnimationCount = pending;
    Field_triggerMarkerFrameTicks = marker_ticks;
  }
  AnimationQueueFixture(const AnimationQueueFixture&) = delete;
  AnimationQueueFixture& operator=(const AnimationQueueFixture&) = delete;
  static void BindDoors(std::span<Field_MpdLockTrigger> targets) {
    Field_roomData.lockTriggers = targets.data();
    Field_roomData.header.lockTriggersLength =
        static_cast<std::uint32_t>(targets.size_bytes());
    VS::Field::InvalidateLiveInteractionTargets();
  }
};

struct ChestTargetFixture {
  Field_RoomRecord saved = Field_roomData;
  std::uint32_t control_mode = Field_controlMode;
  decltype(Battle_actionFlowMode) action_flow = Battle_actionFlowMode;
  Actors_ActorLocationQuery actor_query = Field_interactionFocus.actor_query;
  AnimationQueueFixture queue;
  std::array<Items_TreasureChest, 1> chests{};

  ChestTargetFixture() {
    Field_roomData.containers = chests.data();
    Field_roomData.header.containersLength = 0x234;
    VS::Field::InvalidateLiveInteractionTargets();
    chests[0].tileX = -1;
    chests[0].tileZ = -1;
    chests[0].layer = -1;
    chests[0].lidRotationId = -1;
  }
  ~ChestTargetFixture() {
    Field_roomData = saved;
    VS::Field::InvalidateLiveInteractionTargets();
    Field_controlMode = control_mode;
    Battle_actionFlowMode = action_flow;
    Field_interactionFocus.actor_query = actor_query;
  }
};

void ConfigureOneFrameDelayedDoor(Field_MpdLockTrigger& target) {
  target.trigger.tileX = -1;
  target.trigger.tileZ = -1;
  target.trigger.floorLayer = -1;
  target.trigger.lockState.lock.roomEffect = -1;
  target.linkedLockIndex = -1;
  target.isTriggered = 1;
  target.delayFrames = 1;
}

void QueueOneFrameSwitchJobs(std::span<Field_MpdLockTrigger> targets) {
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    auto& target = targets[slot];
    target.trigger.tileX = -1;
    target.trigger.tileZ = -1;
    target.trigger.floorLayer = -1;
    target.trigger.kind = -1;
    target.trigger.activation = 1;
    Field_QueueGimmickAnimationEntry(2, &target);
  }
  Field_QueueGimmickAnimationEntry(2, &targets.back());
}

void CheckDrainedSwitchJobs(std::span<const Field_MpdLockTrigger> targets) {
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    CHECK(targets[slot].trigger.activation == 0);
    CHECK(targets[slot].trigger.lockState.switchValue == 1);
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 0);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == nullptr);
  }
}

void CheckAnimationKindResets(Field_MpdLockTrigger& target) {
  for (const auto kind : {1, 2, 3}) {
    Field_QueueGimmickAnimationEntry(kind, &target);
    CHECK(Field_pendingGimmickAnimationCount == 1);
    Field_ResetGimmickAnimationQueueEntry();
    CHECK(Field_pendingGimmickAnimationCount == 0);
  }
}

enum class SwitchCallKind { kSound, kEffect, kStore, kCell };

struct SwitchCall {
  SwitchCallKind kind = SwitchCallKind::kSound;
  std::int32_t value = 0;
  friend bool operator==(const SwitchCall&, const SwitchCall&) = default;
};

struct RecordingDoorCollisionHost final : VS::Field::RoomDoorCollisionHost {
  std::optional<VS::Field::RoomDoorPlacement> state;
  std::vector<VS::Field::RoomDoorTileQuery> cells;
  std::vector<std::uint8_t> masks;
  bool invalidate = false;
  std::uint32_t packed_cell = 0xA5A5A5A5;

  std::optional<VS::Field::RoomDoorPlacement> State() const override {
    return state;
  }
  void ToggleCell(const VS::Field::RoomDoorTileQuery& query,
                  std::uint8_t xor_mask) override {
    cells.push_back(query);
    masks.push_back(xor_mask);
    constexpr unsigned kPassabilityShift = 10;
    packed_cell ^= static_cast<std::uint32_t>(xor_mask) << kPassabilityShift;
    if (invalidate) state.reset();
  }
};

struct RecordingDoorAnimationHost final : VS::Field::RoomDoorAnimationHost {
  std::optional<VS::Field::RoomDoorAnimationState> primary{
      VS::Field::RoomDoorAnimationState{}};
  std::optional<VS::Field::RoomDoorAnimationState> linked;
  std::vector<std::int32_t> calls;
  std::optional<std::uint32_t> marked;
  bool queue_available = true;
  bool accept_queue = true;
  bool accept_store = true;
  bool active_effect = false;
  bool invalidate_effect = false;

  VS::Field::RoomDoorAnimationState& Primary() {
    if (!primary) {
      FAIL("Missing primary door fixture");
      throw std::bad_optional_access{};
    }
    return *primary;
  }
  VS::Field::RoomDoorAnimationState& Linked() {
    if (!linked) {
      FAIL("Missing linked door fixture");
      throw std::bad_optional_access{};
    }
    return *linked;
  }

  std::optional<VS::Field::RoomDoorAnimationState> State(
      VS::Field::DoorMember member) const override {
    return member == VS::Field::DoorMember::kPrimary ? primary : linked;
  }
  bool QueueAvailable() const override { return queue_available; }
  void PlaySound(std::int32_t sound) override { calls.push_back(sound); }
  void StartEffect(std::uint8_t effect) override {
    calls.push_back(1000 + effect);
    if (invalidate_effect) {
      primary.reset();
      linked.reset();
    }
  }
  bool EffectActive(std::uint8_t) const override { return active_effect; }
  bool ToggleCollision(VS::Field::DoorMember member) override {
    calls.push_back(2000 + static_cast<std::int32_t>(member));
    return State(member).has_value();
  }
  bool StoreBusy(VS::Field::DoorMember member, std::uint8_t busy) override {
    auto& state = member == VS::Field::DoorMember::kPrimary ? primary : linked;
    if (!accept_store || !state) return false;
    state->busy = busy;
    calls.push_back(3000 + 10 * static_cast<std::int32_t>(member) + busy);
    return true;
  }
  bool StoreDelay(std::uint8_t delay) override {
    if (!accept_store || !primary) return false;
    primary->delay = delay;
    calls.push_back(4000 + delay);
    return true;
  }
  bool StoreOpen(VS::Field::DoorMember member, std::int16_t value,
                 std::uint8_t busy) override {
    auto& state = member == VS::Field::DoorMember::kPrimary ? primary : linked;
    if (!accept_store || !state) return false;
    state->open_value = value;
    state->busy = busy;
    calls.push_back(5000 + 10 * static_cast<std::int32_t>(member) + value);
    return true;
  }
  bool Queue(VS::Field::GimmickAnimationKind kind) override {
    calls.push_back(6000 + static_cast<std::int32_t>(kind));
    return accept_queue;
  }
  void MarkSigil(std::uint32_t sigil_index) override {
    marked = sigil_index;
    calls.push_back(7000 + static_cast<std::int32_t>(sigil_index));
  }
};

struct RecordingSwitchHost final : VS::Field::RoomSwitchHost {
  std::optional<VS::Field::RoomSwitchState> state;
  std::optional<VS::Field::RoomSwitchState> after_effect;
  bool remove_after_sound = false;
  std::vector<SwitchCall> calls;
  std::array<std::int32_t, 3> cell_query{};

  std::optional<VS::Field::RoomSwitchState> State() const override {
    return state;
  }
  void PlaySound(std::int32_t sound) override {
    calls.push_back({SwitchCallKind::kSound, sound});
    if (remove_after_sound) {
      state.reset();
    }
  }
  void StartEffect(std::int32_t kind) override {
    calls.push_back({SwitchCallKind::kEffect, kind});
    if (after_effect) {
      state = after_effect;
    }
  }
  void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) override {
    calls.push_back({SwitchCallKind::kStore, value});
    if (state) {
      state->switch_value = value;
      state->marker_fade = marker_fade;
    }
  }
  void SetCellPassability(const VS::Field::RoomSwitchState& query,
                          std::uint8_t passability) override {
    calls.push_back({SwitchCallKind::kCell, passability});
    cell_query = {query.tile_x, query.tile_z, query.floor_layer};
  }
};

struct RecordingSwitchStartHost final : VS::Field::RoomSwitchStartHost {
  std::optional<VS::Field::RoomSwitchState> state{VS::Field::RoomSwitchState{}};
  bool refuse = false;
  bool refuse_store = false;
  std::vector<std::int32_t> calls;

  std::optional<VS::Field::RoomSwitchState> State() const override {
    return state;
  }
  bool StoreStartState(std::uint8_t marker, std::uint8_t activation) override {
    if (refuse_store || !state) {
      return false;
    }
    state->marker_fade = marker;
    state->activation = activation;
    calls.push_back(marker);
    calls.push_back(activation);
    return true;
  }
  bool QueueToggle() override {
    calls.push_back(2);
    return !refuse;
  }
  bool ToggleImmediately() override {
    calls.push_back(3);
    return !refuse;
  }
};

void CheckOriginalCountdownByte(VS::Field::GimmickAnimationKind kind,
                                int byte) {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      kind, static_cast<std::uint8_t>(byte), 1, true};
  const int original_decrement = byte - 1;
  const auto original_byte = static_cast<std::uint8_t>(original_decrement);
  const bool complete = kind == VS::Field::GimmickAnimationKind::kToggleSwitch
                            ? std::bit_cast<std::int8_t>(original_byte) <= 0
                            : original_byte == 0;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) ==
        static_cast<std::size_t>(complete));
  CHECK(host.PendingCount() == static_cast<std::int32_t>(!complete));
  if (const auto& job = host.jobs[0]) {
    CHECK(job->countdown == original_byte);
  }
}

void CheckChestOpenAndBlockedRules(VS::Field::ChestTarget target) {
  RecordingChestInteractionHost open;
  CHECK(VS::Field::TryOpenChest(target, open));
  CHECK(open.calls == std::vector<std::int32_t>{2, 1002});
  RecordingChestInteractionHost blocked;
  blocked.chest = VS::Field::ChestLockState{.lock_level = 2};
  CHECK_FALSE(VS::Field::TryOpenChest(target, blocked));
  CHECK(blocked.calls == std::vector<std::int32_t>{3});
  blocked.chest.reset();
  CHECK_FALSE(VS::Field::TryOpenChest(target, blocked));
}

void CheckChestLockedItemRules(VS::Field::ChestTarget target) {
  RecordingChestInteractionHost locked;
  locked.chest =
      VS::Field::ChestLockState{.lock_level = 1, .required_item = 0x1D2};
  CHECK_FALSE(VS::Field::TryOpenChest(target, locked));
  CHECK(locked.calls == std::vector<std::int32_t>{3});
  locked.item = true;
  locked.calls.clear();
  CHECK(VS::Field::TryOpenChest(target, locked));
  CHECK(locked.calls ==
        std::vector<std::int32_t>{0x388, 0x1D2, 1000, 1001, 2, 1002});
}

void CheckChestBeginFinishRules(RecordingChestInteractionHost& host,
                                VS::Field::ChestTarget target) {
  CHECK(VS::Field::BeginChestInteraction(target, 0, host));
  CHECK(host.selected == target);
  CHECK(host.calls == std::vector<std::int32_t>{1010, 1012, 1, 1013});
  host.calls.clear();
  CHECK(VS::Field::FinishChestInteraction(host));
  CHECK(host.calls == std::vector<std::int32_t>{1011, 4, 1014});
  host.calls.clear();
}

void CheckChestContentsRules(RecordingChestInteractionHost& host) {
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{1015, 1017});
  host.calls.clear();
  host.chest = VS::Field::ChestLockState{.lock_level = 3, .required_item = 9};
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{3, 1016});
  host.calls.clear();
  host.chest = VS::Field::ChestLockState{.opened_flag = 67};
  CHECK(VS::Field::OpenChestContents(host));
  CHECK(host.calls == std::vector<std::int32_t>{3, 1017});
  host.selected.reset();
  CHECK_FALSE(VS::Field::OpenChestContents(host));
}

void CheckTargetKindSelection(VS::Field::InteractionTargetState& state) {
  state.Select(VS::Field::ChestTarget{3, 7});
  CHECK(state.Trap() == nullptr);
  REQUIRE(state.Chest());
  CHECK(*state.Chest() == VS::Field::ChestTarget{3, 7});
  state.Select(VS::Field::TrapTarget{4, 8});
  CHECK(state.Chest() == nullptr);
  REQUIRE(state.Trap());
  CHECK(*state.Trap() == VS::Field::TrapTarget{4, 8});
}

void CheckTargetDomainBounds(VS::Field::InteractionTargetDomain domain) {
  CHECK(VS::Field::ResolveTrapTarget({1, 7}, domain) == 1);
  CHECK_FALSE(VS::Field::ResolveTrapTarget({2, 7}, domain));
  CHECK_FALSE(VS::Field::ResolveTrapTarget({0, 6}, domain));
  CHECK(VS::Field::ResolveChestTarget({2, 7}, domain) == 2);
  CHECK_FALSE(VS::Field::ResolveChestTarget({3, 7}, domain));
  CHECK_FALSE(VS::Field::ResolveChestTarget({0, 8}, domain));
}

void CheckSelectedTargetDomain(VS::Field::InteractionTargetDomain domain) {
  VS::Field::InteractionTargetState state;
  state.Select(VS::Field::ChestTarget{2, 7});
  CHECK(state.Chest(domain) != nullptr);
  CHECK(state.Trap(domain) == nullptr);
  CHECK(state.Chest({8, 2, 3}) == nullptr);
  CHECK(state.Chest({7, 2, 2}) == nullptr);
}

}  // namespace

TEST_CASE("Typed chest interaction keeps lock item and pose rules separate",
          "[chest-interaction-core][field]") {
  const VS::Field::ChestTarget target{0, 1};
  CheckChestOpenAndBlockedRules(target);
  CheckChestLockedItemRules(target);
}

namespace {

void CheckStagedTrapVictim(const RecordingTrapActionHost& host,
                           const VS::Field::TrapActionInput& input) {
  if (!host.staged) {
    FAIL("Trap action must retain staged victim values");
    return;
  }
  REQUIRE(host.staged);
  CHECK(host.staged->actor == 3);
  CHECK(host.staged->default_limb == 7);
  CHECK(host.staged->position == input.position);
}

void CheckStagedTrapEffect(const RecordingTrapActionHost& host) {
  if (!host.effect) {
    FAIL("Trap action must retain initialized effect values");
    return;
  }
  REQUIRE(host.effect);
  CHECK(host.effect->effect_id == 6);
  CHECK(host.effect->actor == 3);
  CHECK(host.effect->ground_anchor == 0xFD);
  CHECK_FALSE(host.effect->source_position);
}

void CheckTrapCompatibilitySource(const Effects_ActionEffectRequest& request) {
  CHECK(request.source.type == 5);
  CHECK(request.source.colorPreset == 0);
  CHECK(request.source.value.position.vx == 0);
  CHECK(request.source.value.position.vy == 0);
  CHECK(request.source.value.position.vz == 0);
  CHECK(request.source.value.position.pad == 0);
}

void CheckTrapCompatibilityVictim(const Effects_ActionEffectRequest& request) {
  CHECK(request.targets[0].type == 4);
  CHECK(request.targets[0].colorPreset == 0);
  CHECK(request.targets[0].value.actor.actor == 3);
  CHECK(request.targets[0].value.actor.part == 0xFD);
  CHECK(request.targets[0].value.actor.eventActorSlot == 0);
  CHECK(request.targets[0].value.actor.reserved3 == 0);
}

void CheckInvalidTrapActors(RecordingTrapActionHost& host) {
  for (const auto actor : {-1, 17}) {
    CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = actor}, host));
    CHECK(host.calls.empty());
  }
}

void CheckInvalidTrapKinds(RecordingTrapActionHost& host) {
  for (const auto kind : {-1, 16}) {
    host.kind = static_cast<std::int16_t>(kind);
    CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
    CHECK(host.calls.empty());
  }
}

}  // namespace

TEST_CASE("Trap action stages victim then resolves before initialized effect",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  const VS::Field::TrapActionInput input{
      .actor = 3, .default_limb = 7, .position = {123, -234, 345}};
  REQUIRE(VS::Field::TriggerActorTrap({0, 1}, input, host));
  CHECK(host.calls == std::vector<std::int32_t>{1000, 6, 1001});
  CheckStagedTrapVictim(host, input);
  CheckStagedTrapEffect(host);
}

TEST_CASE("Trap effect compatibility record initializes every supplied field",
          "[trap-action-live][field]") {
  const auto request = VS::Field::LiveTrapEffectRecord(
      {.effect_id = 6, .actor = 3, .source_position = std::nullopt});
  CHECK(request.effectId == 6);
  CHECK(request.count == 1);
  CHECK(request.otherCount == 0);
  CheckTrapCompatibilitySource(request);
  CheckTrapCompatibilityVictim(request);
  CHECK(request.targets[1].type == 0);
  CHECK(request.others[0] == 0);
  CHECK(request.position.vx == 0);
  CHECK(request.position.vy == 0);
  CHECK(request.position.vz == 0);
}

TEST_CASE("Trap effect compatibility record retains explicitly supplied source",
          "[trap-action-live][field]") {
  const auto request = VS::Field::LiveTrapEffectRecord(
      {.effect_id = 9, .actor = 0, .source_position = {{123, -234, 345}}});
  CHECK(request.source.value.position.vx == 123);
  CHECK(request.source.value.position.vy == -234);
  CHECK(request.source.value.position.vz == 345);
}

TEST_CASE("Trap action refuses invalid actor state or kind before mutation",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  CheckInvalidTrapActors(host);
  CheckInvalidTrapKinds(host);
  host.kind = 0;
  host.action_state = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
  CHECK(host.calls.empty());
}

TEST_CASE("Trap action reobserves target after resolver callback",
          "[trap-action-core][field]") {
  RecordingTrapActionHost host;
  host.change_during_resolve = true;
  REQUIRE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, host));
  if (!host.effect) {
    FAIL("Resolver must publish the reobserved trap effect");
    return;
  }
  REQUIRE(host.effect);
  CHECK(host.effect->effect_id == 7);
  RecordingTrapActionHost removed;
  removed.remove_during_resolve = true;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, removed));
  CHECK(removed.calls == std::vector<std::int32_t>{1000, 6});
  CHECK_FALSE(removed.effect);
}

TEST_CASE("Trap action stops on refused staging or resolution",
          "[trap-action-core][field]") {
  RecordingTrapActionHost staged;
  staged.stage_succeeds = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, staged));
  CHECK(staged.calls == std::vector<std::int32_t>{1000});
  RecordingTrapActionHost resolved;
  resolved.resolve_succeeds = false;
  CHECK_FALSE(VS::Field::TriggerActorTrap({0, 1}, {.actor = 0}, resolved));
  CHECK(resolved.calls == std::vector<std::int32_t>{1000, 6});
  CHECK_FALSE(resolved.effect);
}

TEST_CASE("Native trap action preserves its original module binding",
          "[trap-action-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073D30) ==
        reinterpret_cast<const void*>(&Field_TriggerActorTrapEntry));
}

TEST_CASE("Native trap action refuses invalid actor or stale target",
          "[trap-action-live][field]") {
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, -1));
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, 17));
  CHECK_FALSE(VS::Field::TriggerLiveActorTrap({}, {}, 0));
  Field_TriggerActorTrapEntry(nullptr, nullptr, -1);
}

TEST_CASE("Interaction camera focus wait preserves actor gate short circuit",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  host.flags = 0x08000000;
  CHECK_FALSE(VS::Field::WaitForChestCameraFocus(host));
  host.speed = 7;
  const auto reads = host.flag_reads;
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  CHECK(host.flag_reads == reads);
  CHECK(host.mode == 0);
  CHECK(host.requested_speed == 0x28);
}

TEST_CASE("Interaction camera missing focus performs no camera step",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  host.focus.reset();
  CHECK(VS::Field::WaitForChestCameraFocus(host));
  CHECK_FALSE(VS::Field::StepTrapCameraFocus(host));
  CHECK(host.calls.empty());
  CHECK(host.flag_reads == 0);
}

TEST_CASE("Chest animation readiness observes notification only after actor",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  CHECK_FALSE(VS::Field::IsChestAnimationReady(host));
  CHECK(host.calls.empty());
  host.flags = 0x08000000;
  CHECK(VS::Field::IsChestAnimationReady(host));
  host.pending = true;
  CHECK_FALSE(VS::Field::IsChestAnimationReady(host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 1});
}

TEST_CASE("Interaction camera returns from Ashley with original halfword wrap",
          "[interaction-camera-core][field]") {
  RecordingInteractionCameraHost host;
  host.ashley = VS::Field::TrapPosition{123, -32768, -234};
  host.speed = 9;
  CHECK(VS::Field::TrackInteractionCameraToAshley(host) == 9);
  CHECK(host.stepped == VS::Field::TrapPosition{123, 32678, -234});
  CHECK(host.mode == 1);
  CHECK(host.calls == std::vector<std::int32_t>{0, 2});
  host.transition = true;
  CHECK(VS::Field::TrackInteractionCameraToAshley(host) == 0);
  host.ashley.reset();
  host.calls.clear();
  CHECK_FALSE(VS::Field::TrackInteractionCameraToAshley(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Interaction camera entries retain all original module addresses",
          "[interaction-camera-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074120) ==
        reinterpret_cast<const void*>(&Field_WaitForChestCameraFocusEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007418C) ==
        reinterpret_cast<const void*>(&Field_StepTrapCameraFocusEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007424C) ==
        reinterpret_cast<const void*>(&Field_IsChestAnimationReadyEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074374) ==
        reinterpret_cast<const void*>(
            &Field_TrackInteractionCameraToAshleyEntry));
}

TEST_CASE("Interaction camera entries refuse an absent target focus",
          "[interaction-camera-live][field]") {
  const auto saved = VS::Field::LiveInteractionTargetState();
  VS::Field::LiveInteractionTargetState().Clear();
  CHECK(Field_WaitForChestCameraFocusEntry() == 1);
  CHECK(Field_StepTrapCameraFocusEntry() == 0);
  VS::Field::LiveInteractionTargetState() = saved;
}

TEST_CASE("Facing tile rounds every heading into original cardinal quadrants",
          "[facing-tile-core][field]") {
  VS::Actors::ActorPlacementView actor{};
  actor.tile.x = 17;
  actor.tile.z = 23;
  actor.tile.layer = 2;
  for (std::uint32_t heading = 0; heading < 65536; ++heading) {
    actor.position.w =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(heading));
    const auto tile = VS::Field::TileInFrontOfActor(actor);
    const auto expected = ((heading + 0x200) & 0xFFF) >> 10;
    CHECK(static_cast<std::uint8_t>(tile.direction) == expected);
    CHECK(tile.layer == 2);
    CHECK(tile.x == (expected == 1 ? 16 : expected == 3 ? 18 : 17));
    CHECK(tile.z == (expected == 0 ? 22 : expected == 2 ? 24 : 23));
  }
}

TEST_CASE("Facing tile preserves original byte wrapping at room boundaries",
          "[facing-tile-core][field]") {
  VS::Actors::ActorPlacementView actor{};
  CHECK(VS::Field::TileInFrontOfActor(actor).z == 255);
  actor.position.w = 1024;
  CHECK(VS::Field::TileInFrontOfActor(actor).x == 255);
  actor.tile.x = 255;
  actor.tile.z = 255;
  actor.position.w = 2048;
  CHECK(VS::Field::TileInFrontOfActor(actor).z == 0);
  actor.position.w = 3072;
  CHECK(VS::Field::TileInFrontOfActor(actor).x == 0);
}

TEST_CASE("Facing tile entry preserves X layer Z and untouched fourth byte",
          "[facing-tile-live][field]") {
  Actors_ActorLocationQuery actor{};
  actor.location.packedTile.target.tileX = 17;
  actor.location.packedTile.target.tileZ = 23;
  actor.location.packedTile.target.tileLayer = 2;
  actor.location.position.pad = 1024;
  std::array<std::uint8_t, 4> output{0xA1, 0xA2, 0xA3, 0xA4};
  CHECK(Field_TileInFrontOfActorEntry(&actor, output.data()) == 1);
  CHECK(output == std::array<std::uint8_t, 4>{16, 2, 23, 0xA4});
  const auto saved = output;
  CHECK(Field_TileInFrontOfActorEntry(nullptr, output.data()) == -1);
  CHECK(output == saved);
  CHECK(Field_TileInFrontOfActorEntry(&actor, nullptr) == -1);
}

TEST_CASE("Facing tile entry retains original module address",
          "[facing-tile-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074798) ==
        reinterpret_cast<const void*>(&Field_TileInFrontOfActorEntry));
}

TEST_CASE("Facing event uses requested actor and computed adjacent tile",
          "[facing-event-core][field]") {
  RecordingFacingEventHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.x = 17;
  placement.tile.z = 23;
  placement.tile.layer = 2;
  placement.position.w = 1024;
  host.placement = placement;
  host.result = true;
  CHECK(VS::Field::TryTriggerFacingTileEvent(3, host));
  CHECK(host.observed_actor == 3);
  if (!host.evaluated) {
    FAIL("Facing interaction must receive the adjacent tile");
    return;
  }
  REQUIRE(host.evaluated);
  CHECK(host.evaluated->x == 16);
  CHECK(host.evaluated->z == 23);
  CHECK(host.evaluated->layer == 2);
  host.result = false;
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(3, host));
}

TEST_CASE("Ashley tile event wins before trap door and fatal pit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.cell = {.actor = {.room_trigger = true, .pit = true},
               .event_trigger = true,
               .docked_door = true};
  host.event_started = true;
  host.trap = VS::Field::ActorTileTrap{.target = {1, 2}, .kind = 16};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1, .spare = 192};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 15, 16, 20, 17, 18, 7});
  CHECK(host.last == VS::Actors::PackedTile{.x = 1, .z = 1});
}

TEST_CASE("Ashley trigger collision precedes transition and disabled refusal",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  VS::Actors::ActorPlacementView placement{};
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(-1, placement, host));
  CHECK(host.fall.calls.empty());
  host.flags = 0x30000;
  host.transitioning = true;
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12});
  CHECK(host.collision_tile == VS::Actors::PackedTile{.layer = 3});
  host.fall.calls.clear();
  host.transitioning = false;
  host.flags = 0x01000000;
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12});
}

TEST_CASE("Ashley ordinary trap wins before door and fatal pit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.cell = {.actor = {.room_trigger = true, .pit = true},
               .event_trigger = true,
               .docked_door = true};
  host.trap = VS::Field::ActorTileTrap{.target = {1, 2}, .kind = 3};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 15, 17, 19, 20});
  CHECK(host.last == placement.tile);
  CHECK(host.fall.hp == 21);
}

TEST_CASE("Ashley effect-only trap continues to physical-layer zone door",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.flags = 0x20000000;
  host.cell = {.actor = {.room_trigger = true}, .docked_door = true};
  host.trap = VS::Field::ActorTileTrap{.target = {1, 2}, .kind = 16};
  host.doors = {{.destination_zone_id = 10,
                 .destination_room_id = 4,
                 .required_flag_value = 1}};
  host.flag_value = 1;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .layer = 2, .z = 1};
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 17, 18, 21, 22, 6, 7, 8});
  CHECK(host.doors_tile == placement.tile);
  CHECK(host.fall.route == VS::Field::AshleyTransitionRoute::kZone);
  CHECK(host.fall.entrance == host.doors.front());
}

TEST_CASE("Ashley forced-layer door lookup obeys its separate suppression bit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.flags = 0x20030000;
  host.cell.docked_door = true;
  host.doors = {{.destination_zone_id = 9, .destination_room_id = 4}};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .layer = 2, .z = 1};
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13, 14, 20});
  CHECK(host.collision_tile == VS::Actors::PackedTile{.layer = 3});
  CHECK(host.last == placement.tile);
  host.flags = 0x30000;
  host.fall.calls.clear();
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.doors_tile == VS::Actors::PackedTile{.layer = 3});
  CHECK(host.fall.route == VS::Field::AshleyTransitionRoute::kRoom);
}

TEST_CASE("Ashley door chooses the first flag-accepted record",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.cell.docked_door = true;
  host.doors = {{.destination_room_id = 1, .required_flag_value = 2},
                {.destination_zone_id = 9,
                 .destination_room_id = 2,
                 .required_flag_value = 1}};
  host.flag_value = 1;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1};
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls ==
        std::vector<std::int32_t>{10, 11, 12, 13, 14, 21, 22, 22, 6, 7, 8});
  CHECK(host.fall.transition == host.doors[1]);
  CHECK(host.fall.entrance == host.doors[1]);
}

TEST_CASE("Ashley event-owned control caches before still handling a pit",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.event_control = true;
  host.cell = {.actor = {.room_trigger = true, .pit = true},
               .event_trigger = true,
               .docked_door = true};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13, 14, 20, 0,
                                                     1, 3, 5, 6, 2, 7, 8});
  CHECK(host.fall.hp == 1);
  CHECK(host.last == placement.tile);
}

TEST_CASE("Ashley cached tile ignores input spare without refreshing cache",
          "[ashley-tile-core][field]") {
  RecordingAshleyTileHost host;
  host.last = VS::Actors::PackedTile{.x = 1, .z = 1};
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 1, .z = 1, .spare = 192};
  CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
  CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13});
  CHECK(host.last == VS::Actors::PackedTile{.x = 1, .z = 1});
}

TEST_CASE("Ashley trap suppression retains original held-location mode writes",
          "[ashley-tile-core][field]") {
  constexpr std::array<std::uint32_t, 3> kSuppression{0x4000, 0x100000,
                                                      0x400000};
  for (const auto flags : kSuppression) {
    RecordingAshleyTileHost host;
    host.flags = flags;
    host.cell.actor.room_trigger = true;
    VS::Actors::ActorPlacementView placement{};
    placement.tile = {.x = 1, .z = 1, .spare = 192};
    CHECK_FALSE(VS::Field::CheckAshleyTileTriggers(2, placement, host));
    CHECK(host.fall.calls == std::vector<std::int32_t>{10, 11, 12, 13, 14, 20});
    auto expected =
        (flags & 0x104000) == 0 ? placement.tile : VS::Actors::PackedTile{};
    expected.spare = static_cast<std::uint8_t>((flags >> 14) & 1);
    CHECK(host.last == expected);
  }
}

TEST_CASE("Ashley fall refuses invalid actors nonpits and height sixteen",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(-1, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(VS::Field::CheckAshleyFall(17, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(VS::Field::CheckAshleyFall(2, placement, false, host) ==
        VS::Field::AshleyFallResult::kNone);
  placement.position.y = 16;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(host.calls.empty());
}

TEST_CASE("Ashley scripted fall transitions without reading HP",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.destination = VS::Field::ScriptedFallDestination{9, 3};
  host.hp.reset();
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  placement.tile = {.x = 1, .z = 1, .spare = 192};
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kScriptedTransition);
  CHECK(host.calls == std::vector<std::int32_t>{0, 6, 7, 8});
  CHECK(host.transition ==
        VS::Field::BuildScriptedFallDoor({9, 3}, placement.tile));
  CHECK_FALSE(host.entrance);
}

TEST_CASE("Ashley fatal fall keeps signed halfword HP and label ordering",
          "[ashley-fall-core][field]") {
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  constexpr std::array<std::int16_t, 4> kFatalHp{-32768, -1, 1, 20};
  for (const auto hp : kFatalHp) {
    RecordingAshleyFallHost host;
    host.hp = hp;
    CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
          VS::Field::AshleyFallResult::kHandled);
    CHECK(host.hp == 0);
    CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4});
  }
}

TEST_CASE("Ashley zero HP fall defeats without another value label",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.hp = 0;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kHandled);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 3, 4});
}

TEST_CASE("Ashley HP twenty-one loses twenty before reload transition",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kHandled);
  CHECK(host.hp == 1);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 3, 5, 6, 2, 7, 8});
  CHECK(host.transition == host.reload);
  CHECK(host.entrance == host.reload);
}

TEST_CASE("Ashley fall refuses absent HP before damage or transition",
          "[ashley-fall-core][field]") {
  RecordingAshleyFallHost host;
  host.hp.reset();
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1});
}

TEST_CASE("Scripted fall door values preserve original packed field widths",
          "[ashley-fall-core][field]") {
  const auto door = VS::Field::BuildScriptedFallDoor(
      {.zone_id = 9, .room_id = 3},
      {.x = 255, .layer = 255, .z = 254, .spare = 255});
  CHECK(door == VS::Field::DoorRecord{.destination_zone_id = 9,
                                      .destination_room_id = 3,
                                      .tile_x = 31,
                                      .tile_z = 30,
                                      .floor_layer = 7,
                                      .entry_direction = 7,
                                      .entry_tile_x = 31,
                                      .entry_tile_z = 30,
                                      .entry_elevation = 3});
}

TEST_CASE("Scripted fall entry masks every input byte to original field widths",
          "[ashley-fall-core][field]") {
  for (std::uint32_t value = 0; value <= UINT8_MAX; ++value) {
    const auto byte = static_cast<std::uint8_t>(value);
    const auto other = static_cast<std::uint8_t>(UINT8_MAX - value);
    const auto door = VS::Field::BuildScriptedFallDoor(
        {.zone_id = 9, .room_id = 3},
        {.x = byte, .layer = byte, .z = other, .spare = other});
    CHECK(door.tile_x == (value & 31));
    CHECK(door.entry_tile_x == (value & 31));
    CHECK(door.tile_z == ((UINT8_MAX - value) & 31));
    CHECK(door.entry_tile_z == ((UINT8_MAX - value) & 31));
    CHECK(door.floor_layer == (value & 7));
    CHECK(door.entry_elevation == ((UINT8_MAX - value) >> 6));
    CHECK_FALSE(door.has_entry_step);
    CHECK_FALSE(door.reserved_entry_bit);
  }
}

TEST_CASE(
    "Nonfatal pit reload stays in the room route with foreign entrance "
    "metadata",
    "[fall-reload-route][field]") {
  RecordingAshleyFallHost host;
  host.reload.destination_zone_id = 10;
  VS::Actors::ActorPlacementView placement;
  placement.tile = {.x = 1, .z = 1};
  placement.position.y = 17;
  CHECK(VS::Field::CheckAshleyFall(2, placement, true, host) ==
        VS::Field::AshleyFallResult::kHandled);
  CHECK(host.hp == 1);
  CHECK(host.transition == host.reload);
  CHECK(host.route == VS::Field::AshleyTransitionRoute::kRoom);
}

TEST_CASE("Scripted fall producer initializes the complete transition door",
          "[fall-door-initialization][field]") {
  ScriptedFallDoorFixture fixture;
  std::uint8_t destination_zone = 9;
  SECTION("Same-zone fall retains original room transition") {
    destination_zone = 9;
  }
  SECTION("Cross-zone fall retains original zone transition") {
    destination_zone = 10;
  }
  fixture.environment.fallDestinationZone = destination_zone;
  REQUIRE(VS::Field::LiveRoomFallDestination() ==
          static_cast<int>(destination_zone) * 65536 + 3);
  REQUIRE(VS::Field::LiveCollisionCellOrAbsent(1, 1, 0)->isPit == 1);
  Actors_ActorLocation location{};
  location.packedTile.word =
      std::bit_cast<std::int32_t>(std::uint32_t{0xC0010001});
  location.position.vy = 17;
  CHECK(Field_CheckAshleyTileTriggersEntry(
            2, reinterpret_cast<unsigned char*>(&location)) == 0);
  CHECK(Field_screenTransitionStep == (destination_zone == 9 ? 1 : 2));
  const auto transition = VS::Field::LoadLiveRoomTransition();
  CHECK(transition.pending_door ==
        VS::Field::DoorRecord{.destination_zone_id = destination_zone,
                              .destination_room_id = 3,
                              .tile_x = 1,
                              .tile_z = 1,
                              .entry_direction = 7,
                              .entry_tile_x = 1,
                              .entry_tile_z = 1,
                              .entry_elevation = 3});
}

TEST_CASE("Actor tile triggers preserve collision before transition refusal",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  host.flags = 0x50000;
  host.transition = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2});
  CHECK(host.collision_tile.x == 0);
  CHECK(host.collision_tile.z == 0);
  CHECK(host.collision_tile.layer == 5);
  host.calls.clear();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(-1, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls.empty());
}

TEST_CASE("Actor new tile starts trap before checking a fatal pit",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 3, .layer = 2, .z = 4};
  placement.position.y = 100;
  host.cell = {.room_trigger = true, .pit = true};
  host.trap = VS::Field::ActorTileTrap{.target = {1, 7}, .kind = 3};
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kTrap);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 5, 6, 7});
  CHECK(host.last == placement.tile);
}

TEST_CASE("Actor fatal pit defeats before requesting its event",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  host.cell.pit = true;
  placement.position.y = 16;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3});
  placement.position.y = 17;
  host.calls.clear();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kDefeated);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 8, 9});
  host.fall_event = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kFallEvent);
}

TEST_CASE("Actor cached tile ignores placement spare and skips trap reads",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.spare = 255;
  host.cell.room_trigger = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3});
  host.last.reset();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
}

TEST_CASE("Actor trigger location held flags still replace cached mode",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile = {.x = 3, .layer = 2, .z = 4};
  host.last = VS::Actors::PackedTile{.x = 9, .layer = 8, .z = 7};
  host.flags = 0x4000;
  host.cell.room_trigger = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.last ==
        VS::Actors::PackedTile{.x = 9, .layer = 8, .z = 7, .spare = 1});
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 7});
}

TEST_CASE("Actor effect-only trap falls through to original pit result",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.x = 3;
  placement.position.y = 17;
  host.cell = {.room_trigger = true, .pit = true};
  host.trap = VS::Field::ActorTileTrap{.kind = 16};
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kDefeated);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 5, 7, 8, 9});
}

TEST_CASE("Actor tile trap suppression bits never read the trap record",
          "[actor-tile-trigger-core][field]") {
  for (const auto flags : {0x4000U, 0x100000U, 0x200000U, 0x400000U, 0x2000000U,
                           0x4000000U, 0x10000000U}) {
    RecordingActorTileTriggerHost host;
    VS::Actors::ActorPlacementView placement{};
    placement.tile.x = 3;
    host.flags = flags;
    host.cell.room_trigger = true;
    CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
          VS::Field::ActorTileTriggerResult::kNone);
    CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 7});
  }
}

TEST_CASE("Actor event control holds traps but still remembers the tile",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.tile.x = 3;
  host.cell.room_trigger = true;
  host.event_control = true;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 4, 7});
  CHECK(host.last == placement.tile);
}

TEST_CASE("Actor disabled tile trigger refuses before actor cache access",
          "[actor-tile-trigger-core][field]") {
  RecordingActorTileTriggerHost host;
  VS::Actors::ActorPlacementView placement{};
  host.flags = 0x01000000;
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2});
  host.flags = 0;
  host.cell.pit = true;
  host.defeat = false;
  placement.position.y = 17;
  host.calls.clear();
  CHECK(VS::Field::CheckActorTrapAndFallTriggers(2, placement, host) ==
        VS::Field::ActorTileTriggerResult::kNone);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 8});
}

TEST_CASE("Actor tile trigger C boundary retains module and refuses bad input",
          "[actor-tile-trigger-live][field]") {
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, nullptr) == 0);
  Actors_ActorLocation location{};
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(-1, &location) == 0);
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(17, &location) == 0);
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800751B8) ==
      reinterpret_cast<const void*>(&Field_CheckActorTrapAndFallTriggersEntry));
}

TEST_CASE("Actor native tile trigger stores packed location and computed mode",
          "[actor-tile-trigger-live][field]") {
  ActorTileTriggerLiveFixture fixture;
  fixture.actor->lastTriggerTile.word = 0x01070809;
  Actors_ActorLocation location{};
  location.packedTile.target = {
      .tileX = 3, .tileLayer = 2, .tileZ = 4, .facing = 255};
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, &location) == 0);
  CHECK(fixture.actor->lastTriggerTile.word == 0x00040203);
  CHECK(std::bit_cast<std::uint32_t>(location.packedTile.word) == 0xFF040203);
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, &location) == 0);
  CHECK(fixture.actor->lastTriggerTile.word == 0x00040203);
  Battle_actors[2] = nullptr;
  CHECK(Field_CheckActorTrapAndFallTriggersEntry(2, &location) == 0);
}

TEST_CASE("Facing event refuses invalid actor and absent placement",
          "[facing-event-core][field]") {
  RecordingFacingEventHost host;
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(-1, host));
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(17, host));
  CHECK(host.reads == 0);
  CHECK_FALSE(host.evaluated);
  host.placement.reset();
  CHECK_FALSE(VS::Field::TryTriggerFacingTileEvent(0, host));
  CHECK(host.reads == 1);
  CHECK_FALSE(host.evaluated);
}

TEST_CASE("Facing room query keeps unsigned height window boundaries",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  for (const auto height : {-64, -63, 0, 63, 64}) {
    host.floor = height;
    host.calls.clear();
    CHECK(VS::Field::FindFacingRoomObject(0, host).has_value() ==
          (height >= -63 && height <= 63));
    CHECK(host.calls == std::vector<std::int32_t>{0, 2});
  }
  host.floor = 0x800000;
  CHECK(VS::Field::FindFacingRoomObject(0, host) == host.object);
}

TEST_CASE("Facing chest query gates facing flag and exact raised floor",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = 96;
  host.placement = placement;
  CHECK(VS::Field::FindFacingUnlootedChest(0, host) ==
        VS::Field::ChestTarget{3, 7});
  CHECK(host.calls == std::vector<std::int32_t>{1, 3, 2});
  CHECK(host.observed_flag == 0x3C9);
  host.calls.clear();
  host.chest = VS::Field::FacingChestState{
      .target = {3, 7}, .facing = 1, .opened_flag = 9};
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{1});
  host.chest = VS::Field::FacingChestState{.target = {3, 7}, .opened_flag = 9};
  host.collected = true;
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 3});
}

TEST_CASE("Facing resource query refuses missing actor or object state",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  host.placement.reset();
  CHECK_FALSE(VS::Field::FindFacingRoomObject(0, host));
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls.empty());
  CHECK_FALSE(VS::Field::FindFacingRoomObject(-1, host));
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(17, host));
}

TEST_CASE(
    "Facing chest query rejects absent record without flag or floor reads",
    "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  host.chest.reset();
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{1});
  host.object.reset();
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindFacingRoomObject(0, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 2});
}

TEST_CASE("Facing resource floor uses original low fifteen bit sign extension",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  VS::Actors::ActorPlacementView placement{};
  placement.position.y = -16384;
  host.placement = placement;
  host.floor = 0x4000;
  CHECK(VS::Field::FindFacingRoomObject(0, host) == host.object);
  placement.position.y = -16288;
  host.placement = placement;
  CHECK(VS::Field::FindFacingUnlootedChest(0, host) ==
        VS::Field::ChestTarget{3, 7});
  placement.position.y = -16289;
  host.placement = placement;
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
}

TEST_CASE("Facing chest collection flag keeps original halfword argument wrap",
          "[facing-resource-core][field]") {
  RecordingFacingResourceHost host;
  host.chest =
      VS::Field::FacingChestState{.target = {3, 7}, .opened_flag = 32767};
  host.collected = true;
  CHECK_FALSE(VS::Field::FindFacingUnlootedChest(0, host));
  CHECK(host.observed_flag == -31809);
}

TEST_CASE("Facing query C entries refuse invalid actor indices",
          "[facing-query-live][field]") {
  CHECK(Field_TryTriggerFacingTileEventEntry(-1) == 0);
  CHECK(Field_TryTriggerFacingTileEventEntry(17) == 0);
  CHECK(Field_FindFacingRoomObjectEntry(-1) == nullptr);
  CHECK(Field_FindFacingRoomObjectEntry(17) == nullptr);
  CHECK(Field_FindFacingUnlootedChestEntry(-1) == nullptr);
  CHECK(Field_FindFacingUnlootedChestEntry(17) == nullptr);
}

TEST_CASE("Facing queries retain all original module entries",
          "[facing-query-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074860) ==
        reinterpret_cast<const void*>(&Field_TryTriggerFacingTileEventEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800748B8) ==
        reinterpret_cast<const void*>(&Field_FindFacingRoomObjectEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80074950) ==
        reinterpret_cast<const void*>(&Field_FindFacingUnlootedChestEntry));
}

TEST_CASE("Room object query target refuses stale and out of range resolution",
          "[facing-query-live][field]") {
  RoomObjectRecordFixture fixture;
  const auto domain = VS::Field::LiveInteractionTargetDomain();
  const VS::Field::RoomObjectTarget target{1, domain.generation};
  CHECK(VS::Field::ResolveLiveRoomObjectTarget(target) == &fixture.locks[1]);
  CHECK(VS::Field::ResolveLiveRoomObjectTarget({2, domain.generation}) ==
        nullptr);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveRoomObjectTarget(target) == nullptr);
  CHECK(VS::Field::InteractLiveRoomObject(target) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
}

TEST_CASE("Object interaction wait selects its mode before pausing",
          "[object-interaction-wait-core][field]") {
  RecordingObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
  CHECK(host.mode == VS::Field::FieldControlMode::kObjectInteractionWait);
  CHECK(host.calls == std::vector<int>{0, 1});
  VS::Field::BeginObjectInteractionWait(host);
  CHECK(host.calls == std::vector<int>{0, 1, 0, 1});
}

TEST_CASE("Object interaction wait releases and resumes before exploration",
          "[object-interaction-wait-core][field]") {
  RecordingObjectInteractionWaitHost host;
  VS::Field::BeginObjectInteractionWait(host);
  host.calls.clear();
  VS::Field::EndObjectInteractionWait(host);
  CHECK(host.mode == VS::Field::FieldControlMode::kExploration);
  CHECK(host.calls == std::vector<int>{2, 3, 4});
}

TEST_CASE("Object interaction wait keeps both original module bindings",
          "[object-interaction-wait-core][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073AA4) ==
        reinterpret_cast<const void*>(&Field_BeginObjectInteractionWaitEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80073ACC) ==
        reinterpret_cast<const void*>(&Field_EndObjectInteractionWaitEntry));
}

TEST_CASE("Room object interaction keeps reusable keys and consumed sigils",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost key;
  key.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  key.has_item = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(key) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(key.calls == std::vector<std::int32_t>{0, 1000, 7, 5, 1001});
  CHECK(key.state->lock_state == 0);
  RecordingRoomObjectInteractionHost sigil;
  sigil.state =
      VS::Field::RoomObjectLockState{.required_item = 466, .lock_state = 3};
  sigil.has_item = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(sigil) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(sigil.calls ==
        std::vector<std::int32_t>{0, 0x388, 466, 1000, 9, 5, 1001});
  CHECK(sigil.flag_value == 2);
}

TEST_CASE("Room object prior unlock opens silently and busy records do nothing",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  host.previous_flag = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 1000, 5, 1001});
  host.calls.clear();
  host.state->is_open = true;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kBusy);
  CHECK(host.calls.empty());
}

TEST_CASE("Room object rejection preserves notification sounds and idle order",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state = VS::Field::RoomObjectLockState{.lock_state = 2};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{5, 0x65, 0x66, 1002});
  host.calls.clear();
  host.state = VS::Field::RoomObjectLockState{.lock_state = 3};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 10, 0x65, 0x66, 1002});
}

TEST_CASE("Room object busy and invalid targets perform no item reads",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  host.state = VS::Field::RoomObjectLockState{.is_triggered = true};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kBusy);
  CHECK(host.calls.empty());
  host.state.reset();
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls.empty());
  host.state = VS::Field::RoomObjectLockState{.lock_state = 4};
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls.empty());
}

TEST_CASE("Room object refused unlock cannot start pose or toggle",
          "[room-object-interaction-core][field]") {
  RecordingRoomObjectInteractionHost host;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kWait);
  CHECK(host.calls == std::vector<std::int32_t>{5, 1001});
  host.calls.clear();
  host.state =
      VS::Field::RoomObjectLockState{.required_item = 458, .lock_state = 1};
  host.has_item = true;
  host.unlock_succeeds = false;
  CHECK(VS::Field::InteractWithLockedRoomObject(host) ==
        VS::Field::RoomObjectInteractionResult::kRefused);
  CHECK(host.calls == std::vector<std::int32_t>{0, 1000});
  CHECK(host.state->lock_state == 1);
}

TEST_CASE("Native room object entry refuses null and foreign record addresses",
          "[room-object-interaction-live][field]") {
  RoomObjectRecordFixture fixture;
  Field_MpdLockTrigger foreign{};
  CHECK(Field_InteractWithLockedRoomObjectEntry(nullptr) == 1);
  CHECK(Field_InteractWithLockedRoomObjectEntry(&foreign) == 1);
  CHECK(Field_InteractWithLockedRoomObjectEntry(
            reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1})) == 1);
}

TEST_CASE("Native room object entry reads only its present busy lock record",
          "[room-object-interaction-live][field]") {
  RoomObjectRecordFixture fixture;
  fixture.locks[1].isOpen = 1;
  CHECK(Field_InteractWithLockedRoomObjectEntry(&fixture.locks[1]) == 1);
  fixture.locks[1].isOpen = 0;
  fixture.locks[1].isTriggered = 1;
  CHECK(Field_InteractWithLockedRoomObjectEntry(&fixture.locks[1]) == 1);
}

TEST_CASE("Native room object interaction keeps its original module binding",
          "[room-object-interaction-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x80073AFC) ==
      reinterpret_cast<const void*>(&Field_InteractWithLockedRoomObjectEntry));
}

TEST_CASE("Typed chest flow owns selected begin finish and contents decisions",
          "[chest-interaction-core][field]") {
  RecordingChestInteractionHost host;
  const VS::Field::ChestTarget target{0, 1};
  CheckChestBeginFinishRules(host, target);
  CheckChestContentsRules(host);
}

TEST_CASE("Typed chest selection preserves accepted and cancelled outcomes",
          "[chest-interaction-core][field]") {
  RecordingChestInteractionHost host;
  host.selected = VS::Field::ChestTarget{0, 1};
  host.chest = VS::Field::ChestLockState{.opened_flag = 7};
  CHECK(VS::Field::UpdateChestContentsSelection(host));
  host.contents_result = VS::Field::ChestSelectionResult::kAccepted;
  CHECK_FALSE(VS::Field::UpdateChestContentsSelection(host));
  CHECK(host.calls == std::vector<std::int32_t>{0x3C7, 1018, 1019});
  host.calls.clear();
  host.contents_result = VS::Field::ChestSelectionResult::kCancelled;
  CHECK_FALSE(VS::Field::UpdateChestContentsSelection(host));
  CHECK(host.calls == std::vector<std::int32_t>{1002, 1018, 1019});
}

TEST_CASE("Interaction dispatch refuses mismatched chest and trap modes",
          "[interaction-target][field]") {
  RecordingInteractionDispatchHost host;
  host.target = VS::Field::ChestTarget{0, 1};
  CHECK(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls == std::vector<int>{0, 2});
  host.calls.clear();
  host.trap_mode = true;
  CHECK_FALSE(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls.empty());
  host.target = VS::Field::TrapTarget{0, 1};
  CHECK(VS::Field::DispatchTrapOrChestInteraction(host));
  CHECK(host.calls == std::vector<int>{0, 1});
}

TEST_CASE("Typed trap begin preserves actor capture and original reset order",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  const VS::Field::TrapTarget target{2, 7};
  REQUIRE(VS::Field::BeginTrapInteraction(target, 3, host));
  CHECK(host.selected == target);
  CHECK(host.selected_actor == 3);
  CHECK(host.calls == std::vector<std::int32_t>{
                          1000, 1001, 0,  1,  2,  3,  4,  5,  6,    7,
                          8,    9,    10, 11, 12, 13, 14, 15, 1002, 1003});
}

TEST_CASE("Typed trap begin refuses invalid capture before side effects",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  host.valid = false;
  CHECK_FALSE(VS::Field::BeginTrapInteraction({2, 7}, 3, host));
  CHECK(host.calls.empty());
  CHECK_FALSE(host.selected);
}

TEST_CASE("Typed trap finish disables only its selected target then clears it",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  const VS::Field::TrapTarget target{2, 7};
  host.selected = target;
  CHECK(VS::Field::FinishTrapInteraction(host));
  CHECK(host.disabled == target);
  CHECK_FALSE(host.selected);
  CHECK(host.calls == std::vector<std::int32_t>{1004, 1005, 1006, 1007});
  host.calls.clear();
  CHECK_FALSE(VS::Field::FinishTrapInteraction(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Typed trap finish clears a target refused during disable",
          "[interaction-target][field]") {
  RecordingTrapInteractionHost host;
  host.selected = VS::Field::TrapTarget{2, 7};
  host.disable_succeeds = false;
  CHECK_FALSE(VS::Field::FinishTrapInteraction(host));
  CHECK_FALSE(host.selected);
  CHECK(host.calls == std::vector<std::int32_t>{1004, 1005, 1006, 1007});
}

TEST_CASE(
    "Interaction target state cannot expose a chest through the trap kind",
    "[interaction-target][field]") {
  VS::Field::InteractionTargetState state;
  CHECK(state.Trap() == nullptr);
  CHECK(state.Chest() == nullptr);
  CheckTargetKindSelection(state);
  state.Clear();
  CHECK(state.Trap() == nullptr);
  CHECK(state.Chest() == nullptr);
}

TEST_CASE("Interaction target domain refuses stale and out-of-range identities",
          "[interaction-target][field]") {
  const VS::Field::InteractionTargetDomain domain{7, 2, 3};
  CheckTargetDomainBounds(domain);
  CheckSelectedTargetDomain(domain);
}

TEST_CASE("Live interaction target indexes refuse room-generation replacement",
          "[interaction-target][field]") {
  const auto saved = Field_roomData;
  struct RestoreRoom {
    Field_RoomRecord record;
    ~RestoreRoom() {
      Field_roomData = record;
      VS::Field::InvalidateLiveInteractionTargets();
    }
  } restore{saved};
  std::array<Field_MpdFloorTrigger, 2> traps{};
  std::array<Items_TreasureChest, 2> chests{};
  Field_roomData.floorTriggers = traps.data();
  Field_roomData.header.floorTriggersLength = 24;
  Field_roomData.containers = chests.data();
  Field_roomData.header.containersLength = 0x468;
  const auto trap = VS::Field::FindLiveTrapTarget(&traps[1]);
  const auto chest = VS::Field::FindLiveChestTarget(&chests[1]);
  REQUIRE(trap);
  REQUIRE(chest);
  CHECK(VS::Field::ResolveLiveTrapTarget(*trap) == &traps[1]);
  CHECK(VS::Field::ResolveLiveChestTarget(*chest) == &chests[1]);
  CHECK_FALSE(VS::Field::FindLiveTrapTarget(nullptr));
  CHECK_FALSE(VS::Field::FindLiveChestTarget(nullptr));
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveTrapTarget(*trap) == nullptr);
  CHECK(VS::Field::ResolveLiveChestTarget(*chest) == nullptr);
}

TEST_CASE("Typed chest animation never stores a lock-record alias",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  REQUIRE(VS::Field::BeginLiveChestSwitch(*target));
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 2);
  CHECK(Field_gimmickAnimationQueue[0].trigger == nullptr);
  CHECK(static_cast<std::uint16_t>(fixture.chests[0].reservedA) == 0x0114);
  fixture.chests[0].reservedA = 0x0101;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(fixture.chests[0].isOpen == 1);
  CHECK(fixture.chests[0].reservedA == 0);
  CHECK(Field_pendingGimmickAnimationCount == 0);
}

TEST_CASE("Typed chest animation refuses a stale room-generation target",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  REQUIRE(VS::Field::BeginLiveChestSwitch(*target));
  VS::Field::InvalidateLiveInteractionTargets();
  const auto before = fixture.chests[0].reservedA;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(fixture.chests[0].reservedA == before);
  CHECK_FALSE(VS::Field::BeginLiveChestSwitch(*target));
}

TEST_CASE("Native chest focus selects typed record and refuses stale dispatch",
          "[interaction-target][field]") {
  ChestTargetFixture fixture;
  const auto target = VS::Field::FindLiveChestTarget(&fixture.chests[0]);
  REQUIRE(target);
  CHECK(VS::Field::BeginLiveChestInteraction(*target, 0));
  const auto* selected = VS::Field::LiveInteractionTargetState().Chest(
      VS::Field::LiveInteractionTargetDomain());
  REQUIRE(selected);
  CHECK(*selected == *target);
  CHECK(VS::Field::LiveInteractionTargetState().Trap() == nullptr);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK_FALSE(VS::Field::TryOpenLiveChest(*target));
}

TEST_CASE("Interactive door table uses only its first activation guard",
          "[interactive-door-core][field]") {
  using Doors = std::array<VS::Field::RoomDoorPlacement, 2>;
  Doors doors{{{.tile_x = 2, .tile_z = 3, .activation = 1},
               {.tile_x = 5, .tile_z = 6, .floor_layer = 2}}};
  const VS::Field::RoomDoorTileQuery query{
      .tile_x = 5, .tile_z = 6, .floor_layer = 2};
  CHECK(VS::Field::InteractiveDoorAtTile(doors, query) == 1);
  doors[0].activation = 0;
  CHECK_FALSE(VS::Field::InteractiveDoorAtTile(doors, query));
  CHECK_FALSE(VS::Field::InteractiveDoorAtTile({}, query));
}

TEST_CASE("Interactive door orientations cover exactly their companion tile",
          "[interactive-door-core][field]") {
  using Doors = std::array<VS::Field::RoomDoorPlacement, 1>;
  const std::array<VS::Field::RoomDoorTileQuery, 4> neighbors{
      VS::Field::RoomDoorTileQuery{.tile_x = 3, .tile_z = 4, .floor_layer = 2},
      VS::Field::RoomDoorTileQuery{.tile_x = 4, .tile_z = 5, .floor_layer = 2},
      VS::Field::RoomDoorTileQuery{.tile_x = 3, .tile_z = 6, .floor_layer = 2},
      VS::Field::RoomDoorTileQuery{.tile_x = 2, .tile_z = 5, .floor_layer = 2}};
  for (std::size_t direction = 0; direction < neighbors.size(); ++direction) {
    Doors doors{{{.tile_x = 3,
                  .tile_z = 5,
                  .floor_layer = 2,
                  .kind = static_cast<std::int16_t>(direction + 4),
                  .activation = 1}}};
    CHECK(VS::Field::InteractiveDoorAtTile(
              doors, {.tile_x = 3, .tile_z = 5, .floor_layer = 2}) == 0);
    for (std::size_t neighbor = 0; neighbor < neighbors.size(); ++neighbor) {
      CHECK(VS::Field::InteractiveDoorAtTile(doors, neighbors[neighbor])
                .has_value() == (neighbor == direction));
    }
    auto wrong_layer = neighbors[direction];
    wrong_layer.floor_layer = 1;
    CHECK_FALSE(VS::Field::InteractiveDoorAtTile(doors, wrong_layer));
  }
}

TEST_CASE(
    "Interactive door lookup keeps order unknown kinds and wide arithmetic",
    "[interactive-door-core][field]") {
  using Doors = std::array<VS::Field::RoomDoorPlacement, 2>;
  Doors doors{{{.tile_x = 3, .tile_z = 5, .kind = -1, .activation = 1},
               {.tile_x = 3, .tile_z = 5, .kind = 4}}};
  CHECK(VS::Field::InteractiveDoorAtTile(doors, {.tile_x = 3, .tile_z = 5}) ==
        0);
  CHECK(VS::Field::InteractiveDoorAtTile(doors, {.tile_x = 3, .tile_z = 4}) ==
        1);
  doors[0].tile_x = std::numeric_limits<std::int32_t>::min();
  doors[0].tile_z = std::numeric_limits<std::int32_t>::max();
  doors[0].kind = 4;
  CHECK(VS::Field::InteractiveDoorAtTile(
            doors,
            {.tile_x = doors[0].tile_x, .tile_z = doors[0].tile_z - 1}) == 0);
  CHECK_FALSE(VS::Field::InteractiveDoorAtTile(
      doors, {.tile_x = std::numeric_limits<std::int32_t>::max(),
              .tile_z = std::numeric_limits<std::int32_t>::min()}));
}

TEST_CASE("Sigil door matching uses either endpoint and the first pair only",
          "[sigil-door-core][field]") {
  using Pairs = std::array<VS::Field::SigilDoorRoomPair, 3>;
  const Pairs pairs{{{.first = {9, 247}, .second = {10, 1}},
                     {.first = {9, 247}, .second = {11, 2}},
                     {.first = {12, 3}, .second = {13, 4}}}};
  const std::array<std::int16_t, 3> ranges{0, 2, 3};
  std::array<std::byte, 3> flags{std::byte{128}, std::byte{254},
                                 std::byte{127}};
  const VS::Field::SigilDoorTables tables{ranges, pairs};
  const VS::Field::SigilDoorRoomQuery query{.zone_id = 9, .map_id = 247};
  CHECK(VS::Field::RoomSigilDoorUnlockByte(tables, query, flags) == 128);
  CHECK(VS::Field::MarkSigilDoorUnlocked(tables, query, flags));
  CHECK(flags == std::array{std::byte{1}, std::byte{254}, std::byte{127}});
  CHECK(VS::Field::RoomSigilDoorUnlockByte(tables, {.zone_id = 10, .map_id = 1},
                                           flags) == 1);
  CHECK(VS::Field::RoomSigilDoorUnlockByte(tables, {.zone_id = 11, .map_id = 2},
                                           flags) == 254);
  CHECK(VS::Field::RoomSigilDoorUnlockByte(
            tables, {.zone_id = 13, .map_id = 4, .sigil_index = 1}, flags) ==
        127);
  CHECK(VS::Field::RoomSigilDoorUnlockByte(
            tables, {.zone_id = 9, .map_id = 247, .sigil_index = 1}, flags) ==
        0);
}

TEST_CASE("Sigil endpoints and persistent flags preserve every unsigned byte",
          "[sigil-door-core][field]") {
  const std::array<std::int16_t, 2> ranges{0, 1};
  using Pairs = std::array<VS::Field::SigilDoorRoomPair, 1>;
  for (std::uint32_t value = 0; value <= UINT8_MAX; ++value) {
    const auto byte = static_cast<std::uint8_t>(value);
    const auto other = static_cast<std::uint8_t>(UINT8_MAX - value);
    const Pairs pairs{{{.first = {byte, other}, .second = {other, byte}}}};
    const VS::Field::SigilDoorTables tables{ranges, pairs};
    std::array<std::byte, 1> flags{static_cast<std::byte>(value)};
    CHECK(VS::Field::RoomSigilDoorUnlockByte(
              tables, {.zone_id = byte, .map_id = other}, flags) == value);
    CHECK(VS::Field::RoomSigilDoorUnlockByte(
              tables, {.zone_id = other, .map_id = byte}, flags) == value);
    CHECK(VS::Field::MarkSigilDoorUnlocked(
        tables, {.zone_id = byte, .map_id = other}, flags));
    CHECK(flags[0] == std::byte{1});
  }
}

TEST_CASE("Sigil ranges and missing flags refuse without mutation",
          "[sigil-door-core][field]") {
  using Pairs = std::array<VS::Field::SigilDoorRoomPair, 1>;
  const Pairs pairs{{{.first = {9, 247}, .second = {10, 1}}}};
  const VS::Field::SigilDoorRoomQuery query{.zone_id = 9, .map_id = 247};
  const std::array<std::array<std::int16_t, 2>, 3> malformed{
      std::array<std::int16_t, 2>{-1, 1}, {1, 0}, {0, 2}};
  std::array<std::byte, 1> flags{std::byte{254}};
  for (const auto& ranges : malformed) {
    const VS::Field::SigilDoorTables tables{ranges, pairs};
    CHECK_FALSE(VS::Field::MarkSigilDoorUnlocked(tables, query, flags));
    CHECK(VS::Field::RoomSigilDoorUnlockByte(tables, query, flags) == 0);
    CHECK(flags[0] == std::byte{254});
  }
  const std::array<std::int16_t, 2> ranges{0, 1};
  const VS::Field::SigilDoorTables tables{ranges, pairs};
  CHECK_FALSE(VS::Field::MarkSigilDoorUnlocked(tables, query, {}));
  CHECK_FALSE(VS::Field::MarkSigilDoorUnlocked(
      tables, {.zone_id = 9, .map_id = 247, .sigil_index = UINT32_MAX}, flags));
  CHECK_FALSE(VS::Field::MarkSigilDoorUnlocked(
      tables, {.zone_id = -1, .map_id = 247}, flags));
  CHECK_FALSE(VS::Field::MarkSigilDoorUnlocked({{}, pairs}, query, flags));
  CHECK(flags[0] == std::byte{254});
}

TEST_CASE("Live door lookup returns generation checked room identities",
          "[interactive-door-live][field]") {
  RoomObjectRecordFixture fixture;
  fixture.locks[0].trigger.activation = 1;
  fixture.locks[1].trigger.tileX = 5;
  fixture.locks[1].trigger.tileZ = 6;
  fixture.locks[1].trigger.floorLayer = 2;
  const auto target = VS::Field::FindLiveInteractiveDoorAtTile(
      {.tile_x = 5, .tile_z = 6, .floor_layer = 2});
  REQUIRE(target);
  if (!target) return;
  CHECK(target->index == 1);
  CHECK(VS::Field::ResolveLiveRoomObjectTarget(*target) == &fixture.locks[1]);
  CHECK(Field_InteractiveDoorAtTileEntry(5, 6, 2) == &fixture.locks[1]);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveRoomObjectTarget(*target) == nullptr);
  fixture.locks[0].trigger.activation = 0;
  CHECK(Field_InteractiveDoorAtTileEntry(5, 6, 2) == nullptr);
}

TEST_CASE("Live door lookup refuses partial misaligned and absent records",
          "[interactive-door-live][field]") {
  RoomObjectRecordFixture fixture;
  fixture.locks[0].trigger.activation = 1;
  Field_roomData.header.lockTriggersLength = sizeof fixture.locks - 1;
  CHECK(Field_InteractiveDoorAtTileEntry(0, 0, 0) == nullptr);
  Field_roomData.header.lockTriggersLength = sizeof fixture.locks;
  Field_roomData.lockTriggers = reinterpret_cast<Field_MpdLockTrigger*>(
      reinterpret_cast<std::byte*>(fixture.locks.data()) + 1);
  CHECK(Field_InteractiveDoorAtTileEntry(0, 0, 0) == nullptr);
  Field_roomData.lockTriggers = nullptr;
  CHECK(Field_InteractiveDoorAtTileEntry(0, 0, 0) == nullptr);
}

TEST_CASE("Native sigil lookup and marking preserve unsigned first-pair state",
          "[sigil-door-live][field]") {
  SigilDoorLiveFixture fixture;
  Audio_Seq_unusedStateFlags.opaque186[0] =
      std::bit_cast<char>(std::uint8_t{128});
  Audio_Seq_unusedStateFlags.opaque186[1] =
      std::bit_cast<char>(std::uint8_t{254});
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(9, 247, 0) == 128);
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(10, 1, 0) == 128);
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(11, 2, 0) == 254);
  auto expected = Audio_Seq_unusedStateFlags;
  expected.opaque186[0] = 1;
  Field_MarkSigilDoorUnlockedEntry(0);
  using FlagBytes = std::array<std::byte, sizeof(expected)>;
  CHECK(std::bit_cast<FlagBytes>(Audio_Seq_unusedStateFlags) ==
        std::bit_cast<FlagBytes>(expected));
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(9, 247, 0) == 1);
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(11, 2, 0) == 254);
}

TEST_CASE(
    "Native sigil marking refuses malformed ranges and missing room headers",
    "[sigil-door-live][field]") {
  SigilDoorLiveFixture fixture;
  const auto expected = Audio_Seq_unusedStateFlags;
  using FlagBytes = std::array<std::byte, sizeof(expected)>;
  for (const auto range : std::array{std::array<std::int16_t, 2>{-1, 1},
                                     std::array<std::int16_t, 2>{1, 0},
                                     std::array<std::int16_t, 2>{0, 24}}) {
    Field_lockedDoorIndexRanges[0] = range[0];
    Field_lockedDoorIndexRanges[1] = range[1];
    Field_MarkSigilDoorUnlockedEntry(0);
    CHECK(Field_IsRoomSigilDoorUnlockedEntry(9, 247, 0) == 0);
    CHECK(std::bit_cast<FlagBytes>(Audio_Seq_unusedStateFlags) ==
          std::bit_cast<FlagBytes>(expected));
  }
  Field_lockedDoorIndexRanges[0] = 0;
  Field_lockedDoorIndexRanges[1] = 2;
  Field_roomData.header.minimapSceneLength = sizeof fixture.scene - 1;
  CHECK_FALSE(VS::Field::MarkLiveSigilDoorUnlocked(0));
  Field_roomData.minimapScene = nullptr;
  CHECK_FALSE(VS::Field::MarkLiveSigilDoorUnlocked(0));
  Field_MarkSigilDoorUnlockedEntry(-1);
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(9, 247, UINT32_MAX) == 0);
  CHECK(std::bit_cast<FlagBytes>(Audio_Seq_unusedStateFlags) ==
        std::bit_cast<FlagBytes>(expected));
}

TEST_CASE("Live sigil groups stop at the eight original boundary words",
          "[sigil-door-live-bounds][sigil-door-live][field]") {
  SigilDoorLiveFixture fixture;
  Field_lockedDoorIndexRanges[7] = 0;
  Field_lockedDoorIndexRanges[8] = 2;
  Audio_Seq_unusedStateFlags.opaque186[0] =
      std::bit_cast<char>(std::uint8_t{128});
  CHECK(Field_IsRoomSigilDoorUnlockedEntry(9, 247, 7) == 0);
  CHECK_FALSE(VS::Field::MarkLiveSigilDoorUnlocked(7));
  CHECK(std::bit_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.opaque186[0]) ==
        128);
}

TEST_CASE("Door table native entries retain original module addresses",
          "[interactive-door-live][sigil-door-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008BC04) ==
        reinterpret_cast<const void*>(&Field_InteractiveDoorAtTileEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008E6DC) ==
        reinterpret_cast<const void*>(&Field_MarkSigilDoorUnlockedEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008E7BC) ==
        reinterpret_cast<const void*>(&Field_IsRoomSigilDoorUnlockedEntry));
}

TEST_CASE(
    "Typed queued doors retire stale room identities before record access",
    "[door-queue-target][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  const VS::Field::RoomObjectTarget target{
      0, VS::Field::LiveInteractionTargetDomain().generation};
  room.locks[0].isOpen = 2;
  room.locks[0].delayFrames = 8;
  REQUIRE(VS::Field::QueueLiveGimmickAnimation(3, target));
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(Field_gimmickAnimationQueue[0].trigger == &room.locks[0]);
  VS::Field::InvalidateLiveInteractionTargets();
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
  CHECK(room.locks[0].delayFrames == 8);
  CHECK(room.locks[0].isOpen == 2);
  CHECK_FALSE(VS::Field::QueueLiveGimmickAnimation(3, target));
}

TEST_CASE("Typed door queue reset removes captured target metadata",
          "[door-queue-target][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  const auto generation = VS::Field::LiveInteractionTargetDomain().generation;
  REQUIRE(VS::Field::QueueLiveGimmickAnimation(
      3, VS::Field::RoomObjectTarget{0, generation}));
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  REQUIRE(VS::Field::QueueLiveGimmickAnimation(
      3, VS::Field::RoomObjectTarget{1, generation}));
  CHECK(Field_gimmickAnimationQueue[0].trigger == &room.locks[1]);
  room.locks[1].delayFrames = 2;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(room.locks[1].delayFrames == 1);
  CHECK(Field_pendingGimmickAnimationCount == 1);
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
}

TEST_CASE("Door collision toggles primary and original companion coordinates",
          "[door-collision-core][field]") {
  const std::array<std::array<std::int32_t, 2>, 4> neighbors{
      std::array<std::int32_t, 2>{3, 4}, {4, 5}, {3, 6}, {2, 5}};
  for (std::size_t direction = 0; direction < neighbors.size(); ++direction) {
    RecordingDoorCollisionHost host;
    host.state = VS::Field::RoomDoorPlacement{
        .tile_x = 3,
        .tile_z = 5,
        .floor_layer = 2,
        .kind = static_cast<std::int16_t>(direction + 4)};
    REQUIRE(VS::Field::ToggleDoorCollision(host));
    REQUIRE(host.cells.size() == 2);
    CHECK(host.cells[0].tile_x == 3);
    CHECK(host.cells[0].tile_z == 5);
    CHECK(host.cells[1].tile_x == neighbors[direction][0]);
    CHECK(host.cells[1].tile_z == neighbors[direction][1]);
    CHECK(host.cells[0].floor_layer == 2);
    CHECK(host.cells[1].floor_layer == 2);
    CHECK(host.masks == std::vector<std::uint8_t>{5, 5});
  }
}

TEST_CASE(
    "Door collision preserves unrelated packed bits and skips unknown "
    "companions",
    "[door-collision-core][field]") {
  for (std::uint32_t passability = 0; passability < 8; ++passability) {
    RecordingDoorCollisionHost host;
    host.state = VS::Field::RoomDoorPlacement{.kind = 8};
    host.packed_cell = (0xA5A5A5A5 & ~0x1C00u) | (passability << 10);
    const auto expected = host.packed_cell ^ (5u << 10);
    REQUIRE(VS::Field::ToggleDoorCollision(host));
    CHECK(host.packed_cell == expected);
    CHECK(host.cells.size() == 1);
  }
}

TEST_CASE("Door collision refuses absent state and stops after invalidation",
          "[door-collision-core][field]") {
  RecordingDoorCollisionHost host;
  CHECK_FALSE(VS::Field::ToggleDoorCollision(host));
  CHECK(host.cells.empty());
  host.state = VS::Field::RoomDoorPlacement{.kind = 4};
  host.invalidate = true;
  CHECK_FALSE(VS::Field::ToggleDoorCollision(host));
  CHECK(host.cells.size() == 1);
}

TEST_CASE("Full door queue refusal keeps a closed door retryable",
          "[door-begin-backpressure][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  for (std::size_t slot = 0; slot < VS::Field::kGimmickAnimationSlots; ++slot) {
    REQUIRE(VS::Field::QueueLiveGimmickAnimation(1, &room.locks[1]));
  }
  room.locks[0].delayFrames = 7;
  CHECK(Field_BeginDoorToggleEntry(&room.locks[0]) == 0);
  CHECK(room.locks[0].isTriggered == 0);
  CHECK(room.locks[0].delayFrames == 7);
  CHECK(Field_pendingGimmickAnimationCount == 8);
  Field_ResetGimmickAnimationQueueEntry();
}

TEST_CASE("Door begin delays closed doors and preserves every key width",
          "[door-animation-core][field]") {
  for (std::uint32_t key = 0; key <= UINT16_MAX; ++key) {
    RecordingDoorAnimationHost host;
    host.Primary().key_or_sigil = static_cast<std::uint16_t>(key);
    REQUIRE(VS::Field::BeginDoorToggle(host));
    CHECK(host.Primary().busy == 1);
    CHECK(host.Primary().delay == 8);
    CHECK(host.calls.back() == 6003);
    const bool sigil = key >= 0x1CA && key <= 0x1D0;
    CHECK(host.marked.has_value() == sigil);
    if (sigil) CHECK(host.marked == key - 0x1CA);
  }
}

TEST_CASE("Door admission refusal leaves busy delay and effects unchanged",
          "[door-animation-core][field]") {
  RecordingDoorAnimationHost host;
  host.Primary().delay = 7;
  host.Primary().key_or_sigil = 0x1CA;
  host.queue_available = false;
  CHECK_FALSE(VS::Field::BeginDoorToggle(host));
  CHECK_FALSE(VS::Field::StartDoorAnimation(host));
  CHECK(host.calls.empty());
  CHECK(host.Primary().busy == 0);
  CHECK(host.Primary().delay == 7);
}

TEST_CASE("Door late admission refusal restores original busy and delay",
          "[door-animation-core][field]") {
  RecordingDoorAnimationHost host;
  host.Primary().delay = 7;
  host.Primary().key_or_sigil = 0x1CA;
  host.accept_queue = false;
  CHECK_FALSE(VS::Field::BeginDoorToggle(host));
  CHECK(host.Primary().busy == 0);
  CHECK(host.Primary().delay == 7);
}

TEST_CASE("Door start preserves sound effect collision linked and queue order",
          "[door-animation-core][field]") {
  RecordingDoorAnimationHost host;
  host.Primary().open_value = 2;
  host.Primary().effect = 9;
  host.linked = VS::Field::RoomDoorAnimationState{
      .placement = {.activation = 1}, .open_value = -1, .effect = 8};
  REQUIRE(VS::Field::StartDoorAnimation(host));
  CHECK(host.calls == std::vector<std::int32_t>{0x16, 0x17, 1009, 2000, 1008,
                                                2001, 3011, 6001});
  CHECK(host.Linked().busy == 1);
  host.calls.clear();
  host.Primary().open_value = 0;
  host.Linked().placement.activation = 0;
  REQUIRE(VS::Field::StartDoorAnimation(host));
  CHECK(host.calls == std::vector<std::int32_t>{0x1D, 0x1E, 1009, 6001});
}

TEST_CASE("Door completion waits then opens or closes both active records",
          "[door-animation-core][field]") {
  RecordingDoorAnimationHost host;
  host.Primary().busy = 1;
  host.linked = VS::Field::RoomDoorAnimationState{
      .placement = {.activation = 1}, .open_value = 2, .busy = 1};
  host.active_effect = true;
  CHECK_FALSE(VS::Field::FinishDoorAnimation(host));
  CHECK(host.calls.empty());
  host.active_effect = false;
  REQUIRE(VS::Field::FinishDoorAnimation(host));
  CHECK(host.calls == std::vector<std::int32_t>{2000, 5001, 5010});
  CHECK(host.Primary().open_value == 1);
  CHECK(host.Primary().busy == 0);
  CHECK(host.Linked().open_value == 0);
  CHECK(host.Linked().busy == 0);
}

TEST_CASE(
    "Door lifecycle stops after effect invalidation and refuses busy state",
    "[door-animation-core][field]") {
  RecordingDoorAnimationHost host;
  host.Primary().busy = 128;
  CHECK_FALSE(VS::Field::BeginDoorToggle(host));
  CHECK(host.calls.empty());
  host.Primary().busy = 0;
  host.invalidate_effect = true;
  CHECK_FALSE(VS::Field::StartDoorAnimation(host));
  CHECK(host.calls == std::vector<std::int32_t>{0x1D, 0x1E, 1000});
  CHECK_FALSE(VS::Field::FinishDoorAnimation(host));
}

TEST_CASE("Native door collision preserves every packed cell field except XOR5",
          "[door-collision-live][field]") {
  RoomObjectRecordFixture room;
  Field_MpdCollisionHeader collision{
      .width = 2, .height = 2, .heightOrigin = 0, .cellCount = 4};
  std::array<Field_MpdTileCell, 4> cells{};
  Field_roomData.collisionMap = &collision;
  Field_roomData.tileCells = cells.data();
  Field_roomData.header.collisionMapLength = sizeof collision;
  Field_roomData.header.tileCellsLength = sizeof cells;
  room.locks[0].trigger.tileX = 0;
  room.locks[0].trigger.tileZ = 1;
  room.locks[0].trigger.kind = 4;
  for (std::uint32_t passability = 0; passability < 8; ++passability) {
    const auto word = (0xA5A5A5A5 & ~0x1C00u) | (passability << 10);
    for (auto& cell : cells) cell = std::bit_cast<Field_MpdTileCell>(word);
    Field_ToggleDoorCollisionEntry(&room.locks[0]);
    CHECK(std::bit_cast<std::uint32_t>(cells[0]) == (word ^ (5u << 10)));
    CHECK(std::bit_cast<std::uint32_t>(cells[2]) == (word ^ (5u << 10)));
    CHECK(std::bit_cast<std::uint32_t>(cells[1]) == word);
    CHECK(std::bit_cast<std::uint32_t>(cells[3]) == word);
  }
}

TEST_CASE(
    "Native door begin refuses and retries after bounded queue capacity "
    "returns",
    "[door-animation-live][door-begin-backpressure][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  room.locks[0].linkedLockIndex = -1;
  room.locks[0].trigger.lockState.lock.roomEffect = -1;
  room.locks[0].delayFrames = 7;
  for (std::size_t slot = 0; slot < VS::Field::kGimmickAnimationSlots; ++slot) {
    REQUIRE(VS::Field::QueueLiveGimmickAnimation(1, &room.locks[1]));
  }
  CHECK(Field_BeginDoorToggleEntry(&room.locks[0]) == 0);
  CHECK(room.locks[0].isTriggered == 0);
  CHECK(room.locks[0].delayFrames == 7);
  Field_ResetGimmickAnimationQueueEntry();
}

TEST_CASE(
    "Native accepted door begin runs the original eight-tick continuation",
    "[door-animation-live][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  room.locks[0].linkedLockIndex = -1;
  room.locks[0].trigger.lockState.lock.roomEffect = -1;
  REQUIRE(Field_BeginDoorToggleEntry(&room.locks[0]) == 1);
  CHECK(room.locks[0].isTriggered == 1);
  CHECK(room.locks[0].delayFrames == 8);
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 3);
  room.CheckEightTickDoorCompletion();
}

TEST_CASE(
    "Native linked door finish preserves disabled and self-linked semantics",
    "[door-animation-live][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  room.locks[0].trigger.lockState.lock.roomEffect = -1;
  room.locks[0].linkedLockIndex = 1;
  room.locks[0].isTriggered = 1;
  room.locks[1].trigger.activation = 1;
  room.locks[1].isOpen = 2;
  room.locks[1].isTriggered = 1;
  REQUIRE(Field_FinishDoorAnimationEntry(&room.locks[0]) == 1);
  CHECK(room.locks[0].isOpen == 1);
  CHECK(room.locks[0].isTriggered == 0);
  CHECK(room.locks[1].isOpen == 0);
  CHECK(room.locks[1].isTriggered == 0);
}

TEST_CASE("Native self-linked active door toggles its state twice",
          "[door-animation-live][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  room.locks[0].trigger.lockState.lock.roomEffect = -1;
  room.locks[0].isOpen = 0;
  room.locks[0].linkedLockIndex = 0;
  room.locks[0].trigger.activation = 1;
  REQUIRE(Field_FinishDoorAnimationEntry(&room.locks[0]) == 1);
  CHECK(room.locks[0].isOpen == 0);
}

TEST_CASE("Native disabled linked door remains unchanged",
          "[door-animation-live][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  room.locks[0].trigger.lockState.lock.roomEffect = -1;
  room.locks[0].linkedLockIndex = 1;
  room.locks[1].trigger.activation = 0;
  room.locks[1].isOpen = -1;
  REQUIRE(Field_FinishDoorAnimationEntry(&room.locks[0]) == 1);
  CHECK(room.locks[1].isOpen == -1);
}

TEST_CASE("Native door entries refuse out of room and stale typed targets",
          "[door-animation-live][field]") {
  RoomObjectRecordFixture room;
  Field_MpdLockTrigger outsider{};
  CHECK(Field_BeginDoorToggleEntry(nullptr) == 0);
  CHECK(Field_StartDoorAnimationEntry(&outsider) == 0);
  CHECK(Field_FinishDoorAnimationEntry(&outsider) == 0);
  const VS::Field::RoomObjectTarget target{
      0, VS::Field::LiveInteractionTargetDomain().generation};
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK_FALSE(VS::Field::BeginLiveDoorToggle(target));
  CHECK_FALSE(VS::Field::StartLiveDoorAnimation(target));
  CHECK_FALSE(VS::Field::FinishLiveDoorAnimation(target));
  CHECK_FALSE(VS::Field::ToggleLiveDoorCollision(target));
}

TEST_CASE("Door lifecycle entries retain every original module address",
          "[door-animation-live][door-collision-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008BD74) ==
        reinterpret_cast<const void*>(&Field_ToggleDoorCollisionEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008BEBC) ==
        reinterpret_cast<const void*>(&Field_BeginDoorToggleEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008BF48) ==
        reinterpret_cast<const void*>(&Field_StartDoorAnimationEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C1C8) ==
        reinterpret_cast<const void*>(&Field_FinishDoorAnimationEntry));
}

TEST_CASE(
    "Queued door identities retire when the room record extent becomes partial",
    "[door-queue-bounds][door-queue-target][field]") {
  AnimationQueueFixture queue;
  RoomObjectRecordFixture room;
  const VS::Field::RoomObjectTarget target{
      0, VS::Field::LiveInteractionTargetDomain().generation};
  room.locks[0].delayFrames = 8;
  REQUIRE(VS::Field::QueueLiveGimmickAnimation(3, target));
  Field_roomData.header.lockTriggersLength = sizeof room.locks - 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(room.locks[0].delayFrames == 8);
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
}

TEST_CASE("Unlock effect preserves eligible lock levels and sound order",
          "[unlock-effect-core][field]") {
  const VS::Field::UnlockEffectTarget target =
      VS::Field::RoomObjectTarget{1, 2};
  const VS::Field::TrapPosition point{1, -2, 3};
  for (std::int16_t level = -1; level <= 4; ++level) {
    RecordingUnlockEffectHost host;
    host.state = VS::Field::UnlockEffectState{.lock_level = level};
    const bool eligible = level == 2 || level == 3;
    CHECK(VS::Field::ApplyUnlockEffectToRoomObject(target, point, host) ==
          eligible);
    CHECK(host.calls == (eligible
                             ? std::vector<std::int32_t>{0, 2, 3, 0x3F, 0x40}
                             : std::vector<std::int32_t>{0, 4}));
    CHECK(host.label.has_value() != eligible);
  }
}

TEST_CASE("Unlock effect chest opened gate runs before risk or mutation",
          "[unlock-effect-core][field]") {
  const VS::Field::UnlockEffectTarget target = VS::Field::ChestTarget{1, 2};
  RecordingUnlockEffectHost host;
  host.state =
      VS::Field::UnlockEffectState{.lock_level = 3, .opened_flag = 0x7FFF};
  host.opened = true;
  CHECK_FALSE(VS::Field::ApplyUnlockEffectToRoomObject(target, {}, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 4});
  CHECK(host.flag == std::bit_cast<std::int16_t>(std::uint16_t{0x83BF}));
  host.calls.clear();
  host.opened = false;
  REQUIRE(VS::Field::ApplyUnlockEffectToRoomObject(target, {}, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 1, 2, 3, 0x3F, 0x40});
}

TEST_CASE("Unlock effect preserves every wrapped opened flag halfword",
          "[unlock-effect-core][field]") {
  const VS::Field::UnlockEffectTarget target = VS::Field::ChestTarget{0, 1};
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    RecordingUnlockEffectHost host;
    host.state = VS::Field::UnlockEffectState{
        .lock_level = 2, .opened_flag = static_cast<std::uint16_t>(bits)};
    REQUIRE(VS::Field::ApplyUnlockEffectToRoomObject(target, {}, host));
    CHECK(host.flag == std::bit_cast<std::int16_t>(
                           static_cast<std::uint16_t>(bits + 0x3C0)));
  }
}

TEST_CASE(
    "Unlock effect refuses unknown absent and failed targets without sounds",
    "[unlock-effect-core][field]") {
  RecordingUnlockEffectHost host;
  CHECK_FALSE(VS::Field::ApplyUnlockEffectToRoomObject(std::nullopt, {}, host));
  CHECK(host.calls.empty());
  const VS::Field::UnlockEffectTarget target =
      VS::Field::RoomObjectTarget{0, 1};
  host.state.reset();
  CHECK_FALSE(VS::Field::ApplyUnlockEffectToRoomObject(target, {}, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 4});
  host.calls.clear();
  host.state = VS::Field::UnlockEffectState{.lock_level = 2};
  host.roll = false;
  CHECK_FALSE(VS::Field::ApplyUnlockEffectToRoomObject(target, {}, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 2, 4});
  host.calls.clear();
  host.roll = true;
  host.clear = false;
  CHECK_FALSE(VS::Field::ApplyUnlockEffectToRoomObject(target, {}, host));
  CHECK(host.calls == std::vector<std::int32_t>{0, 2, 3, 4});
}

TEST_CASE("Native unlock effect clears eligible doors and both chest tags",
          "[unlock-effect-live][field]") {
  UnlockEffectLiveFixture fixture;
  SVECTOR point{.vx = -1, .vy = -2, .vz = 3, .pad = 0x10};
  const auto original =
      std::bit_cast<std::array<std::byte, sizeof(point)>>(point);
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  CHECK(fixture.room.locks[0].trigger.lockState.lock.state == 0);
  CHECK(Battle_floatingLabelCount == 0);
  CHECK(std::bit_cast<std::array<std::byte, sizeof(point)>>(point) == original);
  point.pad = 0x20;
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  CHECK(fixture.chests[0].lockLevel == 0);
  fixture.chests[0].lockLevel = 2;
  point.pad = 0x100;
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  CHECK(fixture.chests[0].lockLevel == 0);
}

TEST_CASE("Native unlock effect invalid indices label without reading a record",
          "[unlock-effect-live][field]") {
  UnlockEffectLiveFixture fixture;
  SVECTOR point{.vx = -1, .vy = -2, .vz = 3, .pad = 0x1F};
  const auto saved_risk = Core_randomShuffleIndex;
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  CHECK(Battle_floatingLabelCount == 1);
  CHECK(fixture.room.locks[0].trigger.lockState.lock.state == 2);
  CHECK(Core_randomShuffleIndex == saved_risk);
  point.pad = 0x2F;
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  CHECK(Battle_floatingLabelCount == 2);
  CHECK(fixture.chests[0].lockLevel == 3);
  CHECK(Core_randomShuffleIndex == saved_risk);
}

TEST_CASE("Native unlock effect unknown and null targets remain no ops",
          "[unlock-effect-live][field]") {
  UnlockEffectLiveFixture fixture;
  SVECTOR point{.vx = 0, .vy = 0, .vz = 0, .pad = 0x30};
  const auto saved_risk = Core_randomShuffleIndex;
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  Field_ApplyUnlockEffectToRoomObjectEntry(nullptr);
  CHECK(Battle_floatingLabelCount == 0);
  CHECK(Core_randomShuffleIndex == saved_risk);
  CHECK(fixture.room.locks[0].trigger.lockState.lock.state == 2);
  CHECK(fixture.chests[0].lockLevel == 3);
}

TEST_CASE("Native opened chest unlock effect skips risk and preserves its lock",
          "[unlock-effect-live][field]") {
  UnlockEffectLiveFixture fixture;
  fixture.chests[0].openedFlag = 1;
  VS::State::SetLiveScriptFlag(0x3C1, 1);
  const auto saved_risk = Core_randomShuffleIndex;
  SVECTOR point{.vx = 1, .vy = -2, .vz = 3, .pad = 0x20};
  Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  CHECK(fixture.chests[0].lockLevel == 3);
  CHECK(Battle_floatingLabelCount == 1);
  CHECK(Core_randomShuffleIndex == saved_risk);
}

TEST_CASE("Native unlock effect copies unaligned packed input without mutation",
          "[unlock-effect-live][field]") {
  UnlockEffectLiveFixture fixture;
  const SVECTOR point{.vx = -1, .vy = -2, .vz = 3, .pad = 0x10};
  std::array<std::byte, sizeof(point) + 1> bytes{};
  std::memcpy(bytes.data() + 1, &point, sizeof(point));
  const auto saved = bytes;
  Field_ApplyUnlockEffectToRoomObjectEntry(
      reinterpret_cast<SVECTOR*>(bytes.data() + 1));
  CHECK(fixture.room.locks[0].trigger.lockState.lock.state == 0);
  CHECK(bytes == saved);
}

TEST_CASE("Unlock effect keeps its original module entry address",
          "[unlock-effect-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x8006CE70) ==
      reinterpret_cast<const void*>(&Field_ApplyUnlockEffectToRoomObjectEntry));
}

TEST_CASE("Active room trigger values preserve order and current identities",
          "[active-trigger-view][field]") {
  SigilDoorLiveFixture state;
  std::array<Field_MpdFloorTrigger, 2> triggers{};
  triggers[0].kind = 128;
  triggers[0].tileX = -1;
  triggers[0].tileZ = 31;
  triggers[1].kind = -1;
  triggers[1].tileX = 15;
  triggers[1].tileZ = 7;
  Field_roomData.floorTriggers = triggers.data();
  Field_roomData.header.floorTriggersLength = sizeof triggers;
  const auto values = VS::Field::LiveActiveRoomTriggerInfo();
  REQUIRE(values.size() == 2);
  CHECK(values[0].kind == 128);
  CHECK(values[0].tile_x == -1);
  CHECK(values[0].tile_z == 31);
  CHECK(values[1].kind == -1);
  CHECK(values[1].target.index == 1);
  CHECK(VS::Field::ResolveLiveTrapTarget(values[1].target) == &triggers[1]);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK(VS::Field::ResolveLiveTrapTarget(values[1].target) == nullptr);
}

namespace {
struct RoomTriggerLiveFixture : SigilDoorLiveFixture {
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  using FlagRow = std::uint16_t[32];
  std::array<FlagRow, 32> flags{};
  std::array<Field_MpdFloorTrigger, 3> triggers{};
  RoomTriggerLiveFixture() {
    Field_roomData.floorTriggers = triggers.data();
    Field_roomData.header.floorTriggersLength = sizeof(triggers);
    Battle_Ai_tileFlagMap = flags.data();
    triggers[0].kind = 15;
    triggers[1].kind = 16;
    triggers[2].tileX = -1;
    for (auto& trigger : triggers) trigger.activation = 7;
    flags[0][0] = 0x80;
  }
  ~RoomTriggerLiveFixture() { Battle_Ai_tileFlagMap = saved_flags; }
};
}  // namespace

TEST_CASE(
    "Native room trigger lookup preserves first-match suppression and signed "
    "query",
    "[room-trigger-live][field]") {
  RoomTriggerLiveFixture fixture;
  CHECK(Field_RoomTriggerAtTileEntry(0, 0, 0) == &fixture.triggers[0]);
  Audio_Seq_unusedStateFlags.opaque20 = std::bit_cast<char>(std::uint8_t{128});
  CHECK(Field_RoomTriggerAtTileEntry(0, 0, 0) == nullptr);
  fixture.triggers[0].kind = 16;
  CHECK(Field_RoomTriggerAtTileEntry(0, 0, 0) == &fixture.triggers[0]);
  CHECK(Field_RoomTriggerAtTileEntry(-1, 0, 0) == nullptr);
  Audio_Seq_unusedStateFlags.opaque20 = 0;
  CHECK(Field_RoomTriggerAtTileEntry(-1, 0, 0) == &fixture.triggers[2]);
  CHECK(Field_RoomTriggerAtTileEntry(65535, 0, 0) == nullptr);
}

TEST_CASE(
    "Native room trigger activation mutates only low-byte activation fields",
    "[room-trigger-live][field]") {
  RoomTriggerLiveFixture fixture;
  const auto before = fixture.triggers;
  Field_SetRoomTriggerEnabledEntry(&fixture.triggers[1], -1);
  CHECK(static_cast<std::uint8_t>(fixture.triggers[0].activation) == 7);
  CHECK(static_cast<std::uint8_t>(fixture.triggers[1].activation) == 255);
  Field_SetRoomTriggerEnabledEntry(nullptr, 257);
  const auto prior = std::as_bytes(std::span(before));
  const auto after = std::as_bytes(std::span(fixture.triggers));
  for (std::size_t offset = 0; offset < after.size(); ++offset) {
    const auto activation = offset % sizeof(Field_MpdFloorTrigger) ==
                            offsetof(Field_MpdFloorTrigger, activation);
    CHECK(after[offset] == (activation ? std::byte{1} : prior[offset]));
  }
}

TEST_CASE(
    "Native room trigger activation refuses stale partial and foreign targets",
    "[room-trigger-live][field]") {
  RoomTriggerLiveFixture fixture;
  const auto target = VS::Field::FindLiveTrapTarget(&fixture.triggers[0]);
  REQUIRE(target);
  VS::Field::InvalidateLiveInteractionTargets();
  CHECK_FALSE(VS::Field::SetLiveRoomTriggerEnabled(target, 1));
  Field_MpdFloorTrigger foreign{};
  foreign.activation = 9;
  Field_SetRoomTriggerEnabledEntry(&foreign, 1);
  CHECK(foreign.activation == 9);
  Field_roomData.header.floorTriggersLength = sizeof(fixture.triggers) - 1;
  CHECK(Field_RoomTriggerAtTileEntry(0, 0, 0) == nullptr);
  CHECK_FALSE(VS::Field::SetLiveRoomTriggerEnabled(std::nullopt, 1));
  CHECK(fixture.triggers[0].activation == 7);
}

TEST_CASE(
    "Native trap disabling activates before marking actual AI obstacle flag",
    "[trap-disable-live][field]") {
  RoomTriggerLiveFixture fixture;
  SVECTOR position{};
  position.vx = -127;
  position.vz = -1;
  Field_DisableTrapAtPositionEntry(&position);
  CHECK(fixture.triggers[0].activation == 1);
  CHECK(fixture.triggers[1].activation == 7);
  CHECK(fixture.flags[0][0] == 0x480);
  Field_DisableTrapAtPositionEntry(nullptr);
}

TEST_CASE(
    "Room trigger query activation and disabling retain original module "
    "addresses",
    "[room-trigger-live][trap-disable-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D438) ==
        reinterpret_cast<const void*>(&Field_RoomTriggerAtTileEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008DEAC) ==
        reinterpret_cast<const void*>(&Field_SetRoomTriggerEnabledEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8006CDD8) ==
        reinterpret_cast<const void*>(&Field_DisableTrapAtPositionEntry));
}

TEST_CASE("Active room triggers preserve both unsigned suppression flags",
          "[active-trigger-view][field]") {
  SigilDoorLiveFixture state;
  std::array<Field_MpdFloorTrigger, 1> triggers{};
  Field_roomData.floorTriggers = triggers.data();
  Field_roomData.header.floorTriggersLength = sizeof triggers;
  Audio_Seq_unusedStateFlags.opaque20 = std::bit_cast<char>(std::uint8_t{128});
  CHECK(VS::Field::LiveActiveRoomTriggerInfo().empty());
  Audio_Seq_unusedStateFlags.opaque20 = 0;
  Audio_Seq_unusedStateFlags.opaque40 = std::bit_cast<char>(std::uint8_t{255});
  CHECK(VS::Field::LiveActiveRoomTriggerInfo().empty());
  Audio_Seq_unusedStateFlags.opaque40 = 0;
  CHECK(VS::Field::LiveActiveRoomTriggerInfo().size() == 1);
}

TEST_CASE("Active trigger views and identities reject incomplete record tables",
          "[active-trigger-view][field]") {
  SigilDoorLiveFixture state;
  std::array<Field_MpdFloorTrigger, 1> triggers{};
  Field_roomData.floorTriggers = triggers.data();
  Field_roomData.header.floorTriggersLength = sizeof triggers - 1;
  CHECK(VS::Field::LiveActiveRoomTriggerInfo().empty());
  CHECK_FALSE(VS::Field::FindLiveTrapTarget(&triggers[0]));
  Field_roomData.header.floorTriggersLength = sizeof triggers;
  Field_roomData.floorTriggers = reinterpret_cast<Field_MpdFloorTrigger*>(
      reinterpret_cast<std::byte*>(triggers.data()) + 1);
  CHECK(VS::Field::LiveActiveRoomTriggerInfo().empty());
  Field_roomData.floorTriggers = nullptr;
  CHECK(VS::Field::LiveActiveRoomTriggerInfo().empty());
}

TEST_CASE("Typed switch start restores state on refused delayed admission",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.state = VS::Field::RoomSwitchState{.activation = 7};
  host.refuse = true;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  REQUIRE(host.state);
  CHECK(host.state->marker_fade == 0);
  CHECK(host.state->activation == 7);
  CHECK(host.calls == std::vector<std::int32_t>{1, 20, 2, 0, 7});
  host.refuse = false;
  CHECK(VS::Field::BeginSwitchToggle(host));
  CHECK(host.state->marker_fade == 1);
  CHECK(host.state->activation == 20);
}

TEST_CASE("Typed switch start preserves immediate and busy behavior",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.state = VS::Field::RoomSwitchState{.switch_value = 2, .activation = 7};
  CHECK(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == std::vector<std::int32_t>{1, 7, 3});
  const auto before = host.calls;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == before);
  host.state.reset();
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls == before);
}

TEST_CASE("Typed switch start cannot dispatch after a refused state store",
          "[gimmick-switch-admission-core][field]") {
  RecordingSwitchStartHost host;
  host.refuse_store = true;
  CHECK_FALSE(VS::Field::BeginSwitchToggle(host));
  CHECK(host.calls.empty());
}

TEST_CASE("Typed animation reset preserves original count tick and slot order",
          "[gimmick-animation-reset-core][field]") {
  RecordingAnimationResetHost host;
  VS::Field::ResetGimmickAnimationQueue(host);
  CHECK(host.calls == std::vector<std::size_t>{8, 9, 0, 1, 2, 3, 4, 5, 6, 7});
  host.calls.clear();
  VS::Field::ResetGimmickAnimationQueue(host);
  CHECK(host.calls == std::vector<std::size_t>{8, 9, 0, 1, 2, 3, 4, 5, 6, 7});
}

TEST_CASE(
    "Native animation reset clears every slot counters and module binding",
    "[gimmick-animation-reset-core][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008B6B4) ==
        reinterpret_cast<const void*>(&Field_ResetGimmickAnimationQueueEntry));
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  for (auto& target : targets) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_triggerMarkerFrameTicks = 0xFFFF;
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_triggerMarkerFrameTicks == 0);
  for (const auto& job : Field_gimmickAnimationQueue) {
    CHECK(job.jobKind == 0);
    CHECK(job.trigger == nullptr);
  }
  Field_ResetGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_triggerMarkerFrameTicks == 0);
}

TEST_CASE("Typed animation dispatch tolerates a reset inside its callback",
          "[gimmick-animation-reset-core][field]") {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      VS::Field::GimmickAnimationKind::kFinishDoor, 0, 1, true};
  host.reset_during_finish = true;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 0);
  CHECK(host.PendingCount() == 0);
  CHECK(host.calls == std::vector<std::int32_t>{1});
}

TEST_CASE("Typed animation dispatch preserves every original countdown byte",
          "[gimmick-animation-dispatch-core][field]") {
  for (const auto kind : {VS::Field::GimmickAnimationKind::kToggleSwitch,
                          VS::Field::GimmickAnimationKind::kStartDoor}) {
    for (int byte = 0; byte < 256; ++byte) {
      CheckOriginalCountdownByte(kind, byte);
    }
  }
}

TEST_CASE("Typed animation dispatch reserves full door slot before callback",
          "[gimmick-animation-dispatch-core][field]") {
  RecordingAnimationDispatchHost host;
  for (std::size_t slot = 0; slot < host.jobs.size(); ++slot) {
    host.jobs[slot] = VS::Field::GimmickAnimationJobSnapshot{
        VS::Field::GimmickAnimationKind::kStartDoor, 1, slot + 1, true};
  }
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 8);
  REQUIRE(host.calls.size() == 16);
  CHECK(host.calls[0] == 4);
  CHECK(host.calls[1] == 3);
  CHECK(host.calls[2] == 3);
  CHECK(host.calls[3] == 4);
}

TEST_CASE("Typed animation dispatch never clears a callback replacement job",
          "[gimmick-animation-dispatch-core][field]") {
  RecordingAnimationDispatchHost host;
  host.jobs[0] = VS::Field::GimmickAnimationJobSnapshot{
      VS::Field::GimmickAnimationKind::kFinishDoor, 0, 1, true};
  host.replace_during_finish = true;
  CHECK(VS::Field::UpdateGimmickAnimationQueue(host) == 0);
  CHECK(host.PendingCount() == 1);
  CHECK(host.calls == std::vector<std::int32_t>{1});
}

TEST_CASE("Typed gimmick admission rejects invalid requests before slot reads",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  for (const auto kind :
       {-1, 0, 4, 257, std::numeric_limits<std::int32_t>::max()}) {
    CHECK_FALSE(VS::Field::QueueGimmickAnimation(
        static_cast<VS::Field::GimmickAnimationKind>(kind), host));
  }
  host.target = false;
  CHECK_FALSE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kFinishDoor, host));
  CHECK(host.reads == 0);
  CHECK(host.stores == 0);
  CHECK(host.count_writes == 0);
}

TEST_CASE("Typed gimmick admission counts only committed first-free slots",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  host.slots[0] = true;
  host.slots[2] = true;
  REQUIRE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kToggleSwitch, host));
  CHECK(host.stored_slot == 1);
  CHECK(host.pending == 3);
  host.refuse_store = true;
  CHECK_FALSE(VS::Field::QueueGimmickAnimation(
      VS::Field::GimmickAnimationKind::kStartDoor, host));
  CHECK(host.pending == 3);
  CHECK(host.count_writes == 1);
}

TEST_CASE("Typed gimmick admission cannot drift when a full queue refuses",
          "[gimmick-animation-queue-core][field]") {
  RecordingAnimationQueueHost host;
  for (std::size_t slot = 0; slot < VS::Field::kGimmickAnimationSlots; ++slot) {
    REQUIRE(VS::Field::QueueGimmickAnimation(
        VS::Field::GimmickAnimationKind::kFinishDoor, host));
    CHECK(host.stored_slot == slot);
    CHECK(host.pending == slot + 1);
  }
  const auto before = host.slots;
  for (int refusal = 0; refusal < 16; ++refusal) {
    CHECK_FALSE(VS::Field::QueueGimmickAnimation(
        VS::Field::GimmickAnimationKind::kFinishDoor, host));
    CHECK(host.pending == 8);
    CHECK(host.slots == before);
  }
  CHECK(host.stores == 8);
  CHECK(host.count_writes == 8);
}

TEST_CASE("Gimmick animation queue does not count a refused ninth job",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots + 1> targets{};
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    Field_QueueGimmickAnimationEntry(1, &targets[slot]);
  }
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_QueueGimmickAnimationEntry(1, &targets.back());
  CHECK(Field_pendingGimmickAnimationCount == 8);
  for (std::size_t slot = 0; slot < AnimationQueueFixture::kSlots; ++slot) {
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 1);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == &targets[slot]);
  }
}

TEST_CASE("Native gimmick admission refuses invalid calls and preserves module",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C070) ==
        reinterpret_cast<const void*>(&Field_QueueGimmickAnimationEntry));
  Field_MpdLockTrigger target{};
  for (const auto kind :
       {-1, 0, 4, 257, std::numeric_limits<std::int32_t>::max()}) {
    Field_QueueGimmickAnimationEntry(kind, &target);
  }
  Field_QueueGimmickAnimationEntry(1, nullptr);
  Field_QueueGimmickAnimationEntry(
      1, reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1}));
  CHECK(Field_pendingGimmickAnimationCount == 0);
  for (const auto& job : Field_gimmickAnimationQueue) {
    CHECK(job.jobKind == 0);
    CHECK(job.trigger == nullptr);
  }
}

TEST_CASE("Native gimmick admission drains accepted jobs and resets cleanly",
          "[gimmick-animation-queue][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots + 1> targets{};
  QueueOneFrameSwitchJobs(targets);
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CheckDrainedSwitchJobs(targets);
  CheckAnimationKindResets(targets.back());
}

TEST_CASE("Delayed door follow-up survives a saturated animation queue",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  AnimationQueueFixture::BindDoors(targets);
  for (auto& target : targets) {
    ConfigureOneFrameDelayedDoor(target);
    Field_QueueGimmickAnimationEntry(3, &target);
  }
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 8);
  for (const auto& target : targets) {
    const auto found = std::ranges::find_if(
        Field_gimmickAnimationQueue, [&target](const auto& job) {
          return job.jobKind == 1 && job.trigger == &target;
        });
    CHECK(found != std::end(Field_gimmickAnimationQueue));
  }
}

TEST_CASE("Saturated door continuations drain exactly on the following tick",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> targets{};
  AnimationQueueFixture::BindDoors(targets);
  for (auto& target : targets) {
    ConfigureOneFrameDelayedDoor(target);
    Field_QueueGimmickAnimationEntry(3, &target);
  }
  Field_UpdateGimmickAnimationQueueEntry();
  REQUIRE(Field_pendingGimmickAnimationCount == 8);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  for (std::size_t slot = 0; slot < targets.size(); ++slot) {
    CHECK(targets[slot].isOpen == 1);
    CHECK(targets[slot].isTriggered == 0);
    CHECK(Field_gimmickAnimationQueue[slot].jobKind == 0);
    CHECK(Field_gimmickAnimationQueue[slot].trigger == nullptr);
  }
}

TEST_CASE("Unsaturated delayed doors retain original same-tick finish order",
          "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  Field_MpdLockTrigger target{};
  AnimationQueueFixture::BindDoors({&target, 1});
  ConfigureOneFrameDelayedDoor(target);
  Field_QueueGimmickAnimationEntry(3, &target);
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(target.isOpen == 1);
  CHECK(target.isTriggered == 0);
}

TEST_CASE(
    "Native animation dispatcher refuses bad entries and preserves module",
    "[gimmick-door-continuation][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C0C0) ==
        reinterpret_cast<const void*>(&Field_UpdateGimmickAnimationQueueEntry));
  Field_gimmickAnimationQueue[0] = {2, nullptr};
  Field_pendingGimmickAnimationCount = 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
  Field_MpdLockTrigger target{};
  Field_gimmickAnimationQueue[0] = {99, &target};
  Field_pendingGimmickAnimationCount = 1;
  Field_UpdateGimmickAnimationQueueEntry();
  CHECK(Field_pendingGimmickAnimationCount == 0);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 0);
}

TEST_CASE("Refused switch start leaves its busy marker retryable",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> occupied{};
  for (auto& target : occupied) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_MpdLockTrigger rejected{};
  rejected.trigger.activation = 7;
  Field_BeginSwitchToggleEntry(&rejected);
  CHECK(Field_pendingGimmickAnimationCount == 8);
  CHECK(rejected.trigger.markerFade == 0);
  CHECK(rejected.trigger.activation == 7);
}

TEST_CASE("Native switch start retries after queue capacity is freed",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  std::array<Field_MpdLockTrigger, AnimationQueueFixture::kSlots> occupied{};
  for (auto& target : occupied) {
    Field_QueueGimmickAnimationEntry(1, &target);
  }
  Field_MpdLockTrigger target{};
  target.trigger.activation = 7;
  Field_BeginSwitchToggleEntry(&target);
  REQUIRE(target.trigger.markerFade == 0);
  Field_ResetGimmickAnimationQueueEntry();
  Field_BeginSwitchToggleEntry(&target);
  CHECK(Field_pendingGimmickAnimationCount == 1);
  CHECK(target.trigger.markerFade == 1);
  CHECK(target.trigger.activation == 20);
  CHECK(Field_gimmickAnimationQueue[0].jobKind == 2);
  CHECK(Field_gimmickAnimationQueue[0].trigger == &target);
  Field_BeginSwitchToggleEntry(&target);
  CHECK(Field_pendingGimmickAnimationCount == 1);
}

TEST_CASE("Native switch start preserves immediate state and original module",
          "[gimmick-switch-admission][field]") {
  const AnimationQueueFixture fixture;
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D5FC) ==
        reinterpret_cast<const void*>(&Field_BeginSwitchToggleEntry));
  Field_BeginSwitchToggleEntry(nullptr);
  Field_BeginSwitchToggleEntry(
      reinterpret_cast<Field_MpdLockTrigger*>(std::uintptr_t{1}));
  for (const auto value : {-1, 1, 32767, -32768}) {
    Field_MpdLockTrigger target{};
    target.trigger.lockState.switchValue = static_cast<std::int16_t>(value);
    target.trigger.kind = -1;
    target.trigger.tileX = -1;
    target.trigger.tileZ = -1;
    target.trigger.floorLayer = -1;
    Field_BeginSwitchToggleEntry(&target);
    CHECK(Field_pendingGimmickAnimationCount == 0);
    CHECK(target.trigger.lockState.switchValue == 0);
    CHECK(target.trigger.markerFade == 0);
  }
}

TEST_CASE("Room switches preserve original sounds and normalized state order",
          "[room-switches][field]") {
  for (const auto value : {0, 1, 2, 65535}) {
    RecordingSwitchHost host;
    host.state = VS::Field::RoomSwitchState{
        .switch_value = static_cast<std::uint16_t>(value),
        .marker_fade = 0xFF,
        .effect_kind = 7,
        .tile_x = 3,
        .tile_z = 9,
        .floor_layer = -1};
    REQUIRE(VS::Field::ToggleRoomSwitch(host));
    const std::vector<SwitchCall> expected{
        {SwitchCallKind::kSound, value == 0 ? 0x3B : 0x3D},
        {SwitchCallKind::kSound, value == 0 ? 0x3C : 0x3E},
        {SwitchCallKind::kEffect, 7},
        {SwitchCallKind::kStore, value == 0 ? 1 : 0},
        {SwitchCallKind::kCell, value == 0 ? 5 : 0}};
    CHECK(host.calls == expected);
    if (!host.state) {
      FAIL("Room switch state must remain valid after a completed toggle");
      return;
    }
    CHECK(host.state->marker_fade == 0);
    CHECK(host.cell_query == std::array<std::int32_t, 3>{3, 9, -1});
  }
}

TEST_CASE("Room switches use state after effect callbacks",
          "[room-switches][field]") {
  RecordingSwitchHost host;
  host.state = VS::Field::RoomSwitchState{.effect_kind = 7};
  host.after_effect = VS::Field::RoomSwitchState{.switch_value = 2,
                                                 .marker_fade = 5,
                                                 .tile_x = 11,
                                                 .tile_z = 12,
                                                 .floor_layer = 3};
  REQUIRE(VS::Field::ToggleRoomSwitch(host));
  const std::vector<SwitchCall> expected{{SwitchCallKind::kSound, 0x3B},
                                         {SwitchCallKind::kSound, 0x3C},
                                         {SwitchCallKind::kEffect, 7},
                                         {SwitchCallKind::kStore, 0},
                                         {SwitchCallKind::kCell, 0}};
  CHECK(host.calls == expected);
  CHECK(host.cell_query == std::array<std::int32_t, 3>{11, 12, 3});
}

TEST_CASE("Room switches refuse absent and invalidated targets",
          "[room-switches][field]") {
  RecordingSwitchHost host;
  CHECK_FALSE(VS::Field::ToggleRoomSwitch(host));
  CHECK(host.calls.empty());
  host.state = VS::Field::RoomSwitchState{};
  host.remove_after_sound = true;
  CHECK_FALSE(VS::Field::ToggleRoomSwitch(host));
  CHECK(host.calls.size() == 2);
}

TEST_CASE("Native switch entry preserves original module and signed word state",
          "[room-switches][room-switches-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008D658) ==
        reinterpret_cast<const void*>(&Field_ToggleRoomSwitchEntry));
  Field_ToggleRoomSwitchEntry(nullptr);
  for (const auto value : {0, 1, 2, -1, -32768, 32767}) {
    Field_MpdLockTrigger record{};
    record.trigger.tileX = -1;
    record.trigger.tileZ = -1;
    record.trigger.floorLayer = -1;
    record.trigger.kind = -1;
    record.trigger.lockState.switchValue = static_cast<std::int16_t>(value);
    record.trigger.markerFade = -1;
    Field_ToggleRoomSwitchEntry(&record);
    CHECK(record.trigger.lockState.switchValue == (value == 0 ? 1 : 0));
    CHECK(record.trigger.markerFade == 0);
  }
}

TEST_CASE(
    "Native gimmick placement entry preserves the original module address",
    "[gimmick-placement][gimmick-placement-core][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A0024) ==
        reinterpret_cast<const void*>(&Field_GimmickPlacementEntry));
}

TEST_CASE("Typed gimmick placement refuses slots before record access",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  for (const auto slot : {std::numeric_limits<std::int32_t>::min(), -1, 16,
                          std::numeric_limits<std::int32_t>::max()}) {
    CHECK_FALSE(VS::Field::GimmickPlacement(slot, host));
  }
  CHECK(host.reads == 0);
  CHECK(host.support_calls == 0);
  for (const auto slot : {0, 15}) {
    CHECK_FALSE(VS::Field::GimmickPlacement(slot, host));
  }
  CHECK(host.reads == 2);
  CHECK(host.support_calls == 0);
}

TEST_CASE("Typed gimmick placement refuses motion before support sampling",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  for (const auto mode : {0, 1, -1, 32767, -32768}) {
    VS::Field::GimmickPlacementState state{
        .attached = mode == 0,
        .movement_mode = static_cast<std::int16_t>(mode)};
    host.states[0] = state;
    CHECK_FALSE(VS::Field::GimmickPlacement(0, host));
  }
  host.states[0] = VS::Field::GimmickPlacementState{.falling = true};
  CHECK_FALSE(VS::Field::GimmickPlacement(0, host));
  CHECK(host.support_calls == 0);
}

TEST_CASE("Typed gimmick placement matches original word stores and rounding",
          "[gimmick-placement][gimmick-placement-core][field]") {
  RecordingGimmickHost host;
  VS::Field::GimmickPlacementState state{
      .tile_x = 0xFF,
      .tile_z = 9,
      .orientation = 0x81,
      .position_x = -1234,
      .position_z = 2345,
      .position_tail = std::bit_cast<std::int16_t>(std::uint16_t{0xFEDC})};
  for (const auto support :
       {std::numeric_limits<std::int32_t>::min(), -3000, -1, 0, 1, 3000,
        std::numeric_limits<std::int32_t>::max()}) {
    host.support_height = support;
    for (const auto height :
         {-32768, -129, -128, -127, -1, 0, 1, 127, 128, 129, 32767}) {
      state.position_y = static_cast<std::int16_t>(height);
      host.states[0] = state;
      CheckTypedGimmickPlacement(host, state);
    }
  }
  CHECK(host.support_calls == 77);
}

TEST_CASE("Gimmick snapshots refuse invalid and absent slots without writing",
          "[gimmick-placement][field]") {
  SnapshotFixture fixture;
  auto output = SentinelSnapshot();
  const auto before = std::bit_cast<SnapshotBytes>(output);
  for (int slot = -2; slot < 18; ++slot) {
    CHECK(Field_GimmickPlacementEntry(slot, &output) == -1);
    CHECK(std::bit_cast<SnapshotBytes>(output) == before);
  }
  Field_gimmickInstances[0] = fixture.object.get();
  CHECK(Field_GimmickPlacementEntry(0, nullptr) == -1);
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
    CHECK(Field_GimmickPlacementEntry(0, &output) == -1);
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
  CHECK(Event_EvaluateRoomEventTriggersEntry(4, 0, tile.data(), 1) == 1);
  CHECK(VS::Event::LiveScriptVm().EventId() == 12);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  CHECK(Event_EvaluateRoomEventTriggersEntry(4, 0, nullptr, 1) == 0);
}

TEST_CASE("Facing event entry dispatches adjacent tile through native triggers",
          "[facing-query-live][room-triggers][event-vm-live]") {
  TriggerFixture fixture;
  auto& object = *fixture.snapshots.object;
  object.tileX = 7;
  object.tileZ = 10;
  Field_gimmickInstances[0] = &object;
  PublishTrigger({4, 0, 255, 255, 4, 12, 0, 8, 7, 9, 255});
  CHECK(Field_TryTriggerFacingTileEventEntry(0) == 1);
  CHECK(VS::Event::LiveScriptVm().EventId() == 12);
  CHECK(VS::Event::LiveScriptVm().Phase() ==
        VS::Event::PlaybackPhase::kRequested);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kRunning);
  Event_isDirectEventRequest = 1;
  CHECK(Field_TryTriggerFacingTileEventEntry(0) == 0);
  VS::Event::LiveScriptVm().SetPhase(VS::Event::PlaybackPhase::kIdle);
  Event_isDirectEventRequest = 0;
  object.tileZ = 11;
  CHECK(Field_TryTriggerFacingTileEventEntry(0) == 0);
  CHECK(VS::Event::LiveScriptVm().Phase() == VS::Event::PlaybackPhase::kIdle);
}

TEST_CASE(
    "Gimmick creation refuses busy before publication and publishes before "
    "model failure",
    "[gimmick-create-core][field]") {
  RecordingGimmickCreationHost host;
  VS::Content::ActorAssetRequest request{.target_slot = 3, .sub_kind = 6};
  host.states[3] = VS::Field::GimmickCreationState{};
  CHECK(VS::Field::CreateGimmickInstance(request, host) == -3);
  CHECK(host.calls == std::vector<std::string>{"occupied"});
  host.states[3].reset();
  host.calls.clear();
  host.model = false;
  CHECK(VS::Field::CreateGimmickInstance(request, host) == -2);
  CHECK(host.calls == std::vector<std::string>{"occupied", "publish", "clear",
                                               "slots", "model6:5"});
  CHECK(host.states[3].has_value());
}

TEST_CASE("Gimmick creation retains null destination and invalid-slot refusal",
          "[gimmick-create-core][field]") {
  RecordingGimmickCreationHost host;
  VS::Content::ActorAssetRequest request{.target_slot = -1};
  CHECK(VS::Field::CreateGimmickInstance(request, host) == -2);
  CHECK(host.calls.empty());
  request.target_slot = 0;
  host.destination = false;
  CHECK(VS::Field::CreateGimmickInstance(request, host) == -2);
  CHECK(host.calls == std::vector<std::string>{"occupied", "publish"});
  CHECK_FALSE(host.states[0].has_value());
}

TEST_CASE(
    "Gimmick creation shares first other model and applies original defaults",
    "[gimmick-create-core][field]") {
  RecordingGimmickCreationHost host;
  host.states[1] =
      VS::Field::GimmickCreationState{.object_id = 1, .subtype = 7};
  VS::Content::ActorAssetRequest request{.target_slot = 3,
                                         .placement = 0x77040802,
                                         .sub_kind = 1,
                                         .texture_slot = 255,
                                         .material = 3,
                                         .variant = 1};
  REQUIRE(VS::Field::CreateGimmickInstance(request, host) == 0);
  CHECK(request.data == VS::Content::SharedAssetAddress(1));
  REQUIRE(host.states[3].has_value());
  CHECK(host.State(3).subtype == 255);
  CHECK(host.State(3).tile_word == 0x00040802);
  CHECK(host.State(3).x == 320);
  CHECK(host.State(3).z == 576);
  CHECK(host.State(3).y == 384);
}

TEST_CASE("Gimmick creation uses unsigned placement bytes and wrapped height",
          "[gimmick-create-core][field]") {
  RecordingGimmickCreationHost host;
  VS::Content::ActorAssetRequest request{
      .target_slot = 0, .placement = 0xFFFE00FF, .sub_kind = 0, .variant = 255};
  REQUIRE(VS::Field::CreateGimmickInstance(request, host) == 0);
  REQUIRE(host.states[0].has_value());
  CHECK(host.State(0).x == 32704);
  CHECK(host.State(0).z == 32576);
  CHECK(host.State(0).y == -32128);
  CHECK(host.State(0).scale_x == 64);
  CHECK(host.State(0).skipped);
  CHECK_FALSE(host.State(0).visible);
}

TEST_CASE("Gimmick creation stack takes the first earlier supporting type",
          "[gimmick-create-core][field]") {
  for (const std::uint8_t supporting : {5, 6}) {
    RecordingGimmickCreationHost host;
    host.states[0] = VS::Field::GimmickCreationState{
        .object_id = supporting, .tile_word = 0x00040802, .y = 512};
    VS::Content::ActorAssetRequest request{
        .target_slot = 3, .placement = 0x77040802, .sub_kind = 5, .variant = 1};
    REQUIRE(VS::Field::CreateGimmickInstance(request, host) == 0);
    REQUIRE(host.states[3].has_value());
    CHECK(host.State(3).interaction == (supporting == 5 ? 2 : 1));
    CHECK(host.State(3).y == (supporting == 5 ? 256 : 384));
    CHECK(std::count(host.calls.begin(), host.calls.end(), "occupancy") ==
          (supporting == 5 ? 2 : 1));
  }
}

TEST_CASE("Gimmick below keeps the first closest eligible full-word tile match",
          "[gimmick-stack-query-core][field]") {
  using State = VS::Field::GimmickQueryState;
  std::array<State, 4> states{};
  const State reference{.tile_word = 0xC0050203, .position_y = 256};
  states[0] = {
      .present = true, .tile_word = reference.tile_word, .position_y = 128};
  states[1] = states[0];
  states[2] = {
      .present = true, .tile_word = reference.tile_word, .position_y = 384};
  states[3] = {.present = true, .tile_word = 0x00050203, .position_y = 256};
  CHECK(VS::Field::FindGimmickBelow(states, reference) == 0);
  states[1].position_y = 256;
  CHECK(VS::Field::FindGimmickBelow(states, reference) == 1);
  states[1].carrier = 7;
  CHECK(VS::Field::FindGimmickBelow(states, reference) == 1);
}

TEST_CASE("Gimmick stack query height sentinels cover every signed halfword",
          "[gimmick-stack-query-core][field]") {
  using State = VS::Field::GimmickQueryState;
  std::array<State, 1> states{State{.present = true}};
  const State reference{.position_y = 32767};
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    const auto height =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
    states[0].position_y = height;
    CHECK(VS::Field::FindGimmickBelow(states, reference).has_value() ==
          (height > -30000));
    CHECK(VS::Field::FindTopGimmickAtTile(states, {}, false).has_value() ==
          (height <= 4096));
  }
}

TEST_CASE(
    "Gimmick below rejects original inactive falling moving and attached "
    "states",
    "[gimmick-stack-query-core][field]") {
  std::array<VS::Field::GimmickQueryState, 1> states{};
  for (std::uint32_t flags = 0; flags < 32; ++flags) {
    states[0] = {
        .present = (flags & 1) == 0,
        .skipped = (flags & 2) != 0,
        .falling = (flags & 4) != 0,
        .movement_mode = static_cast<std::int16_t>((flags & 8) != 0),
        .parent = static_cast<std::uint8_t>((flags & 16) != 0 ? 0 : 255)};
    CHECK(VS::Field::FindGimmickBelow(states, {}).has_value() == (flags == 0));
  }
}

TEST_CASE(
    "Gimmick top uses unsigned coordinates last equal height and separate idle "
    "gate",
    "[gimmick-stack-query-core][field]") {
  using State = VS::Field::GimmickQueryState;
  std::array<State, 3> states{
      State{.present = true, .tile_word = 0xC0FE00FF},
      State{.present = true, .tile_word = 0x00FE07FF},
      State{.present = true, .tile_word = 0x00FE00FF, .position_y = 1}};
  const VS::Field::GimmickTileQuery query{255, 254};
  CHECK(VS::Field::FindTopGimmickAtTile(states, query, false) == 1);
  states[1].falling = true;
  CHECK(VS::Field::FindTopGimmickAtTile(states, query, false) == 1);
  states[1].movement_mode = -1;
  CHECK(VS::Field::FindTopGimmickAtTile(states, query, false) == 1);
  CHECK(VS::Field::FindTopGimmickAtTile(states, query, true) == 0);
  states[0].carrier = 0;
  CHECK(VS::Field::FindTopGimmickAtTile(states, query, true) == 2);
  CHECK_FALSE(VS::Field::FindTopGimmickAtTile(states, {-1, -2}, false));
  CHECK_FALSE(VS::Field::FindTopGimmickAtTile(states, {256, 254}, false));
}

TEST_CASE(
    "Gimmick stack queries ignore records beyond sixteen and handle empty "
    "spans",
    "[gimmick-stack-query-core][field]") {
  std::array<VS::Field::GimmickQueryState, 17> states{};
  states.back().present = true;
  CHECK_FALSE(VS::Field::FindGimmickBelow(states, {}));
  CHECK_FALSE(VS::Field::FindTopGimmickAtTile(states, {}, false));
  CHECK_FALSE(VS::Field::FindGimmickBelow({}, {}));
  CHECK_FALSE(VS::Field::FindTopGimmickAtTile({}, {}, true));
}

TEST_CASE(
    "Native stack queries preserve unsigned tiles first below and last top tie",
    "[gimmick-stack-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  for (auto& record : fixture.instances) {
    record.parent = record.objectInfo.carrier = 255;
    record.tileX = std::bit_cast<char>(std::uint8_t{255});
    record.tileZ = std::bit_cast<char>(std::uint8_t{254});
    record.reserved5F = std::bit_cast<char>(std::uint8_t{192});
    record.positionY = 128;
  }
  using InstanceBytes = std::array<std::byte, sizeof(fixture.instances)>;
  const auto saved = std::bit_cast<InstanceBytes>(fixture.instances);
  auto reference = fixture.instances[0];
  reference.positionY = 256;
  CHECK(Field_FindGimmickBelowEntry(&reference) == 1);
  CHECK(Field_FindTopGimmickAtTileEntry(255, 254, 0) == 5);
  CHECK(Field_FindTopGimmickAtTileEntry(-1, 254, 0) == 0);
  CHECK(std::bit_cast<InstanceBytes>(fixture.instances) == saved);
  reference.reserved5F = 0;
  CHECK(Field_FindGimmickBelowEntry(&reference) == -1);
}

TEST_CASE("Native stack query idle control keeps all nonzero original requests",
          "[gimmick-stack-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.parent = record.objectInfo.carrier = 255;
  record.mode = -1;
  CHECK(Field_FindTopGimmickAtTileEntry(0, 0, 0) == 1);
  CHECK(Field_FindTopGimmickAtTileEntry(0, 0, 1) == 0);
  CHECK(Field_FindTopGimmickAtTileEntry(0, 0, -1) == 0);
  record.mode = 0;
  CHECK(Field_FindTopGimmickAtTileEntry(0, 0, -1) == 1);
}

TEST_CASE(
    "Native gimmick query snapshots detach values from pointer replacement",
    "[gimmick-stack-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  fixture.instances[0].positionY = 128;
  const auto state = VS::Field::LiveGimmickQueryState(1);
  REQUIRE(state.has_value());
  if (!state) return;
  fixture.instances[0].positionY = 256;
  Field_gimmickInstances[1] = nullptr;
  CHECK(state->position_y == 128);
  CHECK(state->object_id == 3);
  CHECK_FALSE(VS::Field::LiveGimmickQueryState(1));
  CHECK_FALSE(VS::Field::LiveGimmickQueryState(16));
}

TEST_CASE(
    "Native stack queries reject null short and misaligned record prefixes",
    "[gimmick-stack-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  auto* record =
      static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(64));
  REQUIRE(record != nullptr);
  std::memset(record, 0xA5, 64);
  Field_gimmickInstances[1] = reinterpret_cast<Field_GimmickInstance*>(record);
  CHECK_FALSE(VS::Field::LiveGimmickQueryState(1));
  CHECK(Field_FindGimmickBelowEntry(Field_gimmickInstances[1]) == -1);
  CHECK(Field_FindGimmickBelowEntry(nullptr) == -1);
  Field_gimmickInstances[1] =
      reinterpret_cast<Field_GimmickInstance*>(record + 1);
  CHECK_FALSE(VS::Field::LiveGimmickQueryState(1));
  CHECK(record[0] == std::byte{0xA5});
  CHECK(record[1] == std::byte{0xA5});
}

TEST_CASE("Gimmick stack queries retain both original module addresses",
          "[gimmick-stack-query-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8009F794) ==
        reinterpret_cast<const void*>(&Field_FindGimmickBelowEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A91DC) ==
        reinterpret_cast<const void*>(&Field_FindTopGimmickAtTileEntry));
}

TEST_CASE(
    "Gimmick subtype preserves all unsigned bytes and the original kind mask",
    "[gimmick-status-core][field]") {
  std::array<VS::Field::GimmickQueryState, 1> states{};
  states[0].present = true;
  for (std::uint32_t kind = 0; kind <= UINT8_MAX; ++kind) {
    states[0].object_id = static_cast<std::uint8_t>(kind);
    for (std::uint32_t subtype = 0; subtype <= UINT8_MAX; ++subtype) {
      states[0].subtype = static_cast<std::uint8_t>(subtype);
      const auto value = VS::Field::GimmickSubtype(states, 0);
      CHECK(value.has_value() == ((kind & 7) == 1));
      if (value) CHECK(*value == subtype);
    }
  }
}

TEST_CASE("Gimmick subtype refuses absent invalid and beyond-sixteen slots",
          "[gimmick-status-core][field]") {
  std::array<VS::Field::GimmickQueryState, 17> states{};
  states.back() = {.present = true, .object_id = 1, .subtype = 255};
  CHECK_FALSE(VS::Field::GimmickSubtype(states, -1));
  CHECK_FALSE(VS::Field::GimmickSubtype(states, 0));
  CHECK_FALSE(VS::Field::GimmickSubtype(states, 16));
  CHECK_FALSE(VS::Field::GimmickSubtype({}, 0));
}

TEST_CASE(
    "Gimmick movement status preserves every signed mode and motion nibble",
    "[gimmick-status-core][field]") {
  std::array<VS::Field::GimmickQueryState, 1> states{};
  states[0] = {.present = true, .falling = true, .parent = 0, .carrier = 0};
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    states[0].movement_mode =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
    CHECK(VS::Field::AreGimmicksMoving(states) == (bits != 0));
  }
  states[0].movement_mode = 1;
  for (std::uint32_t motion = 0; motion <= UINT8_MAX; ++motion) {
    states[0].motion_state = static_cast<std::uint8_t>(motion);
    CHECK(VS::Field::AreGimmicksMoving(states) == ((motion & 15) == 0));
  }
}

TEST_CASE("Gimmick movement status checks only active first-sixteen records",
          "[gimmick-status-core][field]") {
  std::array<VS::Field::GimmickQueryState, 17> states{};
  states.back() = {.present = true, .movement_mode = 1};
  CHECK_FALSE(VS::Field::AreGimmicksMoving(states));
  states[0] = {.present = true, .skipped = true, .movement_mode = 1};
  CHECK_FALSE(VS::Field::AreGimmicksMoving(states));
  states[0].skipped = false;
  CHECK(VS::Field::AreGimmicksMoving(states));
  states[0].present = false;
  CHECK_FALSE(VS::Field::AreGimmicksMoving(states));
  CHECK_FALSE(VS::Field::AreGimmicksMoving({}));
}

TEST_CASE("Native gimmick subtype returns every byte only for kind one",
          "[gimmick-status-live][field]") {
  GimmickResourcesLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.objectInfo.objectId = 1;
  for (std::uint32_t subtype = 0; subtype <= UINT8_MAX; ++subtype) {
    record.objectInfo.subtype = static_cast<std::uint8_t>(subtype);
    CHECK(Field_GimmickSubtypeEntry(1) == subtype);
  }
  record.objectInfo.objectId = 3;
  CHECK(Field_GimmickSubtypeEntry(1) == -1);
  CHECK(Field_GimmickSubtypeEntry(-1) == -1);
  CHECK(Field_GimmickSubtypeEntry(16) == -1);
  CHECK(Field_GimmickSubtypeEntry(0) == -1);
}

TEST_CASE(
    "Native gimmick movement status preserves skipped nibble and signed mode "
    "gates",
    "[gimmick-status-live][field]") {
  GimmickResourcesLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = -1;
  record.objectInfo.isFalling = 1;
  record.parent = 0;
  CHECK(Field_AreGimmicksMovingEntry() == 1);
  CHECK(VS::Field::AreLiveGimmicksMoving());
  record.motionState = 1;
  CHECK(Field_AreGimmicksMovingEntry() == 0);
  record.motionState = 0;
  record.isSkipped = -1;
  CHECK(Field_AreGimmicksMovingEntry() == 0);
  record.isSkipped = 0;
  record.mode = 0;
  CHECK(Field_AreGimmicksMovingEntry() == 0);
}

TEST_CASE(
    "Native gimmick status ignores unreadable prefixes before any field access",
    "[gimmick-status-live][field]") {
  GimmickResourcesLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  auto* record =
      static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(64));
  REQUIRE(record != nullptr);
  std::memset(record, 0xA5, 64);
  Field_gimmickInstances[1] = reinterpret_cast<Field_GimmickInstance*>(record);
  CHECK(Field_GimmickSubtypeEntry(1) == -1);
  CHECK(Field_AreGimmicksMovingEntry() == 0);
  CHECK(record[0] == std::byte{0xA5});
  CHECK(record[63] == std::byte{0xA5});
}

TEST_CASE("Gimmick status entries retain both original module addresses",
          "[gimmick-status-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8009F858) ==
        reinterpret_cast<const void*>(&Field_GimmickSubtypeEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A47C4) ==
        reinterpret_cast<const void*>(&Field_AreGimmicksMovingEntry));
}

TEST_CASE(
    "Menu stack measurement masks interaction and accumulates only stationary "
    "matches",
    "[gimmick-menu-query-core][field]") {
  std::array<VS::Field::GimmickQueryState, 2> states{};
  states[0] = {.present = true, .tile_word = 0x00FE00FF};
  states[1] = states[0];
  for (std::uint32_t interaction = 0; interaction <= UINT8_MAX; ++interaction) {
    states[0].interaction = static_cast<std::uint8_t>(interaction);
    const bool special = (interaction & 3) == 2;
    CHECK(VS::Field::MeasureTileStack(states, {255, 254}) ==
          VS::Field::GimmickStackMeasure{special ? -128 : -256, special});
  }
  states[0].movement_mode = -1;
  CHECK(VS::Field::MeasureTileStack(states, {255, 254}).height_offset == -128);
  CHECK(VS::Field::MeasureTileStack(states, {-1, -2}) ==
        VS::Field::GimmickStackMeasure{});
}

TEST_CASE(
    "Menu height band covers every signed halfword without a movement-mode "
    "filter",
    "[gimmick-menu-query-core][field]") {
  std::array<VS::Field::GimmickQueryState, 1> states{};
  states[0] = {
      .present = true, .falling = true, .movement_mode = -1, .parent = 0};
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    const auto height =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
    states[0].position_y = height;
    CHECK(VS::Field::IsGimmickWithinHeightBand(states, {}, 64) ==
          (height >= 64 && height <= 192));
  }
  CHECK_FALSE(VS::Field::IsGimmickWithinHeightBand(states, {}, INT32_MAX));
  CHECK_FALSE(VS::Field::IsGimmickWithinHeightBand(states, {}, INT32_MIN));
}

TEST_CASE(
    "Menu stack queries share active-tile gates but retain distinct height "
    "rules",
    "[gimmick-menu-query-core][field]") {
  std::array<VS::Field::GimmickQueryState, 1> states{};
  for (std::uint32_t flags = 0; flags < 8; ++flags) {
    states[0] = {.present = (flags & 1) == 0,
                 .skipped = (flags & 2) != 0,
                 .motion_state = static_cast<std::uint8_t>((flags & 4) != 0)};
    CHECK(VS::Field::MeasureTileStack(states, {}).height_offset ==
          (flags == 0 ? -128 : 0));
    CHECK(VS::Field::IsGimmickWithinHeightBand(states, {}, 0) == (flags == 0));
  }
}

TEST_CASE(
    "Menu stack query spans retain the sixteen-record limit and empty defaults",
    "[gimmick-menu-query-core][field]") {
  std::array<VS::Field::GimmickQueryState, 17> states{};
  states.fill({.present = true});
  CHECK(VS::Field::MeasureTileStack(states, {}).height_offset == -2048);
  states.fill({});
  states.back().present = true;
  CHECK(VS::Field::MeasureTileStack(states, {}) ==
        VS::Field::GimmickStackMeasure{});
  CHECK_FALSE(VS::Field::IsGimmickWithinHeightBand(states, {}, 0));
  CHECK(VS::Field::MeasureTileStack({}, {}) ==
        VS::Field::GimmickStackMeasure{});
  CHECK_FALSE(VS::Field::IsGimmickWithinHeightBand({}, {}, 0));
}

namespace {
void CheckNativeMenuStackAndBand(int& offset) {
  CHECK(Field_MeasureTileStackEntry(255, 254, &offset) == 1);
  CHECK(offset == -128);
  CHECK(Field_IsGimmickWithinHeightBandEntry(255, 254, 0) == 1);
  CHECK(Field_IsGimmickWithinHeightBandEntry(255, 254, 128) == 1);
  CHECK(Field_IsGimmickWithinHeightBandEntry(255, 254, 129) == 0);
}
}  // namespace

TEST_CASE(
    "Native menu stack checks keep unsigned coordinates and distinct mode "
    "filters",
    "[gimmick-menu-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  for (auto& record : fixture.instances) {
    record.tileX = std::bit_cast<char>(std::uint8_t{255});
    record.tileZ = std::bit_cast<char>(std::uint8_t{254});
    record.positionY = 128;
  }
  fixture.instances[0].objectInfo.interaction = 2;
  using Bytes = std::array<std::byte, sizeof(fixture.instances)>;
  const auto before = std::bit_cast<Bytes>(fixture.instances);
  int offset = 7;
  CheckNativeMenuStackAndBand(offset);
  CHECK(std::bit_cast<Bytes>(fixture.instances) == before);
  fixture.instances[0].mode = fixture.instances[1].mode = -1;
  CHECK(Field_MeasureTileStackEntry(255, 254, &offset) == 0);
  CHECK(offset == 0);
  CHECK(Field_IsGimmickWithinHeightBandEntry(255, 254, 0) == 1);
  CHECK(Field_IsGimmickWithinHeightBandEntry(-1, -2, 0) == 0);
}

TEST_CASE(
    "Native menu stack output supports unaligned destinations and null refusal",
    "[gimmick-menu-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  std::array<std::byte, 6> output{};
  output.fill(std::byte{0xA5});
  auto expected = output;
  const std::int32_t offset = -256;
  std::memcpy(expected.data() + 1, &offset, sizeof(offset));
  CHECK(Field_MeasureTileStackEntry(
            0, 0, reinterpret_cast<int*>(output.data() + 1)) == 0);
  CHECK(output == expected);
  CHECK(Field_MeasureTileStackEntry(0, 0, nullptr) == 0);
}

TEST_CASE(
    "Native menu stack checks refuse short prefixes and preserve empty "
    "defaults",
    "[gimmick-menu-query-live][field]") {
  GimmickResourcesLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  auto* record =
      static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(64));
  REQUIRE(record != nullptr);
  std::memset(record, 0xA5, 64);
  Field_gimmickInstances[1] = reinterpret_cast<Field_GimmickInstance*>(record);
  Field_gimmickInstances[5] = nullptr;
  int offset = 7;
  CHECK(Field_MeasureTileStackEntry(0, 0, &offset) == 0);
  CHECK(offset == 0);
  CHECK(Field_IsGimmickWithinHeightBandEntry(0, 0, 0) == 0);
  CHECK(record[0] == std::byte{0xA5});
  CHECK(record[63] == std::byte{0xA5});
}

TEST_CASE("Menu gimmick queries retain both original module addresses",
          "[gimmick-menu-query-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MAINMENU.PRG",
                                          0x800FA188) ==
        reinterpret_cast<const void*>(&Field_MeasureTileStackEntry));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MAINMENU.PRG",
                                          0x800FA238) ==
        reinterpret_cast<const void*>(&Field_IsGimmickWithinHeightBandEntry));
}

TEST_CASE(
    "Gimmick orientation reset clears both halfwords in the original Z word",
    "[gimmick-orientation-word-reset][field]") {
  GimmickResourcesLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.objectInfo.objectId = 1;
  record.rotationTicks = 9;
  record.rotationX = 13;
  record.rotationY = 17;
  record.rotationZ = 23;
  record.reserved2A = 19;
  REQUIRE(Field_SetGimmickOrientationEntry(1, 0) == 0);
  CHECK(record.rotationTicks == 0);
  CHECK(record.rotationX == 0);
  CHECK(record.rotationY == 0);
  CHECK(record.rotationZ == 0);
  CHECK(record.reserved2A == 0);
}

namespace {
struct RecordingGimmickOrientationHost : VS::Field::GimmickOrientationHost {
  std::optional<std::uint8_t> kind = 1;
  std::uint8_t material = 0;
  std::optional<VS::Core::Math::Mat3> rotation;
  std::vector<int> calls;
  bool rotate = true;
  std::optional<std::uint8_t> ObjectKind() const override { return kind; }
  bool StoreMaterial(std::uint8_t value) override {
    material = value;
    calls.push_back(1);
    return true;
  }
  bool StoreRotation(const VS::Core::Math::Mat3& value) override {
    calls.push_back(2);
    rotation = value;
    return rotate;
  }
  bool ResetRotation() override {
    calls.push_back(3);
    return true;
  }
};

struct GimmickOrientationLiveFixture : GimmickResourcesLiveFixture {
  static constexpr std::size_t kRotationBytes =
      VS::Field::kGimmickOrientationCount *
      sizeof(VS::Field::GimmickOrientationRow);
  std::array<std::uint8_t, kRotationBytes> saved{};
  GimmickOrientationLiveFixture() {
    std::copy_n(Field_kGimmickOrientationRotations, saved.size(),
                saved.begin());
    std::fill_n(Field_kGimmickOrientationRotations, saved.size(), 0);
    instances[0].objectInfo.objectId = 1;
  }
  ~GimmickOrientationLiveFixture() {
    std::copy(saved.begin(), saved.end(), Field_kGimmickOrientationRotations);
  }
};
}  // namespace

TEST_CASE(
    "Gimmick orientation keeps masked kind gate and material matrix reset "
    "order",
    "[gimmick-orientation-core][field]") {
  std::array<VS::Field::GimmickOrientationRow, 1> rows{};
  for (std::uint32_t kind = 0; kind <= UINT8_MAX; ++kind) {
    RecordingGimmickOrientationHost host;
    host.kind = static_cast<std::uint8_t>(kind);
    const bool allowed = (kind & 7) < 2;
    CHECK(VS::Field::SetGimmickOrientation(0, rows, host) == allowed);
    CHECK(host.calls ==
          (allowed ? std::vector<int>{1, 2, 3} : std::vector<int>{}));
  }
}

TEST_CASE(
    "Gimmick orientation stages every unsigned quarter-turn byte as a halfword",
    "[gimmick-orientation-core][field]") {
  std::array<VS::Field::GimmickOrientationRow, 1> rows{};
  for (std::uint32_t bits = 0; bits <= UINT8_MAX; ++bits) {
    const auto byte = static_cast<std::uint8_t>(bits);
    rows[0] = {byte, byte, byte, 255};
    RecordingGimmickOrientationHost host;
    REQUIRE(VS::Field::SetGimmickOrientation(0, rows, host));
    const auto angle =
        VS::Core::Math::WrapSigned16(static_cast<std::int32_t>(bits) * 1024);
    CHECK(host.rotation ==
          VS::Core::Math::BuildEulerRotationMatrix({angle, angle, angle}));
  }
}

TEST_CASE(
    "Gimmick orientation refuses bounded lookup after low-byte material store",
    "[gimmick-orientation-core][field]") {
  std::array<VS::Field::GimmickOrientationRow, 24> rows{};
  for (const auto value : {-1, 24, 256, INT32_MIN, INT32_MAX}) {
    RecordingGimmickOrientationHost host;
    CHECK_FALSE(VS::Field::SetGimmickOrientation(value, rows, host));
    CHECK(host.material == static_cast<std::uint8_t>(value));
    CHECK(host.calls == std::vector<int>{1});
  }
  RecordingGimmickOrientationHost host;
  CHECK_FALSE(VS::Field::SetGimmickOrientation(0, {}, host));
  CHECK(host.calls == std::vector<int>{1});
  host.kind.reset();
  host.calls.clear();
  CHECK_FALSE(VS::Field::SetGimmickOrientation(0, rows, host));
  CHECK(host.calls.empty());
}

TEST_CASE("Gimmick orientation stops before reset on refused matrix store",
          "[gimmick-orientation-core][field]") {
  RecordingGimmickOrientationHost host;
  host.rotate = false;
  std::array<VS::Field::GimmickOrientationRow, 1> rows{};
  CHECK_FALSE(VS::Field::SetGimmickOrientation(0, rows, host));
  CHECK(host.calls == std::vector<int>{1, 2});
}

TEST_CASE(
    "Native gimmick orientation rebuilds selected matrix without translation "
    "or padding writes",
    "[gimmick-orientation-live][field]") {
  GimmickOrientationLiveFixture fixture;
  auto& record = fixture.instances[0];
  Field_kGimmickOrientationRotations[4] = 1;
  record.matrices[4].t[0] = 0x12345678;
  constexpr auto kPaddingStart = sizeof(MATRIX{}.m);
  static_assert(offsetof(MATRIX, t) >= kPaddingStart + sizeof(std::uint16_t));
  auto matrix_bytes = std::as_writable_bytes(std::span(&record.matrices[4], 1));
  matrix_bytes[kPaddingStart] = std::byte{0x34};
  matrix_bytes[kPaddingStart + 1] = std::byte{0x12};
  const auto expected = VS::Core::Math::BuildEulerRotationMatrix({1024, 0, 0});
  REQUIRE(Field_SetGimmickOrientationEntry(1, 1) == 0);
  CHECK(VS::Core::Math::MatrixOf(record.matrices[4]) == expected);
  CHECK(record.matrices[4].t[0] == 0x12345678);
  CHECK(matrix_bytes[kPaddingStart] == std::byte{0x34});
  CHECK(matrix_bytes[kPaddingStart + 1] == std::byte{0x12});
  CHECK(record.objectInfo.material == 1);
}

TEST_CASE(
    "Native gimmick orientation logical table ends before neighboring actor "
    "data",
    "[gimmick-orientation-live][field]") {
  GimmickOrientationLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.rotationTicks = 9;
  const auto before = VS::Core::Math::MatrixOf(record.matrices[4]);
  CHECK(Field_SetGimmickOrientationEntry(1, 24) == -1);
  CHECK(record.objectInfo.material == 24);
  CHECK(record.rotationTicks == 9);
  CHECK(VS::Core::Math::MatrixOf(record.matrices[4]) == before);
  CHECK(Field_SetGimmickOrientationEntry(-1, 0) == -1);
  CHECK(Field_SetGimmickOrientationEntry(16, 0) == -1);
  record.objectInfo.objectId = 3;
  CHECK(Field_SetGimmickOrientationEntry(1, 0) == -1);
  CHECK(record.objectInfo.material == 24);
}

TEST_CASE("Gimmick orientation retains its original module address",
          "[gimmick-orientation-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A0104) ==
        reinterpret_cast<const void*>(&Field_SetGimmickOrientationEntry));
}

namespace {
struct RecordingGimmickPointHost : VS::Field::GimmickPointHost {
  std::int32_t height = 0;
  std::uint32_t probe = 0;
  bool occupied = false;
  mutable std::vector<int> calls;
  VS::Field::GimmickPlacementPoint point;
  VS::Field::GimmickOccupancyQuery query;
  std::int32_t SupportHeight(
      const VS::Field::GimmickPlacementPoint& value) override {
    calls.push_back(1);
    point = value;
    return height;
  }
  std::uint32_t LastProbeWord() const override {
    calls.push_back(2);
    return probe;
  }
  bool ActorOccupies(const VS::Field::GimmickOccupancyQuery& value) override {
    calls.push_back(3);
    query = value;
    return occupied;
  }
};
}  // namespace

namespace {
struct RecordingGimmickInteractionHost
    : VS::Field::GimmickInteractionQueryHost {
  std::optional<VS::Actors::MovementActor> actor{std::in_place};
  std::optional<VS::Field::GimmickQueryState> object{
      VS::Field::GimmickQueryState{.present = true, .position_y = 100}};
  std::optional<std::size_t> top = 3;
  std::vector<int> calls;
  std::vector<VS::Field::GimmickWorldPoint> ceilings;
  VS::Field::GimmickTileQuery tile;
  VS::Field::GimmickPlacementPoint point;
  std::uint8_t direction = 255;
  std::int32_t ceiling = -3000;
  bool allowed = true;
  bool occupied = false;
  bool writable = true;
  VS::Actors::MovementActor& ActorState() {
    if (!actor) throw std::bad_optional_access();
    return *actor;
  }
  VS::Field::GimmickQueryState& ObjectState() {
    if (!object) throw std::bad_optional_access();
    return *object;
  }
  RecordingGimmickInteractionHost() {
    ActorState().attach_bone = 3;
    ActorState().motion.current_tile = {.x = 3, .z = 4};
    ActorState().motion.position = {.x = 384, .y = 100, .z = 512};
  }
  std::optional<VS::Actors::MovementActor> Actor(std::int32_t) override {
    calls.push_back(0);
    return actor;
  }
  bool StoreDirection(std::uint8_t value) override {
    calls.push_back(1);
    direction = value;
    return writable;
  }
  std::optional<VS::Field::GimmickQueryState> Gimmick(std::size_t) override {
    calls.push_back(2);
    return object;
  }
  std::optional<std::size_t> TopAt(
      const VS::Field::GimmickTileQuery& value) override {
    calls.push_back(3);
    tile = value;
    return top;
  }
  bool PointAllowed(const VS::Field::GimmickPlacementPoint& value) override {
    calls.push_back(4);
    point = value;
    return allowed;
  }
  std::int32_t CeilingHeight(
      const VS::Field::GimmickWorldPoint& value) override {
    calls.push_back(5);
    ceilings.push_back(value);
    return ceiling;
  }
  bool ActorAbove(const VS::Field::GimmickQueryState&) override {
    calls.push_back(6);
    return occupied;
  }
};

void CheckGroundInteractionCoordinates(
    const RecordingGimmickInteractionHost& host) {
  CHECK(host.tile.tile_x == 3);
  CHECK(host.tile.tile_z == 3);
  CHECK(host.point.x == 448);
  CHECK(host.point.z == 320);
  CHECK(host.point.minimum_height == 100);
}

void CheckHeldInteractionCoordinates(
    const RecordingGimmickInteractionHost& host) {
  CHECK(host.point.x == 448);
  CHECK(host.point.z == 448);
  CHECK(host.point.minimum_height == -28);
  REQUIRE(host.ceilings.size() == 2);
  CHECK(host.ceilings[0].x == 384);
  CHECK(host.ceilings[0].z == 512);
  CHECK(host.ceilings[1].x == 448);
  CHECK(host.ceilings[1].z == 448);
}

struct RecordingGimmickStartHost : VS::Field::GimmickInteractionStartHost {
  std::optional<VS::Actors::MovementActor> actor{std::in_place};
  std::optional<VS::Field::GimmickInteractionObject> object{
      VS::Field::GimmickInteractionObject{
          .query = {.present = true, .position_y = 100, .object_id = 2}}};
  std::optional<std::size_t> selected = 3;
  std::optional<std::int32_t> direction = 0;
  std::array<std::uint8_t, 96> rows{};
  std::vector<int> calls;
  std::uint8_t material = 0;
  std::uint16_t orientation = 0;
  std::uint16_t sound_first = 0;
  std::uint16_t sound_second = 0;
  std::int32_t animation = 0;
  std::int32_t refusal = -1;
  bool mutate_after_animation = false;
  bool mutate_after_turn = false;
  VS::Field::GimmickTileQuery reservation;
  VS::Actors::PackedTile turn;
  VS::Actors::MotionVector translation;
  VS::Actors::MovementActor& ActorState() {
    if (!actor) throw std::bad_optional_access();
    return *actor;
  }
  VS::Field::GimmickInteractionObject& ObjectState() {
    if (!object) throw std::bad_optional_access();
    return *object;
  }
  RecordingGimmickStartHost() {
    ActorState().motion.current_tile = {.x = 3, .layer = 7, .z = 4};
    ActorState().motion.position.y = 100;
    ActorState().tile_spare = 255;
    ObjectState().position_x = 448;
    ObjectState().position_z = 576;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      rows[index] = static_cast<std::uint8_t>((index + 1) % 24);
    }
  }
  bool Record(int event) {
    calls.push_back(event);
    return refusal != event;
  }
  std::optional<VS::Actors::MovementActor> Actor(std::int32_t) override {
    return actor;
  }
  std::optional<std::size_t> SelectAhead(std::int32_t) override {
    Record(0);
    return selected;
  }
  std::optional<std::int32_t> Direction() override { return direction; }
  std::optional<VS::Field::GimmickInteractionObject> Object(
      std::size_t) override {
    return object;
  }
  bool StoreOrientation(std::size_t, std::uint16_t value) override {
    orientation = value;
    return Record(1);
  }
  bool PublishSelected(std::size_t) override { return Record(2); }
  bool ClearSlide(std::size_t) override {
    ObjectState().slide_x = 0;
    ObjectState().slide_z = 0;
    return Record(3);
  }
  std::optional<std::uint8_t> Material(std::size_t) override {
    return material;
  }
  std::span<const std::uint8_t> RollOrientations() override { return rows; }
  bool StoreMaterial(std::size_t, std::uint8_t value) override {
    material = value;
    return Record(4);
  }
  bool StartSlide(std::size_t, std::int16_t x, std::int16_t z) override {
    ObjectState().slide_x = x;
    ObjectState().slide_z = z;
    return Record(5);
  }
  bool PlaySounds(std::int32_t, std::uint16_t first,
                  std::uint16_t second) override {
    sound_first = first;
    sound_second = second;
    return Record(6);
  }
  bool PlayAnimation(std::int32_t, std::int32_t value) override {
    animation = value;
    if (mutate_after_animation) {
      ObjectState().position_x = -100;
      ObjectState().slide_x = -128;
      ActorState().motion.current_tile = {.x = 255, .layer = 7, .z = 254};
      direction = 3;
    }
    return Record(7);
  }
  bool SetWeaponPose(std::int32_t) override {
    ActorState().motion.weapon_pose_active = true;
    return Record(8);
  }
  bool ReserveTile(const VS::Field::GimmickTileQuery& tile) override {
    reservation = tile;
    return Record(9);
  }
  bool TurnToward(std::int32_t, const VS::Actors::PackedTile& tile) override {
    turn = tile;
    if (mutate_after_turn) {
      ActorState().motion.current_tile = {.x = 255, .layer = 6, .z = 254};
      ActorState().motion.position.y = -123;
    }
    return Record(10);
  }
  bool TranslateToward(std::int32_t,
                       const VS::Actors::MotionVector& point) override {
    translation = point;
    return Record(11);
  }
};

void CheckInitiationPushState(RecordingGimmickStartHost& host) {
  CHECK(host.ObjectState().slide_x == 0);
  CHECK(host.ObjectState().slide_z == -128);
  CHECK(host.animation == 0x50);
  CHECK(host.sound_first == 0x2C);
  CHECK(host.sound_second == 0x2D);
}

struct RecordingGimmickCommandHost : VS::Field::GimmickInteractionCommandHost {
  std::optional<VS::Actors::MovementActor> actor{std::in_place};
  std::optional<VS::Field::GimmickCommandObject> object{std::in_place};
  std::optional<std::int32_t> direction = 0;
  std::array<std::optional<std::int16_t>, 2> bones{-1, 0};
  std::vector<int> calls;
  VS::Field::GimmickTileQuery release;
  VS::Field::GimmickTileQuery drop;
  std::int16_t mode = 99;
  std::int16_t height = 0;
  std::uint8_t parent = 255;
  bool support_cleared = false;
  bool carrying = false;
  bool mutate_removal = false;
  bool mutate_bone = false;
  int refusal = -1;
  VS::Actors::MovementActor& ActorState() {
    if (!actor) throw std::bad_optional_access();
    return *actor;
  }
  VS::Field::GimmickCommandObject& ObjectState() {
    if (!object) throw std::bad_optional_access();
    return *object;
  }
  RecordingGimmickCommandHost() {
    ActorState().motion.slot = 3;
    ActorState().motion.current_tile = {.x = 255, .layer = 7, .z = 0};
    ObjectState().state.query.present = true;
    ObjectState().state.query.tile_word = 0xFFFEFF;
  }
  bool Record(int event) {
    calls.push_back(event);
    return event != refusal;
  }
  std::optional<VS::Actors::MovementActor> Actor() override { return actor; }
  std::optional<VS::Field::GimmickCommandObject> Object() override {
    return object;
  }
  std::optional<std::int32_t> Direction() override { return direction; }
  bool PreparePush(std::optional<std::int16_t> value) override {
    if (value) mode = *value;
    support_cleared = true;
    return Record(1);
  }
  bool RemoveOccupant() override {
    if (mutate_removal) {
      ObjectState().state.query.interaction = 2;
      ObjectState().state.slide_z = INT16_MAX;
      direction = 0;
    }
    return Record(2);
  }
  bool ReleaseTile(const VS::Field::GimmickTileQuery& tile) override {
    release = tile;
    return Record(3);
  }
  bool StoreDropPlacement(const VS::Field::GimmickTileQuery& tile,
                          std::uint16_t yaw) override {
    drop = tile;
    ObjectState().yaw = yaw;
    return Record(4);
  }
  std::optional<std::int16_t> ParentBoneHeight(std::uint8_t tag) override {
    const auto index = static_cast<std::size_t>(tag - 0xF0);
    Record(5 + static_cast<int>(index));
    if (mutate_bone && index == 0) bones[1] = -3;
    return bones[index];
  }
  bool FinishDrop(std::int16_t value) override {
    height = value;
    return Record(7);
  }
  bool SetCarrying(bool value) override {
    carrying = value;
    return Record(8);
  }
  bool StoreSlide(std::int16_t x, std::int16_t z) override {
    ObjectState().state.slide_x = x;
    ObjectState().state.slide_z = z;
    return Record(9);
  }
  bool StoreYaw(std::uint16_t value) override {
    ObjectState().yaw = value;
    return Record(10);
  }
  bool AttachObject(std::uint8_t slot) override {
    parent = slot;
    return Record(11);
  }
  bool PlayIdleAnimation() override { return Record(12); }
};

struct RecordingGimmickReactionHost : VS::Field::GimmickReactionHost {
  std::optional<VS::Field::GimmickReactionState> state{std::in_place};
  std::vector<int> calls;
  std::uint8_t reset_slot = 0;
  std::uint8_t reset_material = 0;
  std::uint8_t released_slot = 0;
  bool mutate_reset = false;
  VS::Field::GimmickReactionState& StateValue() {
    if (!state) throw std::bad_optional_access();
    return *state;
  }
  std::optional<VS::Field::GimmickReactionState> State() override {
    return state;
  }
  bool Store(const VS::Field::GimmickReactionState& value) override {
    state = value;
    calls.push_back(1);
    return true;
  }
  bool ResetOrientation(std::uint8_t slot, std::uint8_t material) override {
    reset_slot = slot;
    reset_material = material;
    if (mutate_reset) StateValue().rotation = {123, 456, 789};
    calls.push_back(2);
    return true;
  }
  bool BeginBreaking() override {
    calls.push_back(3);
    return true;
  }
  bool Release(std::uint8_t slot) override {
    released_slot = slot;
    calls.push_back(4);
    return true;
  }
};

struct GimmickReactionTestTables {
  std::array<VS::Field::GimmickSpinFrame, 16> spin{};
  std::array<VS::Field::GimmickTiltFrame, 12> tilt{};
  std::array<std::int8_t, 4> bounce{};
  std::array<VS::Field::GimmickRollFrame, 16> roll{};
  std::array<VS::Field::GimmickSquashFrame, 16> squash{};
  VS::Field::GimmickReactionTables View() const {
    return {spin, tilt, bounce, roll, squash};
  }
};

struct RecordingRoomTriggerActivationHost
    : VS::Field::RoomTriggerActivationHost {
  VS::Field::InteractionTargetDomain domain{7, 3, 0};
  std::array<std::uint8_t, 3> values{17, 18, 19};
  std::vector<std::uint32_t> calls;
  bool invalidate_after_first = false;
  VS::Field::InteractionTargetDomain Domain() override { return domain; }
  bool StoreActivation(VS::Field::TrapTarget target,
                       std::uint8_t activation) override {
    const auto index = VS::Field::ResolveTrapTarget(target, domain);
    if (!index) return false;
    values[*index] = activation;
    calls.push_back(target.index);
    if (invalidate_after_first) ++domain.generation;
    return true;
  }
};

struct RecordingTrapDisableHost : VS::Field::TrapDisableHost {
  VS::Field::InteractionTargetDomain domain{7, 1, 0};
  std::optional<VS::Field::TrapTarget> target{VS::Field::TrapTarget{0, 7}};
  VS::Field::RoomTriggerTile query;
  VS::Field::RoomTriggerTile tile{3, 4, 0};
  VS::Field::RoomTriggerTile marked;
  std::vector<int> calls;
  std::uint8_t activation = 99;
  bool mutate_tile = false;
  bool refuse_store = false;
  VS::Field::InteractionTargetDomain Domain() override { return domain; }
  bool StoreActivation(VS::Field::TrapTarget, std::uint8_t value) override {
    calls.push_back(1);
    activation = value;
    if (mutate_tile) tile.x = 9;
    return !refuse_store;
  }
  std::optional<VS::Field::TrapTarget> Find(
      const VS::Field::RoomTriggerTile& value) override {
    calls.push_back(0);
    query = value;
    return target;
  }
  std::optional<VS::Field::RoomTriggerTile> Tile(
      VS::Field::TrapTarget) override {
    calls.push_back(2);
    return tile;
  }
  bool MarkObstacle(const VS::Field::RoomTriggerTile& value) override {
    calls.push_back(3);
    marked = value;
    return true;
  }
};

struct RecordingGimmickDynamicsHost : VS::Field::GimmickDynamicsHost {
  std::optional<VS::Field::GimmickDynamicsState> state{std::in_place};
  std::array<std::optional<VS::Field::GimmickDynamicsState>, 16> children{};
  std::vector<int> calls;
  std::optional<std::int32_t> support = 0;
  std::optional<std::uint8_t> top_kind;
  VS::Field::GimmickReactionResult reaction =
      VS::Field::GimmickReactionResult::kAdvanced;
  VS::Field::GimmickPlacementPoint point;
  VS::Field::GimmickTileQuery released;
  bool point_allowed = true;
  bool mutate_support = false;
  bool clear_mode_on_reaction = true;
  bool mutate_tile_on_second_add = false;
  int additions = 0;
  VS::Field::GimmickDynamicsState& StateValue() {
    if (!state) throw std::bad_optional_access();
    return *state;
  }
  std::optional<VS::Field::GimmickDynamicsState> State() override {
    return state;
  }
  bool Store(const VS::Field::GimmickDynamicsState& value) override {
    state = value;
    calls.push_back(1);
    return true;
  }
  VS::Field::GimmickReactionResult React() override {
    calls.push_back(9);
    if (reaction == VS::Field::GimmickReactionResult::kComplete &&
        clear_mode_on_reaction) {
      StateValue().reaction.mode = 0;
    }
    return reaction;
  }
  bool PointAllowed(const VS::Field::GimmickPlacementPoint& value) override {
    point = value;
    calls.push_back(2);
    return point_allowed;
  }
  std::optional<std::int32_t> SupportHeight(
      const std::array<std::int16_t, VS::Field::kGimmickReactionAxes>&,
      std::int32_t mode) override {
    calls.push_back(mode == 1 ? 3 : 4);
    if (mutate_support) StateValue().reaction.position[1] = 5;
    return support;
  }
  std::optional<std::uint8_t> TopKind(
      const VS::Field::GimmickTileQuery&) override {
    calls.push_back(5);
    return top_kind;
  }
  bool AddOccupant() override {
    calls.push_back(6);
    ++additions;
    if (mutate_tile_on_second_add && additions == 2) StateValue().tile_x = 8;
    return true;
  }
  bool ReleaseTile(const VS::Field::GimmickTileQuery& tile) override {
    calls.push_back(7);
    released = tile;
    return true;
  }
  bool PlaySound(std::uint16_t effect,
                 const std::array<std::int16_t,
                                  VS::Field::kGimmickReactionAxes>&) override {
    calls.push_back(effect == 0x30 ? 8 : 18);
    return true;
  }
  std::optional<VS::Field::GimmickDynamicsState> Child(
      std::size_t slot) override {
    return children[slot];
  }
  bool StoreChild(std::size_t slot, std::int16_t height,
                  std::uint8_t carrier) override {
    if (!children[slot]) return false;
    ChildState(slot).reaction.position[1] = height;
    ChildState(slot).carrier = carrier;
    calls.push_back(100 + static_cast<int>(slot));
    return true;
  }
  VS::Field::GimmickDynamicsState& ChildState(std::size_t slot) {
    auto& child = children.at(slot);
    if (!child) throw std::bad_optional_access();
    return *child;
  }
};
}  // namespace

TEST_CASE("Room trigger lookup suppression stops at first matching signed kind",
          "[room-trigger-query-core][field]") {
  std::array<VS::Field::RoomTriggerInfo, 2> records{
      {{{0, 7}, -1, 3, 4, 2}, {{1, 7}, 16, 3, 4, 2}}};
  CHECK(VS::Field::FindRoomTriggerAtTile(records, {3, 4, 2}, false) ==
        VS::Field::TrapTarget{0, 7});
  CHECK_FALSE(VS::Field::FindRoomTriggerAtTile(records, {3, 4, 2}, true));
  records[0].kind = 16;
  CHECK(VS::Field::FindRoomTriggerAtTile(records, {3, 4, 2}, true) ==
        VS::Field::TrapTarget{0, 7});
  records[0].kind = 15;
  CHECK_FALSE(VS::Field::FindRoomTriggerAtTile(records, {3, 4, 2}, true));
}

TEST_CASE(
    "Room trigger lookup compares signed halfwords against full query words",
    "[room-trigger-query-core][field]") {
  const std::array<VS::Field::RoomTriggerInfo, 1> records{
      {{{0, 7}, INT16_MIN, INT16_MIN, INT16_MAX, -1}}};
  CHECK(VS::Field::FindRoomTriggerAtTile(records, {INT16_MIN, INT16_MAX, -1},
                                         false) == VS::Field::TrapTarget{0, 7});
  CHECK_FALSE(
      VS::Field::FindRoomTriggerAtTile(records, {32768, INT16_MAX, -1}, false));
  CHECK_FALSE(
      VS::Field::FindRoomTriggerAtTile(records, {INT16_MIN, 65535, -1}, false));
  CHECK_FALSE(VS::Field::FindRoomTriggerAtTile(
      records, {INT16_MIN, INT16_MAX, 65535}, false));
  CHECK_FALSE(VS::Field::FindRoomTriggerAtTile(
      records, {INT16_MIN, INT16_MAX, -1}, true));
}

TEST_CASE(
    "Room trigger activation stores only low argument byte on selected target",
    "[room-trigger-activation-core][field]") {
  for (const auto value : {-1, 0, 1, 255, 256, 257, INT32_MIN, INT32_MAX}) {
    RecordingRoomTriggerActivationHost host;
    REQUIRE(VS::Field::SetRoomTriggerEnabled(VS::Field::TrapTarget{1, 7}, value,
                                             host));
    CHECK(host.values == std::array<std::uint8_t, 3>{
                             17, static_cast<std::uint8_t>(value), 19});
    CHECK(host.calls == std::vector<std::uint32_t>{1});
  }
}

TEST_CASE("Room trigger null selection activates all records in order",
          "[room-trigger-activation-core][field]") {
  RecordingRoomTriggerActivationHost host;
  REQUIRE(VS::Field::SetRoomTriggerEnabled(std::nullopt, -1, host));
  CHECK(host.values == std::array<std::uint8_t, 3>{255, 255, 255});
  CHECK(host.calls == std::vector<std::uint32_t>{0, 1, 2});
}

TEST_CASE(
    "Room trigger activation refuses stale targets and stops on generation "
    "change",
    "[room-trigger-activation-core][field]") {
  RecordingRoomTriggerActivationHost host;
  CHECK_FALSE(
      VS::Field::SetRoomTriggerEnabled(VS::Field::TrapTarget{0, 6}, 1, host));
  CHECK_FALSE(
      VS::Field::SetRoomTriggerEnabled(VS::Field::TrapTarget{3, 7}, 1, host));
  CHECK(host.calls.empty());
  host.invalidate_after_first = true;
  CHECK_FALSE(VS::Field::SetRoomTriggerEnabled(std::nullopt, 1, host));
  CHECK(host.calls == std::vector<std::uint32_t>{0});
  CHECK(host.values == std::array<std::uint8_t, 3>{1, 18, 19});
}

TEST_CASE("Trap disabling stores activation before rereading obstacle tile",
          "[trap-disable-core][field]") {
  RecordingTrapDisableHost host;
  host.mutate_tile = true;
  REQUIRE(VS::Field::DisableTrapAtPosition({-127, 511}, host));
  CHECK(host.query.x == 0);
  CHECK(host.query.z == 3);
  CHECK(host.query.layer == 0);
  CHECK(host.activation == 1);
  CHECK(host.marked.x == 9);
  CHECK(host.calls == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE(
    "Trap disabling covers every signed halfword with truncating tile division",
    "[trap-disable-core][field]") {
  RecordingTrapDisableHost host;
  host.target.reset();
  for (std::int32_t value = INT16_MIN; value <= INT16_MAX; ++value) {
    REQUIRE(VS::Field::DisableTrapAtPosition(
        {static_cast<std::int16_t>(value), static_cast<std::int16_t>(value)},
        host));
    CHECK(host.query.x == value / 128);
    CHECK(host.query.z == value / 128);
    host.calls.clear();
  }
}

TEST_CASE("Trap disabling missing or refused target never marks occupancy",
          "[trap-disable-core][field]") {
  RecordingTrapDisableHost host;
  host.target.reset();
  CHECK(VS::Field::DisableTrapAtPosition({}, host));
  CHECK(host.calls == std::vector<int>{0});
  host.target = VS::Field::TrapTarget{0, 7};
  host.refuse_store = true;
  host.calls.clear();
  CHECK_FALSE(VS::Field::DisableTrapAtPosition({}, host));
  CHECK(host.calls == std::vector<int>{0, 1});
}

TEST_CASE("Gimmick dynamics delay consumes every nonzero byte before rotation",
          "[gimmick-dynamics-core][field]") {
  for (const auto delay : {1, 127, 128, 255}) {
    RecordingGimmickDynamicsHost host;
    host.StateValue().delay = static_cast<std::uint8_t>(delay);
    host.StateValue().rotation_ticks = 7;
    REQUIRE(VS::Field::UpdateGimmickMotion(host));
    CHECK(host.StateValue().delay == delay - 1);
    CHECK(host.StateValue().rotation_ticks == 7);
    CHECK(host.calls == std::vector<int>{1});
  }
}

TEST_CASE(
    "Gimmick dynamics rotation preserves signed remainder and angle masks",
    "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().rotation_ticks = 2;
  host.StateValue().rotation_remaining = {-5, 7, INT16_MIN};
  host.StateValue().reaction.rotation = {0, 4095, 0};
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.StateValue().rotation_ticks == 1);
  CHECK(host.StateValue().rotation_remaining ==
        std::array<std::int16_t, 3>{-3, 4, -16384});
  CHECK(host.StateValue().reaction.rotation ==
        std::array<std::int16_t, 3>{4094, 2, 0});
}

TEST_CASE("Gimmick dynamics half slide covers every nonzero signed halfword",
          "[gimmick-dynamics-core][field]") {
  for (std::int32_t value = INT16_MIN; value <= INT16_MAX; ++value) {
    if (value == 0) continue;
    RecordingGimmickDynamicsHost host;
    host.StateValue().reaction.mode = 255;
    host.StateValue().slide[0] = static_cast<std::int16_t>(value);
    REQUIRE(VS::Field::UpdateGimmickMotion(host));
    const auto step = value / 2;
    const auto remaining = value - step;
    const auto snapped = remaining >= -1 && remaining < 2;
    CHECK(host.StateValue().slide[0] == (snapped ? 0 : remaining));
    CHECK(host.StateValue().reaction.position[0] == (snapped ? value : step));
    CHECK(host.StateValue().reaction.frame == 1);
  }
}

TEST_CASE(
    "Gimmick dynamics countdown reaches parent-held endpoint without falling",
    "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 1;
  host.StateValue().parent = 0;
  host.StateValue().slide = {-32768, -3};
  host.StateValue().reaction.vertical_delta = 7;
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.StateValue().reaction.mode == 0);
  CHECK(host.StateValue().reaction.position ==
        std::array<std::int16_t, 3>{-32768, 7, -3});
  CHECK(host.calls == std::vector<int>{1});
}

TEST_CASE("Gimmick dynamics falling publishes gravity before carrying children",
          "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 252;
  host.StateValue().reaction.slot = 3;
  host.support = 100;
  host.children[7] = VS::Field::GimmickDynamicsState{};
  host.ChildState(7).carrier = 3;
  host.ChildState(7).reaction.position[1] = 10;
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.StateValue().reaction.position[1] == 3);
  CHECK(host.StateValue().reaction.vertical_delta == 3);
  CHECK(host.calls == std::vector<int>{1, 4, 1, 107});
  REQUIRE(host.children[7]);
  CHECK(host.ChildState(7).reaction.position[1] == 13);
}

TEST_CASE("Gimmick dynamics final durability landing skips child propagation",
          "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 252;
  host.StateValue().reaction.slot = 3;
  host.StateValue().kind = 1;
  host.StateValue().durability = 1;
  host.children[7] = VS::Field::GimmickDynamicsState{};
  host.ChildState(7).carrier = 3;
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.StateValue().reaction.mode == 248);
  CHECK(host.StateValue().durability == 0);
  CHECK(host.calls == std::vector<int>{1, 4, 1, 1, 6, 7, 1});
}

TEST_CASE("Gimmick dynamics landing dispatches same and different stack kinds",
          "[gimmick-dynamics-core][field]") {
  for (const auto same : {true, false}) {
    RecordingGimmickDynamicsHost host;
    host.StateValue().reaction.mode = 252;
    host.StateValue().kind = 5;
    host.top_kind = same ? 5 : 6;
    REQUIRE(VS::Field::UpdateGimmickMotion(host));
    CHECK(host.StateValue().reaction.mode == (same ? 253 : 0));
    CHECK(host.StateValue().interaction == (same ? 2 : 1));
    CHECK(host.additions == (same ? 0 : 1));
  }
}

TEST_CASE("Gimmick dynamics spin completion releases tile after two additions",
          "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 253;
  host.reaction = VS::Field::GimmickReactionResult::kComplete;
  host.mutate_tile_on_second_add = true;
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.calls == std::vector<int>{9, 6, 6, 7});
  CHECK(host.released.tile_x == 8);
}

TEST_CASE("Gimmick dynamics support callback updates are reread before landing",
          "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 252;
  host.support = 4;
  host.mutate_support = true;
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.StateValue().reaction.position[1] == 4);
  CHECK(host.StateValue().reaction.vertical_delta == -1);
  CHECK(host.StateValue().reaction.mode == 250);
}

TEST_CASE(
    "Gimmick dynamics slow slide point wraps halfword before terrain call",
    "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 254;
  host.StateValue().reaction.orientation = 3;
  host.StateValue().reaction.position = {32704, 0, 64};
  host.StateValue().slide = {32, 0};
  REQUIRE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.point.x == -32704);
  CHECK(host.StateValue().tile_x == 255);
  CHECK(host.StateValue().reaction.position[0] == 32736);
}

TEST_CASE(
    "Gimmick dynamics reaction refusal never completes occupancy transitions",
    "[gimmick-dynamics-core][field]") {
  RecordingGimmickDynamicsHost host;
  host.StateValue().reaction.mode = 253;
  host.reaction = VS::Field::GimmickReactionResult::kRefused;
  CHECK_FALSE(VS::Field::UpdateGimmickMotion(host));
  CHECK(host.calls == std::vector<int>{9});
  CHECK(host.additions == 0);
}

TEST_CASE(
    "Gimmick reaction frame zero uses signed first samples then stores frame "
    "two",
    "[gimmick-reaction-core][field]") {
  RecordingGimmickReactionHost host;
  GimmickReactionTestTables tables;
  host.StateValue().mode = 253;
  host.StateValue().position[1] = INT16_MIN;
  tables.spin[1] = {-1, -128, 127, -1};
  CHECK(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
        VS::Field::GimmickReactionResult::kAdvanced);
  CHECK(host.StateValue().frame == 2);
  CHECK(host.StateValue().position[1] == INT16_MAX);
  CHECK(host.StateValue().rotation ==
        std::array<std::int16_t, 3>{-128, 127, -1});
}

TEST_CASE(
    "Gimmick reaction terminal frame clears mode without advancing cursor",
    "[gimmick-reaction-core][field]") {
  for (const auto mode : {253, 251, 250, 247}) {
    RecordingGimmickReactionHost host;
    GimmickReactionTestTables tables;
    host.StateValue().mode = static_cast<std::int16_t>(mode);
    host.StateValue().frame = mode == 251 ? 9 : mode == 250 ? 3 : 14;
    const auto frame = host.StateValue().frame;
    CHECK(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
          VS::Field::GimmickReactionResult::kComplete);
    CHECK(host.StateValue().mode == 0);
    CHECK(host.StateValue().frame == frame);
  }
}

TEST_CASE("Gimmick reaction tilt sign and axis follow signed orientation",
          "[gimmick-reaction-core][field]") {
  for (const auto orientation :
       {-1, 0, 1, 2, 3, 4, static_cast<int>(INT16_MAX)}) {
    RecordingGimmickReactionHost host;
    GimmickReactionTestTables tables;
    host.StateValue().mode = 251;
    host.StateValue().orientation = static_cast<std::int16_t>(orientation);
    tables.tilt[1] = {-128, -127};
    REQUIRE(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
            VS::Field::GimmickReactionResult::kAdvanced);
    CHECK(host.StateValue().position[2] == -128);
    CHECK(host.StateValue().rotation[(orientation & 1) != 0 ? 2 : 0] ==
          (orientation == 0 || orientation == 3 ? 127 : -127));
  }
}

TEST_CASE("Gimmick reaction bounce replaces vertical delta with signed byte",
          "[gimmick-reaction-core][field]") {
  RecordingGimmickReactionHost host;
  GimmickReactionTestTables tables;
  host.StateValue().mode = 250;
  host.StateValue().vertical_delta = 99;
  tables.bounce[1] = -128;
  REQUIRE(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
          VS::Field::GimmickReactionResult::kAdvanced);
  CHECK(host.StateValue().vertical_delta == -128);
  CHECK(host.StateValue().position[1] == -128);
}

TEST_CASE(
    "Gimmick reaction roll angles remain unsigned before directional negation",
    "[gimmick-reaction-core][field]") {
  for (std::int16_t orientation = 0; orientation < 4; ++orientation) {
    RecordingGimmickReactionHost host;
    GimmickReactionTestTables tables;
    host.StateValue().mode = 249;
    host.StateValue().orientation = orientation;
    tables.roll[1] = {-1, -128, 255};
    REQUIRE(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
            VS::Field::GimmickReactionResult::kAdvanced);
    CHECK(host.StateValue().position[(orientation & 1) != 0 ? 0 : 2] ==
          (orientation >= 2 ? 128 : -128));
    CHECK(host.StateValue().rotation[(orientation & 1) != 0 ? 2 : 0] ==
          (orientation == 1 || orientation == 2 ? -255 : 255));
  }
}

TEST_CASE(
    "Gimmick reaction roll terminal rereads orientation callback before mode "
    "clear",
    "[gimmick-reaction-core][field]") {
  RecordingGimmickReactionHost host;
  GimmickReactionTestTables tables;
  host.StateValue().mode = 249;
  host.StateValue().frame = 14;
  host.StateValue().slot = 15;
  host.StateValue().material = 255;
  host.mutate_reset = true;
  REQUIRE(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
          VS::Field::GimmickReactionResult::kComplete);
  CHECK(host.calls == std::vector<int>{1, 2, 1});
  CHECK(host.reset_slot == 15);
  CHECK(host.reset_material == 255);
  CHECK(host.StateValue().rotation ==
        std::array<std::int16_t, 3>{123, 456, 789});
  CHECK(host.StateValue().mode == 0);
  CHECK(host.StateValue().frame == 14);
}

TEST_CASE(
    "Gimmick reaction break restore and release keep original frame boundaries",
    "[gimmick-reaction-core][field]") {
  for (const auto frame : {8, 9, 10, 39, 40}) {
    RecordingGimmickReactionHost host;
    GimmickReactionTestTables tables;
    host.StateValue().mode = 248;
    host.StateValue().frame = static_cast<std::uint8_t>(frame);
    host.StateValue().slot = 7;
    const auto result =
        VS::Field::UpdateGimmickReactionAnimation(tables.View(), host);
    CHECK(result == (frame == 40
                         ? VS::Field::GimmickReactionResult::kComplete
                         : VS::Field::GimmickReactionResult::kAdvanced));
    CHECK(host.StateValue().position[1] == (frame >= 9 ? 3 : 0));
    CHECK(host.calls == (frame == 40  ? std::vector<int>{1, 4}
                         : frame == 9 ? std::vector<int>{1, 3, 1}
                         : frame >= 9 ? std::vector<int>{1, 1}
                                      : std::vector<int>{1}));
  }
}

TEST_CASE("Gimmick reaction squash stores signed scale bytes directly",
          "[gimmick-reaction-core][field]") {
  RecordingGimmickReactionHost host;
  GimmickReactionTestTables tables;
  host.StateValue().mode = 247;
  tables.squash[1] = {-128, {-128, -1, 127}};
  REQUIRE(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
          VS::Field::GimmickReactionResult::kAdvanced);
  CHECK(host.StateValue().scale == std::array<std::int16_t, 3>{-128, -1, 127});
  CHECK(host.StateValue().position[1] == -128);
}

TEST_CASE("Gimmick reaction malformed frame refuses before any mutation",
          "[gimmick-reaction-core][field]") {
  RecordingGimmickReactionHost host;
  GimmickReactionTestTables tables;
  host.StateValue().mode = 253;
  host.StateValue().frame = 255;
  CHECK(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
        VS::Field::GimmickReactionResult::kRefused);
  CHECK(host.calls.empty());
  CHECK(host.StateValue().frame == 255);
  host.state.reset();
  CHECK(VS::Field::UpdateGimmickReactionAnimation(tables.View(), host) ==
        VS::Field::GimmickReactionResult::kRefused);
}

TEST_CASE(
    "Gimmick command push maps every kind and clears support before removal",
    "[gimmick-interaction-command-core][field]") {
  for (std::uint8_t kind = 0; kind < 8; ++kind) {
    RecordingGimmickCommandHost host;
    host.ObjectState().state.query.object_id = kind;
    host.actor.reset();
    REQUIRE(VS::Field::ApplyGimmickInteractionCommand(2, host));
    CHECK(host.mode == (kind < 2    ? 249
                        : kind == 2 ? 255
                        : kind == 3 ? 254
                                    : 99));
    CHECK(host.support_cleared);
    CHECK(host.calls == std::vector<int>{1, 2});
  }
}

TEST_CASE(
    "Gimmick command drop preserves full tile coordinates and signed bone "
    "average",
    "[gimmick-interaction-command-core][field]") {
  RecordingGimmickCommandHost host;
  host.ObjectState().yaw = 4095;
  REQUIRE(VS::Field::ApplyGimmickInteractionCommand(1, host));
  CHECK(host.calls == std::vector<int>{4, 5, 6, 7, 8});
  CHECK(host.drop.tile_x == 255);
  CHECK(host.drop.tile_z == -1);
  CHECK(host.ObjectState().yaw == 4095);
  CHECK(host.height == 60);
  CHECK_FALSE(host.carrying);
}

TEST_CASE("Gimmick command drop reads the second bone after the first callback",
          "[gimmick-interaction-command-core][field]") {
  RecordingGimmickCommandHost host;
  host.mutate_bone = true;
  REQUIRE(VS::Field::ApplyGimmickInteractionCommand(1, host));
  CHECK(host.height == 58);
  host.bones[1].reset();
  host.mutate_bone = false;
  host.calls.clear();
  CHECK_FALSE(VS::Field::ApplyGimmickInteractionCommand(1, host));
  CHECK(host.calls == std::vector<int>{4, 5, 6});
}

TEST_CASE(
    "Gimmick command attach releases unsigned tiles before balanced removal",
    "[gimmick-interaction-command-core][field]") {
  RecordingGimmickCommandHost host;
  host.ObjectState().state.query.interaction = 2;
  REQUIRE(VS::Field::ApplyGimmickInteractionCommand(0, host));
  CHECK(host.release.tile_x == 255);
  CHECK(host.release.tile_z == 255);
  CHECK(host.calls == std::vector<int>{3, 2, 2, 9, 10, 10, 11, 8});
  CHECK(host.ObjectState().state.slide_z == 128);
  CHECK(host.parent == 3);
  CHECK(host.carrying);
}

TEST_CASE(
    "Gimmick command attach rereads removal state and wraps halfword slide",
    "[gimmick-interaction-command-core][field]") {
  RecordingGimmickCommandHost host;
  host.mutate_removal = true;
  REQUIRE(VS::Field::ApplyGimmickInteractionCommand(-1, host));
  CHECK(host.calls == std::vector<int>{3, 2, 2, 9, 10, 10, 11, 8});
  CHECK(host.ObjectState().state.slide_z == -32641);
}

TEST_CASE(
    "Gimmick command attach rounds every unsigned yaw to cardinal orientation",
    "[gimmick-interaction-command-core][field]") {
  for (std::uint32_t yaw = 0; yaw <= UINT16_MAX; ++yaw) {
    RecordingGimmickCommandHost host;
    host.ObjectState().yaw = static_cast<std::uint16_t>(yaw);
    REQUIRE(VS::Field::ApplyGimmickInteractionCommand(0, host));
    CHECK(host.ObjectState().yaw == ((yaw + 512) & 4095) / 1024 * 1024);
  }
}

TEST_CASE(
    "Gimmick command missing or moving object plays idle without occupancy "
    "writes",
    "[gimmick-interaction-command-core][field]") {
  for (const auto motion : {1, 8, 15}) {
    RecordingGimmickCommandHost host;
    host.ObjectState().state.query.motion_state =
        static_cast<std::uint8_t>(motion);
    REQUIRE(VS::Field::ApplyGimmickInteractionCommand(0, host));
    CHECK(host.calls == std::vector<int>{12});
  }
  RecordingGimmickCommandHost host;
  host.object.reset();
  REQUIRE(VS::Field::ApplyGimmickInteractionCommand(0, host));
  CHECK(host.calls == std::vector<int>{12});
  CHECK_FALSE(VS::Field::ApplyGimmickInteractionCommand(2, host));
}

TEST_CASE("Gimmick command refusal stops geometry and actor attachment writes",
          "[gimmick-interaction-command-core][field]") {
  RecordingGimmickCommandHost host;
  host.refusal = 2;
  CHECK_FALSE(VS::Field::ApplyGimmickInteractionCommand(0, host));
  CHECK(host.calls == std::vector<int>{3, 2});
  CHECK_FALSE(host.carrying);
}

TEST_CASE(
    "Gimmick initiation refuses actor gates before selection and publication",
    "[gimmick-interaction-start-core][field]") {
  RecordingGimmickStartHost host;
  CHECK_FALSE(VS::Field::BeginGimmickInteraction(-1, host));
  CHECK_FALSE(VS::Field::BeginGimmickInteraction(17, host));
  host.ActorState().motion.weapon_pose_active = true;
  CHECK_FALSE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.calls.empty());
  host.ActorState().motion.weapon_pose_active = false;
  host.selected.reset();
  CHECK_FALSE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.calls == std::vector<int>{0});
}

TEST_CASE("Gimmick initiation pushes in sound animation pose reservation order",
          "[gimmick-interaction-start-core][field]") {
  RecordingGimmickStartHost host;
  REQUIRE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.calls == std::vector<int>{0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11});
  CheckInitiationPushState(host);
  CHECK(host.reservation.tile_x == 3);
  CHECK(host.reservation.tile_z == 3);
  CHECK(host.turn ==
        VS::Actors::PackedTile{.x = 3, .layer = 7, .z = 3, .spare = 255});
}

TEST_CASE("Gimmick initiation uses bounded four-column material rolls",
          "[gimmick-interaction-start-core][field]") {
  for (std::uint8_t material = 0; material < 24; ++material) {
    for (std::int32_t direction = 0; direction < 4; ++direction) {
      RecordingGimmickStartHost host;
      host.ObjectState().query.object_id = 1;
      host.material = material;
      host.direction = direction;
      REQUIRE(VS::Field::BeginGimmickInteraction(0, host));
      CHECK(host.material == host.rows[material * 4 + direction]);
      CHECK(host.calls[4] == 4);
    }
  }
  RecordingGimmickStartHost host;
  host.ObjectState().query.object_id = 0;
  host.material = 255;
  CHECK_FALSE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.calls == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE(
    "Gimmick initiation dispatches quarter slide carry and unused kind "
    "distinctly",
    "[gimmick-interaction-start-core][field]") {
  for (std::uint8_t kind = 3; kind < 8; ++kind) {
    RecordingGimmickStartHost host;
    host.ObjectState().query.object_id = kind;
    REQUIRE(VS::Field::BeginGimmickInteraction(0, host));
    if (kind == 3) {
      CHECK(host.ObjectState().slide_z == -32);
      CHECK(host.calls[4] == 3);
    } else if (kind < 7) {
      CHECK(host.animation == 0x48);
      CHECK(host.calls == std::vector<int>{0, 1, 2, 3, 7, 6, 8, 9, 10, 11});
    } else {
      CHECK(host.calls == std::vector<int>{0, 1, 2, 3, 10, 11});
      CHECK_FALSE(host.ActorState().motion.weapon_pose_active);
    }
  }
}

TEST_CASE(
    "Gimmick initiation placing chooses its original animation and sound pair",
    "[gimmick-interaction-start-core][field]") {
  RecordingGimmickStartHost host;
  host.ObjectState().query.object_id = 4;
  host.ActorState().motion.action_pose_active = true;
  REQUIRE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.animation == 0x4C);
  CHECK(host.sound_first == 0x2E);
  CHECK(host.sound_second == 0x2F);
  CHECK(host.reservation.tile_x == 3);
  CHECK(host.reservation.tile_z == 3);
}

TEST_CASE(
    "Gimmick initiation rereads callback changes and wraps only packed outputs",
    "[gimmick-interaction-start-core][field]") {
  RecordingGimmickStartHost host;
  host.mutate_after_animation = true;
  host.mutate_after_turn = true;
  REQUIRE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.reservation.tile_x == -1);
  CHECK(host.turn ==
        VS::Actors::PackedTile{.x = 0, .layer = 7, .z = 254, .spare = 255});
  CHECK(host.translation == VS::Actors::MotionVector{32704, -123, 32576, 0});
  RecordingGimmickStartHost carry;
  carry.ObjectState().query.object_id = 6;
  carry.mutate_after_animation = true;
  REQUIRE(VS::Field::BeginGimmickInteraction(0, carry));
  CHECK(carry.reservation.tile_x == 256);
  CHECK(carry.reservation.tile_z == 254);
}

TEST_CASE(
    "Gimmick initiation height class truncates signed differences before "
    "adding one",
    "[gimmick-interaction-start-core][field]") {
  for (const auto delta :
       {-129, -128, -127, -65, -64, -63, -1, 0, 1, 63, 64, 65, 127, 128}) {
    RecordingGimmickStartHost host;
    host.ActorState().motion.position.y =
        static_cast<std::int16_t>(100 + delta);
    REQUIRE(VS::Field::BeginGimmickInteraction(0, host));
    CHECK(host.animation == delta / 64 + 1 + 0x4F);
  }
}

TEST_CASE("Gimmick initiation stops later callbacks when a host stage refuses",
          "[gimmick-interaction-start-core][field]") {
  RecordingGimmickStartHost host;
  host.refusal = 9;
  CHECK_FALSE(VS::Field::BeginGimmickInteraction(0, host));
  CHECK(host.calls == std::vector<int>{0, 1, 2, 3, 5, 6, 7, 8, 9});
}

TEST_CASE(
    "Gimmick interaction writes direction after idle gates before grid refusal",
    "[gimmick-interaction-core][field]") {
  RecordingGimmickInteractionHost host;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(-1, host));
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(17, host));
  CHECK(host.calls.empty());
  host.ActorState().motion.route_phase = 1;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  CHECK(host.direction == 255);
  host.ActorState().motion.route_phase = 0;
  host.ActorState().motion.current_tile.z = 0;
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  CHECK(host.direction == 0);
  CHECK(host.calls == std::vector<int>{0, 1});
}

TEST_CASE("Gimmick interaction heading covers every signed halfword",
          "[gimmick-interaction-core][field]") {
  RecordingGimmickInteractionHost host;
  host.writable = false;
  for (std::int32_t heading = INT16_MIN; heading <= INT16_MAX; ++heading) {
    host.ActorState().motion.facing = static_cast<std::int16_t>(heading);
    CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
    CHECK(host.direction == ((heading + 512) & 4095) / 1024);
    host.calls.clear();
  }
}

TEST_CASE(
    "Gimmick interaction ground selection checks far placement then occupancy",
    "[gimmick-interaction-core][field]") {
  RecordingGimmickInteractionHost host;
  CHECK(VS::Field::FindInteractableGimmickAhead(0, host) == 3);
  CHECK(host.calls == std::vector<int>{0, 1, 3, 2, 4, 6});
  CheckGroundInteractionCoordinates(host);
  host.occupied = true;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  host.allowed = false;
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  CHECK(host.calls == std::vector<int>{0, 1, 3, 2, 4});
}

TEST_CASE(
    "Gimmick interaction held selection uses adjacent point and ordered "
    "ceilings",
    "[gimmick-interaction-core][field]") {
  RecordingGimmickInteractionHost host;
  host.ActorState().motion.action_pose_active = true;
  host.occupied = true;
  CHECK(VS::Field::FindInteractableGimmickAhead(0, host) == 3);
  CHECK(host.calls == std::vector<int>{0, 1, 2, 4, 5, 5});
  CheckHeldInteractionCoordinates(host);
  host.ceiling = -283;
  host.calls.clear();
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  CHECK(host.calls == std::vector<int>{0, 1, 2, 4, 5});
}

TEST_CASE("Gimmick interaction ground height window is signed and inclusive",
          "[gimmick-interaction-core][field]") {
  for (const auto height : {-29, -28, 0, 100, 164, 165}) {
    RecordingGimmickInteractionHost host;
    host.ObjectState().position_y = static_cast<std::int16_t>(height);
    CHECK(VS::Field::FindInteractableGimmickAhead(0, host).has_value() ==
          (height >= -28 && height <= 164));
  }
  RecordingGimmickInteractionHost host;
  host.ActorState().motion.position.y = INT16_MIN;
  host.ObjectState().position_y = INT16_MIN;
  CHECK(VS::Field::FindInteractableGimmickAhead(0, host) == 3);
  CHECK(host.point.minimum_height == INT16_MIN);
}

TEST_CASE(
    "Gimmick interaction high kinds check ceiling instead of far placement",
    "[gimmick-interaction-core][field]") {
  for (std::uint8_t kind = 0; kind < 8; ++kind) {
    RecordingGimmickInteractionHost host;
    host.ObjectState().object_id = kind;
    CHECK(VS::Field::FindInteractableGimmickAhead(0, host) == 3);
    CHECK(host.calls == (kind < 4 ? std::vector<int>{0, 1, 3, 2, 4, 6}
                                  : std::vector<int>{0, 1, 3, 2, 5, 5, 6}));
  }
}

TEST_CASE(
    "Gimmick interaction does not wrap byte edges or reject the far negative "
    "tile",
    "[gimmick-interaction-core][field]") {
  RecordingGimmickInteractionHost host;
  host.ActorState().motion.facing = 2048;
  host.ActorState().motion.current_tile.z = 255;
  CHECK(VS::Field::FindInteractableGimmickAhead(0, host) == 3);
  CHECK(host.tile.tile_z == 256);
  CHECK(host.point.z == 32960);
  host.ActorState().motion.facing = 0;
  host.ActorState().motion.current_tile.z = 1;
  CHECK(VS::Field::FindInteractableGimmickAhead(0, host) == 3);
  CHECK(host.tile.tile_z == 0);
  CHECK(host.point.z == -64);
}

TEST_CASE(
    "Gimmick interaction refuses missing zero sentinel moving and reserved "
    "objects",
    "[gimmick-interaction-core][field]") {
  RecordingGimmickInteractionHost host;
  host.top = 0;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  host.top = 16;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  host.top = 3;
  host.ObjectState().interaction = 1;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  host.ObjectState().interaction = 0;
  host.ObjectState().movement_mode = -1;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  host.ActorState().motion.action_pose_active = true;
  host.ActorState().attach_bone = 255;
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
  host.ActorState().attach_bone = 3;
  host.object.reset();
  CHECK_FALSE(VS::Field::FindInteractableGimmickAhead(0, host));
}

TEST_CASE(
    "Gimmick point placement preserves support probe occupancy refusal order",
    "[gimmick-placeability-core][field]") {
  RecordingGimmickPointHost host;
  CHECK_FALSE(VS::Field::CanPlaceGimmickAtPoint({.minimum_height = 1}, host));
  CHECK(host.calls == std::vector<int>{1});
  host.calls.clear();
  host.probe = 1u << 20;
  CHECK_FALSE(VS::Field::CanPlaceGimmickAtPoint({}, host));
  CHECK(host.calls == std::vector<int>{1, 2});
  host.calls.clear();
  host.probe = 0;
  host.occupied = true;
  CHECK_FALSE(VS::Field::CanPlaceGimmickAtPoint({}, host));
  CHECK(host.calls == std::vector<int>{1, 2, 3});
}

TEST_CASE(
    "Gimmick point placement uses full words and truncates negative tile "
    "division",
    "[gimmick-placeability-core][field]") {
  for (const auto coordinate : {INT32_MIN, -32769, -257, -129, -128, -127, -1,
                                0, 1, 127, 128, 129, 32768, INT32_MAX}) {
    RecordingGimmickPointHost host;
    const VS::Field::GimmickPlacementPoint point{coordinate, coordinate, -1};
    REQUIRE(VS::Field::CanPlaceGimmickAtPoint(point, host));
    CHECK(host.point.x == coordinate);
    CHECK(host.point.z == coordinate);
    CHECK(host.query.tile.tile_x == coordinate / 128);
    CHECK(host.query.tile.tile_z == coordinate / 128);
    CHECK(host.query.minimum_height == 0);
  }
}

TEST_CASE("Gimmick point placement gates only original probe bit twenty",
          "[gimmick-placeability-core][field]") {
  for (unsigned bit = 0; bit < 32; ++bit) {
    RecordingGimmickPointHost host;
    host.probe = std::uint32_t{1} << bit;
    CHECK(VS::Field::CanPlaceGimmickAtPoint({}, host) == (bit != 20));
    CHECK(host.calls.size() == (bit == 20 ? 2 : 3));
  }
}

TEST_CASE(
    "Gimmick point placement forwards signed support height at the minimum "
    "boundary",
    "[gimmick-placeability-core][field]") {
  for (const auto height :
       {INT32_MIN, -32768, -3000, -1, 0, 1, 32767, INT32_MAX}) {
    RecordingGimmickPointHost host;
    host.height = height;
    REQUIRE(
        VS::Field::CanPlaceGimmickAtPoint({.minimum_height = height}, host));
    CHECK(host.query.minimum_height == height);
  }
}

namespace {
struct GimmickPointLiveFixture : GimmickResourcesLiveFixture {
  decltype(Field_roomData) saved_room = Field_roomData;
  decltype(std::to_array(Actors_actorRecords)) saved_actors =
      std::to_array(Actors_actorRecords);
  VS::Field::CollisionProbe saved_probe = VS::Field::LiveCollisionProbe();
  VS::Field::CollisionSamples saved_samples = VS::Field::LiveCollisionSamples();
  struct CollisionStorage {
    Field_MpdCollisionHeader header{
        .width = 2, .height = 2, .heightOrigin = 0, .cellCount = 4};
    std::array<std::uint16_t, 8> descriptors{};
    std::array<std::uint8_t, 16> pattern{};
  } collision;
  std::array<Field_MpdTileCell, 4> cells{};
  Actors_ActorRecord actor{};
  GimmickPointLiveFixture() {
    std::fill_n(Field_gimmickInstances, saved_instances.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_actors.size(), nullptr);
    Field_roomData = {};
    Field_roomData.collisionMap = &collision.header;
    Field_roomData.tileCells = cells.data();
    Field_roomData.header.collisionMapLength = sizeof(collision);
    Field_roomData.header.tileCellsLength = sizeof(cells);
  }
  ~GimmickPointLiveFixture() {
    Field_roomData = saved_room;
    std::copy(saved_actors.begin(), saved_actors.end(), Actors_actorRecords);
    VS::Field::LiveCollisionProbe() = saved_probe;
    VS::Field::LiveCollisionSamples() = saved_samples;
  }
};
}  // namespace

TEST_CASE(
    "Native gimmick point placement samples actual support before occupancy",
    "[gimmick-placeability-live][field]") {
  GimmickPointLiveFixture fixture;
  const auto result = Field_CanPlaceGimmickAtPointEntry(64, 64, 0);
  SVECTOR origin{};
  CHECK(VS::Field::LiveSampleActorSupportHeight(&origin, 64, 64, 1) >= 0);
  CHECK((VS::Field::LiveCollisionProbeCellWord() & (1u << 20)) == 0);
  CHECK(result == 1);
  CHECK(Field_CanPlaceGimmickAtPointEntry(64, 64, 1) == 0);
  Actors_actorRecords[2] = &fixture.actor;
  fixture.actor.instance.currentTileX = 0;
  fixture.actor.instance.currentTileZ = 0;
  CHECK(Field_CanPlaceGimmickAtPointEntry(64, 64, 0) == 0);
}

TEST_CASE("Native gimmick point placement rejects actual probe bit twenty",
          "[gimmick-placeability-live][field]") {
  GimmickPointLiveFixture fixture;
  const auto original = std::bit_cast<std::uint32_t>(fixture.cells[0]);
  fixture.cells[0] = std::bit_cast<Field_MpdTileCell>(original | (1u << 20));
  CHECK(Field_CanPlaceGimmickAtPointEntry(64, 64, 0) == 0);
  CHECK((VS::Field::LiveCollisionProbeCellWord() & (1u << 20)) != 0);
}

TEST_CASE(
    "Native gimmick point placement refuses absent and out-of-room probes",
    "[gimmick-placeability-live][field]") {
  GimmickPointLiveFixture fixture;
  CHECK(Field_CanPlaceGimmickAtPointEntry(INT32_MAX, INT32_MIN, 0) == 0);
  Field_roomData.collisionMap = nullptr;
  CHECK(Field_CanPlaceGimmickAtPointEntry(64, 64, 0) == 0);
}

TEST_CASE("Gimmick point placement retains its original module address",
          "[gimmick-placeability-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A3DB4) ==
        reinterpret_cast<const void*>(&Field_CanPlaceGimmickAtPointEntry));
}

namespace {
struct GimmickInteractionLiveFixture : GimmickPointLiveFixture {
  decltype(Field_gimmickInteractionDirection) saved_direction =
      Field_gimmickInteractionDirection;
  GimmickInteractionLiveFixture() {
    Actors_actorRecords[0] = &actor;
    actor.instance.currentTileX = 0;
    actor.instance.currentTileZ = 2;
    actor.instance.position.vx = 64;
    actor.instance.position.vz = 320;
    auto& object = instances[0];
    Field_gimmickInstances[1] = &object;
    object.parent = 255;
    object.tileZ = 1;
    object.objectInfo.carrier = 255;
    object.objectInfo.objectId = 0;
    Field_gimmickInteractionDirection = 77;
  }
  ~GimmickInteractionLiveFixture() {
    Field_gimmickInteractionDirection = saved_direction;
  }
};

struct GimmickStartLiveFixture : GimmickInteractionLiveFixture {
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  decltype(Field_interactingGimmickId) saved_selected =
      Field_interactingGimmickId;
  using FlagsRow = std::uint16_t[32];
  std::array<FlagsRow, 32> flags{};
  std::array<std::uint8_t, 96> saved_roll{};
  GimmickStartLiveFixture() {
    std::copy_n(Field_kGimmickRollOrientations, saved_roll.size(),
                saved_roll.begin());
    Battle_Ai_tileFlagMap = flags.data();
    instances[0].positionX = 64;
    instances[0].positionZ = 192;
    instances[0].objectInfo.objectId = 2;
    instances[0].velocityY = 99;
    instances[0].slideX = 37;
    instances[0].slideZ = 29;
    actor.instance.reserved5F = 255;
    actor.instance.facing = 100;
    Field_interactingGimmickId = -1;
    flags[0][0] = 0x400;
  }
  ~GimmickStartLiveFixture() {
    std::copy(saved_roll.begin(), saved_roll.end(),
              Field_kGimmickRollOrientations);
    Battle_Ai_tileFlagMap = saved_flags;
    Field_interactingGimmickId = saved_selected;
  }
};

struct GimmickCommandLiveFixture : GimmickStartLiveFixture {
  decltype(Battle_Ai_specialTerrainHeights) saved_heights =
      Battle_Ai_specialTerrainHeights;
  using HeightRow =
      std::remove_pointer_t<decltype(Battle_Ai_specialTerrainHeights)>;
  std::array<HeightRow, 32> heights{};
  MATRIX saved_camera = Actors_boneMatrixScratch;
  struct Model {
    Actors_ShpModel header{};
    std::array<Actors_ShpBone, 2> bones{};
  } model;
  GimmickCommandLiveFixture() {
    static_assert(offsetof(Model, bones) == sizeof(Actors_ShpModel));
    Battle_Ai_specialTerrainHeights = heights.data();
    heights[1][0] = 0x800;
    flags[1][0] = 0x480;
    Field_interactingGimmickId = 1;
    Field_gimmickInteractionDirection = 0;
    instances[0].rotationY = 1000;
    instances[0].objectInfo.supportIndex = 9;
    Actors_boneMatrixScratch = IdentityMatrix();
    for (auto& matrix : actor.boneMatrices) matrix = IdentityMatrix();
    actor.boneMatrices[0].t[1] = -1;
    model.bones[0].tag = 0xF0;
    model.bones[1].tag = 0xF1;
    actor.instance.model = &model.header;
    actor.instance.boneCount = 2;
    actor.modelDataSize = sizeof(model);
    actor.scale = 4096;
    VS::Render::RecordLiveModelExtent(&model, sizeof(model));
  }
  ~GimmickCommandLiveFixture() {
    VS::Render::ForgetLiveModelExtent(&model);
    Battle_Ai_specialTerrainHeights = saved_heights;
    Actors_boneMatrixScratch = saved_camera;
  }
  static MATRIX IdentityMatrix() {
    MATRIX matrix{};
    matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 4096;
    return matrix;
  }
};

struct GimmickDynamicsLiveFixture : GimmickCommandLiveFixture {
  GimmickDynamicsLiveFixture() {
    auto& record = instances[0];
    record.slot = 1;
    record.positionY = 0;
    record.slideX = record.slideZ = record.velocityY = 0;
    record.objectInfo.supportIndex = 0;
    record.parent = 255;
    record.rotationTicks = 0;
    heights[1][0] = 0;
    auto& child = instances[1];
    child.slot = 5;
    child.mode = 8;
    child.positionY = 20;
    child.objectInfo.carrier = 1;
    Field_gimmickInstances[5] = &child;
  }
};

void CheckNativeInitiationSlide(const GimmickStartLiveFixture& fixture) {
  CHECK(fixture.instances[0].orientation == 0);
  CHECK(fixture.instances[0].slideX == 0);
  CHECK(fixture.instances[0].slideZ == -128);
  CHECK(fixture.instances[0].velocityY == 0);
  CHECK(fixture.flags[0][0] == 0x480);
}

void CheckNativeRollRefusal(const GimmickStartLiveFixture& fixture) {
  CHECK(Field_interactingGimmickId == 1);
  CHECK(fixture.instances[0].slideX == 0);
  CHECK(fixture.instances[0].slideZ == 0);
  CHECK(fixture.actor.weaponPoseActive == 0);
  CHECK(fixture.flags[0][0] == 0x400);
}

void CheckNativeCommandAttachment(const GimmickCommandLiveFixture& fixture) {
  CHECK(fixture.instances[0].mode == 8);
  CHECK(fixture.instances[0].parent == 0);
  CHECK(static_cast<std::uint8_t>(fixture.instances[0].tileX) == 255);
  CHECK(fixture.instances[0].slideZ == 157);
  CHECK(fixture.instances[0].rotationY == 1024);
}

void CheckNativeCommandDrop(const GimmickCommandLiveFixture& fixture) {
  CHECK(fixture.instances[0].positionY == 60);
  CHECK(fixture.instances[0].positionZ == 192);
  CHECK(fixture.instances[0].mode == 252);
  CHECK(fixture.instances[0].velocityY == 24);
  CHECK(fixture.instances[0].parent == 255);
  CHECK(fixture.actor.actionPoseActive == 0);
}
}  // namespace

TEST_CASE(
    "Native gimmick dynamics falling carries actual child with gravity delta",
    "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 252;
  record.positionY = -10;
  REQUIRE(VS::Field::UpdateLiveGimmickMotion(&record));
  CHECK(record.positionY == -7);
  CHECK(record.velocityY == 3);
  CHECK(record.mode == 252);
  CHECK(fixture.instances[1].positionY == 23);
  CHECK(fixture.instances[1].objectInfo.carrier == 1);
}

TEST_CASE(
    "Native gimmick dynamics slide completion lands and balances real maps",
    "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 255;
  REQUIRE(VS::Field::UpdateLiveGimmickMotion(&record));
  CHECK(record.mode == 0);
  CHECK(record.objectInfo.supportIndex == 0);
  CHECK(fixture.heights[1][0] == 0x400);
  CHECK(fixture.flags[1][0] == 0x400);
  CHECK(fixture.instances[1].objectInfo.carrier == 255);
}

TEST_CASE("Native gimmick dynamics final durability leaves child untouched",
          "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 252;
  record.objectInfo.objectId = 1;
  record.objectInfo.subtype = 1;
  REQUIRE(VS::Field::UpdateLiveGimmickMotion(&record));
  CHECK(record.mode == 248);
  CHECK(record.objectInfo.subtype == 0);
  CHECK(record.objectInfo.supportIndex == 0);
  CHECK(fixture.instances[1].positionY == 20);
  CHECK(fixture.instances[1].objectInfo.carrier == 1);
}

TEST_CASE(
    "Native gimmick dynamics spin completion uses owned reaction and two "
    "occupancies",
    "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 253;
  record.objectInfo.supportIndex = 14;
  REQUIRE(VS::Field::UpdateLiveGimmickMotion(&record));
  CHECK(record.mode == 0);
  CHECK(record.objectInfo.supportIndex == 14);
  CHECK(fixture.heights[1][0] == 0x800);
  CHECK(fixture.flags[1][0] == 0x400);
}

TEST_CASE(
    "Native gimmick dynamics slow slide probes actual room before clearing "
    "tiles",
    "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 254;
  record.orientation = 1;
  record.positionX = 192;
  record.slideX = -32;
  REQUIRE(VS::Field::UpdateLiveGimmickMotion(&record));
  CHECK(record.positionX == 160);
  CHECK(record.mode == 254);
  CHECK(static_cast<std::uint8_t>(record.tileX) == 255);
  CHECK(static_cast<std::uint8_t>(record.tileZ) == 255);
}

TEST_CASE(
    "Native gimmick dynamics parent-held countdown stops without floor queries",
    "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  auto& record = fixture.instances[0];
  record.mode = 1;
  record.parent = 0;
  record.slideX = -3;
  record.slideZ = 5;
  record.velocityY = 7;
  REQUIRE(VS::Field::UpdateLiveGimmickMotion(&record));
  CHECK(record.positionX == 61);
  CHECK(record.positionY == 7);
  CHECK(record.positionZ == 197);
  CHECK(record.mode == 0);
  CHECK(fixture.heights[1][0] == 0);
}

TEST_CASE(
    "Native gimmick dynamics refuses missing short record and occupancy maps",
    "[gimmick-dynamics-live][field]") {
  GimmickDynamicsLiveFixture fixture;
  CHECK_FALSE(VS::Field::UpdateLiveGimmickMotion(nullptr));
  fixture.instances[0].mode = 253;
  fixture.instances[0].objectInfo.supportIndex = 14;
  Battle_Ai_specialTerrainHeights = nullptr;
  CHECK_FALSE(VS::Field::UpdateLiveGimmickMotion(&fixture.instances[0]));
  CHECK(fixture.flags[1][0] == 0x480);
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  void* record = VS::Core::Memory::AllocateLiveFromLowEnd(32);
  REQUIRE(record != nullptr);
  CHECK_FALSE(VS::Field::UpdateLiveGimmickMotion(
      static_cast<Field_GimmickInstance*>(record)));
  Field_UpdateGimmickMotionEntry(nullptr, 0);
}

TEST_CASE("Gimmick dynamics retains its original module address",
          "[gimmick-dynamics-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B0908) ==
        reinterpret_cast<const void*>(&Field_UpdateGimmickMotionEntry));
}

TEST_CASE(
    "Native gimmick command push clears support and removes real occupancy",
    "[gimmick-interaction-command-live][field]") {
  GimmickCommandLiveFixture fixture;
  REQUIRE(VS::Field::ApplyLiveGimmickInteractionCommand(nullptr, 2));
  CHECK(fixture.instances[0].mode == 255);
  CHECK(fixture.instances[0].objectInfo.supportIndex == 0);
  CHECK(fixture.heights[1][0] == 0x400);
}

TEST_CASE(
    "Native gimmick command attach releases and balances double occupancy",
    "[gimmick-interaction-command-live][field]") {
  GimmickCommandLiveFixture fixture;
  fixture.instances[0].objectInfo.interaction = 2;
  REQUIRE(VS::Field::ApplyLiveGimmickInteractionCommand(&fixture.actor, 0));
  CHECK(fixture.flags[1][0] == 0x400);
  CHECK(fixture.heights[1][0] == 0);
  CheckNativeCommandAttachment(fixture);
  CHECK(fixture.actor.instance.attachBone == 1);
  CHECK(fixture.actor.actionPoseActive == 1);
}

TEST_CASE("Native gimmick command drop uses actual tagged parent bone heights",
          "[gimmick-interaction-command-live][field]") {
  GimmickCommandLiveFixture fixture;
  fixture.instances[0].parent = 0;
  fixture.actor.actionPoseActive = 1;
  SVECTOR first{};
  SVECTOR second{};
  REQUIRE(VS::Actors::LiveActorTaggedBonePosition(0, 0xF0, &first, 1) == 0);
  REQUIRE(VS::Actors::LiveActorTaggedBonePosition(0, 0xF1, &second, 1) == 0);
  CHECK(first.vy == -1);
  CHECK(second.vy == 0);
  REQUIRE(VS::Field::ApplyLiveGimmickInteractionCommand(&fixture.actor, 1));
  CheckNativeCommandDrop(fixture);
}

TEST_CASE("Native gimmick command refuses missing data without fatal helpers",
          "[gimmick-interaction-command-live][field]") {
  GimmickCommandLiveFixture fixture;
  CHECK_FALSE(VS::Field::ApplyLiveGimmickInteractionCommand(nullptr, 1));
  Battle_Ai_tileFlagMap = nullptr;
  CHECK_FALSE(VS::Field::ApplyLiveGimmickInteractionCommand(&fixture.actor, 0));
  CHECK(fixture.heights[1][0] == 0x800);
  CHECK(fixture.actor.actionPoseActive == 0);
  Field_gimmickInstances[1] = nullptr;
  CHECK_FALSE(VS::Field::ApplyLiveGimmickInteractionCommand(nullptr, 2));
}

TEST_CASE(
    "Native gimmick command refuses misaligned actor before reading its fields",
    "[gimmick-interaction-command-live][field]") {
  GimmickCommandLiveFixture fixture;
  alignas(Actors_ActorRecord)
      std::array<std::byte, sizeof(Actors_ActorRecord) + 1>
          storage{};
  auto* actor = reinterpret_cast<Actors_ActorRecord*>(storage.data() + 1);
  CHECK_FALSE(VS::Field::ApplyLiveGimmickInteractionCommand(actor, 1));
  CHECK(fixture.instances[0].mode == 0);
  CHECK(fixture.heights[1][0] == 0x800);
}

TEST_CASE("Gimmick interaction command retains its original module address",
          "[gimmick-interaction-command-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A41D0) ==
        reinterpret_cast<const void*>(
            &Field_ApplyGimmickInteractionCommandEntry));
}

TEST_CASE(
    "Native gimmick initiation writes original motion state and real reserved "
    "tile",
    "[gimmick-interaction-start-live][field]") {
  GimmickStartLiveFixture fixture;
  REQUIRE(Field_BeginGimmickInteractionEntry(0) == 0);
  CHECK(Field_interactingGimmickId == 1);
  CheckNativeInitiationSlide(fixture);
  CHECK(fixture.actor.weaponPoseActive == 1);
  CHECK(fixture.actor.instance.reserved5F == 255);
  CHECK(fixture.actor.instance.turnFrames == 4);
}

TEST_CASE(
    "Native gimmick initiation refuses absent actors and missing reservation "
    "grid",
    "[gimmick-interaction-start-live][field]") {
  GimmickStartLiveFixture fixture;
  CHECK(Field_BeginGimmickInteractionEntry(-1) == -1);
  CHECK(Field_BeginGimmickInteractionEntry(17) == -1);
  CHECK(Field_BeginGimmickInteractionEntry(2) == -1);
  fixture.actor.weaponPoseActive = 1;
  CHECK(Field_BeginGimmickInteractionEntry(0) == -1);
  CHECK(Field_interactingGimmickId == -1);
  fixture.actor.weaponPoseActive = 0;
  Battle_Ai_tileFlagMap = nullptr;
  CHECK(Field_BeginGimmickInteractionEntry(0) == -1);
  CHECK(fixture.actor.instance.turnFrames == 0);
}

TEST_CASE(
    "Native gimmick initiation rolls only the logical material table prefix",
    "[gimmick-interaction-start-live][field]") {
  GimmickStartLiveFixture fixture;
  fixture.instances[0].objectInfo.objectId = 0;
  SECTION("Original unsigned table byte becomes material") {
    Field_kGimmickRollOrientations[0] = 17;
    REQUIRE(Field_BeginGimmickInteractionEntry(0) == 0);
    CHECK(fixture.instances[0].objectInfo.material == 17);
    CHECK(fixture.instances[0].slideZ == -128);
  }
  SECTION("Oversized backing declaration does not authorize material255") {
    fixture.instances[0].objectInfo.material = 255;
    CHECK(Field_BeginGimmickInteractionEntry(0) == -1);
    CheckNativeRollRefusal(fixture);
  }
}

TEST_CASE("Gimmick interaction initiation retains its original module address",
          "[gimmick-interaction-start-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A3E6C) ==
        reinterpret_cast<const void*>(&Field_BeginGimmickInteractionEntry));
}

TEST_CASE(
    "Native gimmick interaction uses real top placement and actor occupancy",
    "[gimmick-interaction-live][field]") {
  GimmickInteractionLiveFixture fixture;
  CHECK(Field_FindInteractableGimmickAheadEntry(0) == 1);
  CHECK(Field_gimmickInteractionDirection == 0);
  Actors_ActorRecord blocker{};
  blocker.instance.currentTileZ = 1;
  Actors_actorRecords[2] = &blocker;
  CHECK(Field_FindInteractableGimmickAheadEntry(0) == -1);
  Actors_actorRecords[2] = nullptr;
  fixture.instances[0].objectInfo.interaction = 1;
  CHECK(Field_FindInteractableGimmickAheadEntry(0) == -1);
}

TEST_CASE(
    "Native gimmick interaction refuses invalid actor and held record after "
    "direction",
    "[gimmick-interaction-live][field]") {
  GimmickInteractionLiveFixture fixture;
  CHECK(Field_FindInteractableGimmickAheadEntry(-1) == -1);
  CHECK(Field_FindInteractableGimmickAheadEntry(17) == -1);
  CHECK(Field_FindInteractableGimmickAheadEntry(2) == -1);
  CHECK(Field_gimmickInteractionDirection == 77);
  fixture.actor.routePhase = 1;
  CHECK(Field_FindInteractableGimmickAheadEntry(0) == -1);
  CHECK(Field_gimmickInteractionDirection == 77);
  fixture.actor.routePhase = 0;
  fixture.actor.actionPoseActive = 1;
  fixture.actor.instance.attachBone = 255;
  CHECK(Field_FindInteractableGimmickAheadEntry(0) == -1);
  CHECK(Field_gimmickInteractionDirection == 0);
  fixture.actor.instance.attachBone = 1;
  Field_gimmickInstances[1] = nullptr;
  CHECK(Field_FindInteractableGimmickAheadEntry(0) == -1);
}

TEST_CASE("Gimmick interaction selector retains its original module address",
          "[gimmick-interaction-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        0x800A38E0) ==
      reinterpret_cast<const void*>(&Field_FindInteractableGimmickAheadEntry));
}

TEST_CASE("Retired actor-above alias leaves no original module binding",
          "[gimmick-actor-above-retirement][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800A3C00) == nullptr);
}

TEST_CASE(
    "Direct actor-above scalar loads retain unsigned tiles and signed support "
    "height",
    "[gimmick-actor-above-retirement][field]") {
  GimmickPointLiveFixture fixture;
  Actors_actorRecords[2] = &fixture.actor;
  fixture.actor.instance.currentTileX = 255;
  fixture.actor.instance.currentTileZ = 254;
  Field_GimmickInstance support{};
  support.tileX = std::bit_cast<char>(std::uint8_t{255});
  support.tileZ = std::bit_cast<char>(std::uint8_t{254});
  support.positionY = -128;
  for (const auto mode : {0, 1, 2}) {
    CHECK(VS::Field::LiveFindActorOccupyingTile(
              static_cast<std::uint8_t>(support.tileX),
              static_cast<std::uint8_t>(support.tileZ), support.positionY,
              mode) == &fixture.actor);
  }
  CHECK(VS::Field::LiveFindActorOccupyingTile(-1, -2, support.positionY, 1) ==
        nullptr);
}

namespace {
struct RecordingGimmickStackDropHost : VS::Field::GimmickStackDropHost {
  std::array<std::optional<VS::Field::GimmickStackState>, 16> states{};
  std::vector<int> calls;
  std::vector<VS::Field::GimmickSupportedActor> actors;
  std::vector<int> actor_queries;
  bool refuse_remove = false;
  std::optional<VS::Field::GimmickStackState> Gimmick(
      std::size_t slot) override {
    return states[slot];
  }
  bool StoreGimmick(std::size_t slot,
                    const VS::Field::GimmickStackState& state) override {
    states[slot] = state;
    calls.push_back(static_cast<int>(slot));
    return true;
  }
  bool SameGimmick(std::size_t first, std::size_t second) const override {
    return first == second;
  }
  bool RemoveOccupant(std::size_t slot) override {
    calls.push_back(100 + static_cast<int>(slot));
    return !refuse_remove;
  }
  std::optional<VS::Field::GimmickSupportedActor> ActorAbove(
      const VS::Field::GimmickQueryState&, std::int32_t first) override {
    actor_queries.push_back(first);
    for (const auto& actor : actors) {
      if (actor.state.motion.slot >= first) return actor;
    }
    return std::nullopt;
  }
  bool StoreActor(const VS::Field::GimmickSupportedActor& actor) override {
    calls.push_back(200 + static_cast<int>(actor.table_index));
    actors[actor.table_index] = actor;
    return true;
  }
  VS::Field::GimmickStackState& State(std::size_t slot) {
    auto& state = states[slot];
    if (!state) {
      FAIL("Missing stack drop state fixture");
      throw std::bad_optional_access{};
    }
    return *state;
  }
  void Stack() {
    states[3] = VS::Field::GimmickStackState{
        .query = {.present = true, .tile_word = 0x00050203, .position_y = 256}};
    states[1] = VS::Field::GimmickStackState{
        .query = {.present = true, .tile_word = 0x00050203, .position_y = 128},
        .velocity_y = 17};
  }
};
}  // namespace

TEST_CASE(
    "Gimmick stack drop marks falling before choosing and unregistering base",
    "[gimmick-stack-drop-core][field]") {
  RecordingGimmickStackDropHost host;
  host.Stack();
  host.State(1).query.interaction = 2;
  REQUIRE(VS::Field::DropUnsupportedGimmickStack(3, host));
  CHECK(host.State(3).query.falling);
  CHECK(host.State(1).velocity_y == 0);
  CHECK(host.State(1).query.movement_mode == 252);
  CHECK(std::count(host.calls.begin(), host.calls.end(), 101) == 2);
  REQUIRE(host.calls.size() >= 4);
  CHECK(host.calls[0] == 3);
  CHECK(host.calls[1] == 1);
  CHECK(host.calls[2] == 101);
}

TEST_CASE(
    "Gimmick stack drop carrier uses low24 tile bytes and clears ineligible "
    "survivors",
    "[gimmick-stack-drop-core][field]") {
  RecordingGimmickStackDropHost host;
  host.Stack();
  host.states[5] = VS::Field::GimmickStackState{
      .query = {.present = true, .tile_word = 0x77050203, .position_y = 64}};
  host.states[6] = VS::Field::GimmickStackState{
      .query = {.present = true, .movement_mode = -1, .carrier = 7}};
  host.states[7] = VS::Field::GimmickStackState{
      .query = {.present = true, .skipped = true, .carrier = 7}};
  REQUIRE(VS::Field::DropUnsupportedGimmickStack(3, host));
  CHECK(host.State(5).query.carrier == 1);
  CHECK(host.State(6).query.carrier == 255);
  CHECK(host.State(7).query.carrier == 7);
  CHECK(host.State(3).query.carrier == 255);
}

TEST_CASE("Gimmick stack drop starts actor fall and skips companion slot one",
          "[gimmick-stack-drop-core][field]") {
  RecordingGimmickStackDropHost host;
  host.Stack();
  VS::Field::GimmickSupportedActor leader;
  leader.state.motion.root_motion = {1, 2, 3, 4};
  leader.state.motion.destination_mode = 9;
  leader.state.motion.movement_pending = true;
  leader.state.support_gimmick = 7;
  host.actors.push_back(leader);
  auto companion = leader;
  companion.table_index = 1;
  companion.state.motion.slot = 2;
  companion.state.motion.current_tile = {.x = 255, .z = 254};
  host.actors.push_back(companion);
  REQUIRE(VS::Field::DropUnsupportedGimmickStack(3, host));
  CHECK(host.actors[0].state.motion.movement_mode == 3);
  CHECK(host.actors[0].state.motion.root_motion == VS::Actors::MotionVector{});
  CHECK(host.actors[0].state.motion.destination_mode == 9);
  CHECK(host.actors[1].state.motion.destination_x == 255);
  CHECK(host.actors[1].state.motion.destination_z == 254);
  CHECK(host.actors[1].state.motion.destination_mode == 1);
  CHECK(host.actor_queries == std::vector<int>{0, 2, 3});
}

TEST_CASE(
    "Gimmick stack drop leaves existing fall motion and jump state intact",
    "[gimmick-stack-drop-core][field]") {
  RecordingGimmickStackDropHost host;
  host.Stack();
  VS::Field::GimmickSupportedActor actor;
  actor.state.motion.movement_mode = 3;
  actor.state.motion.root_motion = {1, 2, 3, 4};
  actor.state.motion.jump_countdown = 9;
  actor.state.motion.locomotion_state = 2;
  actor.state.motion.movement_pending = true;
  actor.state.support_gimmick = 7;
  host.actors.push_back(actor);
  REQUIRE(VS::Field::DropUnsupportedGimmickStack(3, host));
  CHECK(host.actors[0].state.motion.root_motion ==
        VS::Actors::MotionVector{1, 2, 3, 4});
  CHECK(host.actors[0].state.motion.jump_countdown == 9);
  CHECK(host.actors[0].state.motion.locomotion_state == 0);
  CHECK_FALSE(host.actors[0].state.motion.movement_pending);
  CHECK(host.actors[0].state.support_gimmick == 0);
}

TEST_CASE(
    "Gimmick stack drop refuses absent records and stops on cleanup refusal",
    "[gimmick-stack-drop-core][field]") {
  RecordingGimmickStackDropHost host;
  CHECK_FALSE(VS::Field::DropUnsupportedGimmickStack(-1, host));
  CHECK_FALSE(VS::Field::DropUnsupportedGimmickStack(16, host));
  CHECK_FALSE(VS::Field::DropUnsupportedGimmickStack(3, host));
  CHECK(host.calls.empty());
  host.Stack();
  host.refuse_remove = true;
  CHECK_FALSE(VS::Field::DropUnsupportedGimmickStack(3, host));
  CHECK(host.actor_queries.empty());
}

namespace {
struct GimmickStackDropLiveFixture : GimmickPointLiveFixture {
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  decltype(Battle_Ai_specialTerrainHeights) saved_heights =
      Battle_Ai_specialTerrainHeights;
  GimmickStackDropLiveFixture() {
    Battle_Ai_tileFlagMap = nullptr;
    Battle_Ai_specialTerrainHeights = nullptr;
    Field_gimmickInstances[3] = &instances[0];
    Field_gimmickInstances[1] = &instances[1];
    for (auto& record : instances) {
      record.parent = record.objectInfo.carrier = 255;
      record.tileX = 0;
      record.tileZ = 0;
    }
    instances[0].positionY = 256;
    instances[1].positionY = 128;
    instances[1].velocityY = 17;
    actor.instance.slot = 2;
    actor.instance.currentTileX = 0;
    actor.instance.currentTileZ = 0;
    actor.instance.position.vy = 256;
    Actors_actorRecords[2] = &actor;
  }
  ~GimmickStackDropLiveFixture() {
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_Ai_specialTerrainHeights = saved_heights;
  }
  void CheckActorFall() {
    CHECK(actor.instance.movementMode == 3);
    CHECK(actor.instance.rootMotion.vx == 0);
    CHECK(actor.instance.rootMotion.vy == 0);
    CHECK(actor.instance.rootMotion.vz == 0);
    CHECK(actor.instance.rootMotion.pad == 0);
    CHECK(actor.instance.jumpCountdown == 4);
    CHECK(actor.destinationMode == 1);
    CHECK(actor.instance.movementPending == 0);
    CHECK(actor.supportGimmick == 0);
  }
};
}  // namespace

TEST_CASE(
    "Native unsupported stack drop applies real base and actor fall resets",
    "[gimmick-stack-drop-live][field]") {
  GimmickStackDropLiveFixture fixture;
  fixture.instances[1].objectInfo.interaction = 2;
  fixture.actor.instance.rootMotion = {1, 2, 3, 4};
  fixture.actor.instance.movementPending = 1;
  fixture.actor.supportGimmick = 7;
  REQUIRE(VS::Field::DropLiveUnsupportedGimmickStack(3));
  CHECK(fixture.instances[0].objectInfo.isFalling == 1);
  CHECK(fixture.instances[1].velocityY == 0);
  CHECK(fixture.instances[1].mode == 252);
  fixture.CheckActorFall();
}

TEST_CASE(
    "Native unsupported stack drop excludes pointer aliases and preserves "
    "existing fall state",
    "[gimmick-stack-drop-live][field]") {
  GimmickStackDropLiveFixture fixture;
  Field_gimmickInstances[5] = &fixture.instances[1];
  fixture.actor.instance.movementMode = 3;
  fixture.actor.instance.rootMotion = {1, 2, 3, 4};
  fixture.actor.instance.jumpCountdown = 9;
  REQUIRE(VS::Field::DropLiveUnsupportedGimmickStack(3));
  CHECK(fixture.instances[1].objectInfo.carrier == 255);
  CHECK(fixture.actor.instance.rootMotion.vx == 1);
  CHECK(fixture.actor.instance.rootMotion.pad == 4);
  CHECK(fixture.actor.instance.jumpCountdown == 9);
}

TEST_CASE(
    "Native unsupported stack drop refuses absent input and ends safely "
    "without a top",
    "[gimmick-stack-drop-live][field]") {
  GimmickStackDropLiveFixture fixture;
  CHECK_FALSE(VS::Field::DropLiveUnsupportedGimmickStack(-1));
  CHECK_FALSE(VS::Field::DropLiveUnsupportedGimmickStack(16));
  CHECK_FALSE(VS::Field::DropLiveUnsupportedGimmickStack(0));
  Field_gimmickInstances[1] = nullptr;
  fixture.instances[0].parent = 0;
  REQUIRE(VS::Field::DropLiveUnsupportedGimmickStack(3));
  CHECK(fixture.instances[0].objectInfo.isFalling == 1);
  CHECK(fixture.actor.instance.movementMode == 0);
}

TEST_CASE("Unsupported gimmick stack drop retains its original module address",
          "[gimmick-stack-drop-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8009F530) ==
        reinterpret_cast<const void*>(&Field_DropUnsupportedGimmickStackEntry));
}

namespace {
struct GimmickCreationLiveFixture : GimmickReleaseLiveFixture {
  decltype(std::to_array(Field_gimmickModelTemplates)) saved_templates =
      std::to_array(Field_gimmickModelTemplates);
  decltype(std::to_array(Field_gimmickTemplateIndices)) saved_indices =
      std::to_array(Field_gimmickTemplateIndices);
  decltype(std::to_array(Field_gimmickPaletteParameters)) saved_uv =
      std::to_array(Field_gimmickPaletteParameters);
  decltype(Field_roomData) saved_room = Field_roomData;
  VS::Field::CollisionProbe saved_probe = VS::Field::LiveCollisionProbe();
  VS::Field::CollisionSamples saved_samples = VS::Field::LiveCollisionSamples();
  VS::Render::PaletteAnimation saved_animation =
      VS::Render::LivePaletteAnimation();
  GimmickCreationLiveFixture() {
    std::fill_n(Field_gimmickInstances, saved_instances.size(), nullptr);
    std::fill_n(Field_gimmickModelTemplates, saved_templates.size(), nullptr);
    std::fill_n(Field_gimmickTemplateIndices, saved_indices.size(), 0);
    Field_roomData = {};
  }
  ~GimmickCreationLiveFixture() {
    for (const auto& record : instances) {
      VS::Render::ForgetLiveModelExtent(record.model);
    }
    std::copy(saved_templates.begin(), saved_templates.end(),
              Field_gimmickModelTemplates);
    std::copy(saved_indices.begin(), saved_indices.end(),
              Field_gimmickTemplateIndices);
    std::copy(saved_uv.begin(), saved_uv.end(), Field_gimmickPaletteParameters);
    Field_roomData = saved_room;
    VS::Field::LiveCollisionProbe() = saved_probe;
    VS::Field::LiveCollisionSamples() = saved_samples;
    const_cast<VS::Render::PaletteAnimation&>(
        VS::Render::LivePaletteAnimation()) = saved_animation;
  }
  void Template() {
    constexpr int kModelBytes = 96;
    auto* bytes =
        static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(
            sizeof(Field_GimmickTemplate) + kModelBytes));
    REQUIRE(bytes != nullptr);
    if (bytes == nullptr) return;
    Field_GimmickTemplate header{0x03020101, 0x1234, 1, kModelBytes};
    std::memcpy(bytes, &header, sizeof(header));
    std::memset(bytes + sizeof(header), 0, kModelBytes);
    Actors_ShpModel model{};
    model.polygonSection = sizeof(model);
    std::memcpy(bytes + sizeof(header), &model, sizeof(model));
    auto* polygons = bytes + sizeof(header) + sizeof(model);
    polygons[11] = std::byte{0x3C};
    for (const auto offset : {24, 26, 28, 30}) {
      polygons[offset] = std::byte{250};
      polygons[offset + 1] = std::byte{3};
    }
    Field_gimmickModelTemplates[0] = bytes;
    Field_gimmickPaletteParameters[0] = 10;
    Field_gimmickPaletteParameters[1] = 249;
  }
  void CheckModelUvs(const Actors_ShpModel* model) {
    const auto bytes = VS::Render::LiveModelBytes(model);
    if (bytes.size() != 96) {
      FAIL("Created model must retain its complete extent");
      return;
    }
    for (const auto offset : {24, 26, 28, 30}) {
      CHECK(bytes[64 + offset] == std::byte{4});
      CHECK(bytes[64 + offset + 1] == std::byte{252});
    }
  }
  void CheckModelGeometry() {
    auto& record = instances[0];
    REQUIRE(record.model != nullptr);
    CHECK(record.boneCount == 1);
    CHECK(record.reserved4 == 0x1234);
    CHECK(VS::Render::LiveModelBytes(record.model).size() == 96);
    CheckModelUvs(record.model);
  }
};
}  // namespace

TEST_CASE(
    "Native gimmick creation copies a bounded heap model and offsets real "
    "polygon UVs",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  fixture.Template();
  Content_ActorAssetRequest request{};
  request.targetSlot = 2;
  request.destination = &fixture.instances[0];
  REQUIRE(Field_CreateGimmickInstanceEntry(&request) == 0);
  auto& record = fixture.instances[0];
  fixture.CheckModelGeometry();
  CHECK(record.basePalette[0] == 0x3535);
  CHECK(VS::Render::LivePaletteAnimation().colors[13][32] == 0x3535);
  CHECK(VS::Render::LivePaletteAnimation().colors[13][47] == 0x3535);
  CHECK(record.slot == 2);
  REQUIRE(Field_ReleaseGimmickInstanceEntry(2) == 0);
}

TEST_CASE(
    "Native gimmick creation shares prefix but preserves the caller-owned tail",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  fixture.Template();
  Content_ActorAssetRequest request{};
  request.targetSlot = 1;
  request.destination = &fixture.instances[0];
  request.subKind = 3;
  REQUIRE(Field_CreateGimmickInstanceEntry(&request) == 0);
  fixture.instances[0].matrices[0].t[0] = 0x12345678;
  fixture.instances[1].reserved178[0] = 0xA5;
  request.targetSlot = 5;
  request.destination = &fixture.instances[1];
  request.dataAddress = 0x9876;
  REQUIRE(Field_CreateGimmickInstanceEntry(&request) == 0);
  CHECK(request.dataAddress == 1);
  CHECK(fixture.instances[1].model == fixture.instances[0].model);
  CHECK(fixture.instances[1].matrices[0].t[0] == 0x12345678);
  CHECK(fixture.instances[1].reserved178[0] == 0xA5);
  REQUIRE(Field_ReleaseGimmickInstanceEntry(1) == 0);
  REQUIRE(Field_ReleaseGimmickInstanceEntry(5) == 0);
}

TEST_CASE(
    "Native gimmick creation bounds logical template indices without undoing "
    "publication",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  Content_ActorAssetRequest request{};
  request.targetSlot = 2;
  request.destination = &fixture.instances[0];
  request.subKind = 8;
  fixture.instances[0].reserved178[0] = 0xA5;
  CHECK(Field_CreateGimmickInstanceEntry(&request) == -2);
  CHECK(Field_gimmickInstances[2] == &fixture.instances[0]);
  CHECK(fixture.instances[0].model == nullptr);
  CHECK(fixture.instances[0].reserved178[0] == 0xA5);
  Field_gimmickInstances[2] = nullptr;
  request.subKind = 0;
  Field_gimmickTemplateIndices[0] = 3;
  CHECK(Field_CreateGimmickInstanceEntry(&request) == -2);
  CHECK(Field_gimmickInstances[2] == &fixture.instances[0]);
}

TEST_CASE(
    "Native gimmick creation rejects short and misaligned known destinations "
    "before publication",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  auto* destination =
      static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(64));
  REQUIRE(destination != nullptr);
  std::memset(destination, 0xA5, 64);
  Content_ActorAssetRequest request{};
  request.targetSlot = 2;
  request.destination = destination;
  CHECK(Field_CreateGimmickInstanceEntry(&request) == -2);
  CHECK(Field_gimmickInstances[2] == nullptr);
  CHECK(destination[0] == std::byte{0xA5});
  request.destination = destination + 1;
  CHECK(Field_CreateGimmickInstanceEntry(&request) == -2);
  CHECK(Field_gimmickInstances[2] == nullptr);
  CHECK(destination[1] == std::byte{0xA5});
}

TEST_CASE(
    "Native gimmick creation refuses unbounded and incomplete model templates",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Content_ActorAssetRequest request{};
  request.targetSlot = 2;
  request.destination = &fixture.instances[0];
  Field_gimmickModelTemplates[0] = reinterpret_cast<void*>(0x1000);
  CHECK(Field_CreateGimmickInstanceEntry(&request) == -2);
  CHECK(Field_gimmickInstances[2] == &fixture.instances[0]);
  CHECK(fixture.instances[0].model == nullptr);
  Field_gimmickInstances[2] = nullptr;
  auto* source =
      VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Field_GimmickTemplate));
  REQUIRE(source != nullptr);
  Field_GimmickTemplate header{1, 9, 0, 64};
  std::memcpy(source, &header, sizeof(header));
  Field_gimmickModelTemplates[0] = source;
  CHECK(Field_CreateGimmickInstanceEntry(&request) == -2);
  CHECK(fixture.instances[0].reserved4 == 9);
  CHECK(fixture.instances[0].model == nullptr);
}

TEST_CASE(
    "Native gimmick creation C entry accepts unaligned requests and preserves "
    "other bytes",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  fixture.instances[0].objectInfo.objectId = 3;
  Field_gimmickInstances[1] = &fixture.instances[0];
  Content_ActorAssetRequest input{};
  input.targetSlot = 2;
  input.destination = &fixture.instances[1];
  input.subKind = 3;
  input.dataAddress = 0x1234;
  std::array<std::byte, sizeof(input) + 2> bytes{};
  bytes.fill(std::byte{0xA5});
  std::memcpy(bytes.data() + 1, &input, sizeof(input));
  auto expected = bytes;
  const std::uintptr_t shared = 1;
  std::memcpy(
      expected.data() + 1 + offsetof(Content_ActorAssetRequest, dataAddress),
      &shared, sizeof(shared));
  auto* request =
      reinterpret_cast<Content_ActorAssetRequest*>(bytes.data() + 1);
  CHECK(Field_CreateGimmickInstanceEntry(request) == 0);
  CHECK(bytes == expected);
  CHECK(Field_CreateGimmickInstanceEntry(nullptr) == -2);
}

TEST_CASE(
    "Native gimmick creation C placement retains unsigned tile and variant "
    "bytes",
    "[gimmick-create-live][field]") {
  GimmickCreationLiveFixture fixture;
  fixture.instances[0].objectInfo.objectId = 3;
  Field_gimmickInstances[1] = &fixture.instances[0];
  Content_ActorAssetRequest request{};
  request.targetSlot = 2;
  request.destination = &fixture.instances[1];
  request.subKind = 3;
  request.variant = std::bit_cast<char>(std::uint8_t{255});
  const std::uint32_t placement = 0x77FE00FF;
  std::memcpy(&request.placement, &placement, sizeof(placement));
  CHECK(VS::Field::LiveSampleSupportHeightAt(32704, 0, 32576, 0, 0, 1) ==
        VS::Field::kNoSupportHeight);
  REQUIRE(Field_CreateGimmickInstanceEntry(&request) == 0);
  CHECK(fixture.instances[1].positionX == 32704);
  CHECK(fixture.instances[1].positionZ == 32576);
  CHECK(fixture.instances[1].positionY == 29896);
  CHECK(fixture.instances[1].reserved5F == 0);
}

TEST_CASE("Gimmick creation retains its original module address",
          "[gimmick-create-live][field]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8009998C) ==
        reinterpret_cast<const void*>(&Field_CreateGimmickInstanceEntry));
}
