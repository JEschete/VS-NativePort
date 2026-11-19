#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/TimeAttackTerminalLive.hpp"
#include "Render/FramebufferEffects.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "State/Timers.hpp"

namespace time_attack_terminal_live_tests {
namespace results = VS::Menu::Results;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
namespace render = VS::Render;
using Asset = results::TimeAttackAsset;

inline constexpr std::size_t kPaletteBytes = 512;
inline constexpr std::size_t kImageBytes = std::size_t{64} * 256 * 2;
inline constexpr std::size_t kBlockHeader = 12;
inline constexpr std::size_t kImageBlock = 8 + kBlockHeader + kPaletteBytes;

inline int Initialize(Asset asset) {
  return asset == Asset::kResults ? results::LiveInitializeTimeAttackResults()
                                  : results::LiveInitializeTimeAttackStart();
}

inline void PutWord(std::span<std::byte> bytes, std::size_t offset,
                    std::uint32_t word) {
  std::memcpy(bytes.subspan(offset, sizeof(word)).data(), &word, sizeof(word));
}

inline std::uint32_t Word(std::span<const std::byte> bytes,
                          std::size_t offset) {
  std::uint32_t word = 0;
  std::memcpy(&word, bytes.subspan(offset, sizeof(word)).data(), sizeof(word));
  return word;
}

inline void PutTim(std::span<std::byte> bytes) {
  PutWord(bytes, 0, 0x10);
  PutWord(bytes, 4, 8);
  PutWord(bytes, 8, kBlockHeader + kPaletteBytes);
  PutWord(bytes, 12, 0);
  PutWord(bytes, 16, 0x00010100);
  std::ranges::fill(bytes.subspan(20, kPaletteBytes), std::byte{0x7F});
  PutWord(bytes, kImageBlock, kBlockHeader + kImageBytes);
  PutWord(bytes, kImageBlock + 4, 0);
  PutWord(bytes, kImageBlock + 8, 0x01000040);
}

inline Content_DiscQueueSlot& RequestFor(Asset asset) {
  const auto file = asset == Asset::kResults ? Menu_Results_DisFile_Time
                                             : Menu_Results_DisFile_Attack;
  for (auto& request : Content_discRequestSlots) {
    if (content::LiveDiscRequestGeneration(&request) != 0 &&
        request.cdFile.lba == Menu_Results_kDisFiles[file].lba) {
      return request;
    }
  }
  FAIL("time-attack did not allocate its texture request");
  return Content_discRequestSlots[0];
}

inline std::span<std::byte> InjectLoaded(Asset asset) {
  auto& request = RequestFor(asset);
  const auto capacity = memory::LivePayloadCapacity(request.vram);
  if (!capacity.has_value()) {
    FAIL("texture destination is not owned by the real heap");
    return {};
  }
  std::span bytes(static_cast<std::byte*>(request.vram), *capacity);
  const auto count = asset == Asset::kStart ? results::kTimeAttackStartTims : 1;
  for (std::size_t index = 0; index < count; ++index) {
    PutTim(bytes.subspan(index * results::kGameClearTimStride,
                         results::kGameClearTimStride));
  }
  // Controlled completion of the real request, not physical disc I/O.
  REQUIRE(request.state == Content_DiscRequestState_Enqueued);
  REQUIRE(Content_discRequestCounts.waiting > 0);
  request.state = Content_DiscRequestState_Loaded;
  --Content_discRequestCounts.waiting;
  return bytes;
}

inline void RestoreEffectSettings(
    const render::FramebufferEffectSettings& settings) {
  const VS::Render::FramebufferEffectTranslation scale{settings.scale.x,
                                                       settings.scale.y, 0, 0};
  const VS::Render::FramebufferEffectTranslation offset{
      settings.offset.x, settings.offset.y, 0, 0};
  const CVECTOR tint{settings.tint.red, settings.tint.green, settings.tint.blue,
                     0};
  render::SetLiveFramebufferEffectRotation(settings.roll);
  render::SetLiveFramebufferEffectScale(&scale);
  render::SetLiveFramebufferEffectOffset(&offset);
  render::SetLiveFramebufferEffectTint(&tint);
  render::SetLiveFramebufferEffectBlendMode(settings.blend_mode);
  render::SetLiveFramebufferEffectDepth(settings.depth);
  render::SetLiveFramebufferPreviousFrame(settings.uses_previous_frame);
}

// Adds time-attack ownership to the shared genuine-service fixture. Owners and
// deferred Render packets must be quiesced before discarding their heap.
class TimeAttackFixture : public game_clear_live_tests::Fixture {
 private:
  game_clear_live_tests::SavedGlobals<
      Menu_Results_newTimeSlot, Menu_Results_timeTrialTime,
      Menu_Results_clutBuf, VS::State::tickSpeed>
      saved_;
  render::FramebufferEffectSettings saved_effect_ =
      render::LiveFramebufferEffect().settings;

 public:
  TimeAttackFixture() {
    for (const auto asset : std::array{Asset::kResults, Asset::kStart}) {
      const auto status = results::InspectLiveTimeAttackLoad(asset);
      REQUIRE_FALSE(status.owns_buffer);
      REQUIRE_FALSE(status.owns_request);
      REQUIRE_FALSE(status.owns_pages);
    }
    REQUIRE_FALSE(render::HasLiveFramebufferEffectPackets());
    REQUIRE(render::LiveFramebufferEffect().phase ==
            render::FramebufferEffectPhase::kOff);
    REQUIRE_FALSE(results::LiveTimeAttackOwnsCountdownControls());
    results::ResetLiveTimeAttackModuleData();
  }
  ~TimeAttackFixture() {
    CHECK(results::CleanupLiveTimeAttackOwnedState());
    DrainEffect();
    CHECK_FALSE(render::HasLiveFramebufferEffectPackets());
    results::ResetLiveTimeAttackModuleData();
    RestoreEffectSettings(saved_effect_);
  }
  TimeAttackFixture(const TimeAttackFixture&) = delete;
  TimeAttackFixture& operator=(const TimeAttackFixture&) = delete;

  void DrainEffect() {
    render::SetLiveFramebufferEffectEnabled(0);
    BeginFrame();
    render::DrawLiveFramebufferEffect(
        VS::Render::FrameScratchpad().ordering_table);
    BeginFrame();
    render::DrawLiveFramebufferEffect(
        VS::Render::FrameScratchpad().ordering_table);
  }

  void LoadThroughTextures(Asset asset) {
    REQUIRE(Initialize(asset) == 0);
    static_cast<void>(InjectLoaded(asset));
    REQUIRE(Initialize(asset) == 0);
    REQUIRE(Battle_unusedSubmenuState == 2);
  }
};
using Fixture = TimeAttackFixture;
}  // namespace time_attack_terminal_live_tests
