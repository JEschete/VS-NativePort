#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/StatusTerminalLiveFixture.hpp"

using VS::Core::Gte::RotTransPers;
using VS::Core::Gte::SVECTOR;

namespace {
namespace st = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
namespace memory = VS::Core::Memory;
using status_terminal_live_tests::Fixture;

struct ModuleBindingSnapshot {
  const VS::Content::ModuleSymbol* symbol;
  std::vector<std::byte> bytes;
};

std::vector<ModuleBindingSnapshot> SnapshotModuleBindings() {
  std::vector<ModuleBindingSnapshot> snapshots;
  for (const auto& symbol : VS::Content::ModuleSymbols()) {
    if (symbol.target == nullptr || symbol.nativeSize == 0) continue;
    const auto* begin = static_cast<const std::byte*>(symbol.target);
    snapshots.push_back(
        {.symbol = &symbol, .bytes = {begin, begin + symbol.nativeSize}});
  }
  return snapshots;
}

std::vector<std::string> ChangedModuleBindings(
    const std::vector<ModuleBindingSnapshot>& snapshots) {
  std::vector<std::string> changed;
  for (const auto& snapshot : snapshots) {
    if (std::memcmp(snapshot.bytes.data(), snapshot.symbol->target,
                    snapshot.bytes.size()) != 0) {
      changed.push_back(std::string(snapshot.symbol->module) + ":" +
                        std::to_string(snapshot.symbol->address));
    }
  }
  return changed;
}
}  // namespace

TEST_CASE("Terminal fixture owns production model resources",
          "[status-terminal][status-terminal-live]") {
  Fixture fixture;
  const auto* source = VS::Actors::actorRecords[0];
  const auto* preview = VS::Actors::actorRecords[1];
  REQUIRE(source != nullptr);
  REQUIRE(preview != nullptr);
  CHECK(preview != source);
  CHECK(memory::LivePayloadCapacity(source).has_value());
  CHECK(memory::LivePayloadCapacity(preview).has_value());
  CHECK(preview->instance.bone_count > 0);
  CHECK(VS::Render::ValidateLiveActorModel(*preview));
  CHECK(preview->instance.model == source->instance.model);
}

TEST_CASE("Terminal fixture loads production sequence font and queue resources",
          "[status-terminal][status-terminal-live]") {
  Fixture fixture;
  const auto* source = VS::Actors::actorRecords[0];
  const auto* preview = VS::Actors::actorRecords[1];
  REQUIRE(source != nullptr);
  REQUIRE(preview != nullptr);
  CHECK_FALSE(VS::Actors::SequenceBankBytes(preview->common_sequence).empty());
  CHECK_FALSE(VS::Actors::SequenceBankBytes(preview->battle_sequence).empty());
  CHECK(preview->battle_sequence != source->battle_sequence);
  CHECK(std::ranges::all_of(
      VS::Content::legacyActorAssetRequests,
      [](const auto& request) { return request.asset_kind == 0; }));
  CHECK(std::ranges::any_of(VS::Render::compactGlyphWidths,
                            [](auto width) { return width != 0; }));
}

TEST_CASE("Live preview replacement keeps one owner and releases it once",
          "[status-terminal-live][menu-actor-preview][asset-ownership]") {
  Fixture fixture;
  const auto* source = VS::Actors::actorRecords[0];
  const auto free_units = memory::GameHeap().FreeUnits();
  for (int replacement = 0; replacement < 3; ++replacement) {
    fw::CloneLiveActorPreview(0);
    REQUIRE(VS::Actors::actorRecords[1] != nullptr);
    CHECK(memory::GameHeap().FreeUnits() == free_units);
    CHECK(VS::Actors::actorRecords[0] == source);
  }
  REQUIRE(fw::CleanupLiveActorPreviewOwnedState());
  const auto released_units = memory::GameHeap().FreeUnits();
  REQUIRE(fw::CleanupLiveActorPreviewOwnedState());
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK(memory::GameHeap().FreeUnits() == released_units);
  CHECK(VS::Actors::actorRecords[0] == source);
}

