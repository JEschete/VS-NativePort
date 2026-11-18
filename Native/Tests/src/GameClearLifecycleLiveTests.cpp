#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Content/CdDriveLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/TimeAttackTerminalLive.hpp"
#include "Render/TexturePagesLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/StatusEffects.h"
#include "DiscQueue.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "State/Progress.h"
}

namespace {
namespace results = VS::Menu::Results;
namespace memory = VS::Core::Memory;
namespace content = VS::Content;
namespace render = VS::Render;
using LiveFixture = game_clear_live_tests::Fixture;
constexpr std::size_t kClutBytes = 512;
constexpr std::size_t kTimImageBytes = std::size_t{64} * 256 * 2;
constexpr std::size_t kTimBlockHeader = 12;
constexpr Content_DiscFile kForeignFile{.lba = 1, .size = 64};

void PutWord(std::span<std::byte> bytes, std::size_t offset,
             std::uint32_t word) {
  std::memcpy(bytes.subspan(offset, sizeof(word)).data(), &word, sizeof(word));
}

void PutTim(std::span<std::byte> bytes) {
  PutWord(bytes, 0, 0x10);
  PutWord(bytes, 4, 8);
  PutWord(bytes, 8, kTimBlockHeader + kClutBytes);
  PutWord(bytes, 12, 0);
  PutWord(bytes, 16, 0x0001'0100);
  PutWord(bytes, 20, 0x7FFF'7FFF);
  constexpr auto kImage = 8 + kTimBlockHeader + kClutBytes;
  PutWord(bytes, kImage, kTimBlockHeader + kTimImageBytes);
  PutWord(bytes, kImage + 4, 0);
  PutWord(bytes, kImage + 8, 0x0100'0040);
}

std::span<std::byte> InjectLoadedRank() {
  auto& request = Content_discRequestSlots[0];
  const auto capacity = memory::LivePayloadCapacity(request.vram);
  if (!capacity.has_value()) {
    FAIL("game-clear did not allocate its request destination");
    return {};
  }
  std::span bytes(static_cast<std::byte*>(request.vram), *capacity);
  for (std::size_t index = 0; index < results::kGameClearTimCount; ++index) {
    PutTim(bytes.subspan(index * results::kGameClearTimStride,
                         results::kGameClearTimStride));
  }
  // An endpoint fixture, not evidence that the physical CD completed I/O.
  request.state = Content_DiscRequestState_Loaded;
  Content_discRequestCounts.waiting = 0;
  return bytes;
}

void PrimeClosing(LiveFixture& fixture) {
  Menu_Results_buffReels[0][15] = {.stat = Menu_Results_BuffReelStat_Str,
                                   .amount = 7};
  fixture.stats.strength = 100;
  Menu_Results_buffReelSelection = 0xFFFF'FFF8;
  Menu_Results_screenState = 3;
  REQUIRE(render::ReserveLiveTexturePageRange(29, 3, 5) == 29);
  for (int tick = 0; tick < 8; ++tick) {
    fixture.BeginFrame();
    REQUIRE(results::LiveUpdateGameClear() == 0);
  }
}

void QueueGameClearMusic() {
  REQUIRE(results::LiveInitializeGameClear() == 0);
  REQUIRE_FALSE(InjectLoadedRank().empty());
  REQUIRE(results::LiveInitializeGameClear() == 0);
}

void CheckCommittedScore() {
  CHECK(Battle_unusedSubmenuState == 0);
  CHECK(Menu_Results_buffReelIndex == 0);
  CHECK(Audio_Seq_unusedScoredata.miscScore == 90100);
  CHECK(Audio_Seq_unusedScoredata.healCount == 0);
}

void CheckPlayingMusic(const LiveFixture& fixture) {
  CHECK(Audio_soundState.currentMusicSlot == LiveFixture::kMusicSlot);
  CHECK(Audio_soundState.playingMusicHandle == LiveFixture::kSequenceId);
  CHECK(fixture.music.musicId == LiveFixture::kSequenceId);
  CHECK(fixture.music.activeChannels == 1);
  // The production zero-tick request schedules a one-tick volume fade.
  CHECK(fixture.music.masterVolumeSlideLength == 1);
  CHECK(fixture.music.masterVolume + fixture.music.masterVolumeStep ==
        0x7F0000);
  CHECK(fixture.music.sequenceBase ==
        reinterpret_cast<std::uintptr_t>(
            Audio_soundState.musicData[LiveFixture::kMusicIndex]));
}

void CheckRealReelFrame(LiveFixture& fixture) {
  QueueGameClearMusic();
  fixture.CompleteMusicLoad();
  REQUIRE(results::LiveInitializeGameClear() == 1);
  CHECK(render::GamePages().At(832, 256) == 0);
  fixture.BeginFrame();
  REQUIRE(fixture.frame.Empty());
  results::LiveDrawBonusReelTexture(120, 190, 0);
  CHECK_FALSE(fixture.frame.Empty());
}

struct SavedServiceViews {
  decltype(Render_frameScratch) frame = Render_frameScratch;
  decltype(Audio_soundState) audio = Audio_soundState;
  Battle_Actor* actor = Battle_characterState;
  unsigned random = Core_Random_State();

  void CheckRestored() const {
    CHECK(Core_Random_State() == random);
    CHECK(Render_frameScratch.primitiveCursor == frame.primitiveCursor);
    CHECK(Render_frameScratch.orderingTable == frame.orderingTable);
    CHECK(Render_frameScratch.overlayOrderingTable ==
          frame.overlayOrderingTable);
    CHECK(Battle_characterState == actor);
    CHECK(Audio_soundState.currentMusicData == audio.currentMusicData);
    CHECK(Audio_soundState.currentMusicSlot == audio.currentMusicSlot);
  }
};

void ResetHeapWithMusicStorage() {
  REQUIRE(results::CleanupLiveGameClearOwnedState());
  void* const music = Audio_soundState.musicData[LiveFixture::kMusicIndex];
  REQUIRE(memory::LivePayloadCapacity(music).has_value());
  memory::ResetLiveGameHeap();
  CHECK_FALSE(memory::LivePayloadCapacity(music).has_value());
}

void TransferPageOwner(int previous, int next) {
  REQUIRE(render::RetireLiveTexturePageRange(29, 3, previous) == 1);
  render::AdvanceLiveTexturePageRetirements();
  render::AdvanceLiveTexturePageRetirements();
  REQUIRE(render::ReserveLiveTexturePageRange(29, 3, next) == 29);
}

struct RankRead {
  Content_DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;
  void* buffer = nullptr;
  std::size_t heap_blocks = 0;
};

// Starts the MENUF load, then fails RANK.DIS through the real drive: no
// extracted file holds the sector, so the queue resolves the read as Error.
RankRead StartFailedRankRead(std::array<unsigned char, 1>& outer) {
  constexpr int kQueueUpdates = 2;
  constexpr int kUnmappedSector = 0x7FFF'0000;
  REQUIRE(results::LiveUpdateResultsScreen(outer.data()) == 0);
  REQUIRE(Battle_unusedSubmenuState == 1);
  auto& request = Content_discRequestSlots[0];
  REQUIRE(request.state == Content_DiscRequestState_Enqueued);
  const RankRead rank{
      .request = &request,
      .generation = content::LiveDiscRequestGeneration(&request),
      .buffer = request.vram,
      .heap_blocks = memory::GameHeap().AllocatedBlocks().size()};
  request.cdFile.lba = kUnmappedSector;
  for (int update = 0; update < kQueueUpdates; ++update) {
    content::UpdateLiveDiscQueue();
  }
  REQUIRE(request.state == Content_DiscRequestState_Error);
  return rank;
}

void CheckFailedRequestKept(const RankRead& rank) {
  CHECK(Battle_unusedSubmenuState == 1);
  CHECK(rank.request->state == Content_DiscRequestState_Error);
  CHECK(content::LiveDiscRequestGeneration(rank.request) == rank.generation);
  CHECK(rank.request->vram == rank.buffer);
  CHECK(Content_discRequestCounts.waiting == 0);
  CHECK(Content_discRequestCounts.reading == 0);
}

void CheckFailedLoadOwners(const RankRead& rank) {
  CHECK(memory::GameHeap().AllocatedBlocks().size() == rank.heap_blocks);
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kOwned);
  const auto status = results::InspectLiveGameClear();
  CHECK(status.owns_request);
  CHECK(status.owns_buffer);
  CHECK(status.owns_pages);
}
}  // namespace

TEST_CASE("Game-clear Live ordinary MENUF reload resets file-backed counters",
          "[game-clear-live][F03015][reload]") {
  LiveFixture fixture;
  Menu_Results_screenState = 2;
  Menu_Results_screenTimer = -1000;
  Menu_Results_buffReelSelection = 248;
  Menu_Results_score = 400;
  Menu_Results_incrementingScore = 7;
  CHECK(results::LiveUpdateGameClear() == 0);
  CHECK(results::InspectLiveGameClear().state_timer == 1);
  results::ResetLiveGameClearModuleData();
  CHECK(results::InspectLiveGameClear().state_timer == 0);
  CHECK(Menu_Results_screenState == 0);
  CHECK(Menu_Results_screenTimer == 0);
  CHECK(Menu_Results_buffReelSelection == 0);
  CHECK(Menu_Results_score == 0);
  CHECK(Menu_Results_incrementingScore == 0);
}

TEST_CASE("Game-clear Live MENUF reload does not reset resident owners",
          "[game-clear-live][F03010][reload]") {
  LiveFixture fixture;
  Battle_unusedSubmenuState = 7;
  fixture.stats.strength = 77;
  Audio_Seq_unusedScoredata.miscScore = 12345;
  Menu_Results_mapCompletion = 89;
  Menu_Results_rank = 15;
  Menu_Results_clearCount = 8;
  Menu_Results_incrementingMapCompletion = 9;
  results::ResetLiveGameClearModuleData();
  CHECK(Battle_unusedSubmenuState == 7);
  CHECK(fixture.stats.strength == 77);
  CHECK(Audio_Seq_unusedScoredata.miscScore == 12345);
  CHECK(Menu_Results_mapCompletion == 0);
  CHECK(Menu_Results_rank == 0);
  CHECK(Menu_Results_clearCount == 0);
  CHECK(Menu_Results_incrementingMapCompletion == 0);
}

TEST_CASE(
    "Game-clear Live reload unbinds slots but retains pending resource "
    "ownership",
    "[game-clear-live][F03010][ownership]") {
  LiveFixture fixture;
  CHECK(results::LiveInitializeGameClear() == 0);
  const auto generation =
      content::LiveDiscRequestGeneration(&Content_discRequestSlots[0]);
  REQUIRE(generation != 0);
  REQUIRE(results::InspectLiveGameClear().owns_buffer);
  results::ResetLiveGameClearModuleData();
  const auto status = results::InspectLiveGameClear();
  CHECK_FALSE(status.request_bound);
  CHECK_FALSE(status.buffer_bound);
  CHECK(status.owns_request);
  CHECK(status.owns_buffer);
  CHECK(content::LiveDiscRequestGeneration(&Content_discRequestSlots[0]) ==
        generation);
}

TEST_CASE(
    "Game-clear Live cold cleanup releases only its heap queue and page loan",
    "[game-clear-live][F03010][ownership]") {
  LiveFixture fixture;
  void* const other = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(other != nullptr);
  REQUIRE(render::ReserveLiveTexturePageRange(25, 1, 9) == 25);
  CHECK(results::LiveInitializeGameClear() == 0);
  results::ResetLiveGameClearModuleData();
  CHECK(results::CleanupLiveGameClearOwnedState());
  CHECK_FALSE(results::InspectLiveGameClear().owns_buffer);
  CHECK(content::LiveDiscRequestGeneration(&Content_discRequestSlots[0]) == 0);
  CHECK(memory::LivePayloadCapacity(other).has_value());
  CHECK(render::LiveTexturePages()[25] ==
        render::TexturePage{render::TexturePageState::kOwned, 9});
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kRetiring);
  memory::FreeLiveFromLowEnd(other);
}

TEST_CASE("Game-clear Live cleanup rejects a reused request identity",
          "[game-clear-live][F03010][ownership]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  content::ReleaseLiveDiscRequest(&Content_discRequestSlots[0]);
  auto* const foreign = content::AllocateLiveDiscRequest(&kForeignFile);
  REQUIRE(foreign == &Content_discRequestSlots[0]);
  void* const other = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(other != nullptr);
  content::EnqueueLiveDiscRequest(foreign, other);
  const auto generation = content::LiveDiscRequestGeneration(foreign);
  CHECK(results::CleanupLiveGameClearOwnedState());
  CHECK(content::LiveDiscRequestGeneration(foreign) == generation);
  CHECK(foreign->state == Content_DiscRequestState_Enqueued);
  CHECK(memory::LivePayloadCapacity(other).has_value());
  CHECK_FALSE(results::InspectLiveGameClear().owns_request);
  CHECK(content::CancelLiveDiscWrites({static_cast<std::byte*>(other), 64}));
  content::ReleaseLiveDiscRequest(foreign);
  memory::FreeLiveFromLowEnd(other);
}

TEST_CASE("Game-clear Live retains ownership when its heap is unavailable",
          "[game-clear-live][F03010][ownership]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  {
    std::array<std::byte, 256> other_arena{};
    memory::ScopedLiveGameHeap other_heap(other_arena);
    CHECK_FALSE(results::CleanupLiveGameClearOwnedState());
    CHECK(results::InspectLiveGameClear().owns_buffer);
    CHECK(results::InspectLiveGameClear().owns_request);
    CHECK(results::InspectLiveGameClear().owns_pages);
  }
  CHECK(results::CleanupLiveGameClearOwnedState());
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE("Game-clear Live retains a page loan when retirement is refused",
          "[game-clear-live][F03010][ownership]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  TransferPageOwner(5, 9);
  CHECK_FALSE(results::CleanupLiveGameClearOwnedState());
  CHECK(results::InspectLiveGameClear().owns_pages);
  CHECK(render::LiveTexturePages()[29].owner == 9);
  TransferPageOwner(9, 5);
  CHECK(results::CleanupLiveGameClearOwnedState());
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kRetiring);
}

