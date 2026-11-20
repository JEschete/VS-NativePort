#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Seq/Channel.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.h"
#include "Battle/StatusEffects.hpp"
#include "Content/CdDrive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/CubePuzzle.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Menu/Results/TimeAttackTerminalLive.hpp"
#include "Menu/TimeAttackTerminalLiveFixture.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/Progress.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
namespace render = VS::Render;
using Asset = r::TimeAttackAsset;
using LiveFixture = time_attack_terminal_live_tests::Fixture;
using time_attack_terminal_live_tests::Initialize;
using time_attack_terminal_live_tests::InjectLoaded;
using time_attack_terminal_live_tests::kImageBlock;
using time_attack_terminal_live_tests::PutWord;
using time_attack_terminal_live_tests::RequestFor;
using time_attack_terminal_live_tests::Word;
constexpr std::array kAssets{Asset::kResults, Asset::kStart};
constexpr VS::Content::DiscFile kForeignFile{.lba = 1, .size = 64};

std::vector<std::uint64_t> FillForeignRequestSlots() {
  std::vector<std::uint64_t> generations;
  for (auto& request : VS::Content::discRequestSlots) {
    REQUIRE(content::AllocateLiveDiscRequest(&kForeignFile) == &request);
    generations.push_back(content::LiveDiscRequestGeneration(&request));
  }
  return generations;
}

void ReleaseUnchangedForeignRequests(
    const std::vector<std::uint64_t>& generations) {
  for (std::size_t index = 0; index < generations.size(); ++index) {
    auto& request = VS::Content::discRequestSlots[index];
    CHECK(content::LiveDiscRequestGeneration(&request) ==
          generations.at(index));
    CHECK(request.state == VS::Content::DiscRequestState::kAllocated);
    content::ReleaseLiveDiscRequest(&request);
  }
}

void TransferPageOwner(std::uint32_t count, int before, int after) {
  REQUIRE(render::RetireLiveTexturePageRange(29, count, before) == 1);
  render::AdvanceLiveTexturePageRetirements();
  render::AdvanceLiveTexturePageRetirements();
  REQUIRE(render::ReserveLiveTexturePageRange(29, count, after) == 29);
}

void* AllocateForeignResources() {
  auto* const foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  REQUIRE(render::ReserveLiveTexturePageRange(25, 1, 9) == 25);
  return foreign;
}

void CheckTextureStorageFreed(Asset asset) {
  CHECK_FALSE(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
  CHECK(r::InspectLiveTimeAttackLoad(asset).owns_pages);
}

void CheckEffectSettings(render::FramebufferEffectPair scale,
                         render::FramebufferEffectTint tint, int blend) {
  const auto& settings = render::LiveFramebufferEffect().settings;
  CHECK(settings.scale == scale);
  CHECK(settings.tint == tint);
  CHECK(settings.blend_mode == blend);
}

void CheckStoppedCountdown() {
  CHECK(VS::State::tickSpeed == 2);
  CHECK_FALSE(r::LiveTimeAttackOwnsCountdownControls());
  CHECK_FALSE(render::IsLiveFramebufferEffectShowing());
  CHECK(render::LiveFramebufferEffect().settings.scale ==
        render::FramebufferEffectPair{0, 0});
}

void CheckRestoredTerminalState(
    const render::FramebufferEffectSettings& settings) {
  CHECK(render::LiveFramebufferEffect().settings == settings);
  CHECK_FALSE(render::HasLiveFramebufferEffectPackets());
  CHECK_FALSE(r::LiveTimeAttackOwnsCountdownControls());
  CHECK(VS::State::tickSpeed == 4);
  CHECK(r::newTimeSlot == 3);
  CHECK(r::timeTrialTime == 0x10203);
  CHECK(r::clutBuf[159] == 0x5A5A);
}

void AdvanceCountdownToExit(LiveFixture& fixture) {
  fixture.LoadThroughTextures(Asset::kStart);
  REQUIRE(r::LiveInitializeTimeAttackStart() == 1);
  r::screenState = 2;
  REQUIRE(r::LiveUpdateTimeAttackStart() == 0);
  VS::Input::buttonsPressed = 0x20;
  for (int tick = 0; tick < 62; ++tick) {
    fixture.BeginFrame();
    CHECK(r::LiveUpdateTimeAttackStart() == 0);
    CHECK(VS::State::tickSpeed == 4);
  }
  CHECK(r::screenState == 4);
}
}  // namespace

