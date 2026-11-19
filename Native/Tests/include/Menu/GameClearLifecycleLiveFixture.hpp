#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Music.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/Samples/SampleBanks.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundBanks.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/SoundSlotsLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/StatusEffects.h"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "DiscQueue.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/PageSpace.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/Progress.h"
#include "State/StateFlags.h"
#include "TestSupport/TimeAttackDrawRecorder.hpp"

namespace game_clear_live_tests {
namespace results = VS::Menu::Results;
namespace memory = VS::Core::Memory;
namespace content = VS::Content;
namespace render = VS::Render;
namespace audio = VS::Audio;

// Byte snapshots also preserve the legacy records' C arrays and padding.
template <auto&... Values>
class SavedGlobals {
 public:
  SavedGlobals() = default;
  ~SavedGlobals() { Restore(std::index_sequence_for<decltype(Values)...>{}); }
  SavedGlobals(const SavedGlobals&) = delete;
  SavedGlobals& operator=(const SavedGlobals&) = delete;

 private:
  template <auto& Value>
  static auto Capture() {
    using ValueType = std::remove_reference_t<decltype(Value)>;
    static_assert(std::is_trivially_copyable_v<ValueType>);
    std::array<std::byte, sizeof(ValueType)> bytes{};
    std::memcpy(bytes.data(), static_cast<const void*>(&Value), bytes.size());
    return bytes;
  }
  template <std::size_t... Indices>
  void Restore(std::index_sequence<Indices...>) {
    (std::memcpy(static_cast<void*>(&Values), std::get<Indices>(saved_).data(),
                 sizeof(std::remove_reference_t<decltype(Values)>)),
     ...);
  }
  std::tuple<decltype(Capture<Values>())...> saved_{Capture<Values>()...};
};

class SavedModuleData {
 public:
  SavedModuleData() {
    for (const auto& entry : VS::Content::ModuleDataEntries()) {
      Save(entry.target, entry.capacity);
    }
    for (const auto& table : VS::Content::ModulePointerTables()) {
      Save(static_cast<void*>(table.target), table.capacity * sizeof(void*));
    }
    Save(Menu_Results_buffReels,
         results::kBonusReelCount * sizeof(Menu_Results_buffReels[0]));
  }
  ~SavedModuleData() {
    for (const auto& entry : entries_) {
      std::ranges::copy(entry.bytes, entry.target.begin());
    }
  }
  SavedModuleData(const SavedModuleData&) = delete;
  SavedModuleData& operator=(const SavedModuleData&) = delete;
  [[nodiscard]] bool Unchanged() const {
    return std::ranges::all_of(entries_, [](const auto& entry) {
      return std::ranges::equal(entry.target, entry.bytes);
    });
  }

 private:
  struct Entry {
    std::span<std::byte> target;
    std::vector<std::byte> bytes;
  };
  void Save(void* target, std::size_t capacity) {
    if (capacity == 0) return;
    if (target == nullptr) {
      throw std::runtime_error("module snapshot has no native storage");
    }
    const std::span bytes(static_cast<std::byte*>(target), capacity);
    entries_.push_back({bytes, {bytes.begin(), bytes.end()}});
  }
  std::vector<Entry> entries_;
};

struct SavedRuntime {
  unsigned random = VS::Core::BiosRand().State();
  render::TexturePageTable pages = render::LiveTexturePages();
  render::PageSpace image = render::GamePages();
  ~SavedRuntime() {
    render::LiveTexturePages() = pages;
    render::GamePages() = std::move(image);
    VS::Core::SeedBiosRand(random);
  }
};

// Requires an idle game-clear owner and an unused disc queue. All services are
// production implementations; only the request payload/completion is
// controlled.
class Fixture {
 private:
  SavedModuleData saved_module_data_;
  SavedGlobals<
      Audio_soundState, Audio_Seq_activeMusicConfig,
      Audio_Seq_secondaryMusicConfig, Audio_Seq_secondaryMusicChannels,
      Audio_Seq_primaryMusicConfig, Audio_Seq_savedMusicConfig,
      Audio_Seq_musicChannels, Audio_Seq_savedMusicChannels,
      Audio_Seq_pendingMusicData, Audio_Seq_musicLoopsRemaining,
      Audio_Seq_masterFade, Audio_Driver_commandArguments,
      Audio_Driver_controlLatches, Audio_Driver_legacySpuEvent,
      Audio_Mixer_commandParams, Audio_Mixer_voiceScheduler,
      Audio_Mixer_voiceOwners, Audio_SoundEffects_channels,
      Audio_Cutscene_streamState, Audio_Samples_musicSampleBankIds,
      VS::State::scoreData, VS::State::stateFlags, State_roomVisitProgress,
      Battle_characterState, Battle_unusedSubmenuState,
      VS::Input::buttonsPressed, Content_discRequestSlots,
      Content_discRequestCounts, Content_discDriveState,
      VS::Render::legacyDrawEnvironments, Menu_Results_screenTimer,
      Menu_Results_screenState, Menu_Results_buffReelIndex,
      Menu_Results_buffReelSelection, Menu_Results_elementAnimationState,
      Menu_Results_score, Menu_Results_mapCompletion, Menu_Results_rank,
      Menu_Results_clearCount, Menu_Results_onScreenScore,
      Menu_Results_incrementingScore, Menu_Results_onScreenMapCompletion,
      Menu_Results_incrementingMapCompletion>
      saved_;
  SavedRuntime saved_runtime_;

