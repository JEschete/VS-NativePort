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

#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Music.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/Samples/SampleBanks.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Audio/SoundState.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/StatusEffects.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Results/GameClearLifecycle.hpp"
#include "Menu/Results/GameClearLifecycleLive.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "Render/PageSpace.hpp"
#include "Render/TexturePagesLive.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"
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
      VS::Audio::soundState, VS::Audio::Seq::activeMusicConfig,
      VS::Audio::Seq::secondaryMusicConfig,
      VS::Audio::Seq::secondaryMusicChannels,
      VS::Audio::Seq::primaryMusicConfig, VS::Audio::Seq::savedMusicConfig,
      VS::Audio::Seq::musicChannels, VS::Audio::Seq::savedMusicChannels,
      VS::Audio::Seq::pendingMusicData, VS::Audio::Seq::musicLoopsRemaining,
      VS::Audio::Seq::masterFade, VS::Audio::Driver::commandArguments,
      VS::Audio::Driver::controlLatches, VS::Audio::Driver::legacySpuEvent,
      VS::Audio::Mixer::commandParams, VS::Audio::Mixer::kCommandHandlers,
      VS::Audio::Mixer::voiceScheduler, VS::Audio::Mixer::voiceOwners,
      VS::Audio::SoundEffects::channels, VS::Audio::Cutscene::streamState,
      VS::Audio::Samples::musicSampleBankIds, VS::State::scoreData,
      VS::State::stateFlags, VS::State::roomVisitProgress,
      VS::Battle::characterState, VS::Battle::submenuState,
      VS::Input::buttonsPressed, VS::Content::discRequestSlots,
      VS::Content::discRequestCounts, VS::Content::discDriveState,
      VS::Render::legacyDrawEnvironments, results::screenTimer,
      results::screenState, results::buffReelIndex, results::buffReelSelection,
      results::elementAnimationState, results::score, results::mapCompletion,
      results::rank, results::clearCount, results::onScreenScore,
      results::incrementingScore, results::onScreenMapCompletion,
      results::incrementingMapCompletion>
      saved_;
  SavedRuntime saved_runtime_;
  // The music loader names its file through the extracted data's catalog.
  content::DataStore::ScopedOverride store_;

 public:
  static constexpr std::size_t kArenaBytes = std::size_t{256} * 1024;
  static constexpr int kMusicSlot = 2;
  static constexpr std::size_t kMusicIndex = kMusicSlot - 1;
  static constexpr std::uint16_t kSequenceId = 128;
  std::vector<std::byte> arena = std::vector<std::byte>(kArenaBytes);
  memory::ScopedLiveGameHeap heap{arena};
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};
  VS::Audio::MusicConfig music{};
  VS::test_support::time_attack::LiveTimeAttackFixture frame;

  Fixture() {
    REQUIRE_FALSE(results::InspectLiveGameClear().owns_buffer);
    REQUIRE_FALSE(results::InspectLiveGameClear().owns_request);
    REQUIRE_FALSE(results::InspectLiveGameClear().owns_pages);
    REQUIRE(results::InspectLiveGameClear().state_timer == 0);
    REQUIRE(content::LiveRefusedDiscRecordsInUse() == 0);
    REQUIRE(content::DataStore::Open(R"(Native\data)").has_value());
    REQUIRE(content::DataStore::Get()->RequireCatalog().has_value());
    REQUIRE(std::ranges::all_of(
        VS::Content::discRequestSlots, [](const auto& slot) {
          return slot.state == VS::Content::DiscRequestState::kFree &&
                 content::LiveDiscRequestGeneration(&slot) == 0;
        }));
    content::InitializeLiveDiscQueue();
    render::InitializeLiveTexturePages();
    results::ResetLiveGameClearModuleData();
    VS::Battle::submenuState = 0;
    VS::Input::buttonsPressed = 0;
    VS::State::scoreData = {};
    VS::State::stateFlags = {};
    VS::State::roomVisitProgress = {};
    actor.stats = &stats;
    VS::Battle::characterState = &actor;
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
    VS::Legacy::ClearOTagR(frame.ordering.data(),
                           static_cast<int>(frame.ordering.size()));
    VS::Legacy::SetDefDrawEnv(&VS::Render::legacyDrawEnvironments[0], 0, 0, 320,
                              240);
    VS::Legacy::SetDefDrawEnv(&VS::Render::legacyDrawEnvironments[1], 320, 0,
                              320, 240);
  }

  // The actual music loader allocates this request and high-heap destination.
  // Supply a bounded one-channel AKAO fixture, not a physical-disc I/O claim.
  void CompleteMusicLoad() {
    auto* const request = VS::Audio::soundState.music_requests[kMusicIndex];
    REQUIRE(request != nullptr);
    REQUIRE(content::LiveDiscRequestGeneration(request) != 0);
    REQUIRE(request->state == VS::Content::DiscRequestState::kEnqueued);
    void* const data = VS::Audio::soundState.music_data[kMusicIndex];
    REQUIRE(request->destination == data);
    const auto capacity = memory::LivePayloadCapacity(data);
    REQUIRE(capacity.has_value());
    constexpr std::size_t kProgram = 0x44;
    REQUIRE(*capacity >= kProgram + sizeof(VS::Audio::Seq::kIdleProgram));
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
    put(kProgram, VS::Audio::Seq::kIdleProgram);
    REQUIRE(VS::Content::discRequestCounts.waiting > 0);
    request->state = VS::Content::DiscRequestState::kLoaded;
    --VS::Content::discRequestCounts.waiting;
  }

  void ReleaseMusic() {
    for (std::size_t index = 0; index < audio::kMusicSlotCount; ++index) {
      void* const data = VS::Audio::soundState.music_data[index];
      const auto request =
          audio::LiveBankRequest(VS::Audio::soundState.music_requests[index]);
      auto* const queued = request.mark == audio::RequestMark::kActive
                               ? audio::LiveRequestSlot(request)
                               : nullptr;
      const auto generation =
          queued != nullptr ? content::LiveDiscRequestGeneration(queued) : 0;
      const bool owns_request = generation != 0 && queued->destination == data;
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
      VS::Audio::soundState.music_requests[index] = nullptr;
      if (data == nullptr) continue;
      const int slot = static_cast<int>(index + 1);
      if (capacity.has_value()) {
        CHECK(audio::LiveFreeMusicSlot(slot) == 1);
      } else {
        // A production bulk heap reset may already have discarded this block.
        // Stop the sequencer without trying to free an obsolete allocation.
        CHECK(audio::LiveStopMusicSlotPlayback(slot) == 1);
        VS::Audio::soundState.music_data[index] = nullptr;
        VS::Audio::soundState.music_ids[index] = 0;
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
    const auto entries = VS::Content::ModuleDataEntries();
    const auto idle = std::ranges::find(
        entries, static_cast<void*>(&VS::Audio::Seq::kIdleProgram),
        &VS::Content::ModuleDataEntry::target);
    REQUIRE(idle != entries.end());
    REQUIRE(std::string_view(idle->module) == "SLUS_010.40");
    REQUIRE(content::CopyModuleDataEntry(*idle, image));
    VS::Audio::soundState = {};
    VS::Audio::soundState.playing_music_handle = audio::kNoMusicHandle;
    std::ranges::fill(VS::Audio::soundState.music_handles,
                      audio::kNoMusicHandle);
    VS::Audio::Seq::activeMusicConfig = &music;
    VS::Audio::Seq::secondaryMusicConfig = nullptr;
    VS::Audio::Seq::secondaryMusicChannels = nullptr;
    VS::Audio::Seq::savedMusicConfig = {};
    VS::Audio::Seq::pendingMusicData = {};
    VS::Audio::Seq::musicLoopsRemaining = 0;
    VS::Audio::Seq::masterFade = {};
    VS::Audio::Driver::controlLatches = {};
    // Dispatch synchronously without enabling a caller's sequencer tick event.
    VS::Audio::Driver::legacySpuEvent = 0;
    VS::Audio::Mixer::voiceScheduler = {};
    std::ranges::fill(VS::Audio::Mixer::voiceOwners, nullptr);
    VS::Audio::Samples::musicSampleBankIds = {};
    VS::Audio::Cutscene::streamState = {};
    std::ranges::fill(VS::Audio::SoundEffects::channels,
                      VS::Audio::SeqChannel{});
    for (auto& channel : VS::Audio::Seq::musicChannels) {
      channel = {};
      channel.program_counter = audio::Seq::LiveIdleProgram();
      channel.voice.assigned_voice = audio::Seq::kUnassignedVoice;
    }
  }
};
}  // namespace game_clear_live_tests
