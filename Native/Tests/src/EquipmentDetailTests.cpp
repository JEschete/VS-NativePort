#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
static_assert(sizeof Menu_Framework_comparisonStatTotals == 8);

struct ComparisonFixture {
  Battle_Actor* saved_actor = Battle_actors[0];
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  std::array<int16_t, 4> saved_deltas{};
  std::array<unsigned char, 8> saved_totals{};
  std::array<int16_t, 4> saved_accessory{};
  std::array<int16_t, 8> saved_primary{};
  std::array<char, 8> saved_range{};
  char saved_subtype = Menu_Framework_equipmentSubtype;

  ComparisonFixture() {
    std::copy_n(Menu_Framework_comparisonStatDeltas, saved_deltas.size(),
                saved_deltas.begin());
    std::memcpy(saved_totals.data(), Menu_Framework_comparisonStatTotals,
                sizeof saved_totals);
    std::copy_n(Menu_Framework_comparisonAccessoryStats, saved_accessory.size(),
                saved_accessory.begin());
    std::copy_n(Menu_Framework_strIntAgi, saved_primary.size(),
                saved_primary.begin());
    std::copy_n(Menu_Framework_rangeRiskData, saved_range.size(),
                saved_range.begin());
    std::memset(Menu_Framework_comparisonStatTotals, 0, sizeof saved_totals);
    stats.strength = stats.intelligence = stats.agility = 100;
    stats.totalStrength = stats.totalIntelligence = stats.totalAgility = 100;
    stats.accessory.currentInt = 1;
    actor.stats = &stats;
    Battle_actors[0] = &actor;
  }

  ~ComparisonFixture() {
    Battle_actors[0] = saved_actor;
    std::copy(saved_deltas.begin(), saved_deltas.end(),
              Menu_Framework_comparisonStatDeltas);
    std::memcpy(Menu_Framework_comparisonStatTotals, saved_totals.data(),
                sizeof saved_totals);
    std::copy(saved_accessory.begin(), saved_accessory.end(),
              Menu_Framework_comparisonAccessoryStats);
    std::copy(saved_primary.begin(), saved_primary.end(),
              Menu_Framework_strIntAgi);
    std::copy(saved_range.begin(), saved_range.end(),
              Menu_Framework_rangeRiskData);
    Menu_Framework_equipmentSubtype = saved_subtype;
  }
};

struct EquipmentFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  int saved_color = Render_fontColor;
  char saved_palette = Render_fontPaletteOffset;
  char saved_table = Render_fontTableSelect;
  char saved_page = Menu_Framework_itemStatPage;
  char saved_pages = Menu_Framework_enabledStatPages;
  char saved_subtype = Menu_Framework_equipmentSubtype;
  std::array<int16_t, 64> saved_stats{};
  std::vector<u_long> storage =
      std::vector<u_long>(0x20000 / 4 + 2, 0xDEADBEEF);
  std::array<u_long, 32> ordering{};

  explicit EquipmentFixture(int page) {
    std::copy_n(Menu_Framework_equipmentStats, saved_stats.size(),
                saved_stats.begin());
    std::fill_n(Menu_Framework_equipmentStats, saved_stats.size(), 0);
    Menu_Framework_itemStatPage = static_cast<char>(page);
    Menu_Framework_enabledStatPages = 7;
    Menu_Framework_equipmentSubtype = 0;
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    Render_frameScratch.orderingTable = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  ~EquipmentFixture() {
    std::copy(saved_stats.begin(), saved_stats.end(),
              Menu_Framework_equipmentStats);
    Menu_Framework_itemStatPage = saved_page;
    Menu_Framework_enabledStatPages = saved_pages;
    Menu_Framework_equipmentSubtype = saved_subtype;
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Render_fontColor = saved_color;
    Render_fontPaletteOffset = saved_palette;
    Render_fontTableSelect = saved_table;
  }

  bool Owns(const void* packet) const {
    const auto address = reinterpret_cast<uintptr_t>(packet);
    const auto first_packet = reinterpret_cast<uintptr_t>(storage.data() + 1);
    const auto last_packet =
        reinterpret_cast<uintptr_t>(Render_frameScratch.primitiveCursor);
    const auto first_entry = reinterpret_cast<uintptr_t>(ordering.data());
    const auto last_entry = first_entry + sizeof ordering;
    return (address >= first_packet && address < last_packet) ||
           (address >= first_entry && address < last_entry);
  }

  bool ReachesLastEntry() const {
    const void* packet = &ordering.back();
    const void* last = nullptr;
    for (size_t count = 0; packet != nullptr && count < 4096; ++count) {
      if (!Owns(packet)) return false;
      last = packet;
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    return packet == nullptr && last == ordering.data();
  }

  size_t HeaderGlyphs() const {
    const void* packet = nextPrim(&ordering[14]);
    size_t count = 0;
    size_t visited = 0;
    while (packet != &ordering[13] && visited++ < 64) {
      if (packet == nullptr || !Owns(packet)) return 0;
      const auto* words = static_cast<const u_long*>(packet);
      if (static_cast<const P_TAG*>(packet)->len == 5 &&
          words[5] == 0x000C000C) {
        ++count;
      }
      packet = nextPrim(packet);
    }
    return count;
  }
};
}  // namespace