TEST_CASE(
    "Time-attack Live reload unbinds original slots without losing owners",
    "[time-attack-terminal-live][F03011][F03012][reload]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    CHECK(Initialize(asset) == 0);
    auto& request = RequestFor(asset);
    const auto generation = content::LiveDiscRequestGeneration(&request);
    r::ResetLiveGameClearModuleData();
    r::ResetLiveTimeAttackModuleData();
    const auto status = r::InspectLiveTimeAttackLoad(asset);
    CHECK_FALSE(status.request_bound);
    CHECK_FALSE(status.buffer_bound);
    CHECK(status.owns_request);
    CHECK(status.owns_buffer);
    CHECK(status.owns_pages);
    CHECK(content::LiveDiscRequestGeneration(&request) == generation);
    CHECK(VS::Battle::submenuState == 1);
  }
}

TEST_CASE(
    "Time-attack Live additional module reset preserves shared and resident "
    "data",
    "[time-attack-terminal-live][F03011][reload]") {
  LiveFixture fixture;
  VS::Battle::submenuState = 2;
  VS::State::scoreData.boss_time_trial_scores[7][2].time = 123456;
  r::screenTimer = 97;
  r::screenState = 2;
  r::newTimeSlot = 3;
  r::timeTrialTime = 0x123456;
  std::ranges::fill(r::clutBuf, 0x1234);
  r::ResetLiveTimeAttackModuleData();
  CHECK(r::newTimeSlot == 0);
  CHECK(r::timeTrialTime == 0);
  CHECK(std::ranges::all_of(r::clutBuf, [](auto color) { return color == 0; }));
  CHECK(r::screenTimer == 97);
  CHECK(r::screenState == 2);
  CHECK(VS::Battle::submenuState == 2);
  CHECK(VS::State::scoreData.boss_time_trial_scores[7][2].time == 123456);
}

TEST_CASE(
    "Time-attack Live cleanup affects only matching buffer request and page "
    "loan",
    "[time-attack-terminal-live][F03011][F03012][ownership]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    auto* const foreign = AllocateForeignResources();
    CHECK(Initialize(asset) == 0);
    auto& request = RequestFor(asset);
    void* const buffer = request.destination;
    r::ResetLiveTimeAttackModuleData();
    CHECK(r::CleanupLiveTimeAttackOwnedState());
    CHECK(content::LiveDiscRequestGeneration(&request) == 0);
    CHECK_FALSE(memory::LivePayloadCapacity(buffer).has_value());
    CHECK(memory::LivePayloadCapacity(foreign).has_value());
    CHECK(render::LiveTexturePages()[25] ==
          render::TexturePage{render::TexturePageState::kOwned, 9});
    CHECK(render::LiveTexturePages()[29].state ==
          render::TexturePageState::kRetiring);
    memory::FreeLiveFromLowEnd(foreign);
  }
}

TEST_CASE(
    "Time-attack Live cleanup refuses to release a reused request identity",
    "[time-attack-terminal-live][F03011][F03012][ownership]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    REQUIRE(Initialize(asset) == 0);
    auto& request = RequestFor(asset);
    content::ReleaseLiveDiscRequest(&request);
    auto* const foreign = content::AllocateLiveDiscRequest(&kForeignFile);
    REQUIRE(foreign == &request);
    const auto generation = content::LiveDiscRequestGeneration(foreign);
    CHECK(Initialize(asset) == 0);
    CHECK(VS::Battle::submenuState == 1);
    CHECK(r::CleanupLiveTimeAttackOwnedState());
    CHECK(content::LiveDiscRequestGeneration(foreign) == generation);
    CHECK(foreign->state == VS::Content::DiscRequestState::kAllocated);
    content::ReleaseLiveDiscRequest(foreign);
  }
}

TEST_CASE(
    "Time-attack Live cleanup retains ownership while the heap is unavailable",
    "[time-attack-terminal-live][F03011][F03012][ownership]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    REQUIRE(Initialize(asset) == 0);
    {
      memory::ScopedLiveGameHeap absent(std::span<std::byte>{});
      CHECK_FALSE(r::CleanupLiveTimeAttackOwnedState());
      const auto status = r::InspectLiveTimeAttackLoad(asset);
      CHECK(status.owns_buffer);
      CHECK(status.owns_request);
      CHECK(status.owns_pages);
    }
    CHECK(r::CleanupLiveTimeAttackOwnedState());
  }
}

