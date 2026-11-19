#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "Audio/MusicLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDrive.hpp"
#include "Content/DiscLayout.h"
#include "Content/ModuleData.hpp"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Progress.h"

namespace {
namespace results = VS::Menu::Results;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
namespace render = VS::Render;

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                                 "MENU/MENUF.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved == nullptr) {
    throw std::runtime_error("missing game-clear module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

struct ReloadFixture : game_clear_live_tests::Fixture {
  void Prime() {
    REQUIRE(results::LiveInitializeGameClear() == 0);
    Menu_Results_screenState = 2;
    Menu_Results_screenTimer = -1000;
    Menu_Results_buffReelSelection = 248;
    Menu_Results_score = 400;
    Menu_Results_incrementingScore = 7;
    Menu_Results_incrementingMapCompletion = 9;
    Menu_Results_mapCompletion = 89;
    Menu_Results_rank = 15;
    Menu_Results_clearCount = 8;
    REQUIRE(results::LiveUpdateGameClear() == 0);
    REQUIRE(results::InspectLiveGameClear().state_timer == 1);
    stats.strength = 77;
    VS::State::scoreData.miscScore = 12345;
  }

  static void CheckControllerReset() {
    const auto status = results::InspectLiveGameClear();
    CHECK(status.state_timer == 0);
    CHECK_FALSE(status.request_bound);
    CHECK_FALSE(status.buffer_bound);
    CHECK(Menu_Results_screenState == 0);
    CHECK(Menu_Results_screenTimer == 0);
    CHECK(Menu_Results_buffReelSelection == 0);
    CheckScoreReset();
  }

  static void CheckScoreReset() {
    CHECK(Menu_Results_score == 0);
    CHECK(Menu_Results_incrementingScore == 0);
    CHECK(Menu_Results_incrementingMapCompletion == 0);
    CHECK(Menu_Results_mapCompletion == 0);
    CHECK(Menu_Results_rank == 0);
    CHECK(Menu_Results_clearCount == 0);
  }

  static void CheckOtherAndPending() {
    {
      menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
      REQUIRE(other.Complete() == 1);
    }
    CHECK(results::InspectLiveGameClear().state_timer == 1);
    CHECK(results::InspectLiveGameClear().request_bound);
    menu_overlay_tests::Load pending(16, {VS_MENUF_PRG_LBA, VS_MENUF_PRG_SIZE});
    REQUIRE(pending.Poll() == 0);
    CHECK(results::InspectLiveGameClear().state_timer == 1);
    CHECK(results::InspectLiveGameClear().request_bound);
  }

  void CheckResidentOwners() const {
    CHECK(Battle_unusedSubmenuState == 1);
    CHECK(Battle_characterState == &actor);
    CHECK(stats.strength == 77);
    CHECK(VS::State::scoreData.miscScore == 12345);
  }

  static void Reenter() {
    render::AdvanceLiveTexturePageRetirements();
    render::AdvanceLiveTexturePageRetirements();
    Battle_unusedSubmenuState = 0;
    REQUIRE(results::LiveInitializeGameClear() == 0);
    CHECK(Battle_unusedSubmenuState == 1);
    CHECK(results::InspectLiveGameClear().request_bound);
    CHECK(results::InspectLiveGameClear().buffer_bound);
  }
};

void CheckReleasedOwner() {
  const auto status = results::InspectLiveGameClear();
  CHECK_FALSE(status.owns_buffer);
  CHECK_FALSE(status.owns_request);
  CHECK_FALSE(status.owns_pages);
  CHECK(Content_discRequestCounts.waiting == 0);
  CHECK(content::LiveDiscRequestGeneration(&Content_discRequestSlots[0]) == 0);
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kRetiring);
}

void CheckFallbackRequest(std::size_t records) {
  CHECK(Battle_unusedSubmenuState == 1);
  CHECK(results::InspectLiveGameClear().owns_request);
  CHECK(results::InspectLiveGameClear().request_bound);
  CHECK(content::LiveRefusedDiscRecordsInUse() == records);
}

struct FullQueue {
  std::array<Content_DiscQueueSlot*, content::kDiscRequestCapacity> slots{};
  std::array<std::uint64_t, content::kDiscRequestCapacity> generations{};