TEST_CASE("Game-clear Live interrupted load requires cleanup before restart",
          "[game-clear-live][F03010][ownership]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  void* const buffer = Content_discRequestSlots[0].vram;
  results::ResetLiveGameClearModuleData();
  Battle_unusedSubmenuState = 0;
  CHECK(results::LiveInitializeGameClear() == 0);
  CHECK(Battle_unusedSubmenuState == 0);
  CHECK(Content_discRequestSlots[0].vram == buffer);
  CHECK(memory::GameHeap().AllocatedBlocks().size() == 1);
  CHECK(results::CleanupLiveGameClearOwnedState());
}

TEST_CASE("Game-clear Live missing slot and queue Error remain pending",
          "[game-clear-live][F03010]") {
  LiveFixture fixture;
  Battle_unusedSubmenuState = 1;
  CHECK(results::LiveInitializeGameClear() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  CHECK(memory::GameHeap().Empty());
  Battle_unusedSubmenuState = 0;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  Content_discRequestSlots[0].state = Content_DiscRequestState_Error;
  Content_discRequestCounts.waiting = 0;
  CHECK(results::LiveInitializeGameClear() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  CHECK(results::InspectLiveGameClear().owns_buffer);
}

// #410: the original queue retried a failed read until it loaded, so
// _initCongratulationsScreen waited at its load step. The native queue's
// terminal Error (#314) leaves the MENUF caller pending the same way: no
// retry, no second reservation and no completion, with its owners retained.
TEST_CASE(
    "Game-clear Live MENUF caller stays pending after a real drive failure",
    "[game-clear-live][F03010][F03008]") {
  constexpr int kFrames = 120;
  LiveFixture fixture;
  std::array<unsigned char, 1> outer{0};
  const RankRead rank = StartFailedRankRead(outer);

  int pending = 0;
  int retried = 0;
  for (int frame = 0; frame < kFrames; ++frame) {
    pending += results::LiveUpdateResultsScreen(outer.data()) == 0 ? 1 : 0;
    retried += rank.request->state != Content_DiscRequestState_Error ? 1 : 0;
    content::UpdateLiveDiscQueue();
  }
  CHECK(pending == kFrames);
  CHECK(retried == 0);
  CHECK(outer.front() == 1);
  CheckFailedRequestKept(rank);
  CheckFailedLoadOwners(rank);
  CHECK(results::CleanupLiveGameClearOwnedState());
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE(
    "Game-clear Live parses owned TIMs and preserves only the first palette "
    "clear",
    "[game-clear-live][F03010]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  const auto bytes = InjectLoadedRank();
  REQUIRE_FALSE(bytes.empty());
  CHECK(results::LiveInitializeGameClear() == 0);
  CHECK(Battle_unusedSubmenuState == 2);
  CHECK_FALSE(results::InspectLiveGameClear().owns_request);
  const auto palette_word = [&](std::size_t index) {
    std::uint32_t word = 0;
    std::memcpy(&word, bytes.data() + index * results::kGameClearTimStride + 20,
                sizeof(word));
    return word;
  };
  CHECK(palette_word(0) == 0);
  CHECK(palette_word(1) == 0x7FFF'7FFF);
  CHECK(results::InspectLiveGameClear().owns_buffer);
}

TEST_CASE(
    "Game-clear Live refuses a malformed loaded TIM instead of completing it",
    "[game-clear-live][F03010]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  const auto bytes = InjectLoadedRank();
  REQUIRE_FALSE(bytes.empty());
  PutWord(bytes, 8, 0);
  CHECK(results::LiveInitializeGameClear() == 0);
  CHECK(Battle_unusedSubmenuState == 1);
  CHECK(results::InspectLiveGameClear().owns_request);
  CHECK(results::InspectLiveGameClear().owns_buffer);
}

TEST_CASE(
    "Game-clear Live checks world ownership before freeing loaded storage",
    "[game-clear-live][F03010]") {
  LiveFixture fixture;
  REQUIRE(results::LiveInitializeGameClear() == 0);
  REQUIRE_FALSE(InjectLoadedRank().empty());
  REQUIRE(results::LiveInitializeGameClear() == 0);
  fixture.CompleteMusicLoad();
  Battle_characterState = nullptr;
  CHECK(results::LiveInitializeGameClear() == 0);
  CHECK(results::InspectLiveGameClear().owns_buffer);
  CHECK(Battle_unusedSubmenuState == 2);
}

TEST_CASE("Game-clear Live initialization commits the real resident score",
          "[game-clear-live][F03010]") {
  LiveFixture fixture;
  Audio_Seq_unusedScoredata.miscScore = 100;
  Audio_Seq_unusedScoredata.healCount = 100;
  fixture.stats.totalStrength = 13;
  fixture.stats.totalIntelligence = 19;
  fixture.stats.totalAgility = 27;
  QueueGameClearMusic();
  CHECK(VS::Audio::LivePollMusicLoads() == VS::Audio::kLoadInFlight);
  CHECK(results::LiveInitializeGameClear() == 0);
  fixture.CompleteMusicLoad();
  const auto random_before = Core_Random_State();
  const auto refused_before = VS::Audio::Seq::LiveRefusalCount();
  CHECK(results::LiveInitializeGameClear() == 1);
  CheckCommittedScore();
  CheckPlayingMusic(fixture);
  CHECK(Core_Random_State() != random_before);
  CHECK(VS::Audio::Seq::LiveRefusalCount() == refused_before);
  fixture.ReleaseMusic();
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE("Game-clear Live missing actor cannot complete a reward",
          "[game-clear-live][F03015]") {
  LiveFixture fixture;
  PrimeClosing(fixture);
  Battle_characterState = nullptr;
  CHECK(results::LiveUpdateGameClear() == 0);
  CHECK(fixture.stats.strength == 100);
  CHECK(results::InspectLiveGameClear().state_timer == 8);
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kOwned);
  Battle_characterState = &fixture.actor;
  CHECK(results::LiveUpdateGameClear() == 1);
  CHECK(fixture.stats.strength == 107);
  CHECK(render::LiveTexturePages()[29].state ==
        render::TexturePageState::kRetiring);
}

TEST_CASE(
    "Game-clear Live direct completion calls retain the original reward "
    "semantics",
    "[game-clear-live][F03015]") {
  LiveFixture fixture;
  PrimeClosing(fixture);
  CHECK(results::LiveUpdateGameClear() == 1);
  CHECK(fixture.stats.strength == 107);
  CHECK(results::LiveUpdateGameClear() == 1);
  CHECK(fixture.stats.strength == 114);
  CHECK(results::InspectLiveGameClear().state_timer == 8);
  CHECK(Menu_Results_screenTimer == 8);
}

TEST_CASE("Game-clear Live no-frame and invalid glyph calls preserve owners",
          "[game-clear-live][F03043][F03045]") {
  LiveFixture fixture;
  Render_frameScratch = {};
  results::LiveDrawBonusReelTexture(0, 0, -1);
  Menu_Results_buffReelIndex = -1;
  results::LiveDrawBonusReel(130, 190, 64, 2);
  CHECK(memory::GameHeap().Empty());
  CHECK_FALSE(results::InspectLiveGameClear().owns_pages);
  CHECK(Menu_Results_buffReelIndex == -1);
}

TEST_CASE("Game-clear Live healing touches only its score and heal count",
          "[game-clear-live][F03064]") {
  LiveFixture fixture;
  Audio_Seq_unusedScoredata.miscScore = 123;
  Audio_Seq_unusedScoredata.healCount = 2;
  Audio_Seq_unusedScoredata.streakScore = 99;
  results::LiveApplyHealingScoreBonus();
  CHECK(Audio_Seq_unusedScoredata.miscScore == 99923);
  CHECK(Audio_Seq_unusedScoredata.healCount == 0);
  CHECK(Audio_Seq_unusedScoredata.streakScore == 99);
}

TEST_CASE("Game-clear fixture releases a pending production music request",
          "[game-clear-live][fixture]") {
  LiveFixture fixture;
  results::LiveQueueResultsMusic();
  auto* const request =
      Audio_soundState.musicRequests[LiveFixture::kMusicIndex];
  REQUIRE(request != nullptr);
  REQUIRE(content::LiveDiscRequestGeneration(request) != 0);
  CHECK(VS::Audio::LivePollMusicLoads() == VS::Audio::kLoadInFlight);
  fixture.ReleaseMusic();
  CHECK(content::LiveDiscRequestGeneration(request) == 0);
  CHECK(Content_discRequestCounts.waiting == 0);
  CHECK(Audio_soundState.musicData[LiveFixture::kMusicIndex] == nullptr);
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE("Game-clear fixture restores production services and real frames",
          "[game-clear-live][fixture]") {
  const game_clear_live_tests::SavedModuleData modules;
  const game_clear_live_tests::SavedRuntime runtime;
  std::array<std::byte, 256> outer_arena{};
  memory::ScopedLiveGameHeap outer_heap(outer_arena);
  void* const other = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(other != nullptr);
  const SavedServiceViews views;
  constexpr std::uint16_t kPixel = 0x1234;
  render::GamePages().Upload({832, 256, 1, 1}, &kPixel);
  {
    LiveFixture fixture;
    CheckRealReelFrame(fixture);
  }
  CHECK(modules.Unchanged());
  views.CheckRestored();
  CHECK(render::GamePages().At(832, 256) == kPixel);
  CHECK(memory::LivePayloadCapacity(other).has_value());
  memory::FreeLiveFromLowEnd(other);
}

TEST_CASE("Game-clear fixture teardown accepts an already reset game heap",
          "[game-clear-live][fixture]") {
  const auto refused = memory::RefusedLiveHeapRequestCount();
  {
    LiveFixture fixture;
    QueueGameClearMusic();
    SECTION("pending music") {
      CHECK(VS::Audio::LivePollMusicLoads() == VS::Audio::kLoadInFlight);
    }
    SECTION("playing music") {
      fixture.CompleteMusicLoad();
      REQUIRE(results::LiveInitializeGameClear() == 1);
    }
    ResetHeapWithMusicStorage();
  }
  CHECK(memory::RefusedLiveHeapRequestCount() == refused);
  CHECK_FALSE(results::InspectLiveGameClear().owns_buffer);
  CHECK_FALSE(results::InspectLiveGameClear().owns_request);
  CHECK_FALSE(results::InspectLiveGameClear().owns_pages);
}