TEST_CASE(
    "Time-attack Live retirement refusal retains the outstanding page loan",
    "[time-attack-terminal-live][F03011][F03012][ownership]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    REQUIRE(Initialize(asset) == 0);
    const std::uint32_t count = asset == Asset::kResults ? 1 : 2;
    TransferPageOwner(count, 5, 9);
    CHECK_FALSE(r::CleanupLiveTimeAttackOwnedState());
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_pages);
    CHECK_FALSE(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
    CHECK(render::LiveTexturePages()[29].owner == 9);
    TransferPageOwner(count, 9, 5);
    CHECK(r::CleanupLiveTimeAttackOwnedState());
  }
}

TEST_CASE(
    "Time-attack Live missing slots and real queue Error never become Loaded",
    "[time-attack-terminal-live][F03011][F03012]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    VS::Battle::submenuState = 1;
    CHECK(Initialize(asset) == 0);
    CHECK_FALSE(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
    VS::Battle::submenuState = 0;
    REQUIRE(Initialize(asset) == 0);
    RequestFor(asset).state = VS::Content::DiscRequestState::kError;
    CHECK(Initialize(asset) == 0);
    CHECK(Initialize(asset) == 0);
    CHECK(VS::Battle::submenuState == 1);
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_request);
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
  }
}

TEST_CASE(
    "Time-attack Live reload requires resource cleanup before a fresh load",
    "[time-attack-terminal-live][F03011][F03012][reload]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    REQUIRE(Initialize(asset) == 0);
    auto& request = RequestFor(asset);
    const auto generation = content::LiveDiscRequestGeneration(&request);
    r::ResetLiveTimeAttackModuleData();
    CHECK(Initialize(asset) == 0);
    VS::Battle::submenuState = 0;
    CHECK(Initialize(asset) == 0);
    CHECK(content::LiveDiscRequestGeneration(&request) == generation);
    CHECK(r::CleanupLiveTimeAttackOwnedState());
    render::AdvanceLiveTexturePageRetirements();
    render::AdvanceLiveTexturePageRetirements();
    CHECK(Initialize(asset) == 0);
    CHECK(VS::Battle::submenuState == 1);
    CHECK(content::LiveDiscRequestGeneration(&request) != generation);
  }
}

TEST_CASE("F03011 Live parses real TIM storage and retains the copied palette",
          "[time-attack-terminal-live][F03011]") {
  LiveFixture fixture;
  REQUIRE(r::LiveInitializeTimeAttackResults() == 0);
  const auto bytes = InjectLoaded(Asset::kResults);
  CHECK(r::LiveInitializeTimeAttackResults() == 0);
  CHECK(Word(bytes, 20) == 0);
  CHECK(Word(bytes, 24) == 0x7F7F7F7F);
  CHECK(Word(bytes, kImageBlock + 4) == 0x01000340);
  CHECK(Word(bytes, kImageBlock + 8) == 0x00FF0040);
  const std::array sample{r::clutBuf[0], r::clutBuf[1], r::clutBuf[159]};
  CHECK(sample == std::array<std::uint16_t, 3>{0, 0, 0x7F7F});
  CHECK_FALSE(r::InspectLiveTimeAttackLoad(Asset::kResults).owns_request);
}

TEST_CASE("F03012 Live parses two TIMs without overwriting the results palette",
          "[time-attack-terminal-live][F03012]") {
  LiveFixture fixture;
  std::ranges::fill(r::clutBuf, 0x1234);
  REQUIRE(r::LiveInitializeTimeAttackStart() == 0);
  const auto bytes = InjectLoaded(Asset::kStart);
  CHECK(r::LiveInitializeTimeAttackStart() == 0);
  CHECK(Word(bytes, 20) == 0);
  CHECK(Word(bytes, r::kGameClearTimStride + 20) == 0x7F7F7F7F);
  CHECK(Word(bytes, r::kGameClearTimStride + kImageBlock + 4) == 0x01000380);
  CHECK(r::clutBuf[0] == 0x1234);
  CHECK(r::clutBuf[159] == 0x1234);
  CHECK(VS::Battle::submenuState == 2);
}

