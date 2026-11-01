// Hand-maintained tests for the typed saved music voice release (#247).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Audio/MusicVoiceConflicts.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

namespace {
class FakeChannels final : public VS::Audio::Seq::MusicChannelVoices {
 public:
  std::array<std::uint32_t, 4> voices{};

  [[nodiscard]] std::size_t ChannelCount() const override {
    return voices.size();
  }
  [[nodiscard]] std::uint32_t AssignedVoice(
      std::size_t channel) const override {
    return voices.at(channel);
  }
  void DetachVoice(std::size_t channel) override {
    voices.at(channel) = VS::Audio::Seq::kUnassignedVoice;
  }
};
}  // namespace

TEST_CASE("Saved music channels lose voices the playing layer took",
          "[audio][music-state]") {
  FakeChannels channels;
  channels.voices = {3, 5, 3, VS::Audio::Seq::kUnassignedVoice};
  const VS::Audio::Seq::MusicLayerVoices playing{(1U << 3) | (1U << 5),
                                                 1U << 5};
  const VS::Audio::Seq::MusicLayerVoices saved{1U << 5, 0};
  const std::uint32_t key_off =
      VS::Audio::Seq::ReleaseSavedVoiceConflicts(playing, saved, channels, 0);
  CHECK(key_off == ((1U << 3) | (1U << 5)));
  CHECK(channels.voices[0] == VS::Audio::Seq::kUnassignedVoice);
  CHECK(channels.voices[1] == VS::Audio::Seq::kUnassignedVoice);
  CHECK(channels.voices[2] == VS::Audio::Seq::kUnassignedVoice);
  CHECK(channels.voices[3] == VS::Audio::Seq::kUnassignedVoice);
}

TEST_CASE("Saved music voices the saved layer keeps stay assigned",
          "[audio][music-state]") {
  FakeChannels channels;
  channels.voices = {3, 5, 7, 2};
  // Voice 3: saved allocation, not keyed by the playing layer.
  // Voice 5: keyed by both layers. Voice 7: never taken by the playing layer.
  const VS::Audio::Seq::MusicLayerVoices playing{(1U << 3) | (1U << 5),
                                                 1U << 5};
  const VS::Audio::Seq::MusicLayerVoices saved{
      (1U << 3) | (1U << 5) | (1U << 7), 1U << 5};
  CHECK(VS::Audio::Seq::ReleaseSavedVoiceConflicts(playing, saved, channels,
                                                   0) == 0);
  CHECK(channels.voices == std::array<std::uint32_t, 4>{3, 5, 7, 2});
}

TEST_CASE("Reserved voices are detached but not keyed off",
          "[audio][music-state]") {
  FakeChannels channels;
  channels.voices = {4, 6, VS::Audio::Seq::kUnassignedVoice,
                     VS::Audio::Seq::kUnassignedVoice};
  const VS::Audio::Seq::MusicLayerVoices playing{(1U << 4) | (1U << 6), 0};
  const VS::Audio::Seq::MusicLayerVoices saved{};
  CHECK(VS::Audio::Seq::ReleaseSavedVoiceConflicts(playing, saved, channels,
                                                   1U << 4) == 1U << 6);
  CHECK(channels.voices[0] == VS::Audio::Seq::kUnassignedVoice);
  CHECK(channels.voices[1] == VS::Audio::Seq::kUnassignedVoice);
}

TEST_CASE("The promoted music voice release has no original module address",
          "[audio][music-state]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80015BACu) == nullptr);
}
