#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Battle/Actor.h"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/StatusEffects.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/StringTables.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Core/Text/TextCode.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/EquipmentRowsLive.hpp"
#include "Menu/RenameLive.hpp"
#include "Menu/RenameLiveState.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/Textures.hpp"
#include "Render/UiPrimitives.hpp"

namespace menu8_rename = VS::Content::text::menu8_prg::rename;

namespace {
static_assert(sizeof VS::Menu::Framework::comparisonStatTotals == 8);

struct ComparisonFixture {
  Battle_Actor* saved_actor = VS::Battle::actors[0];
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  std::array<int16_t, 4> saved_deltas{};
  std::array<unsigned char, 8> saved_totals{};
  std::array<int16_t, 4> saved_accessory{};
  std::array<int16_t, 8> saved_primary{};
  std::array<char, 8> saved_range{};
  char saved_subtype = VS::Menu::Framework::equipmentSubtype;

  ComparisonFixture() {
    std::copy_n(VS::Menu::Framework::comparisonStatDeltas, saved_deltas.size(),
                saved_deltas.begin());
    std::memcpy(saved_totals.data(), VS::Menu::Framework::comparisonStatTotals,
                sizeof saved_totals);
    std::copy_n(VS::Menu::Framework::comparisonAccessoryStats,
                saved_accessory.size(), saved_accessory.begin());
    std::copy_n(VS::Menu::Framework::strIntAgi, saved_primary.size(),
                saved_primary.begin());
    std::copy_n(VS::Menu::Framework::rangeRiskData, saved_range.size(),
                saved_range.begin());
    std::memset(VS::Menu::Framework::comparisonStatTotals, 0,
                sizeof saved_totals);
    stats.strength = stats.intelligence = stats.agility = 100;
    stats.totalStrength = stats.totalIntelligence = stats.totalAgility = 100;
    stats.accessory.current_intelligence = 1;
    actor.stats = &stats;
    VS::Battle::actors[0] = &actor;
  }

  ~ComparisonFixture() {
    VS::Battle::actors[0] = saved_actor;
    std::copy(saved_deltas.begin(), saved_deltas.end(),
              VS::Menu::Framework::comparisonStatDeltas);
    std::memcpy(VS::Menu::Framework::comparisonStatTotals, saved_totals.data(),
                sizeof saved_totals);
    std::copy(saved_accessory.begin(), saved_accessory.end(),
              VS::Menu::Framework::comparisonAccessoryStats);
    std::copy(saved_primary.begin(), saved_primary.end(),
              VS::Menu::Framework::strIntAgi);
    std::copy(saved_range.begin(), saved_range.end(),
              VS::Menu::Framework::rangeRiskData);
    VS::Menu::Framework::equipmentSubtype = saved_subtype;
  }
};

struct EquipmentFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
  char saved_page = VS::Menu::Framework::itemStatPage;
  char saved_pages = VS::Menu::Framework::enabledStatPages;
  char saved_subtype = VS::Menu::Framework::equipmentSubtype;
  std::array<int16_t, 64> saved_stats{};
  std::vector<u_long> storage =
      std::vector<u_long>(0x20000 / 4 + 2, 0xDEADBEEF);
  std::array<u_long, 32> ordering{};