TEST_CASE(
    "Time-attack Live malformed image and undersized palette retain ownership",
    "[time-attack-terminal-live][F03011][F03012]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    REQUIRE(Initialize(asset) == 0);
    const auto bytes = InjectLoaded(asset);
    PutWord(bytes, 16, 0x00010001);
    CHECK(Initialize(asset) == 0);
    CHECK(VS::Battle::submenuState == 1);
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_request);
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
    PutWord(bytes, 8, 0xFFFFFFFF);
    CHECK(Initialize(asset) == 0);
    CHECK(VS::Battle::submenuState == 1);
  }
}

TEST_CASE("F03011 Live validates the course before freeing or writing records",
          "[time-attack-terminal-live][F03011]") {
  LiveFixture fixture;
  fixture.LoadThroughTextures(Asset::kResults);
  VS::State::stateFlags.time_attack_boss = 8;
  VS::State::scoreData.boss_time_trial_scores[0][0].time = 99;
  r::timeTrialTime = 1234;
  CHECK(r::LiveInitializeTimeAttackResults() == 0);
  CHECK(VS::Battle::submenuState == 2);
  CHECK(r::timeTrialTime == 1234);
  CHECK(VS::State::scoreData.boss_time_trial_scores[0][0].time == 99);
  CHECK(r::InspectLiveTimeAttackLoad(Asset::kResults).owns_buffer);
}

TEST_CASE(
    "F03011 Live inserts the real record fields then frees its texture storage",
    "[time-attack-terminal-live][F03011]") {
  LiveFixture fixture;
  fixture.LoadThroughTextures(Asset::kResults);
  fixture.CompleteMusicLoad();
  auto& flags = VS::State::stateFlags;
  flags.time_attack_boss = 7;
  flags.time_attack_seconds = 30;
  flags.clear_count = static_cast<char>(254);
  flags.difficulty = 3;
  auto& row = VS::State::scoreData.boss_time_trial_scores[7];
  row[0] = {.time = 0x1000, .round = 2, .difficulty = 0};
  row[1] = {.time = 0x1E00, .round = 7, .difficulty = 1};
  row[2] = {.time = 0x2000, .round = 9, .difficulty = 0};
  CHECK(r::LiveInitializeTimeAttackResults() == 1);
  CHECK(r::newTimeSlot == 1);
  CHECK(row[1].round == 126);
  CHECK(row[1].difficulty == 1);
  CHECK(row[2].time == 0x1E00);
  CHECK(row[2].round == 7);
  CHECK(row[2].difficulty == 1);
  CheckTextureStorageFreed(Asset::kResults);
}

TEST_CASE("F03012 Live explicit initialization preserves unrelated counters",
          "[time-attack-terminal-live][F03012]") {
  LiveFixture fixture;
  fixture.LoadThroughTextures(Asset::kStart);
  r::onScreenMapCompletion = 44;
  r::onScreenScore = 55;
  r::buffReelSelection = 66;
  r::incrementingScore = 77;
  r::timeTrialTime = 88;
  CHECK(r::LiveInitializeTimeAttackStart() == 1);
  CHECK(r::onScreenMapCompletion == 0);
  CHECK(r::onScreenScore == 0);
  CHECK(r::buffReelSelection == 0);
  CHECK(r::incrementingScore == 77);
  CHECK(r::timeTrialTime == 88);
  CHECK(VS::Battle::submenuState == 0);
  CheckTextureStorageFreed(Asset::kStart);
}

TEST_CASE(
    "Time-attack Live allocation refusal keeps caller phase and owned pages",
    "[time-attack-terminal-live][F03011][F03012]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    {
      std::array<std::byte, 256> tiny{};
      memory::ScopedLiveGameHeap exhausted(tiny);
      CHECK(Initialize(asset) == 0);
      CHECK(VS::Battle::submenuState == 0);
      CHECK_FALSE(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
      CHECK(r::InspectLiveTimeAttackLoad(asset).owns_pages);
      CHECK(r::CleanupLiveTimeAttackOwnedState());
    }
  }
}

TEST_CASE(
    "F03016 Live completion retires its page but remains directly repeatable",
    "[time-attack-terminal-live][F03016]") {
  LiveFixture fixture;
  fixture.LoadThroughTextures(Asset::kResults);
  fixture.CompleteMusicLoad();
  REQUIRE(r::LiveInitializeTimeAttackResults() == 1);
  r::screenState = 3;
  r::screenTimer = 0;
  CHECK(r::LiveUpdateTimeAttackResults() == 1);
  CHECK_FALSE(r::InspectLiveTimeAttackLoad(Asset::kResults).owns_pages);
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kRetiring);
  CHECK(r::LiveUpdateTimeAttackResults() == 1);
  CHECK(r::screenState == 3);
  CHECK(r::elementAnimationState == 128);
}