TEST_CASE("A full queue abandons partially submitted live preview records",
          "[status-terminal-live][menu-actor-preview][asset-ownership]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  const auto free_units = memory::GameHeap().FreeUnits();
  auto* source = VS::Actors::actorRecords[0];
  REQUIRE(source != nullptr);
  VS::Actors::ActorRecord companion = *source;
  equipment_detail_tests::ValueRestore saved_companion{source->companion_slot};
  equipment_detail_tests::ValueRestore saved_record{
      VS::Actors::actorRecords[2]};
  source->companion_slot = 2;
  VS::Actors::actorRecords[2] = &companion;
  VS::Content::ActorAssetRequest foreign{
      .kind = static_cast<std::uint8_t>(
          VS::Content::ActorAssetKind::kActorModelParse),
      .target_slot = 3,
      .model_id = VS::Content::kModellessActorId,
      .shared_record_slot = {}};
  for (int slot = 0; slot < VS::Content::kActorAssetRequestCount - 1; ++slot) {
    REQUIRE(VS::Content::QueueLiveActorAsset(foreign) == slot);
  }
  fw::CloneLiveActorPreview(0);
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK(VS::Content::FindLiveFreeActorAssetRequest() == 15);
  CHECK(memory::GameHeap().FreeUnits() == free_units);
  CHECK(VS::Actors::actorRecords[0] == source);
  VS::Content::ClearLiveActorAssetQueue();
}

TEST_CASE("Preview cleanup preserves weapon records it does not own",
          "[status-terminal-live][menu-actor-preview][asset-ownership]") {
  Fixture fixture;
  VS::Actors::WeaponModel foreign{};
  foreign.bone_count = 1;
  equipment_detail_tests::ValueRestore saved_record{
      VS::Actors::weaponModels[2]};
  VS::Actors::weaponModels[2] = &foreign;
  REQUIRE(fw::CleanupLiveActorPreviewOwnedState());
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK(VS::Actors::weaponModels[2] == &foreign);
  CHECK(foreign.bone_count == 1);
}

TEST_CASE("A failed live preview sequence copy releases its loaded model",
          "[status-terminal-live][menu-actor-preview][asset-ownership]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  const auto free_units = memory::GameHeap().FreeUnits();
  auto* source = VS::Actors::actorRecords[0];
  REQUIRE(source != nullptr);
  std::array<std::byte, 8> invalid_sequence{};
  equipment_detail_tests::ValueRestore saved_sequence{source->battle_sequence};
  source->battle_sequence = invalid_sequence.data();
  const auto refused = VS::Content::LiveActorAssetRefusals();
  fw::CloneLiveActorPreview(0);
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK(memory::GameHeap().FreeUnits() == free_units);
  CHECK(VS::Content::FindLiveFreeActorAssetRequest() == 0);
  CHECK(VS::Content::LiveActorAssetRefusals() == refused + 1);
}

TEST_CASE(
    "A queued preview never overwrites a different owner's record",
    "[status-terminal-live][menu-actor-preview][asset-target-ownership]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  const auto free_units = memory::GameHeap().FreeUnits();
  auto* foreign = static_cast<VS::Actors::ActorRecord*>(
      memory::AllocateLiveFromLowEnd(sizeof(VS::Actors::ActorRecord)));
  REQUIRE(foreign != nullptr);
  REQUIRE(VS::Content::QueueLiveActorAsset(
              {.kind = static_cast<std::uint8_t>(
                   VS::Content::ActorAssetKind::kActorModelParse),
               .target_slot = 1,
               .model_id = VS::Content::kModellessActorId,
               .destination = Fixture::Address(foreign),
               .shared_record_slot = {}}) == 0);
  fw::CloneLiveActorPreview(0);
  CHECK(VS::Actors::actorRecords[1] == foreign);
  CHECK(foreign->model_id == VS::Content::kModellessActorId);
  REQUIRE(fw::CleanupLiveActorPreviewOwnedState());
  CHECK(VS::Actors::actorRecords[1] == foreign);
  static_cast<void>(VS::Content::ReleaseLiveActorModel(1));
  memory::FreeLiveFromLowEnd(foreign);
  CHECK(memory::GameHeap().FreeUnits() == free_units);
}

TEST_CASE(
    "Terminal Live rejects forged expired and mismatched workspace borrows",
    "[status-terminal][status-terminal-live]") {
  Fixture fixture;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  std::array<fw::MenuRow, fw::kRowCount> forged{};
  VS::Menu::Framework::menuItems = forged.data();
  CHECK(st::LiveUpdateEquipmentList(-1) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 1);
  VS::Menu::Framework::menuItems = static_cast<fw::MenuRow*>(fixture.workspace);
  ++VS::Menu::Framework::rowTypeBuf;
  CHECK(st::LiveUpdateEquipmentDetails(1) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 2);
  --VS::Menu::Framework::rowTypeBuf;
  memory::FreeLiveFromHighEnd(fixture.workspace);
  CHECK(st::LiveUpdateEquipmentList(-1) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 3);
}

