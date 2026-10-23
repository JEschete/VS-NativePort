#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/StatusEffects.h"
#include "Content/StringTables.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Text/TextCode.h"
#include "Event/ScriptVm.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Rename.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Textures.h"
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

struct RenameFrameFixture {
  EquipmentFixture frame{0};
  Menu_Framework_MenuRow* saved_rows = Menu_Framework_menuItems;
  decltype(std::to_array(Menu_Rename_renderRenameMenuStrings)) saved_strings =
      std::to_array(Menu_Rename_renderRenameMenuStrings);
  decltype(std::to_array(Menu_Rename_stringBuf)) saved_name =
      std::to_array(Menu_Rename_stringBuf);
  u_char saved_action = Menu_Rename_selectedControllerAction;
  char saved_pulse = Menu_Rename_navIconPulseRequest;
  char saved_animation = Menu_Rename_nameNavIconAnim;
  std::array<Menu_Framework_MenuRow, 40> rows{};

  RenameFrameFixture() {
    Menu_Framework_menuItems = rows.data();
    Render_frameScratch.orderingTable = frame.ordering.data() + 24;
    Render_frameScratch.overlayOrderingTable = frame.ordering.data() + 8;
    std::fill_n(Menu_Rename_renderRenameMenuStrings, saved_strings.size(),
                0xE7E7);
    for (int index = Text_Menu8_rename_Index_ok;
         index <= Text_Menu8_rename_Index_insert; ++index) {
      Menu_Rename_renderRenameMenuStrings[index] = 150;
    }
    Menu_Rename_renderRenameMenuStrings[150] = 0xE70A;
    auto* grid =
        reinterpret_cast<unsigned char*>(Menu_Rename_renderRenameMenuStrings +
                                         Text_Menu8_rename_Offset_charTable);
    std::fill_n(grid, 15 * 9, 10);
    std::fill_n(Menu_Rename_stringBuf, saved_name.size(), Core_TextCode_Space);
    Menu_Rename_stringBuf[0] = 10;
    Menu_Rename_selectedControllerAction = 0;
  }

  ~RenameFrameFixture() {
    Menu_Framework_menuItems = saved_rows;
    std::copy(saved_strings.begin(), saved_strings.end(),
              Menu_Rename_renderRenameMenuStrings);
    std::copy(saved_name.begin(), saved_name.end(), Menu_Rename_stringBuf);
    Menu_Rename_selectedControllerAction = saved_action;
    Menu_Rename_navIconPulseRequest = saved_pulse;
    Menu_Rename_nameNavIconAnim = saved_animation;
  }

  std::vector<const u_long*> Packets() const {
    std::vector<const u_long*> packets;
    const void* packet = &frame.ordering.back();
    for (unsigned count = 0; packet != nullptr && count < 2048; ++count) {
      if (!frame.Owns(packet)) break;
      const auto* words = static_cast<const u_long*>(packet);
      if ((*words >> 24) != 0) packets.push_back(words);
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    return packets;
  }
};

struct EquipmentReloadFixture {
  Actors_ActorRecord* saved_preview = Actors_actorRecords[1];
  decltype(std::to_array(Actors_weaponModels)) saved_models =
      std::to_array(Actors_weaponModels);
  Render_TextureSlot saved_texture = Render_legacyWeaponTextureSlots[0];
  decltype(Event_playbackPhase) saved_event_phase = Event_playbackPhase;
  char saved_reload = Menu_Inventory_equipmentModelLoadStep;
  int saved_alias = Battle_unusedGlobal2;
  Actors_ActorRecord preview{};
  Actors_WeaponModel shield{};
  Actors_WeaponModel preview_shield{};

  EquipmentReloadFixture(int category, bool has_preview, int unused_alias) {
    Actors_actorRecords[1] = &preview;
    preview.sequenceModelId = static_cast<u_char>(category);
    std::fill_n(Actors_weaponModels, saved_models.size(), nullptr);
    shield.boneCount = 1;
    Actors_weaponModels[1] = &shield;
    Actors_weaponModels[3] = has_preview ? &preview_shield : nullptr;
    Render_legacyWeaponTextureSlots[0].referenceCount = 2;
    Event_playbackPhase = 0;
    Menu_Inventory_equipmentModelLoadStep = 0;
    Battle_unusedGlobal2 = unused_alias;
  }