TEST_CASE(
    "F03016 Live invalid drawing course cannot advance presentation state",
    "[time-attack-terminal-live][F03016]") {
  LiveFixture fixture;
  VS::State::stateFlags.time_attack_boss = 255;
  r::screenTimer = 191;
  r::elementAnimationState = 4095;
  CHECK(r::LiveUpdateTimeAttackResults() == 0);
  CHECK(r::screenTimer == 191);
  CHECK(r::elementAnimationState == 4095);
  CHECK(r::screenState == 0);
}

TEST_CASE(
    "F03017 Live uses real framebuffer state and preserves it on MENUF reload",
    "[time-attack-terminal-live][F03017][reload]") {
  LiveFixture fixture;
  r::screenState = 2;
  CHECK(r::LiveUpdateTimeAttackStart() == 0);
  CHECK(VS::State::tickSpeed == 4);
  CHECK(render::HasLiveFramebufferEffectPackets());
  CheckEffectSettings({0x1029, 0x1029}, {64, 128, 220}, 0);
  r::ResetLiveGameClearModuleData();
  r::ResetLiveTimeAttackModuleData();
  CHECK(r::LiveTimeAttackOwnsCountdownControls());
  CHECK(render::IsLiveFramebufferEffectShowing());
  CHECK(VS::State::tickSpeed == 4);
  CHECK(r::screenState == 0);
}

TEST_CASE(
    "F03017 Live GO changes real effect settings and defers packet release",
    "[time-attack-terminal-live][F03017][ownership]") {
  LiveFixture fixture;
  r::screenState = 2;
  REQUIRE(r::LiveUpdateTimeAttackStart() == 0);
  r::screenTimer = 45;
  CHECK(r::LiveUpdateTimeAttackStart() == 0);
  CheckEffectSettings({0x107A, 0x107A}, {220, 80, 64}, 1);
  CHECK(r::CleanupLiveTimeAttackOwnedState());
  CheckStoppedCountdown();
  CHECK(render::HasLiveFramebufferEffectPackets());
  fixture.DrainEffect();
  CHECK_FALSE(render::HasLiveFramebufferEffectPackets());
}

TEST_CASE(
    "F03017 Live effect allocation refusal retains phase and cleanup "
    "responsibility",
    "[time-attack-terminal-live][F03017]") {
  LiveFixture fixture;
  std::array<std::byte, 256> tiny{};
  memory::ScopedLiveGameHeap exhausted(tiny);
  r::screenState = 2;
  CHECK(r::LiveUpdateTimeAttackStart() == 0);
  CHECK(r::screenState == 2);
  CHECK_FALSE(render::HasLiveFramebufferEffectPackets());
  CHECK(r::LiveTimeAttackOwnsCountdownControls());
  CHECK(r::CleanupLiveTimeAttackOwnedState());
  CHECK(VS::State::tickSpeed == 2);
}

TEST_CASE("F03017 Live cleanup does not stop another framebuffer user",
          "[time-attack-terminal-live][F03017][ownership]") {
  LiveFixture fixture;
  render::SetLiveFramebufferEffectEnabled(1);
  REQUIRE(render::HasLiveFramebufferEffectPackets());
  VS::State::SetLiveTickSpeed(4);
  CHECK_FALSE(r::LiveTimeAttackOwnsCountdownControls());
  CHECK(r::CleanupLiveTimeAttackOwnedState());
  CHECK(render::IsLiveFramebufferEffectShowing());
  CHECK(VS::State::tickSpeed == 4);
}

TEST_CASE("F03008 Live drives all three real start initializer stages",
          "[time-attack-terminal-live][F03008][F03009][F03012]") {
  LiveFixture fixture;
  std::array<unsigned char, 2> outer{0, 0xAA};
  VS::State::stateFlags.menu_f_submenu = 1;
  CHECK(r::LiveUpdateResultsScreen(outer.data()) == 0);
  CHECK(outer.front() == 1);
  static_cast<void>(InjectLoaded(Asset::kStart));
  CHECK(r::LiveUpdateResultsScreen(outer.data()) == 0);
  CHECK(VS::Battle::submenuState == 2);
  CHECK(r::LiveUpdateResultsScreen(outer.data()) == 0);
  CHECK(outer.front() == 2);
  CHECK(outer.back() == 0xAA);
  CHECK_FALSE(r::InspectLiveTimeAttackLoad(Asset::kStart).owns_buffer);
}