 public:
  static constexpr std::size_t kArenaBytes = std::size_t{256} * 1024;
  static constexpr int kMusicSlot = 2;
  static constexpr std::size_t kMusicIndex = kMusicSlot - 1;
  static constexpr std::uint16_t kSequenceId = 128;
  std::vector<std::byte> arena = std::vector<std::byte>(kArenaBytes);
  memory::ScopedLiveGameHeap heap{arena};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Audio_MusicConfig music{};
  VS::test_support::time_attack::LiveTimeAttackFixture frame;

  Fixture() {
    REQUIRE_FALSE(results::InspectLiveGameClear().owns_buffer);
    REQUIRE_FALSE(results::InspectLiveGameClear().owns_request);
    REQUIRE_FALSE(results::InspectLiveGameClear().owns_pages);
    REQUIRE(results::InspectLiveGameClear().state_timer == 0);
    REQUIRE(content::LiveRefusedDiscRecordsInUse() == 0);
    REQUIRE(std::ranges::all_of(Content_discRequestSlots, [](const auto& slot) {
      return slot.state == Content_DiscRequestState_Free &&
             content::LiveDiscRequestGeneration(&slot) == 0;
    }));
    content::InitializeLiveDiscQueue();
    render::InitializeLiveTexturePages();
    results::ResetLiveGameClearModuleData();
    Battle_unusedSubmenuState = 0;
    VS::Input::buttonsPressed = 0;
    VS::State::scoreData = {};
    VS::State::stateFlags = {};
    State_roomVisitProgress = {};
    actor.stats = &stats;
    Battle_characterState = &actor;
    InitializeMusic();
    BeginFrame();
  }
  ~Fixture() {
    CHECK(results::CleanupLiveGameClearOwnedState());
    ReleaseMusic();
    results::ResetLiveGameClearModuleData();
    content::InitializeLiveDiscQueue();
  }
  Fixture(const Fixture&) = delete;
  Fixture& operator=(const Fixture&) = delete;

  void BeginFrame() {
    frame.Reset();
    VS::Render::FrameScratchpad().ordering_table = frame.ordering.data() + 18;
    ClearOTagR(frame.ordering.data(), static_cast<int>(frame.ordering.size()));
    SetDefDrawEnv(&VS::Render::legacyDrawEnvironments[0], 0, 0, 320, 240);
    SetDefDrawEnv(&VS::Render::legacyDrawEnvironments[1], 320, 0, 320, 240);
  }

  // The actual music loader allocates this request and high-heap destination.
  // Supply a bounded one-channel AKAO fixture, not a physical-disc I/O claim.
  void CompleteMusicLoad() {
    auto* const request = Audio_soundState.musicRequests[kMusicIndex];
    REQUIRE(request != nullptr);
    REQUIRE(content::LiveDiscRequestGeneration(request) != 0);
    REQUIRE(request->state == Content_DiscRequestState_Enqueued);
    void* const data = Audio_soundState.musicData[kMusicIndex];
    REQUIRE(request->vram == data);
    const auto capacity = memory::LivePayloadCapacity(data);
    REQUIRE(capacity.has_value());
    constexpr std::size_t kProgram = 0x44;
    REQUIRE(*capacity >= kProgram + sizeof(Audio_Seq_kIdleProgram));
    std::span bytes(static_cast<std::byte*>(data), *capacity);
    std::ranges::fill(bytes, std::byte{0});
    const auto put = [&](std::size_t at, auto value) {
      std::memcpy(bytes.subspan(at, sizeof(value)).data(), &value,
                  sizeof(value));
    };
    put(0, std::uint32_t{0x4F414B41});
    put(4, kSequenceId);
    put(6, static_cast<std::uint16_t>(kProgram + 4));
    put(8, static_cast<std::uint16_t>(VS::Audio::Driver::OutputReverbMode()));
    put(0x20, std::uint32_t{1});
    put(0x40, std::uint16_t{4});
    put(kProgram, Audio_Seq_kIdleProgram);
    REQUIRE(Content_discRequestCounts.waiting > 0);
    request->state = Content_DiscRequestState_Loaded;
    --Content_discRequestCounts.waiting;
  }

