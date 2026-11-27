#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActorAssetResources.hpp"
#include "Menu/StatusTerminalLiveFixture.hpp"

using VS::Core::Gte::RotTransPers;
using VS::Core::Gte::SVECTOR;

namespace {
using status_terminal_live_tests::Fixture;
constexpr VS::Content::ActorAssetOwner kResourceTestOwner{3};
VS::Actors::ActorRecord* resourceTarget = nullptr;

VS::Content::AssetAddress ResourceTargetMemory(
    VS::Content::ActorAssetOwner owner, VS::Content::ActorRecordTable table,
    int slot) {
  return owner == kResourceTestOwner &&
                 table == VS::Content::ActorRecordTable::kActor && slot == 2
             ? Fixture::Address(resourceTarget)
             : VS::Content::AssetAddress::kNone;
}

namespace st = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
namespace memory = VS::Core::Memory;
namespace content = VS::Content;

struct QueuedResourceOwner {
  equipment_detail_tests::ValueRestore<VS::Actors::ActorRecord*> saved_target{
      resourceTarget};
  VS::Actors::ActorRecord* target = nullptr;
  std::vector<content::ActorAssetTicket> tickets;

  QueuedResourceOwner() {
    target = static_cast<VS::Actors::ActorRecord*>(
        memory::AllocateLiveFromLowEnd(sizeof(VS::Actors::ActorRecord)));
    REQUIRE(target != nullptr);
    resourceTarget = target;
  }
  ~QueuedResourceOwner() { Release(); }

  void Submit(const content::ActorAssetRequest& request) {
    const auto ticket = content::SubmitLiveActorAsset(
        request, kResourceTestOwner, ResourceTargetMemory);
    REQUIRE(ticket.has_value());
    if (ticket) tickets.push_back(*ticket);
  }
  void Finish() const {
    for (std::size_t step = 0; step < tickets.size(); ++step) {
      INFO(step);
      REQUIRE(content::UpdateLiveActorAssetQueue() == 1);
    }
    for (const auto ticket : tickets) {
      CHECK(content::LiveActorAssetCompletion(ticket).outcome ==
            content::ActorAssetOutcome::kLoaded);
    }
  }
  void Release() {
    if (target == nullptr) return;
    const bool cancelled =
        content::CancelLiveActorAssetRequests(kResourceTestOwner);
    CHECK(cancelled);
    if (!cancelled) return;
    for (const auto ticket : tickets) content::ForgetLiveActorAsset(ticket);
    tickets.clear();
    if (VS::Actors::actorRecords[2] == target) {
      CHECK(content::ReleaseLiveActorModel(2, false) == 0);
    }
    memory::FreeLiveFromLowEnd(target);
    target = nullptr;
    resourceTarget = nullptr;
  }
};

content::ActorAssetRequest SharedBodyRequest(
    const content::ActorModelResourceRef& model) {
  REQUIRE(model != nullptr);
  return {.kind = static_cast<std::uint8_t>(
              content::ActorAssetKind::kActorModelParse),
          .target_slot = 2,
          .model_id = static_cast<std::int16_t>(model->Definition().model_id),
          .shared_record_slot = {},
          .shared_model = model,
          .sub_kind = 0xFF};
}

struct RetainedResources {
  std::weak_ptr<const content::ActorModelResource> model;
  std::weak_ptr<const content::ActorSequenceResource> common;
  std::weak_ptr<const content::ActorSequenceResource> battle;
  const void* common_bank = nullptr;
  const void* battle_bank = nullptr;
  std::uint8_t texture = 0;