  ~EquipmentReloadFixture() {
    Actors_actorRecords[1] = saved_preview;
    std::copy(saved_models.begin(), saved_models.end(), Actors_weaponModels);
    Render_legacyWeaponTextureSlots[0] = saved_texture;
    Event_playbackPhase = saved_event_phase;
    Menu_Inventory_equipmentModelLoadStep = saved_reload;
    Battle_unusedGlobal2 = saved_alias;
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

TEST_CASE("Weapon naming frame retains its grid name commands and backdrops",
          "[render][menu][equipment][rename]") {
  const int offset = GENERATE(0, 32, 320);
  RenameFrameFixture fixture;
  REQUIRE(reinterpret_cast<std::uintptr_t>(fixture.frame.storage.data()) >
          UINT32_MAX);
  Menu_Rename_DrawFrame(offset);
  CHECK(fixture.frame.ReachesLastEntry());
  const auto packets = fixture.Packets();
  const auto glyphs =
      std::count_if(packets.begin(), packets.end(), [](const u_long* packet) {
        return (*packet >> 24) == 5 && packet[5] == 0x000C000C;
      });
  const auto lines =
      std::count_if(packets.begin(), packets.end(), [](const u_long* packet) {
        return (*packet >> 24) == 6 && (packet[2] >> 24) == 0x50;
      });
  const auto backdrops =
      std::count_if(packets.begin(), packets.end(), [](const u_long* packet) {
        return (*packet >> 24) == 9 && (packet[2] >> 24) == 0x38;
      });
  CHECK(glyphs == 9 * 14 + 4 + 1);
  CHECK(lines == 48);
  CHECK(backdrops == (offset == 0 ? 5 : 4));
  CHECK(fixture.rows[29].selected == 1);
  CHECK(fixture.frame.storage.front() == 0xDEADBEEF);
  CHECK(fixture.frame.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Weapon naming selection highlight retains the lower UI chain",
          "[render][menu][equipment][rename]") {
  RenameFrameFixture fixture;
  Menu_Rename_DrawSelectionBox(128, 76, 10, 18);
  CHECK(fixture.frame.ReachesLastEntry());
  const auto packets = fixture.Packets();
  REQUIRE(packets.size() == 1);
  CHECK(packets[0][3] == vs_getXY_2(128, 76));
  CHECK(packets[0][9] == vs_getXY_2(138, 94));
  CHECK(fixture.frame.storage.front() == 0xDEADBEEF);
  CHECK(fixture.frame.storage.back() == 0xDEADBEEF);
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

TEST_CASE("Equipment reload releases shields through the actual preview slot",
          "[menu][equipment][reload]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const bool has_preview = GENERATE(false, true);
  const int unused_alias = GENERATE(0, 1);
  EquipmentReloadFixture fixture(category, has_preview, unused_alias);
  const bool two_handed = category == 3 || category == 5 || category == 7 ||
                          category == 8 || category == 9;
  const bool release = has_preview && two_handed;
  CHECK(Menu_Inventory_IsTwoHandedWeapon(category) == two_handed);
  CHECK(Menu_Inventory_ReloadEquipmentModels(category, 0, 0) == 1);
  CHECK(Menu_Inventory_equipmentModelLoadStep == 1);
  CHECK(Actors_weaponModels[1] == (release ? nullptr : &fixture.shield));
  CHECK(Actors_weaponModels[3] ==
        (has_preview ? &fixture.preview_shield : nullptr));
  CHECK(Render_legacyWeaponTextureSlots[0].referenceCount == (release ? 1 : 2));
}

TEST_CASE("Actor equipment slots contain complete aligned native models",
          "[menu][equipment][model-storage]") {
  CHECK(sizeof(Actors_ActorModelSlot) >= sizeof(Actors_WeaponModel));
  CHECK(alignof(Actors_ActorModelSlot) >= alignof(Actors_WeaponModel));
  if (sizeof(Actors_ActorModelSlot) < sizeof(Actors_WeaponModel) ||
      alignof(Actors_ActorModelSlot) < alignof(Actors_WeaponModel)) {
    return;
  }
  const int index = GENERATE(0, 1);
  const bool clear = GENERATE(false, true);
  struct GuardedAllocation {
    Actors_ActorAllocation actor;
    std::array<unsigned char, 64> guard;
  } allocation{};
  std::memset(&allocation, 0xA5, sizeof allocation);
  std::array<unsigned char, sizeof allocation> expected{};
  std::memcpy(expected.data(), &allocation, sizeof allocation);
  Actors_WeaponModel model{};
  if (!clear) {
    model.modelId = 101;
    model.material = 5;
    model.boneCount = 1;
    model.reserved5E4 = 0x12345678;
  }
  const auto offset = offsetof(Actors_ActorAllocation, modelSlots) +
                      index * sizeof(Actors_ActorModelSlot);
  std::memcpy(expected.data() + offset, &model, sizeof model);
  auto* slot = &allocation.actor.modelSlots[index];
  if (clear) {
    Core_Memory_ZeroAlignedWords(slot, sizeof model);
  } else {
    Core_Memory_CopyAlignedWords(slot, &model, sizeof model);
  }
  std::array<unsigned char, sizeof allocation> actual{};
  std::memcpy(actual.data(), &allocation, sizeof allocation);
  CHECK(actual == expected);
}
