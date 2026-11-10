#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Battle/Ai/ActorSettings.hpp"

namespace {
using VS::Battle::Ai::ApplySavedAiSettingsFlags;
using VS::Battle::Ai::kSavedAiSettingsMask;
}  // namespace

TEST_CASE("AI saved settings flags replace only settings bits 21-31",
          "[ai][actor-settings][saved-settings]") {
  CHECK(kSavedAiSettingsMask == 0xFFE00000u);
  CHECK(ApplySavedAiSettingsFlags(0xFFFFFFFFu, 0x0000u) == 0x001FFFFFu);
  CHECK(ApplySavedAiSettingsFlags(0x00000000u, 0xFFFFu) == 0xFFE00000u);
  CHECK(ApplySavedAiSettingsFlags(0x12345678u, 0x0ACEu) == 0xACF45678u);
}

TEST_CASE("AI saved settings flags move each bit 1-11 to bit 21-31",
          "[ai][actor-settings][saved-settings]") {
  const unsigned bit = GENERATE(range(1u, 12u));
  CHECK(ApplySavedAiSettingsFlags(0u, static_cast<std::uint16_t>(1u << bit)) ==
        1u << (bit + 20));
}

TEST_CASE("AI saved settings ignore the enable bit and flag bits 12-15",
          "[ai][actor-settings][saved-settings]") {
  const std::uint32_t settings =
      GENERATE(0u, 0x001FFFFFu, 0xFFE00000u, 0x80100001u, 0xFFFFFFFFu);
  const std::uint16_t ignored = GENERATE(
      std::uint16_t{0x0001u}, std::uint16_t{0xF000u}, std::uint16_t{0xF001u});
  const auto applied = ApplySavedAiSettingsFlags(settings, 0x0554u);
  CHECK(ApplySavedAiSettingsFlags(settings, static_cast<std::uint16_t>(
                                                0x0554u | ignored)) == applied);
  // Bit 20 stays the template's; saved bit 0 never reaches it.
  CHECK((applied & 0x001FFFFFu) == (settings & 0x001FFFFFu));
  CHECK(applied >> 21 == 0x2AAu);
}