  void CheckLoaded(const QueuedResourceOwner& owner) const {
    REQUIRE(VS::Actors::actorRecords[2] == owner.target);
    CHECK(content::LiveActorModelResource(2) == model.lock());
    CHECK(owner.target->common_sequence == common_bank);
    CHECK(owner.target->battle_sequence == battle_bank);
    CHECK_FALSE(VS::Actors::SequenceBankBytes(common_bank).empty());
    CHECK_FALSE(VS::Actors::SequenceBankBytes(battle_bank).empty());
  }
  void CheckReleased() const {
    CHECK(model.expired());
    CHECK(common.expired());
    CHECK(battle.expired());
    CHECK(VS::Actors::SequenceBankBytes(common_bank).empty());
    CHECK(VS::Actors::SequenceBankBytes(battle_bank).empty());
    CHECK(VS::Render::legacyActorTextureSlots[texture].reference_count == 0);
  }
};

RetainedResources QueueSourceResources(QueuedResourceOwner& owner,
                                       const VS::Actors::ActorRecord& source) {
  const auto sequences = content::LiveActorSequenceResources(0);
  RetainedResources retained{.model = content::LiveActorModelResource(0),
                             .common = sequences.common,
                             .battle = sequences.battle,
                             .common_bank = source.common_sequence,
                             .battle_bank = source.battle_sequence,
                             .texture = source.texture_slot};
  auto request = SharedBodyRequest(retained.model.lock());
  owner.Submit(request);
  request.shared_model.reset();
  request.kind =
      static_cast<std::uint8_t>(content::ActorAssetKind::kSequenceParse);
  request.sub_kind = 0;
  request.model_id = 0;
  request.shared_sequence = sequences.common;
  owner.Submit(request);
  request.model_id = sequences.battle_model;
  request.shared_sequence = sequences.battle;
  owner.Submit(request);
  return retained;
}

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
  CHECK(preview->instance.model != source->instance.model);
  CHECK(VS::Content::LiveActorModelResource(0) ==
        VS::Content::LiveActorModelResource(1));
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
  CHECK(preview->battle_sequence == source->battle_sequence);
  CHECK(VS::Content::LiveActorSequenceResources(0).battle ==
        VS::Content::LiveActorSequenceResources(1).battle);
  CHECK(std::ranges::all_of(
      VS::Content::legacyActorAssetRequests,
      [](const auto& request) { return request.asset_kind == 0; }));
  CHECK(std::ranges::any_of(VS::Render::compactGlyphWidths,
                            [](auto width) { return width != 0; }));
}