  void ReleaseMusic() {
    for (std::size_t index = 0; index < audio::kMusicSlotCount; ++index) {
      void* const data = Audio_soundState.musicData[index];
      const auto request =
          audio::LiveBankRequest(Audio_soundState.musicRequests[index]);
      auto* const queued = request.mark == audio::RequestMark::kActive
                               ? audio::LiveRequestSlot(request)
                               : nullptr;
      const auto generation =
          queued != nullptr ? content::LiveDiscRequestGeneration(queued) : 0;
      const bool owns_request = generation != 0 && queued->vram == data;
      const auto capacity = memory::LivePayloadCapacity(data);
      if (capacity.has_value()) {
        CHECK(content::CancelLiveDiscWrites(
            {static_cast<std::byte*>(data), *capacity}));
      } else if (data != nullptr) {
        CancelDiscardedMusicWrites(data);
      }
      if (owns_request &&
          content::LiveDiscRequestGeneration(queued) == generation) {
        content::ReleaseLiveDiscRequest(queued);
      }
      Audio_soundState.musicRequests[index] = nullptr;
      if (data == nullptr) continue;
      const int slot = static_cast<int>(index + 1);
      if (capacity.has_value()) {
        CHECK(audio::LiveFreeMusicSlot(slot) == 1);
      } else {
        // A production bulk heap reset may already have discarded this block.
        // Stop the sequencer without trying to free an obsolete allocation.
        CHECK(audio::LiveStopMusicSlotPlayback(slot) == 1);
        Audio_soundState.musicData[index] = nullptr;
        Audio_soundState.musicIds[index] = 0;
      }
    }
  }

 private:
  void CancelDiscardedMusicWrites(void* data) {
    // The backing arena survives its heap metadata, including pending writes.
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    const auto base = reinterpret_cast<std::uintptr_t>(arena.data());
    const bool still_backed = address >= base && address - base < arena.size();
    CHECK(still_backed);
    if (still_backed) {
      CHECK(content::CancelLiveDiscWrites(
          std::span(arena).subspan(address - base, 1)));
    } else {
      content::CancelLiveDiscRead(data);
    }
  }

  void InitializeMusic() {
    std::ifstream stream(R"(Native\data\disc\SLUS_010.40)", std::ios::binary);
    REQUIRE(stream.good());
    const std::vector<unsigned char> image{
        std::istreambuf_iterator<char>(stream), {}};
    const auto tables = VS::Content::ModulePointerTables();
    const auto handlers = std::ranges::find(
        tables, reinterpret_cast<void**>(Audio_Mixer_kCommandHandlers),
        &VS::Content::ModulePointerTable::target);
    REQUIRE(handlers != tables.end());
    REQUIRE(content::RelocateModulePointerTable(*handlers, image,
                                                VS::Content::ModuleSymbols()));
    const auto entries = VS::Content::ModuleDataEntries();
    const auto idle =
        std::ranges::find(entries, static_cast<void*>(&Audio_Seq_kIdleProgram),
                          &VS::Content::ModuleDataEntry::target);
    REQUIRE(idle != entries.end());
    REQUIRE(std::string_view(idle->module) == "SLUS_010.40");
    REQUIRE(content::CopyModuleDataEntry(*idle, image));
    Audio_soundState = {};
    Audio_soundState.playingMusicHandle = audio::kNoMusicHandle;
    std::ranges::fill(Audio_soundState.musicHandles, audio::kNoMusicHandle);
    Audio_Seq_activeMusicConfig = &music;
    Audio_Seq_secondaryMusicConfig = nullptr;
    Audio_Seq_secondaryMusicChannels = nullptr;
    Audio_Seq_savedMusicConfig = {};
    Audio_Seq_pendingMusicData = {};
    Audio_Seq_musicLoopsRemaining = 0;
    Audio_Seq_masterFade = {};
    Audio_Driver_controlLatches = {};
    // Dispatch synchronously without enabling a caller's sequencer tick event.
    Audio_Driver_legacySpuEvent = 0;
    Audio_Mixer_voiceScheduler = {};
    std::ranges::fill(Audio_Mixer_voiceOwners, nullptr);
    Audio_Samples_musicSampleBankIds = {};
    Audio_Cutscene_streamState = {};
    std::ranges::fill(Audio_SoundEffects_channels, Audio_SeqChannel{});
    for (auto& channel : Audio_Seq_musicChannels) {
      channel = {};
      channel.programCounter = audio::Seq::LiveIdleProgram();
      channel.voice.assignedVoice = audio::Seq::kUnassignedVoice;
    }
  }
};
}  // namespace game_clear_live_tests