TEST_CASE("Equipment stat pages preserve every lower UI ordering entry",
          "[render][menu][equipment][links]") {
  EquipmentFixture fixture(GENERATE(0, 1, 2));
  Menu_Framework_DrawEquipmentStatPage(0);
  CHECK(fixture.ReachesLastEntry());
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Equipment page headers interpret encoded font controls as controls",
          "[render][menu][equipment][text]") {
  EquipmentFixture fixture(GENERATE(0, 1, 2));
  Menu_Framework_DrawEquipmentStatPage(0);
  CHECK(fixture.HeaderGlyphs() == 19);
}

TEST_CASE(
    "Actor comparison baselines keep totals separate from signed modifiers",
    "[menu][equipment][stats]") {
  ComparisonFixture fixture;
  const int mode = GENERATE(0, 16);
  const int modifier = GENERATE(-20, 0, 15);
  fixture.stats.strength = fixture.stats.intelligence = fixture.stats.agility =
      static_cast<int16_t>(100 + modifier);
  Menu_Framework_SetActorComparisonStats(mode);
  std::array<int16_t, 4> totals{};
  std::memcpy(totals.data(), Menu_Framework_comparisonStatTotals,
              sizeof totals);
  const int16_t expected =
      static_cast<int16_t>(modifier + (mode == 0 ? 0 : 100));
  const int16_t expected_total = mode == 0 ? 0 : 100;
  const std::array<int16_t, 4> expected_totals{expected_total, expected_total,
                                               expected_total, 0};
  CHECK(totals == expected_totals);
  CHECK(Menu_Framework_comparisonStatDeltas[0] == expected);
  CHECK(Menu_Framework_comparisonStatDeltas[1] == expected);
  CHECK(Menu_Framework_comparisonStatDeltas[2] == expected);
  CHECK(fixture.stats.strength == 100 + modifier);
}

TEST_CASE("Primary comparison columns apply actor and accessory values once",
          "[menu][equipment][stats]") {
  ComparisonFixture fixture;
  const bool weapon = GENERATE(false, true);
  const int modifier = GENERATE(-20, 0, 15);
  fixture.stats.strength = fixture.stats.intelligence = fixture.stats.agility =
      static_cast<int16_t>(100 + modifier);
  Menu_Framework_SetActorComparisonStats(weapon ? 16 : 0);
  if (weapon) Menu_Framework_SetPrimaryStats(10, 1, -4, 1);
  std::array<int, 3> equipped{};
  for (size_t index = 0; index < equipped.size(); ++index) {
    equipped[index] = Menu_Framework_strIntAgi[index] +
                      Menu_Framework_comparisonStatDeltas[index] +
                      Menu_Framework_comparisonAccessoryStats[index];
  }
  const std::array<int, 3> expected =
      weapon ? std::array{110 + modifier, 102 + modifier, 96 + modifier}
             : std::array{100 + modifier, 101 + modifier, 100 + modifier};
  CHECK(equipped == expected);
}