TEST_CASE("A preview's vertex displacement does not change its source model",
          "[status-terminal-live][asset-immutable-sharing]") {
  Fixture fixture;
  const auto* source = VS::Actors::actorRecords[0];
  auto* preview = VS::Actors::actorRecords[1];
  REQUIRE(source != nullptr);
  REQUIRE(preview != nullptr);
  const std::span<const std::byte> bytes{
      reinterpret_cast<const std::byte*>(source->instance.model),
      source->model_data_size};
  REQUIRE_FALSE(bytes.empty());
  const std::vector<std::byte> original(bytes.begin(), bytes.end());
  REQUIRE(VS::Render::MoveLiveActorModelVertex(*preview, 0, {1, 2, 3}));
  CHECK(std::ranges::equal(bytes, original));
  const std::span<const std::byte> preview_bytes{
      reinterpret_cast<const std::byte*>(preview->instance.model),
      preview->model_data_size};
  CHECK_FALSE(std::ranges::equal(preview_bytes, original));
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

TEST_CASE(
    "Queued immutable assets outlive their source and ignore legacy snapshots",
    "[status-terminal-live][asset-immutable-sharing][asset-ownership]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  auto* source = VS::Actors::actorRecords[0];
  REQUIRE(source != nullptr);
  QueuedResourceOwner owner;
  const auto resources = QueueSourceResources(owner, *source);
  REQUIRE(content::ReleaseLiveActorModel(0, false) == 0);
  CHECK_FALSE(resources.model.expired());
  CHECK_FALSE(resources.common.expired());
  CHECK_FALSE(resources.battle.expired());
  CHECK(
      VS::Render::legacyActorTextureSlots[resources.texture].reference_count ==
      1);
  content::legacyActorAssetRequests[0] = {};
  owner.Finish();
  resources.CheckLoaded(owner);
  owner.Release();
  resources.CheckReleased();
}

TEST_CASE("A loaded actor is preserved until replacement releases its instance",
          "[status-terminal-live][asset-immutable-sharing][asset-ownership]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  QueuedResourceOwner owner;
  const auto request = SharedBodyRequest(content::LiveActorModelResource(0));
  owner.Submit(request);
  owner.Finish();
  const auto free_units = memory::GameHeap().FreeUnits();
  const auto* geometry = owner.target->instance.model;
  owner.Submit(request);
  CHECK(content::UpdateLiveActorAssetQueue() == -1);
  CHECK(content::LiveActorAssetCompletion(owner.tickets.back()).outcome ==
        content::ActorAssetOutcome::kFailed);
  CHECK(owner.target->instance.model == geometry);
  CHECK(memory::GameHeap().FreeUnits() == free_units);
  REQUIRE(content::ReleaseLiveActorModel(2, false) == 0);
  owner.Submit(request);
  REQUIRE(content::UpdateLiveActorAssetQueue() == 1);
  CHECK(content::LiveActorAssetCompletion(owner.tickets.back()).outcome ==
        content::ActorAssetOutcome::kLoaded);
  CHECK(memory::GameHeap().FreeUnits() == free_units);
}

TEST_CASE(
    "Resource reset rejects stale definitions without double retiring leases",
    "[status-terminal-live][asset-immutable-sharing][session-reset]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  auto model = content::LiveActorModelResource(0);
  REQUIRE(model != nullptr);
  const auto texture = model->Definition().texture_slot;
  content::ResetLiveActorAssetResources();
  const auto references =
      VS::Render::legacyActorTextureSlots[texture].reference_count;
  QueuedResourceOwner owner;
  owner.Submit(SharedBodyRequest(model));
  CHECK(content::UpdateLiveActorAssetQueue() == -1);
  CHECK(content::LiveActorAssetCompletion(owner.tickets.back()).outcome ==
        content::ActorAssetOutcome::kFailed);
  CHECK(VS::Actors::actorRecords[2] == nullptr);
  owner.Release();
  model.reset();
  CHECK(VS::Render::legacyActorTextureSlots[texture].reference_count ==
        references);
  REQUIRE(content::ReleaseLiveActorModel(0, false) == 0);
  CHECK(VS::Render::legacyActorTextureSlots[texture].reference_count == 0);
}

TEST_CASE("Resident texture reload publishes fresh immutable preview defaults",
          "[status-terminal-live][asset-immutable-sharing]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  const auto previous = content::LiveActorModelResource(0);
  const auto* geometry = VS::Actors::actorRecords[0]->instance.model;
  Fixture::ParseAsset(R"(Native\data\disc\OBJ\06.SHP)",
                      {.kind = static_cast<std::uint8_t>(
                           content::ActorAssetKind::kActorModelParse),
                       .target_slot = 0,
                       .model_id = content::kAshleyTextureModelId,
                       .shared_record_slot = {}});
  const auto refreshed = content::LiveActorModelResource(0);
  REQUIRE(previous != nullptr);
  REQUIRE(refreshed != nullptr);
  CHECK(refreshed != previous);
  CHECK(std::ranges::equal(refreshed->Geometry(), previous->Geometry()));
  CHECK(VS::Actors::actorRecords[0]->instance.model == geometry);
  fw::CloneLiveActorPreview(0);
  CHECK(content::LiveActorModelResource(1) == refreshed);
}

TEST_CASE("A full queue abandons partially submitted live preview records",
          "[status-terminal-live][menu-actor-preview][asset-ownership]") {
  Fixture fixture;
  fw::ReleaseLiveActorPreview();
  auto* source = VS::Actors::actorRecords[0];
  REQUIRE(source != nullptr);
  QueuedResourceOwner companion;
  equipment_detail_tests::ValueRestore saved_companion{source->companion_slot};
  source->companion_slot = 2;
  companion.Submit(SharedBodyRequest(content::LiveActorModelResource(0)));
  companion.Finish();
  const auto free_units = memory::GameHeap().FreeUnits();
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

TEST_CASE("Preview sequence binding does not read a changed source record",
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
  REQUIRE(VS::Actors::actorRecords[1] != nullptr);
  CHECK(VS::Actors::actorRecords[1]->battle_sequence !=
        invalid_sequence.data());
  CHECK_FALSE(VS::Actors::SequenceBankBytes(
                  VS::Actors::actorRecords[1]->battle_sequence)
                  .empty());
  fw::ReleaseLiveActorPreview();
  CHECK(memory::GameHeap().FreeUnits() == free_units);
  CHECK(VS::Content::FindLiveFreeActorAssetRequest() == 0);
  CHECK(VS::Content::LiveActorAssetRefusals() == refused);
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
