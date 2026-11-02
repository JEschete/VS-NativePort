// Hand-maintained tests for the typed XA disc mix (#247).
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "Audio/XaMix.hpp"
#include "Audio/XaMixLive.hpp"

using VS::Audio::DiscMixMatrix;

TEST_CASE("XA disc mix passes stereo sides straight through",
          "[audio][xa-mix]") {
  CHECK(VS::Audio::Control::SetDiscMixVolume(0x7F, false) ==
        std::optional(DiscMixMatrix{0x7F, 0, 0, 0x7F}));
  CHECK(VS::Audio::Control::SetDiscMixVolume(0xFF, false) ==
        std::optional(DiscMixMatrix{0xFF, 0, 0, 0xFF}));
  CHECK(VS::Audio::Control::SetDiscMixVolume(0, false) ==
        std::optional(DiscMixMatrix{}));
}

TEST_CASE("XA disc mix feeds both sides to both outputs in mono",
          "[audio][xa-mix]") {
  // (volume * 0xB570) >> 17, as vs_sound_setCdVol computes it.
  CHECK(VS::Audio::Control::SetDiscMixVolume(0x7F, true) ==
        std::optional(DiscMixMatrix{45, 45, 45, 45}));
  CHECK(VS::Audio::Control::SetDiscMixVolume(0xFF, true) ==
        std::optional(DiscMixMatrix{90, 90, 90, 90}));
  CHECK(VS::Audio::Control::SetDiscMixVolume(1, true) ==
        std::optional(DiscMixMatrix{}));
}

TEST_CASE("XA disc mix rejects volumes above one byte", "[audio][xa-mix]") {
  CHECK_FALSE(VS::Audio::Control::SetDiscMixVolume(0x100, false));
  CHECK_FALSE(VS::Audio::Control::SetDiscMixVolume(0x100, true));
  CHECK_FALSE(VS::Audio::Control::SetLiveDiscMixVolume(0x100));
  CHECK(VS::Audio::Control::SetLiveDiscMixVolume(
      VS::Audio::kDefaultDiscMixVolume));
}

TEST_CASE("Sequencer disc volume publishes its signed high half",
          "[audio][xa-mix]") {
  CHECK(VS::Audio::Seq::WriteDiscVolume(0x7FFF0000) == 0x7FFF);
  CHECK(VS::Audio::Seq::WriteDiscVolume(0x4000FFFF) == 0x4000);
  CHECK(VS::Audio::Seq::WriteDiscVolume(0) == 0);
  CHECK(VS::Audio::Seq::WriteDiscVolume(-0x10000) == -1);
  CHECK(VS::Audio::Seq::WriteDiscVolume(INT32_MIN) == INT16_MIN);
}
