#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/StringTables.hpp"
#include "Menu/GazetteTerminalLiveFixture.hpp"
#include "Menu/GazetteTerminalPreviewFixture.hpp"
#include "State/Progress.hpp"

namespace menu9_misc_info = VS::Content::text::menu9_prg::misc_info;

namespace {
namespace live = gazette_terminal_live_tests;
namespace shared = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;

struct ForeignQueue {
  std::array<std::array<std::byte, 2048>, 32> buffers{};
  std::array<VS::Content::DiscQueueSlot*, 32> slots{};
  std::array<std::uint64_t, 32> generations{};
  std::vector<std::byte> snapshot;
  VS::Content::DiscRequestCounts counts{};

  ForeignQueue() = default;
  ForeignQueue(const ForeignQueue&) = delete;
  ForeignQueue& operator=(const ForeignQueue&) = delete;
  ~ForeignQueue() {
    for (std::size_t index = 0; index < slots.size(); ++index) {
      if (slots[index] == nullptr) continue;
      static_cast<void>(live::content::CancelLiveDiscWrites(buffers[index]));
      if (live::content::LiveDiscRequestGeneration(slots[index]) ==
          generations[index]) {
        live::content::ReleaseLiveDiscRequest(slots[index]);
      }
    }
  }
};

void FillForeignQueue(ForeignQueue& foreign) {
  const VS::Content::DiscFile file{score::kMonBinFile.lba, 2048};
  for (std::size_t index = 0; index < foreign.slots.size(); ++index) {
    foreign.slots[index] = live::content::AllocateLiveDiscRequest(&file);
    REQUIRE(foreign.slots[index] != nullptr);
    if ((index & 1) != 0) {
      live::content::EnqueueLiveDiscRequest(foreign.slots[index],
                                            foreign.buffers[index].data());
    }
    foreign.generations[index] =
        live::content::LiveDiscRequestGeneration(foreign.slots[index]);
    REQUIRE(foreign.generations[index] != 0);
  }
  const auto bytes = live::BytesOf(VS::Content::discRequestSlots);
  foreign.snapshot.assign(bytes.begin(), bytes.end());
  foreign.counts = VS::Content::discRequestCounts;
}

void CheckForeignQueue(const ForeignQueue& foreign) {
  CHECK(std::ranges::equal(live::BytesOf(VS::Content::discRequestSlots),
                           foreign.snapshot));
  CHECK(VS::Content::discRequestCounts.reading == foreign.counts.reading);
  CHECK(VS::Content::discRequestCounts.waiting == foreign.counts.waiting);
  for (std::size_t index = 0; index < foreign.slots.size(); ++index) {
    CHECK(live::content::LiveDiscRequestGeneration(foreign.slots[index]) ==
          foreign.generations[index]);
    CHECK(std::ranges::all_of(foreign.buffers[index], [](std::byte byte) {
      return byte == std::byte{};
    }));
  }
}

void ReleaseForeignQueue(ForeignQueue& foreign) {
  for (std::size_t index = 0; index < foreign.slots.size(); ++index) {
    REQUIRE(live::content::CancelLiveDiscWrites(foreign.buffers[index]));
    live::content::ReleaseLiveDiscRequest(foreign.slots[index]);
    foreign.slots[index] = nullptr;
  }
}

TEST_CASE(
    "F02805 Live copies unlocked and locked descriptions through the real "
    "information box",
    "[gazette-terminal][gazette-terminal-live][F02805]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  fw::InitializeLiveInformationBox();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::selectedEnemy = 0;
  live::Fixture::UnlockAll();
  score::ShowLiveEnemyDescription();
  CHECK(shared::Fixture::InformationText() == live::Fixture::Description(0));
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, 0);
  score::InitializeLiveEnemyList();
  score::ShowLiveEnemyDescription();
  CHECK(shared::Fixture::InformationText() ==
        shared::Label(score::miscInfo,
                      menu9_misc_info::index::kEnemyNotDefeated));
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void CheckDigitGeometry(std::span<const std::uint32_t> packet, int x) {
  CHECK(packet[2] == fw::PackXy(x, 26));
  CHECK(packet[4] == fw::PackXy(x + 6, 26));
  CHECK(packet[6] == fw::PackXy(x, 36));
  CHECK(packet[8] == fw::PackXy(x + 6, 36));
}

void CheckDigitPacket(std::span<const std::uint32_t> packet, std::size_t index,
                      bool dimmed) {
  REQUIRE(packet.size() == 10);
  CHECK(packet[0] == 0x09000000);
  CHECK(packet[1] == (dimmed ? 0x2E202020U : 0x2E808080U));
  CheckDigitGeometry(packet, 34 + static_cast<int>(index) * 5);
  const std::array<unsigned, 3> digits{1, 2, 7};
  if (index >= digits.size()) {
    throw std::runtime_error("digit packet index outside the three digits");
  }
  CHECK(packet[3] ==
        ((dimmed ? 0x37F70000U : 0x37F40000U) | digits[index] * 6));
  CHECK(packet[5] ==
        ((dimmed ? 0x002C0000U : 0x000C0000U) | (digits[index] + 1) * 6));
}

TEST_CASE(
    "F02817 Live queues three complete digit quads on the real overlay chain",
    "[gazette-terminal][gazette-terminal-live][F02817]") {
  live::Fixture fixture;
  const bool dimmed = GENERATE(false, true);
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::DrawLiveEnemyNumber(20, 30, 127, dimmed ? 1 : 0);
  REQUIRE(fixture.UsedWords() == 30);
  const auto packets = fixture.Packets();
  REQUIRE(packets.size() == 3);
  for (std::size_t i = 0; i < packets.size(); ++i) {
    CheckDigitPacket(packets[i], i, dimmed);
  }
  CHECK((fixture.ordering[shared::kSceneHead] & shared::kLinkMask) !=
        (fixture.ordering[shared::kOverlayHead] & shared::kLinkMask));
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void CheckUnlockedRing() {
  CHECK(score::monBinData[0].unlocked == 1);
  CHECK(score::monBinData[1].unlocked == 1);
  CHECK(score::monBinData[77].unlocked == 1);
  CHECK(score::monBinData[0].prev == 77);
  CHECK(score::monBinData[0].next == 1);
  CHECK(score::monBinData[1].next == 77);
  CHECK(score::monBinData[77].next == 0);
}

TEST_CASE(
    "F02818 Live decodes real MON ranges and writes only unlocked ring bytes",
    "[gazette-terminal][gazette-terminal-live][F02818]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, 0);
  VS::State::scoreData.enemy_kill_flags[0] = (1 << 2) | (1 << 6);
  VS::State::scoreData.enemy_kill_flags[4] = 1 << 8;
  const auto source = live::Fixture::MonBytes();
  std::vector<std::byte> before(source.begin(), source.end());
  score::InitializeLiveEnemyList();
  CheckUnlockedRing();
  for (std::size_t offset = 0; offset < before.size(); ++offset) {
    const bool link = offset < 78 * live::kEnemyStride &&
                      offset % live::kEnemyStride >= 12 &&
                      offset % live::kEnemyStride <= 14;
    if (link) before[offset] = source[offset];
  }
  CHECK(std::ranges::equal(source, before));
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
}

void CheckRenderedEnemyRows() {
  CHECK(score::monBinData[0].selected == 1);
  CHECK(score::monBinData[0].animation_state == 8);
  CHECK(score::monBinData[1].selected == 0);
  CHECK(shared::Fixture::Row(0).state == fw::RowState::kInactive);
  auto last_name = live::Fixture::EnemyName(7);
  last_name.back() = 0xFF;
  CHECK(std::ranges::equal(
      std::span(shared::Fixture::Row(0).text).first(last_name.size()),
      last_name));
}

TEST_CASE(
    "F02819 Live renders eight real rows digits cursor and copied information",
    "[gazette-terminal][gazette-terminal-live][F02819]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  fw::InitializeLiveInformationBox();
  Battle_unusedMenu9CursorMemory.encyclopaedia_page = 0;
  Battle_unusedMenu9CursorMemory.encyclopaedia_row = 0;
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::RenderLiveEnemyList(8);
  REQUIRE(fixture.UsedWords() > 240);
  const auto packets = fixture.Packets();
  CHECK(std::ranges::count_if(packets, [](const auto& packet) {
          return packet.size() == 10 && (packet[1] >> 24) == 0x2E;
        }) == 24);
  CHECK(fixture.Cursor().position == fw::PackXy(48, 48));
  CHECK(shared::Fixture::InformationText() == live::Fixture::Description(0));
  CheckRenderedEnemyRows();
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void CheckViewerLabels(const live::Fixture& fixture) {
  CHECK(fixture.Text("NO.   001/078", 160).X() == 8);
  CHECK(fixture.Text("ROTATION", 18).X() == 28);
  CHECK(fixture.Text("ZOOM", 36).X() == 28);
  CHECK(fixture.Text("UNDEAD", 155).X() == 188);
  CHECK(shared::Fixture::Row(0).state == fw::RowState::kInactive);
  auto name = live::Fixture::EnemyName(0, true);
  name.back() = 0xFF;
  CHECK(std::ranges::equal(
      std::span(shared::Fixture::Row(0).text).first(name.size()), name));
}

TEST_CASE(
    "F02820 Live viewer emits real font controls identification and panels",
    "[gazette-terminal][gazette-terminal-live][F02820]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  score::selectedEnemy = 0;
  score::rotationColor = 0x808080;
  score::zoomColor = 0x808080;
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::RenderLiveEnemyViewer(8, 3);
  CheckViewerLabels(fixture);
  CHECK(fixture.Packets().size() > 40);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02834 Live navigation emits its real arrow and directional font",
          "[gazette-terminal][gazette-terminal-live][F02834]") {
  live::Fixture fixture;
  const int direction = GENERATE(0, 1, 2, 3);
  score::enemyNavigationState = 0;
  score::hideUi = 0;
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::DrawLiveEnemyNavigation(direction);
  const auto packets = fixture.Packets();
  const auto quad = std::ranges::find_if(
      packets, [](const auto& packet) { return packet.size() == 10; });
  REQUIRE(quad != packets.end());
  CHECK((*quad)[1] == 0x2DADBEEF);
  CHECK(((*quad)[3] >> 16) == (direction < 2 ? 0x37F8 : 0x37F9));
  if ((direction & 1) == 0) {
    CHECK(fixture.Text("L1", 98).X() == 34);
  } else {
    CHECK(fixture.Text("R1", 98).Right() == 286);
  }
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void LoadFirstEnemy(live::PreviewFixture& fixture) {
  REQUIRE(fixture.TickEnemyPreview(score::monBinData[0].zud_id, false) == 1);
  const auto pending = std::ranges::find(
      VS::Content::discRequestSlots, VS::Content::DiscRequestState::kEnqueued,
      &VS::Content::DiscQueueSlot::state);
  REQUIRE(pending != std::end(VS::Content::discRequestSlots));
  CHECK(live::content::LiveDiscRequestGeneration(&*pending) != 0);
  fixture.LoadEnemy(score::monBinData[0].zud_id);
  CHECK(live::content::LiveDiscRequestGeneration(&*pending) == 0);
}

void CheckLoadedEnemyGeometry() {
  REQUIRE(Actors_actorRecords[1] != nullptr);
  REQUIRE(Actors_actorRecords[1]->instance.boneCount > 0);
  REQUIRE(Actors_actorRecords[1]->instance.triangleCount +
              Actors_actorRecords[1]->instance.quadCount >
          0);
}

void CheckRenderedEnemyModel(const live::PreviewFixture& fixture) {
  const auto projection = VS::Render::GetLiveProjectionScreenOffset();
  CHECK(projection.x == 127);
  CHECK(projection.y == 93);
  CHECK(Actors_actorRecords[1]->framesPerStep == 2);
  REQUIRE(fixture.UsedWords() > 0);
  CHECK_FALSE(fixture.Packets().empty());
  CHECK(std::ranges::any_of(fixture.Packets(), [](const auto& packet) {
    return packet.size() > 4 && ((packet[1] >> 24) & 0xE4U) == 0x24U;
  }));
}

TEST_CASE(
    "F02820 Live viewer draws a genuinely loaded enemy model and restores "
    "projection",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
    "F02820]") {
  live::PreviewFixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  const auto assets_before = live::content::LiveActorAssetRefusals();
  LoadFirstEnemy(fixture);
  CheckLoadedEnemyGeometry();
  CHECK(live::content::LiveActorAssetRefusals() == assets_before);
  score::cameraDistance = 1500;
  score::hideUi = 1;
  VS::Render::SetLiveProjectionScreenOffset({127, 93});
  fixture.ResetFrame();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::RenderLiveEnemyViewer(8, 0);
  CheckRenderedEnemyModel(fixture);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void CheckOverviewActor() {
  REQUIRE(Actors_actorRecords[1] != nullptr);
  CHECK(Actors_actorRecords[1]->animationId == 33);
  CHECK(Actors_actorRecords[1]->instance.weaponDrawn == 0);
  CHECK(VS::Menu::textBoxes[0].text != nullptr);
}

void InitializeOverviewForCheck() {
  REQUIRE(score::UpdateLiveGazette() == 0);
  REQUIRE(score::menuState == 2);
  CheckOverviewActor();
  CHECK(live::Fixture::RankText() == shared::Label(score::rankText, 0));
  CHECK(score::cameraDistance == 336);
  CHECK(score::cameraHeightOffset == 107);
}

TEST_CASE("F02803 Live prepares the real overview rank and actor preview",
          "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
          "F02803]") {
  live::PreviewFixture fixture;
  const auto assets_before = live::content::LiveActorAssetRefusals();
  fixture.LoadSourceActor();
  REQUIRE(Actors_actorRecords[0] != nullptr);
  REQUIRE(Actors_actorRecords[0]->instance.boneCount > 0);
  score::menuState = 1;
  const auto refusals = score::LiveGazetteTerminalRefusals();
  InitializeOverviewForCheck();
  fixture.ResetFrame();
  REQUIRE(score::UpdateLiveGazette() == 0);
  CHECK(fixture.UsedWords() > 0);
  CHECK(fixture.Packets().size() > 10);
  CHECK(live::content::LiveActorAssetRefusals() == assets_before);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void CheckMonEntry(live::PreviewFixture& fixture) {
  fixture.SelectPage(3);
  REQUIRE(score::monBinCdQueueSlot != nullptr);
  CHECK(score::enemyDescriptions == nullptr);
  CHECK(live::content::LiveDiscRequestGeneration(score::monBinCdQueueSlot) !=
        0);
  fixture.AdvanceTo(14);
  CHECK(score::monBinCdQueueSlot == nullptr);
  CHECK(score::monBinData[0].zud_id == 4);
  CHECK(score::monBinData[0].unlocked == 1);
  CHECK(fixture.InformationText() == shared::EncodedText{0xE7});
}

void SettleFirstEnemyList(live::PreviewFixture& fixture) {
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(fixture.InformationText() == live::Fixture::Description(0));
  for (int tick = 0; tick < 12; ++tick) {
    REQUIRE(fixture.TickGazette() == 0);
  }
  auto last_name = live::Fixture::EnemyName(7);
  last_name.back() = 0xFF;
  CHECK(std::ranges::equal(
      std::span(fixture.Row(0).text).first(last_name.size()), last_name));
}

void OpenControlledViewer(live::PreviewFixture& fixture) {
  live::Fixture::Input(live::kCircle);
  REQUIRE(fixture.TickGazette() == 0);
  REQUIRE(score::menuState == 15);
  fixture.AdvanceTo(20);
  REQUIRE(Actors_actorRecords[1] != nullptr);
  CHECK(score::selectedEnemy == 0);
  CHECK(score::cameraDistance > 0);
  const auto preview_image =
      shared::ReadImage(R"(Native\data\disc\MAP\Z009U16.ZUD)");
  CHECK(Actors_actorRecords[1]->modelId == preview_image.front());
}

void CheckViewerSlide(live::PreviewFixture& fixture) {
  for (int tick = 0; tick < 8; ++tick) {
    REQUIRE(fixture.TickGazette() == 0);
  }
  CHECK(fixture.Text("NO.   001/078", 160).X() == -8);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(fixture.Text("NO.   001/078", 160).X() == 8);
  REQUIRE(fixture.Packets().size() > 25);
}

void RotateAndZoomViewer(live::PreviewFixture& fixture) {
  const auto yaw = score::cameraAngles.vy;
  live::Fixture::Input(0, 0, 0x2001);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(score::cameraAngles.vy == yaw + 64);
  const auto distance = score::cameraDistance;
  REQUIRE(distance > 640);
  live::Fixture::Input(0, 0, 0x1002);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(score::cameraDistance == distance - 64);
}

void HideViewerUi(live::PreviewFixture& fixture) {
  live::Fixture::Input(0x100);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(score::hideUi == 1);
  CHECK(VS::Menu::Framework::hideMenu == 1);
  const auto hidden_text = fixture.TextRuns();
  CHECK(std::ranges::none_of(hidden_text, [](const auto& run) {
    return run.text.starts_with("NO.");
  }));
}

void ShowViewerUi(live::PreviewFixture& fixture) {
  live::Fixture::Input(0x100);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(score::hideUi == 0);
  CHECK(VS::Menu::Framework::hideMenu == 0);
}

void BeginEnemySwitch(live::PreviewFixture& fixture) {
  live::Fixture::Input(8, 8, 8);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(score::selectedEnemy == 1);
  CHECK(fixture.InformationText() == live::Fixture::Description(1));
  live::Fixture::Input(0, 0, 0, 8);
  REQUIRE(fixture.TickGazette() == 0);
  REQUIRE(score::menuState == 25);
  fixture.AdvanceTo(26);
  CHECK(Actors_actorRecords[1] == nullptr);
}

void FinishEnemySwitch(live::PreviewFixture& fixture) {
  fixture.AdvanceTo(20);
  const auto replacement_image =
      shared::ReadImage(R"(Native\data\disc\MAP\Z013U15.ZUD)");
  REQUIRE(Actors_actorRecords[1] != nullptr);
  CHECK(Actors_actorRecords[1]->modelId == replacement_image.front());
  CHECK(score::selectedEnemy == 1);
  CHECK(fixture.InformationText() == live::Fixture::Description(1));
  CHECK(live::AssetsIdle());
  CHECK(fixture.Text("NO.   002/078", 160).X() == 8);
}

void ReturnViewerToList(live::PreviewFixture& fixture) {
  live::Fixture::Input(live::kCross);
  REQUIRE(fixture.TickGazette() == 0);
  REQUIRE(score::menuState == 21);
  fixture.AdvanceTo(14);
  CHECK(Actors_actorRecords[1] == nullptr);
  CHECK(fixture.InformationText() == live::Fixture::Description(0));
  REQUIRE(score::monBinData != nullptr);
}

void ReturnListToOverview(live::PreviewFixture& fixture) {
  live::Fixture::Input(live::kCross);
  REQUIRE(fixture.TickGazette() == 0);
  fixture.AdvanceTo(1);
  CHECK(score::monBinData == nullptr);
  CHECK(score::enemyDescriptions == nullptr);
  fixture.SettleOverview();
  CHECK(VS::Menu::textBoxes[0].text != nullptr);
}

TEST_CASE(
    "F02803 Live opens MON and an actual preview through controller input",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-control]["
    "F02803]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
  const auto refusals = score::LiveGazetteTerminalRefusals();
  CheckMonEntry(fixture);
  SettleFirstEnemyList(fixture);
  OpenControlledViewer(fixture);
  CheckViewerSlide(fixture);
  RotateAndZoomViewer(fixture);
  HideViewerUi(fixture);
  ShowViewerUi(fixture);
  BeginEnemySwitch(fixture);
  FinishEnemySwitch(fixture);
  ReturnViewerToList(fixture);
  ReturnListToOverview(fixture);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

struct CancellationProbe {
  VS::Content::DiscQueueSlot* owned_request = nullptr;
  std::uint64_t old_generation = 0;
  void* owned_buffer = nullptr;
  std::size_t owned_bytes = 0;
  std::uint64_t read_generation = 0;
  std::array<std::byte, 2048> foreign_buffer{};
  VS::Content::DiscQueueSlot* foreign = nullptr;
  std::uint64_t foreign_generation = 0;
  std::vector<std::byte> foreign_snapshot;

  CancellationProbe() = default;
  CancellationProbe(const CancellationProbe&) = delete;
  CancellationProbe& operator=(const CancellationProbe&) = delete;
};

void ArmMonCancellation(live::PreviewFixture& fixture,
                        CancellationProbe& probe) {
  fixture.SelectPage(3, false);
  probe.owned_request = score::monBinCdQueueSlot;
  REQUIRE(probe.owned_request != nullptr);
  probe.old_generation =
      live::content::LiveDiscRequestGeneration(probe.owned_request);
  REQUIRE(probe.old_generation != 0);
  probe.owned_buffer = score::monBinData;
  probe.owned_bytes = live::Fixture::MonBytes().size();
  REQUIRE(live::content::BeginLiveDiscRead(
      score::kMonBinFile.lba,
      static_cast<std::uint32_t>(score::kMonBinFile.size), probe.owned_buffer));
  probe.read_generation = live::content::LiveDiscReadGeneration();
  REQUIRE_FALSE(live::content::LiveDiscReadCompleted(probe.read_generation));
  REQUIRE(VS::Content::discDriveState.destination == probe.owned_buffer);
}

void QueueForeignCancellation(CancellationProbe& probe, bool reuse_request) {
  if (reuse_request) live::content::ReleaseLiveDiscRequest(probe.owned_request);
  const VS::Content::DiscFile foreign_file{score::kMonBinFile.lba, 2048};
  probe.foreign = live::content::AllocateLiveDiscRequest(&foreign_file);
  REQUIRE(probe.foreign != nullptr);
  if (reuse_request) CHECK(probe.foreign == probe.owned_request);
  live::content::EnqueueLiveDiscRequest(probe.foreign,
                                        probe.foreign_buffer.data());
  probe.foreign_generation =
      live::content::LiveDiscRequestGeneration(probe.foreign);
  CHECK(probe.foreign_generation != probe.old_generation);
  const auto bytes = live::BytesOf(*probe.foreign);
  probe.foreign_snapshot.assign(bytes.begin(), bytes.end());
}

void SnapshotForeignCancellation(CancellationProbe& probe, bool reuse_request) {
  live::fw::ReleaseLiveActorPreview();
  CHECK(std::ranges::equal(live::BytesOf(*probe.foreign),
                           probe.foreign_snapshot));
  if (!reuse_request) {
    // Removing the preceding queued read compacts only the foreign priority.
    const auto priority =
        static_cast<std::int16_t>(probe.foreign->priority - 1);
    std::memcpy(probe.foreign_snapshot.data() +
                    offsetof(VS::Content::DiscQueueSlot, priority),
                &priority, sizeof(priority));
  }
}

void ResetCancelledMon(const CancellationProbe& probe) {
  REQUIRE(score::ResetLiveGazetteTerminalModuleState());
  CHECK(score::monBinData == nullptr);
  CHECK(score::monBinCdQueueSlot == nullptr);
  CHECK(score::enemyDescriptions == nullptr);
  CHECK(score::menuState == 0);
  CHECK_FALSE(
      live::memory::LivePayloadCapacity(probe.owned_buffer).has_value());
}

void CheckCancellationEffects(const CancellationProbe& probe) {
  CHECK(std::ranges::equal(live::BytesOf(*probe.foreign),
                           probe.foreign_snapshot));
  CHECK(live::content::LiveDiscRequestGeneration(probe.foreign) ==
        probe.foreign_generation);
  CHECK(VS::Content::discDriveState.destination == nullptr);
  CHECK_FALSE(live::content::LiveDiscReadCompleted(probe.read_generation));
}

std::span<std::byte> PoisonCancelledMon(const CancellationProbe& probe) {
  auto* const replacement = static_cast<std::byte*>(
      live::memory::AllocateLiveFromHighEnd(probe.owned_bytes));
  REQUIRE(replacement == reinterpret_cast<std::byte*>(probe.owned_buffer));
  const auto poison = std::span(replacement, probe.owned_bytes);
  std::ranges::fill(poison, std::byte{0x5A});
  live::content::UpdateLiveDiscCommands();
  CHECK(std::ranges::all_of(
      poison, [](std::byte byte) { return byte == std::byte{0x5A}; }));
  return poison;
}

void CheckForeignCompletion(const CancellationProbe& probe,
                            std::span<const std::byte> poison) {
  for (std::size_t tick = 0;
       tick < live::kFrameBudget &&
       probe.foreign->state != VS::Content::DiscRequestState::kLoaded;
       ++tick) {
    live::content::UpdateLiveDiscQueue();
  }
  REQUIRE(probe.foreign->state == VS::Content::DiscRequestState::kLoaded);
  const auto mon_image = shared::ReadImage(R"(Native\data\disc\SMALL\MON.BIN)");
  CHECK(std::ranges::equal(probe.foreign_buffer,
                           std::as_bytes(std::span(mon_image)).first(2048)));
  CHECK(live::content::LiveDiscRequestGeneration(probe.foreign) ==
        probe.foreign_generation);
  CHECK(std::ranges::all_of(
      poison, [](std::byte byte) { return byte == std::byte{0x5A}; }));
}

void ReenterCancelledMon(live::PreviewFixture& fixture) {
  fixture.SelectPage(3);
  fixture.AdvanceTo(14);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK(fixture.InformationText() == live::Fixture::Description(0));
  CHECK(score::monBinData[0].unlocked == 1);
  live::Fixture::Input(live::kCross);
  REQUIRE(fixture.TickGazette() == 0);
  fixture.AdvanceTo(1);
  CHECK(score::monBinData == nullptr);
}

TEST_CASE(
    "F02803 Live cancellation rejects a stale request identity and permits "
    "real reentry",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-cancel]["
    "F02803]") {
  const bool reuse_request = GENERATE(false, true);
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
  const auto refusals = score::LiveGazetteTerminalRefusals();
  CancellationProbe probe;
  ArmMonCancellation(fixture, probe);
  QueueForeignCancellation(probe, reuse_request);
  SnapshotForeignCancellation(probe, reuse_request);
  ResetCancelledMon(probe);
  CheckCancellationEffects(probe);
  const auto poison = PoisonCancelledMon(probe);
  CheckForeignCompletion(probe, poison);
  live::content::ReleaseLiveDiscRequest(probe.foreign);
  live::memory::FreeLiveFromHighEnd(poison.data());
  ReenterCancelledMon(fixture);
  CHECK(live::content::LiveRefusedDiscRecordsInUse() == 0);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

void DrawAndNavigatePage(live::PreviewFixture& fixture) {
  for (int tick = 0; tick < 32; ++tick) {
    REQUIRE(fixture.TickGazette() == 0);
  }
  REQUIRE(fixture.InformationText().size() > 1);
  REQUIRE(fixture.Packets().size() > 20);
  const auto cursor = fixture.Cursor();
  live::Fixture::Input(live::kDown, live::kDown);
  REQUIRE(fixture.TickGazette() == 0);
  CHECK((fixture.Cursor().position >> 16) == (cursor.position >> 16) + 16);
}

TEST_CASE(
    "F02803 Live draws each dependent Gazette page and returns through real "
    "controls",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-pages]["
    "F02803]") {
  const int page = GENERATE(0, 1, 2);
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  fixture.SelectPage(page);
  fixture.AdvanceTo(5 + page * 3);
  DrawAndNavigatePage(fixture);
  live::Fixture::Input(live::kCross);
  REQUIRE(fixture.TickGazette() == 0);
  fixture.AdvanceTo(1);
  fixture.SettleOverview();
  CHECK(live::Fixture::RankText() == shared::Label(score::rankText, 0));
  CHECK(score::monBinData == nullptr);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "Gazette terminal reset refuses a borrowed MON block without a storage "
    "owner",
    "[gazette-terminal][gazette-terminal-live][native-refusal]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  score::menuState = 14;
  score::cameraDistance = 777;
  auto* const borrowed = score::monBinData;
  const auto bytes = live::Fixture::MonBytes();
  const std::vector<std::byte> before(bytes.begin(), bytes.end());
  const auto refusals = score::LiveGazetteTerminalRefusals();
  REQUIRE_FALSE(score::ResetLiveGazetteTerminalModuleState());
  CHECK(score::monBinData == borrowed);
  CHECK(score::menuState == 14);
  CHECK(score::cameraDistance == 777);
  CHECK(live::memory::LivePayloadCapacity(borrowed).has_value());
  CHECK(std::ranges::equal(bytes, before));
  CHECK(score::LiveGazetteTerminalRefusals() == refusals + 1);
}

TEST_CASE(
    "Gazette genuine fixture restores its borrowed source and runtime owners",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-lifetime]") {
  const auto snapshot = [] {
    auto result = gazette_statistics_live_tests::SnapshotBorrowedInputs();
    const auto append = [&](const auto owners) {
      for (const auto owner : owners) {
        result.insert(result.end(), owner.begin(), owner.end());
      }
    };
    append(live::ControlOwners());
    append(live::PreviewOwners());
    append(std::array{live::BytesOf(score::menuText),
                      live::BytesOf(score::rankText),
                      live::BytesOf(VS::Menu::menuStrings),
                      live::BytesOf(VS::Actors::textureSlotOrigins),
                      live::BytesOf(VS::Actors::textureSlotRowOffsets),
                      live::BytesOf(VS::Actors::sheathedWeaponAnimationBases),
                      live::BytesOf(VS::Actors::drawnWeaponAnimationBases),
                      live::BytesOf(VS::Actors::animationEventLengths),
                      live::BytesOf(VS::Actors::animationEventPoints),
                      live::BytesOf(VS::Actors::sharedSequenceIdsByKind)});
    return result;
  };
  const auto before = snapshot();
  const auto heap_refusals = live::memory::RefusedLiveHeapRequestCount();
  const auto damaged_guards = live::memory::DamagedLiveHeapGuardCount();
  for (int entry = 0; entry < 2; ++entry) {
    {
      live::PreviewFixture fixture;
      fixture.LoadSourceActor();
      std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
      fixture.SelectPage(3);
      fixture.AdvanceTo(14);
      REQUIRE(fixture.TickGazette() == 0);
      CHECK(fixture.InformationText() == live::Fixture::Description(0));
      CHECK(fixture.GuardsIntact());
    }
    CHECK(snapshot() == before);
    CHECK(live::content::LiveRefusedDiscRecordsInUse() == 0);
    CHECK(live::memory::RefusedLiveHeapRequestCount() == heap_refusals);
    CHECK(live::memory::DamagedLiveHeapGuardCount() == damaged_guards);
  }
}

struct FallbackIdentity {
  VS::Content::DiscQueueSlot* request;
  std::uint64_t generation;
};

FallbackIdentity CheckFallbackRecord() {
  auto* const fallback = score::monBinCdQueueSlot;
  REQUIRE(fallback != nullptr);
  const auto address = reinterpret_cast<std::uintptr_t>(fallback);
  const auto first =
      reinterpret_cast<std::uintptr_t>(VS::Content::discRequestSlots.data());
  CHECK((address < first ||
         address >= first + sizeof(VS::Content::discRequestSlots)));
  REQUIRE(fallback->state == VS::Content::DiscRequestState::kLoaded);
  const auto generation = live::content::LiveDiscRequestGeneration(fallback);
  REQUIRE(generation != 0);
  return {fallback, generation};
}

void CheckFallbackPayload(std::span<const std::byte> mon_image) {
  CHECK(live::content::LiveRefusedDiscRecordsInUse() == 1);
  REQUIRE(score::monBinData != nullptr);
  CHECK(std::ranges::equal(live::Fixture::MonBytes().first(mon_image.size()),
                           mon_image));
  CHECK(score::enemyDescriptions == nullptr);
}

void WaitForFallbackConsumption(live::PreviewFixture& fixture,
                                const FallbackIdentity& fallback) {
  fixture.AdvanceTo(13, false);
  CHECK(live::content::LiveRefusedDiscRecordsInUse() == 1);
  CHECK(live::content::LiveDiscRequestGeneration(fallback.request) ==
        fallback.generation);
}

void ConsumeFallback(live::PreviewFixture& fixture,
                     const FallbackIdentity& fallback) {
  REQUIRE(fixture.TickGazette(false) == 0);
  REQUIRE(score::menuState == 14);
  CHECK(score::monBinCdQueueSlot == nullptr);
  CHECK(live::content::LiveRefusedDiscRecordsInUse() == 0);
  CHECK(live::content::LiveDiscRequestGeneration(fallback.request) == 0);
  CHECK(fallback.request->state == VS::Content::DiscRequestState::kFree);
}

void CheckFallbackList(live::PreviewFixture& fixture) {
  CHECK(score::monBinData[0].unlocked == 1);
  CHECK(score::monBinData[0].next == 1);
  CHECK(fixture.InformationText() == shared::EncodedText{0xE7});
  REQUIRE(fixture.TickGazette(false) == 0);
  CHECK(fixture.InformationText() == live::Fixture::Description(0));
}

void LeaveFallbackList(live::PreviewFixture& fixture) {
  live::Fixture::Input(live::kCross);
  REQUIRE(fixture.TickGazette(false) == 0);
  fixture.AdvanceTo(1, false);
  CHECK(score::monBinData == nullptr);
  CHECK(score::enemyDescriptions == nullptr);
}

TEST_CASE(
    "F02803 Live completes full-queue MON fallback without disturbing any "
    "foreign request",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-queue]["
    "F02803]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
  fixture.SettleOverview();
  const auto mon_image =
      live::shared::ReadImage(R"(Native\data\disc\SMALL\MON.BIN)");
  ForeignQueue foreign;
  FillForeignQueue(foreign);
  const auto refusals = score::LiveGazetteTerminalRefusals();
  const auto queue_refusals = live::content::LiveDiscRequestRefusals();
  VS::Content::DiscQueueSlot* previous_fallback = nullptr;
  std::uint64_t previous_generation = 0;
  for (int entry = 0; entry < 2; ++entry) {
    fixture.SelectPage(3, false);
    const auto fallback = CheckFallbackRecord();
    CheckFallbackPayload(std::as_bytes(std::span(mon_image)));
    if (previous_fallback != nullptr) {
      CHECK(fallback.request == previous_fallback);
      CHECK(fallback.generation != previous_generation);
    }
    WaitForFallbackConsumption(fixture, fallback);
    ConsumeFallback(fixture, fallback);
    CheckFallbackList(fixture);
    LeaveFallbackList(fixture);
    CheckForeignQueue(foreign);
    previous_fallback = fallback.request;
    previous_generation = fallback.generation;
  }
  CHECK(live::content::LiveDiscRequestRefusals() == queue_refusals + 2);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
  ReleaseForeignQueue(foreign);
}

void PrepareGazetteScreen(live::PreviewFixture& fixture, std::uint8_t& phase) {
  REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  REQUIRE(phase == 4);
  CHECK(VS::Battle::hudHidden == 0);
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
  for (std::size_t tick = 0; tick < live::kFrameBudget && phase != 6; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  }
  REQUIRE(phase == 6);
}

void CheckScreenProgress() {
  CHECK(score::score == 0);
  CHECK(score::characterRank == 15);
  CHECK(VS::Battle::hudHidden == 1);
  CHECK(VS::Battle::hudSlideOffset == 0x200000);
  CHECK(VS::Field::isFieldRenderingEnabled == 0);
}

void DrawActiveGazetteScreen(live::PreviewFixture& fixture,
                             std::uint8_t& phase) {
  for (int tick = 0; tick < 40; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  }
  REQUIRE(score::menuState == 2);
  CHECK(VS::Menu::Framework::backgroundFadeStep > 0);
}

void RequestScreenExit(live::PreviewFixture& fixture, std::uint8_t& phase) {
  live::Fixture::Input(live::kTriangle);
  fixture.ResetFrame();
  live::fw::UpdateLiveScreen();
  REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  live::Fixture::Input();
  for (std::size_t tick = 0; tick < live::kFrameBudget && phase != 7; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  }
  REQUIRE(phase == 7);
}

void CheckScreenRestoredField() {
  CHECK(Actors_actorRecords[1] == nullptr);
  CHECK(VS::Battle::hudHidden == 0);
  CHECK(VS::Battle::hudSlideOffset == 0);
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
}

void BeginScreenFade(std::uint8_t& phase) {
  REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  REQUIRE(phase == 8);
  const int initial_fade = VS::Menu::Framework::backgroundFadeStep;
  REQUIRE(initial_fade > 0);
}

void FinishScreenFade(live::PreviewFixture& fixture, std::uint8_t& phase) {
  int result = 0;
  for (std::size_t tick = 0; tick < live::kFrameBudget && result == 0; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    result = score::UpdateLiveGazetteScreen(&phase);
  }
  CHECK(result == 1);
  CHECK(phase == 0);
  CHECK(VS::Menu::Framework::backgroundFadeStep == 0);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 9);
  CHECK(VS::Menu::Framework::hideMenu == 0);
}

TEST_CASE(
    "F04156 Live calculates progress and leaves through real fade and "
    "controller services",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-screen]["
    "F04156]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  VS::Field::isFieldRenderingEnabled = 1;
  VS::Battle::hudHidden = 0;
  std::uint8_t phase = 3;
  const auto refusals = score::LiveGazetteTerminalRefusals();
  PrepareGazetteScreen(fixture, phase);
  CheckScreenProgress();
  DrawActiveGazetteScreen(fixture, phase);
  RequestScreenExit(fixture, phase);
  CheckScreenRestoredField();
  BeginScreenFade(phase);
  FinishScreenFade(fixture, phase);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}
}  // namespace