TEST_CASE("F02675 Live uses real transition rows and diagnoses missing actors",
          "[status-terminal][status-terminal-live][F02675]") {
  Fixture fixture;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  REQUIRE(st::LiveUpdateEquipmentDetails(2) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
  CHECK(st::LiveEquipmentDetailControllerState().selected_row == 1);
  CHECK(fw::LiveMenuRow(11).selected == 1);
  CHECK(fw::LiveMenuRow(11).target0 == 155);
  fixture.IdleRows();
  REQUIRE(st::LiveUpdateEquipmentDetails(0) == 0);
  CHECK(st::LiveEquipmentDetailControllerState().phase == 1);
  st::selectedActor = std::numeric_limits<std::int32_t>::min();
  CHECK(st::LiveUpdateEquipmentDetails(0) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 1);
}

TEST_CASE("F02676 Live enters real list phase and resets before repeated entry",
          "[status-terminal][status-terminal-live][F02676]") {
  Fixture fixture;
  VS::Input::settings.cursor_memory = static_cast<char>(0xA7);
  const auto refused = st::RefusedLiveTerminalControllerCount();
  REQUIRE(st::LiveUpdateEquipmentList(0) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
  CHECK(st::LiveEquipmentListControllerState().phase == 1);
  CHECK(static_cast<std::uint8_t>(VS::Input::settings.cursor_memory) == 0xA7);
  st::selectedStatRow = 250;
  st::ResetLiveTerminalControllersModuleData();
  CHECK(st::LiveEquipmentListControllerState() == st::EquipmentListState{});
  CHECK(st::selectedStatRow == 0);
  REQUIRE(st::LiveUpdateEquipmentList(257) == 0);
  CHECK(VS::Menu::Framework::menuMemory.selected_equipment == 2);
}

TEST_CASE("F02677 and F02678 Live draw into the registered frame storage",
          "[status-terminal][status-terminal-live][F02677][F02678]") {
  Fixture fixture;
  st::fadeScreen = 1;
  st::drawBackgroundFirst = 0;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  st::LiveDrawModelSlideFade();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data() + 14);
  CHECK(fixture.primitives[7] == 0x01050060);
  st::LiveDrawBackground();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data() + 51);
  CHECK(fixture.primitives[44] == 0x010500A0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
  VS::Render::legacyOrderingTables[0] = nullptr;
  st::LiveDrawBackground();
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 1);
}

TEST_CASE(
    "F02679 Live uses real preview dimensions and rejects missing preview",
    "[status-terminal][status-terminal-live][F02679]") {
  Fixture fixture;
  st::delayScreenUpdate = 2;
  VS::Actors::actorRecords[1]->collision_height = 32768;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  st::LiveDrawFrame();
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
  CHECK(st::delayScreenUpdate == 1);
  CHECK(VS::Render::FrameScratchpad().camera.look_at.vy == -67108864);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data() + 37);
  {
    equipment_detail_tests::ValueRestore<VS::Actors::ActorRecord*> preview{
        VS::Actors::actorRecords[1]};
    VS::Actors::actorRecords[1] = nullptr;
    st::LiveDrawFrame();
    CHECK(st::RefusedLiveTerminalControllerCount() == refused + 1);
  }
}

TEST_CASE("Terminal fixture leaves the GTE projection at its baseline",
          "[status-terminal][status-terminal-live][fixture-isolation]") {
  equipment_detail_tests::ValueRestore<decltype(VS::Render::projectionDistance)>
      projection{VS::Render::projectionDistance};
  VS::Core::Gte::Reset();
  VS::Render::projectionDistance = 0;
  SVECTOR point{256, 0, 1024, 0};
  VS::Core::Gte::Word before{};
  VS::Core::Gte::Word after{};
  VS::Core::Gte::Word cue{};
  VS::Core::Gte::Word flags{};
  RotTransPers(&point, &before, &cue, &flags);
  REQUIRE(before != 0);
  {
    equipment_detail_tests::ValueRestore<decltype(st::delayScreenUpdate)> delay{
        st::delayScreenUpdate};
    Fixture fixture;
    st::delayScreenUpdate = 2;
    st::LiveDrawFrame();
  }
  RotTransPers(&point, &after, &cue, &flags);
  CHECK(after == before);
  CHECK(VS::Render::projectionDistance == 0);
}

TEST_CASE(
    "F02682 Live accepts only owner state bytes and current text allocation",
    "[status-terminal][status-terminal-live][F02682]") {
  Fixture fixture;
  std::uint8_t forged = 3;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  CHECK(st::LiveUpdateStatusTerminal(&forged) == 0);
  CHECK(forged == 3);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 1);
  *fixture.State() = 3;
  ++VS::Menu::Framework::itemHelp;
  CHECK(st::LiveUpdateStatusTerminal(fixture.State()) == 0);
  CHECK(*fixture.State() == 3);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 2);
}