  explicit EquipmentFixture(int page) {
    std::copy_n(VS::Menu::Framework::equipmentStats, saved_stats.size(),
                saved_stats.begin());
    std::fill_n(VS::Menu::Framework::equipmentStats, saved_stats.size(), 0);
    VS::Menu::Framework::itemStatPage = static_cast<char>(page);
    VS::Menu::Framework::enabledStatPages = 7;
    VS::Menu::Framework::equipmentSubtype = 0;
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    // The typed UI packets only link into the frame's own ordering table.
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  ~EquipmentFixture() {
    std::copy(saved_stats.begin(), saved_stats.end(),
              VS::Menu::Framework::equipmentStats);
    VS::Menu::Framework::itemStatPage = saved_page;
    VS::Menu::Framework::enabledStatPages = saved_pages;
    VS::Menu::Framework::equipmentSubtype = saved_subtype;
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Render::LiveFontStyle() = saved_style;
  }

  bool Owns(const void* packet) const {
    const auto address = reinterpret_cast<uintptr_t>(packet);
    const auto first_packet = reinterpret_cast<uintptr_t>(storage.data() + 1);
    const auto last_packet = reinterpret_cast<uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
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
  VS::Menu::Framework::MenuRow* saved_rows = VS::Menu::Framework::menuItems;
  decltype(std::to_array(
      VS::Menu::Rename::renderRenameMenuStrings)) saved_strings =
      std::to_array(VS::Menu::Rename::renderRenameMenuStrings);
  VS::Menu::Rename::State saved_state = VS::Menu::Rename::LiveRenameState();
  void* saved_table = VS::Render::legacyOrderingTables[0];
  std::array<VS::Menu::Framework::MenuRow, 40> rows{};

  RenameFrameFixture() {
    VS::Menu::Framework::menuItems = rows.data();
    // The live chain lookups only accept the frame's own ordering table.
    VS::Render::legacyOrderingTables[0] = frame.ordering.data();
    VS::Render::FrameScratchpad().ordering_table = frame.ordering.data() + 24;
    VS::Render::FrameScratchpad().overlay_ordering_table =
        frame.ordering.data() + 8;
    std::fill_n(VS::Menu::Rename::renderRenameMenuStrings, saved_strings.size(),
                0xE7E7);
    for (int index = menu8_rename::index::kOk;
         index <= menu8_rename::index::kInsert; ++index) {
      VS::Menu::Rename::renderRenameMenuStrings[index] = 150;
    }
    VS::Menu::Rename::renderRenameMenuStrings[150] = 0xE70A;
    auto* grid = reinterpret_cast<unsigned char*>(
        VS::Menu::Rename::renderRenameMenuStrings +
        menu8_rename::offset::kCharTable);
    std::fill_n(grid, 15 * 9, 10);
    VS::Menu::Rename::State& state = VS::Menu::Rename::LiveRenameState();
    state.name.fill(VS::Core::TextCode::kSpace);
    state.name[0] = 10;
    state.selected_action = 0;
  }

  ~RenameFrameFixture() {
    VS::Menu::Framework::menuItems = saved_rows;
    VS::Render::legacyOrderingTables[0] = saved_table;
    std::copy(saved_strings.begin(), saved_strings.end(),
              VS::Menu::Rename::renderRenameMenuStrings);
    VS::Menu::Rename::LiveRenameState() = saved_state;
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
  VS::Actors::ActorRecord* saved_preview = VS::Actors::actorRecords[1];
  decltype(std::to_array(VS::Actors::weaponModels)) saved_models =
      std::to_array(VS::Actors::weaponModels);
  VS::Render::TextureSlot saved_texture =
      VS::Render::legacyWeaponTextureSlots[0];
  decltype(VS::Event::playbackPhase) saved_event_phase =
      VS::Event::playbackPhase;
  std::uint8_t saved_reload = VS::Content::objectDataLoadStep;
  int saved_alias = VS::Battle::previewShieldModelAlias;
  VS::Actors::ActorRecord preview{};
  VS::Actors::WeaponModel shield{};
  VS::Actors::WeaponModel preview_shield{};

  EquipmentReloadFixture(int category, bool has_preview, int unused_alias) {
    VS::Actors::actorRecords[1] = &preview;
    preview.sequence_model_id = static_cast<u_char>(category);
    std::fill_n(VS::Actors::weaponModels, saved_models.size(), nullptr);
    shield.bone_count = 1;
    VS::Actors::weaponModels[1] = &shield;
    VS::Actors::weaponModels[3] = has_preview ? &preview_shield : nullptr;
    VS::Render::legacyWeaponTextureSlots[0].reference_count = 2;
    VS::Event::playbackPhase = 0;
    VS::Content::objectDataLoadStep = 0;
    VS::Battle::previewShieldModelAlias = unused_alias;
  }

  ~EquipmentReloadFixture() {
    VS::Actors::actorRecords[1] = saved_preview;
    std::copy(saved_models.begin(), saved_models.end(),
              VS::Actors::weaponModels);
    VS::Render::legacyWeaponTextureSlots[0] = saved_texture;
    VS::Event::playbackPhase = saved_event_phase;
    VS::Content::objectDataLoadStep = saved_reload;
    VS::Battle::previewShieldModelAlias = saved_alias;
  }
};
}  // namespace

TEST_CASE("Equipment stat pages preserve every lower UI ordering entry",
          "[render][menu][equipment][links]") {
  EquipmentFixture fixture(GENERATE(0, 1, 2));
  VS::Menu::Framework::DrawLiveEquipmentStatPage(0);
  CHECK(fixture.ReachesLastEntry());
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Equipment page headers interpret encoded font controls as controls",
          "[render][menu][equipment][text]") {
  EquipmentFixture fixture(GENERATE(0, 1, 2));
  VS::Menu::Framework::DrawLiveEquipmentStatPage(0);
  CHECK(fixture.HeaderGlyphs() == 19);
}

TEST_CASE("Weapon naming frame retains its grid name commands and backdrops",
          "[render][menu][equipment][rename]") {
  const int offset = GENERATE(0, 32, 320);
  RenameFrameFixture fixture;
  REQUIRE(reinterpret_cast<std::uintptr_t>(fixture.frame.storage.data()) >
          UINT32_MAX);
  VS::Menu::Rename::DrawLiveFrame(offset);
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
  VS::Menu::Rename::DrawLiveSelectionBox({.x = 128, .y = 76, .w = 10, .h = 18});
  CHECK(fixture.frame.ReachesLastEntry());
  const auto packets = fixture.Packets();
  REQUIRE(packets.size() == 1);
  CHECK(packets[0][3] == 0x004C0080);
  CHECK(packets[0][9] == 0x005E008A);
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
  VS::Menu::Framework::SetLiveActorComparisonStats(mode);
  std::array<int16_t, 4> totals{};
  std::memcpy(totals.data(), VS::Menu::Framework::comparisonStatTotals,
              sizeof totals);
  const int16_t expected =
      static_cast<int16_t>(modifier + (mode == 0 ? 0 : 100));
  const int16_t expected_total = mode == 0 ? 0 : 100;
  const std::array<int16_t, 4> expected_totals{expected_total, expected_total,
                                               expected_total, 0};
  CHECK(totals == expected_totals);
  CHECK(VS::Menu::Framework::comparisonStatDeltas[0] == expected);
  CHECK(VS::Menu::Framework::comparisonStatDeltas[1] == expected);
  CHECK(VS::Menu::Framework::comparisonStatDeltas[2] == expected);
  CHECK(fixture.stats.strength == 100 + modifier);
}

TEST_CASE("Primary comparison columns apply actor and accessory values once",
          "[menu][equipment][stats]") {
  ComparisonFixture fixture;
  const bool weapon = GENERATE(false, true);
  const int modifier = GENERATE(-20, 0, 15);
  fixture.stats.strength = fixture.stats.intelligence = fixture.stats.agility =
      static_cast<int16_t>(100 + modifier);
  VS::Menu::Framework::SetLiveActorComparisonStats(weapon ? 16 : 0);
  if (weapon) VS::Menu::Framework::SetLivePrimaryStats(10, 1, -4, 1);
  std::array<int, 3> equipped{};
  for (size_t index = 0; index < equipped.size(); ++index) {
    equipped[index] = VS::Menu::Framework::strIntAgi[index] +
                      VS::Menu::Framework::comparisonStatDeltas[index] +
                      VS::Menu::Framework::comparisonAccessoryStats[index];
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
  CHECK(VS::Menu::Inventory::LiveIsTwoHandedWeapon(category) ==
        (two_handed ? 1 : 0));
  CHECK(VS::Menu::Inventory::ReloadLiveEquipmentModels(category, 0, 0) == 1);
  CHECK(VS::Content::objectDataLoadStep == 1);
  CHECK(VS::Actors::weaponModels[1] == (release ? nullptr : &fixture.shield));
  CHECK(VS::Actors::weaponModels[3] ==
        (has_preview ? &fixture.preview_shield : nullptr));
  CHECK(VS::Render::legacyWeaponTextureSlots[0].reference_count ==
        (release ? 1 : 2));
}

TEST_CASE("Actor equipment slots contain complete aligned native models",
          "[menu][equipment][model-storage]") {
  CHECK(sizeof(VS::Actors::ActorModelSlot) >= sizeof(VS::Actors::WeaponModel));
  CHECK(alignof(VS::Actors::ActorModelSlot) >=
        alignof(VS::Actors::WeaponModel));
  if (sizeof(VS::Actors::ActorModelSlot) < sizeof(VS::Actors::WeaponModel) ||
      alignof(VS::Actors::ActorModelSlot) < alignof(VS::Actors::WeaponModel)) {
    return;
  }
  const int index = GENERATE(0, 1);
  const bool clear = GENERATE(false, true);
  struct GuardedAllocation {
    VS::Actors::ActorAllocation actor;
    std::array<unsigned char, 64> guard;
  } allocation{};
  std::ranges::fill(std::as_writable_bytes(std::span(&allocation, 1)),
                    std::byte{0xA5});
  std::array<unsigned char, sizeof allocation> expected{};
  std::memcpy(expected.data(), &allocation, sizeof allocation);
  VS::Actors::WeaponModel model{};
  if (!clear) {
    model.model_id = 101;
    model.material = 5;
    model.bone_count = 1;
    model.reserved5e4 = 0x12345678;
  }
  const auto offset = offsetof(VS::Actors::ActorAllocation, model_slots) +
                      index * sizeof(VS::Actors::ActorModelSlot);
  std::memcpy(expected.data() + offset, &model, sizeof model);
  auto* slot = &allocation.actor.model_slots[index];
  if (clear) {
    VS::Core::Memory::ZeroLiveAlignedWords(slot, sizeof model);
  } else {
    VS::Core::Memory::CopyLiveAlignedWords(slot, &model, sizeof model);
  }
  std::array<unsigned char, sizeof allocation> actual{};
  std::memcpy(actual.data(), &allocation, sizeof allocation);
  CHECK(actual == expected);
}