namespace {
// Stops the roulette on entry 12 of the original first reel: Strength +3.
void SelectStrengthAward() {
  REQUIRE(r::kBuffReels[0][12].stat ==
          std::to_underlying(r::BonusReelStat::kStrength));
  REQUIRE(r::kBuffReels[0][12].amount == 3);
  r::buffReelIndex = 0;
  r::buffReelSelection = 0xC8;
}
}  // namespace

TEST_CASE(
    "F03008 Live completion uses actual game-clear reward and clears outer "
    "state",
    "[time-attack-terminal-live][F03008][F03014]") {
  LiveFixture fixture;
  fixture.stats.strength = 100;
  SelectStrengthAward();
  r::screenState = 3;
  REQUIRE(render::ReserveLiveTexturePageRange(29, 3, 5) == 29);
  for (int tick = 0; tick < 8; ++tick) {
    fixture.BeginFrame();
    REQUIRE(r::LiveUpdateGameClear() == 0);
  }
  std::array<unsigned char, 2> outer{2, 0xAA};
  CHECK(r::LiveUpdateResultsScreen(outer.data()) == 1);
  CHECK(fixture.stats.strength == 103);
  CHECK(outer == std::array<unsigned char, 2>{0, 0xAA});
  CHECK(r::LiveUpdateResultsScreen(outer.data()) == 0);
  CHECK(fixture.stats.strength == 103);
  CHECK(outer.front() == 1);
}

TEST_CASE(
    "F03009 and F03014 Live score-only mode calls the delivered healing API",
    "[time-attack-terminal-live][F03009][F03014]") {
  LiveFixture fixture;
  VS::State::stateFlags.menu_f_submenu = 3;
  VS::State::scoreData.misc_score = 50;
  VS::State::scoreData.heal_count = 2;
  CHECK(r::LiveInitializeResultsScreen() == 1);
  CHECK(VS::State::scoreData.misc_score == 99850);
  CHECK(VS::State::scoreData.heal_count == 0);
  CHECK(r::LiveUpdateResultsPresentation() == 1);
  CHECK(VS::State::scoreData.misc_score == 99850);
}

TEST_CASE(
    "Results Live dispatch refuses null state and unknown unsigned selectors",
    "[time-attack-terminal-live][F03008][F03009][F03014]") {
  LiveFixture fixture;
  VS::State::stateFlags.menu_f_submenu = static_cast<char>(255);
  VS::State::scoreData.misc_score = 123;
  std::array<unsigned char, 1> outer{2};
  CHECK(r::LiveUpdateResultsScreen(nullptr) == 0);
  CHECK(r::LiveInitializeResultsScreen() == 0);
  CHECK(r::LiveUpdateResultsPresentation() == 0);
  CHECK(r::LiveUpdateResultsScreen(outer.data()) == 0);
  CHECK(outer.front() == 2);
  CHECK(VS::State::scoreData.misc_score == 123);
}

TEST_CASE("Time-attack Live queue exhaustion cannot release unrelated requests",
          "[time-attack-terminal-live][F03011][F03012]") {
  for (const auto asset : kAssets) {
    LiveFixture fixture;
    content::ClearLiveDiscReadError();
    REQUIRE(content::LiveDiscState() == content::DiscDriveState::kIdle);
    const auto refused_records = content::LiveRefusedDiscRecordsInUse();
    const auto generations = FillForeignRequestSlots();
    CHECK(Initialize(asset) == 0);
    CHECK(VS::Battle::submenuState == 1);
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_buffer);
    CHECK(r::InspectLiveTimeAttackLoad(asset).owns_request);
    CHECK(content::LiveRefusedDiscRecordsInUse() == refused_records + 1);
    CHECK(r::CleanupLiveTimeAttackOwnedState());
    CHECK(content::LiveRefusedDiscRecordsInUse() == refused_records);
    ReleaseUnchangedForeignRequests(generations);
  }
}