  FullQueue() {
    constexpr Content_DiscFile kForeignFile{.lba = 1, .size = 64};
    for (std::size_t index = 0; index < slots.size(); ++index) {
      slots.at(index) = content::AllocateLiveDiscRequest(&kForeignFile);
      REQUIRE(slots.at(index) != nullptr);
      generations.at(index) =
          content::LiveDiscRequestGeneration(slots.at(index));
    }
  }
  ~FullQueue() {
    for (std::size_t index = 0; index < slots.size(); ++index) {
      if (content::LiveDiscRequestGeneration(slots.at(index)) ==
          generations.at(index)) {
        content::ReleaseLiveDiscRequest(slots.at(index));
      }
    }
  }

  void CheckUnchanged() const {
    for (std::size_t index = 0; index < slots.size(); ++index) {
      CHECK(content::LiveDiscRequestGeneration(slots.at(index)) ==
            generations.at(index));
      CHECK(slots.at(index)->state == Content_DiscRequestState_Allocated);
    }
  }
};
}  // namespace

TEST_CASE("Game-clear module entries use actual controller and score owners",
          "[game-clear-integration][game-clear-module][F03010][F03015]"
          "[F03064]") {
  game_clear_live_tests::Fixture fixture;
  REQUIRE(ModuleEntry(0x80102D10, &results::LiveInitializeGameClear)() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  CHECK(results::InspectLiveGameClear().request_bound);
  Menu_Results_screenState = 2;
  Menu_Results_screenTimer = -1000;
  CHECK(ModuleEntry(0x8010384C, &results::LiveUpdateGameClear)() == 0);
  CHECK(results::InspectLiveGameClear().state_timer == 1);
  VS::State::scoreData.miscScore = 123;
  VS::State::scoreData.healCount = 2;
  VS::State::scoreData.streakScore = 99;
  ModuleEntry(0x80108E48, &results::LiveApplyHealingScoreBonus)();
  CHECK(VS::State::scoreData.miscScore == 99923);
  CHECK(VS::State::scoreData.healCount == 0);
  CHECK(VS::State::scoreData.streakScore == 99);
}

TEST_CASE("Game-clear module renderers submit real current-frame packets",
          "[game-clear-integration][game-clear-module][F03043][F03045]") {
  game_clear_live_tests::Fixture fixture;
  fixture.BeginFrame();
  const auto dropped = render::DroppedLiveUiPacketCount();
  REQUIRE(fixture.frame.Empty());
  ModuleEntry(0x80107698, &results::LiveDrawBonusReelTexture)(10, 190, 0);
  CHECK_FALSE(fixture.frame.Empty());
  CHECK((fixture.frame.storage.at(1) & 0xFF000000UL) == 0x2E000000UL);
  fixture.BeginFrame();
  Menu_Results_buffReelIndex = 0;
  Menu_Results_buffReelSelection = 0;
  ModuleEntry(0x801060A8, &results::LiveDrawBonusReel)(130, 190, 64, 2);
  CHECK_FALSE(fixture.frame.Empty());
  CHECK(render::DroppedLiveUiPacketCount() == dropped);
}

TEST_CASE("Game-clear module music entry loads and plays the selected track",
          "[game-clear-integration][game-clear-module][F03013]") {
  game_clear_live_tests::Fixture fixture;
  VS::Audio::soundState.current_music_id = 31;
  ModuleEntry(0x80103748, &results::LiveQueueResultsMusic)();
  constexpr auto kSlot = game_clear_live_tests::Fixture::kMusicIndex;
  CHECK(static_cast<unsigned char>(VS::Audio::soundState.music_ids[kSlot]) ==
        131);
  REQUIRE(VS::Audio::soundState.music_data[kSlot] != nullptr);
  REQUIRE(VS::Audio::soundState.music_requests[kSlot] != nullptr);
  CHECK(content::LiveDiscRequestGeneration(
            VS::Audio::soundState.music_requests[kSlot]) != 0);
  fixture.CompleteMusicLoad();
  REQUIRE(VS::Audio::LivePollMusicLoads() == VS::Audio::kLoadIdle);
  VS::Audio::LivePlayMusicSlot(game_clear_live_tests::Fixture::kMusicSlot, 127,
                               0);
  CHECK(VS::Audio::LiveCurrentMusicId() == 131);
  CHECK(VS::Audio::LiveCurrentMusicSlot() == 2);
  fixture.ReleaseMusic();
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE("Production MENUF completion resets views but retains results owners",
          "[game-clear-integration][game-clear-reload][F03010][F03015]") {
  ReloadFixture fixture;
  fixture.Prime();
  auto* request = &Content_discRequestSlots[0];
  const auto generation = content::LiveDiscRequestGeneration(request);
  ReloadFixture::CheckOtherAndPending();
  {
    menu_overlay_tests::Load loaded(16, {VS_MENUF_PRG_LBA, VS_MENUF_PRG_SIZE});
    REQUIRE(loaded.Complete() == 1);
  }
  ReloadFixture::CheckControllerReset();
  fixture.CheckResidentOwners();
  CHECK(results::InspectLiveGameClear().owns_buffer);
  CHECK(results::InspectLiveGameClear().owns_request);
  CHECK(results::InspectLiveGameClear().owns_pages);
  CHECK(content::LiveDiscRequestGeneration(request) == generation);
  REQUIRE(results::CleanupLiveGameClearOwnedState());
  ReloadFixture::Reenter();
  CHECK(content::LiveDiscRequestGeneration(request) != generation);
}

TEST_CASE("Cold sessions cancel game-clear writes before fresh entry",
          "[game-clear-integration][game-clear-reload][F03010][F03015]") {
  ReloadFixture fixture;
  fixture.Prime();
  void* const foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  REQUIRE(render::ReserveLiveTexturePageRange(25, 1, 9) == 25);
  const auto entry = +[] { return 0; };
  REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  ReloadFixture::CheckControllerReset();
  fixture.CheckResidentOwners();
  CheckReleasedOwner();
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  CHECK(render::LiveTexturePages()[25] ==
        render::TexturePage{render::TexturePageState::kOwned, 9});
  memory::FreeLiveFromLowEnd(foreign);
  REQUIRE(results::CleanupLiveGameClearOwnedState());
  ReloadFixture::Reenter();
}

TEST_CASE("Production heap replacement cancels results ownership first",
          "[game-clear-integration][game-clear-heap][F03010]") {
  game_clear_live_tests::Fixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  REQUIRE(Content_discRequestCounts.waiting == 1);
  const auto refused = memory::RefusedLiveHeapRequestCount();
  VS::Boot::ResetLiveSessionHeap();
  CheckReleasedOwner();
  CHECK(memory::GameHeap().Empty());
  CHECK(memory::RefusedLiveHeapRequestCount() == refused);
  CHECK(Battle_unusedSubmenuState == 1);
  ReloadFixture::Reenter();
}

TEST_CASE(
    "Game-clear preserves the completed-request fallback for a full queue",
    "[game-clear-integration][game-clear-full-queue][F03010]") {
  game_clear_live_tests::Fixture fixture;
  content::ClearLiveDiscReadError();
  REQUIRE(content::LiveDiscState() ==
          static_cast<int>(content::DiscDriveState::kIdle));
  FullQueue foreign;
  const auto fallback_count = content::LiveRefusedDiscRecordsInUse();
  const auto refusals = content::LiveDiscRequestRefusals();
  CHECK(results::LiveInitializeGameClear() == 0);
  CheckFallbackRequest(fallback_count + 1);
  CHECK(content::LiveDiscRequestRefusals() == refusals + 1);
  foreign.CheckUnchanged();
  REQUIRE(results::CleanupLiveGameClearOwnedState());
  CHECK(content::LiveRefusedDiscRecordsInUse() == fallback_count);
  CHECK(memory::GameHeap().Empty());
  foreign.CheckUnchanged();
}