TEST_CASE("Terminal Live rejects expired item text and preview allocations",
          "[status-terminal][status-terminal-live]") {
  Fixture fixture;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  memory::FreeLiveFromHighEnd(fixture.text);
  *fixture.State() = 3;
  CHECK(st::LiveUpdateStatusTerminal(fixture.State()) == 0);
  CHECK(*fixture.State() == 3);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 1);
  auto* expired_preview = VS::Actors::actorRecords[1];
  fw::ReleaseLiveActorPreview();
  VS::Actors::actorRecords[1] = expired_preview;
  st::LiveDrawFrame();
  CHECK(st::RefusedLiveTerminalControllerCount() == refused + 2);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.primitives.data());
  VS::Actors::actorRecords[1] = nullptr;
}

TEST_CASE(
    "F02682 Live initialization saves state and reload never restores camera",
    "[status-terminal][status-terminal-live][F02682]") {
  Fixture fixture;
  VS::Render::projectionDistance = 333;
  VS::Render::FrameScratchpad().camera.angles.pad = 77;
  *fixture.State() = 3;
  const auto refused = st::RefusedLiveTerminalControllerCount();
  REQUIRE(st::LiveUpdateStatusTerminal(fixture.State()) == 0);
  CHECK(st::RefusedLiveTerminalControllerCount() == refused);
  CHECK(*fixture.State() == 5);
  CHECK(st::LiveStatusTerminalControllerState().saved_projection == 333);
  CHECK(st::LiveStatusTerminalControllerState().screen_enabled == 1);
  const auto current = st::terminal_live::LoadView();
  st::ResetLiveTerminalControllersModuleData();
  CHECK(st::LiveStatusTerminalControllerState() == st::StatusTerminalState{});
  CHECK(st::terminal_live::LoadView() == current);
  CHECK(VS::Render::projectionDistance == 512);
}

TEST_CASE("Terminal fixture restores production module bindings",
          "[status-terminal][status-terminal-live]") {
  const auto snapshots = SnapshotModuleBindings();
  const void* model = nullptr;
  const void* sequence = nullptr;
  {
    Fixture fixture;
    *fixture.State() = 3;
    REQUIRE(st::LiveUpdateStatusTerminal(fixture.State()) == 0);
    model = VS::Actors::actorRecords[0]->instance.model;
    sequence = VS::Actors::actorRecords[1]->battle_sequence;
  }
  const auto changed = ChangedModuleBindings(snapshots);
  CAPTURE(changed);
  CHECK(changed.empty());
  CHECK(VS::Render::LiveModelBytes(model).empty());
  CHECK(VS::Actors::SequenceBankBytes(sequence).empty());
}

TEST_CASE("MENU4 terminal reload preserves BATTLE limb and MAINMENU ownership",
          "[status-terminal][status-terminal-live]") {
  Fixture fixture;
  REQUIRE(st::LiveUpdateEquipmentDetails(2) == 0);
  REQUIRE(st::LiveUpdateLimbPanel(-1) == 0);
  const auto limb = st::LiveLimbPanelControllerState();
  st::selectedElement = 4;
  VS::Menu::Framework::menuMemory.limb_selection_and_dimming = 0x85;
  VS::Battle::stringContext.strings[0] = VS::Menu::Framework::stringBuf;
  st::ResetLiveTerminalControllersModuleData();
  CHECK(st::LiveEquipmentDetailControllerState() == st::EquipmentDetailState{});
  CHECK(st::LiveLimbPanelControllerState().phase == limb.phase);
  CHECK(st::LiveLimbPanelControllerState().actor_slot == limb.actor_slot);
  CHECK(st::LiveLimbPanelControllerState().animation_steps ==
        limb.animation_steps);
  CHECK(st::selectedElement == 4);
  CHECK(VS::Menu::Framework::menuMemory.limb_selection_and_dimming == 0x85);
}

TEST_CASE("Terminal reload retains the actual allocations and text arguments",
          "[status-terminal][status-terminal-live]") {
  Fixture fixture;
  VS::Battle::stringContext.strings[0] = VS::Menu::Framework::stringBuf;
  st::ResetLiveTerminalControllersModuleData();
  CHECK(memory::LivePayloadCapacity(fixture.workspace).has_value());
  CHECK(memory::LivePayloadCapacity(fixture.text).has_value());
  CHECK(VS::Battle::stringContext.strings[0] == VS::Menu::Framework::stringBuf);
}