TEST_CASE(
    "F03017 Live complete countdown retires both pages and stops its effect",
    "[time-attack-terminal-live][F03017]") {
  LiveFixture fixture;
  AdvanceCountdownToExit(fixture);
  CHECK(r::LiveUpdateTimeAttackStart() == 1);
  CheckStoppedCountdown();
  CHECK_FALSE(r::InspectLiveTimeAttackLoad(Asset::kStart).owns_pages);
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kRetiring);
  CHECK(render::LiveTexturePages()[30].state ==
        render::TexturePageState::kRetiring);
  CHECK(r::LiveUpdateTimeAttackStart() == 1);
  CHECK(render::HasLiveFramebufferEffectPackets());
  fixture.DrainEffect();
  CHECK_FALSE(render::HasLiveFramebufferEffectPackets());
}

TEST_CASE("Time-attack production fixture uses real music and render storage",
          "[time-attack-terminal-live][production-fixture]") {
  LiveFixture fixture;
  fixture.LoadThroughTextures(Asset::kResults);
  fixture.CompleteMusicLoad();
  REQUIRE(r::LiveInitializeTimeAttackResults() == 1);
  CHECK(VS::Audio::soundState.playing_music_handle !=
        VS::Audio::kNoMusicHandle);
  CHECK(VS::Audio::soundState.playing_music_handle ==
        VS::Audio::soundState.music_handles[LiveFixture::kMusicIndex]);
  CHECK(memory::LivePayloadCapacity(
            VS::Audio::soundState.music_data[LiveFixture::kMusicIndex])
            .has_value());
  fixture.BeginFrame();
  r::screenState = 1;
  r::screenTimer = 64;
  CHECK(r::LiveUpdateTimeAttackResults() == 0);
  CHECK_FALSE(fixture.frame.Empty());
}

TEST_CASE("Time-attack production fixture restores its caller resources",
          "[time-attack-terminal-live][production-fixture]") {
  LiveFixture outer;
  const render::FramebufferEffectSettings settings{.roll = 1234,
                                                   .scale = {2048, 3072},
                                                   .offset = {5, -9},
                                                   .tint = {17, 34, 51},
                                                   .blend_mode = 2,
                                                   .depth = 19,
                                                   .uses_previous_frame = 1};
  time_attack_terminal_live_tests::RestoreEffectSettings(settings);
  VS::State::tickSpeed = 4;
  r::newTimeSlot = 3;
  r::timeTrialTime = 0x10203;
  r::clutBuf[159] = 0x5A5A;
  auto* const allocation = AllocateForeignResources();
  const auto refusals = memory::RefusedLiveHeapRequestCount();
  {
    LiveFixture inner;
    r::screenState = 2;
    REQUIRE(r::LiveUpdateTimeAttackStart() == 0);
    REQUIRE(render::HasLiveFramebufferEffectPackets());
  }
  CheckRestoredTerminalState(settings);
  CHECK(VS::Battle::characterState == &outer.actor);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        outer.frame.storage.data());
  CHECK(memory::LivePayloadCapacity(allocation).has_value());
  CHECK(render::LiveTexturePages()[25] ==
        render::TexturePage{render::TexturePageState::kOwned, 9});
  CHECK(memory::RefusedLiveHeapRequestCount() == refusals);
  memory::FreeLiveFromLowEnd(allocation);
}

TEST_CASE(
    "Time-attack fixture tears down after explicit quiescence and heap reset",
    "[time-attack-terminal-live][production-fixture]") {
  const auto refusals = memory::RefusedLiveHeapRequestCount();
  {
    LiveFixture fixture;
    fixture.LoadThroughTextures(Asset::kResults);
    fixture.CompleteMusicLoad();
    REQUIRE(r::LiveInitializeTimeAttackResults() == 1);
    void* const music =
        VS::Audio::soundState.music_data[LiveFixture::kMusicIndex];
    REQUIRE(memory::LivePayloadCapacity(music).has_value());
    r::screenState = 2;
    REQUIRE(r::LiveUpdateTimeAttackStart() == 0);
    REQUIRE(r::CleanupLiveTimeAttackOwnedState());
    fixture.DrainEffect();
    REQUIRE_FALSE(render::HasLiveFramebufferEffectPackets());
    memory::ResetLiveGameHeap();
    CHECK_FALSE(memory::LivePayloadCapacity(music).has_value());
  }
  CHECK(memory::RefusedLiveHeapRequestCount() == refusals);
}
