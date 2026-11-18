#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ActorAssets.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Inventory/EquipmentRows.hpp"
#include "Menu/Inventory/EquipmentRowsLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/TextBox.h"
}

// Contracts of the equipment menus' choice list (MENU3.PRG 16C.c:39-336) and
// preview model reload (68C4.c:12-127).
namespace {
namespace inv = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using VS::Content::ActorAssetRequest;
using VS::Content::AssetAddress;

constexpr std::uint32_t kTriangle = 0x10;
constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kCross = 0x40;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::size_t kRows = 32;

template <typename T>
struct Restore {
  T& target;
  T previous;
  explicit Restore(T& value) : target(value), previous(value) {}
  ~Restore() { target = previous; }
  Restore(const Restore&) = delete;
  Restore& operator=(const Restore&) = delete;
};

// Restores a C array global.
template <auto& Global>
struct GlobalArrayRestore {
  using Element =
      std::remove_extent_t<std::remove_reference_t<decltype(Global)>>;
  static constexpr std::size_t kCount =
      std::extent_v<std::remove_reference_t<decltype(Global)>>;
  std::array<Element, kCount> previous{};
  GlobalArrayRestore() { std::ranges::copy(Global, previous.begin()); }
  ~GlobalArrayRestore() { std::ranges::copy(previous, std::begin(Global)); }
  GlobalArrayRestore(const GlobalArrayRestore&) = delete;
  GlobalArrayRestore& operator=(const GlobalArrayRestore&) = delete;
};

// A row's flags word: unselectable bit, outset (biased by 100), material,
// equipped item and icon (16C.c:95-99).
[[nodiscard]] std::int32_t Flags(int unselectable, int outset, int material,
                                 int equipped, int icon) {
  return unselectable | (outset << 9) | (material << 16) | (equipped << 19) |
         (icon << 26);
}

struct Strings {
  std::array<std::uint8_t, inv::kEquipmentNameBytes> name{};
  std::array<std::uint8_t, inv::kEquipmentDescriptionBytes> description{};
  explicit Strings(std::uint8_t seed) {
    for (std::size_t i = 0; i < name.size(); ++i) {
      name[i] = static_cast<std::uint8_t>(seed + i);
    }
    for (std::size_t i = 0; i < description.size(); ++i) {
      description[i] =
          static_cast<std::uint8_t>(static_cast<std::size_t>(seed) * 3 + i);
    }
  }
};

class RecordingRowsHost final : public inv::EquipmentRowsHost {
 public:
  std::vector<fw::ItemText> storage;
  std::size_t allocation_limit = 1024;
  std::array<fw::MenuRow, kRows> rows{};
  std::vector<fw::RowPlacement> placements;
  std::vector<const std::uint8_t*> row_texts;
  std::vector<const std::uint8_t*> messages;
  std::vector<std::string> log;
  bool idle = false;
  std::int32_t cursor_result = 7;

  std::span<fw::ItemText> AllocateText(std::size_t count) override {
    log.emplace_back("alloc " + std::to_string(count));
    storage.assign(std::min(count, allocation_limit), fw::ItemText{});
    return storage;
  }
  void FreeText(std::span<fw::ItemText> text) override {
    log.emplace_back(text.data() == storage.data() ? "free" : "free other");
  }
  fw::MenuRow& SetRow(const fw::RowPlacement& placement,
                      std::span<const std::uint8_t> text) override {
    placements.push_back(placement);
    row_texts.push_back(text.data());
    auto& row = rows.at(static_cast<std::size_t>(placement.row));
    const std::uint8_t gradient = row.gradient_state;
    row = {};
    row.gradient_state = gradient;
    return row;
  }
  fw::MenuRow& Row(int row) override {
    return rows.at(static_cast<std::size_t>(row));
  }
  void ShowInformation(std::span<const std::uint8_t> message) override {
    messages.push_back(message.data());
  }
  bool RowsIdle() override { return idle; }
  void PlayLeaveSound() override { log.emplace_back("leave"); }
  void PlayInvalidSound() override { log.emplace_back("invalid"); }
  void PlayChangeSound() override { log.emplace_back("change"); }
  void PlayConfirmSound(std::int32_t effect) override {
    log.emplace_back("confirm " + std::to_string(effect));
  }
  std::int32_t DrawCursor(std::uint8_t animation,
                          std::int32_t position) override {
    log.emplace_back("cursor " + std::to_string(animation) + " " +
                     std::to_string(position));
    return cursor_result;
  }
};

// An open list of `count` records whose flags name each record's index as
// its equipped item.
struct OpenList {
  RecordingRowsHost host;
  inv::EquipmentRowsState state;
  explicit OpenList(int count, std::uint8_t step = inv::kEquipmentRowsInput) {
    host.storage.assign(static_cast<std::size_t>(count), fw::ItemText{});
    for (int i = 0; i < count; ++i) {
      host.storage[static_cast<std::size_t>(i)].flags = Flags(0, 0, 0, i, 0);
    }
    state.text = host.storage;
    state.row_count = static_cast<std::uint8_t>(count);
    state.step = step;
  }
  bool Update(std::uint32_t pressed, std::uint32_t repeat = 0,
              std::int32_t has_sfx = 1) {
    return inv::UpdateEquipmentChoices(state, host, {pressed, repeat}, has_sfx);
  }
};

[[nodiscard]] bool Has(const std::vector<std::string>& log,
                       const std::string& entry) {
  return std::ranges::find(log, entry) != log.end();
}

// A record CreateEquipmentRows copied from `source` (16C.c:60-93).
void CheckCopiedRecord(const fw::ItemText& record, const Strings& source,
                       std::int32_t flags) {
  CHECK(record.flags == flags);
  CHECK(
      std::equal(source.name.begin(), source.name.end(), record.name.begin()));
  CHECK(record.name[26] == 0xE7);
  CHECK(record.name[27] == 0xE7);
  CHECK(record.description[0] == 0xF8);
  CHECK(record.description[1] == 0);
  CHECK(std::equal(source.description.begin(), source.description.end(),
                   record.description.begin() + 2));
  CHECK(record.description[94] == 0xE7);
  CHECK(record.description[95] == 0xE7);
}

// The title row CreateEquipmentRows placed for a first record with the
// flags Flags(1, 0, 5, 9, 3).
void CheckTitlePlacement(const RecordingRowsHost& host,
                         const inv::EquipmentRowsState& state) {
  REQUIRE(host.placements.size() == 1);
  const fw::RowPlacement& title = host.placements[0];
  CHECK(title.row == 20);
  CHECK(title.x == 155);
  CHECK(title.y == 18);
  CHECK(title.w == 165);
  CHECK(title.background_width == 0);
  CHECK(host.row_texts[0] == state.text[0].name.data());
}

void CheckTitleRow(const RecordingRowsHost& host) {
  const fw::MenuRow& row = host.rows[20];
  CHECK(row.unselectable == 1);
  CHECK(row.row_icon == 3);
  CHECK(row.material == 5);
  CHECK(row.selected == 1);
}

TEST_CASE(
    "CreateEquipmentRows copies names, descriptions and flags into owned "
    "records",
    "[equipment-rows]") {
  const Strings first(1);
  const Strings second(40);
  const std::array rows{
      inv::EquipmentRowSource{first.name, first.description,
                              Flags(1, 0, 5, 9, 3)},
      inv::EquipmentRowSource{second.name, second.description, 0x1234}};
  RecordingRowsHost host;
  inv::EquipmentRowsState state;
  state.first_row = 4;
  state.selected_row = 3;
  REQUIRE(inv::CreateEquipmentRows(state, host, rows));
  CHECK(host.log == std::vector<std::string>{"alloc 2"});
  CHECK(state.row_count == 2);
  CHECK(state.first_row == 0);
  CHECK(state.selected_row == 0);
  CHECK(state.step == 1);
  CHECK(state.equipped_item == 9);
  REQUIRE(state.text.size() == 2);
  CheckCopiedRecord(state.text[0], first, rows[0].flags);
  CheckCopiedRecord(state.text[1], second, rows[1].flags);
  CheckTitlePlacement(host, state);
  CheckTitleRow(host);
}

TEST_CASE("CreateEquipmentRows marks missing strings with terminators",
          "[equipment-rows]") {
  const std::array rows{inv::EquipmentRowSource{{}, {}, 0}};
  RecordingRowsHost host;
  inv::EquipmentRowsState state;
  REQUIRE(inv::CreateEquipmentRows(state, host, rows));
  const fw::ItemText& record = state.text[0];
  CHECK(record.name[0] == 0xE7);
  CHECK(record.name[1] == 0xE7);
  CHECK(record.name[26] == 0);
  CHECK(record.description[0] == 0xE7);
  CHECK(record.description[1] == 0xE7);
  CHECK(record.description[94] == 0);
}

TEST_CASE("CreateEquipmentRows refuses empty, oversized and short rows",
          "[equipment-rows]") {
  RecordingRowsHost host;
  inv::EquipmentRowsState state;
  state.step = 9;
  CHECK_FALSE(inv::CreateEquipmentRows(state, host, {}));
  const std::vector<inv::EquipmentRowSource> many(inv::kEquipmentRowCapacity +
                                                  1);
  CHECK_FALSE(inv::CreateEquipmentRows(state, host, many));
  const std::array<std::uint8_t, 25> short_name{};
  const std::array rows{inv::EquipmentRowSource{short_name, {}, 0}};
  CHECK_FALSE(inv::CreateEquipmentRows(state, host, rows));
  CHECK(state.step == 9);
  CHECK(host.log.empty());
}

TEST_CASE("CreateEquipmentRows leaves no text after a short allocation",
          "[equipment-rows]") {
  RecordingRowsHost host;
  host.allocation_limit = 1;
  const std::array rows{inv::EquipmentRowSource{}, inv::EquipmentRowSource{}};
  inv::EquipmentRowsState state;
  CHECK_FALSE(inv::CreateEquipmentRows(state, host, rows));
  CHECK(state.text.data() == nullptr);
  CHECK(host.placements.empty());
}

TEST_CASE("SelectedEquipmentRow waits while the list holds its text",
          "[equipment-rows]") {
  OpenList list(3);
  list.state.selected_row = 2;
  CHECK(inv::SelectedEquipmentRow(list.state) == -1);
  list.state.text = {};
  CHECK(inv::SelectedEquipmentRow(list.state) == 2);
  list.state.selected_row = inv::kEquipmentChoiceBack;
  CHECK(inv::SelectedEquipmentRow(list.state) == -3);
}

TEST_CASE("UpdateEquipmentChoices slides one row in per step",
          "[equipment-rows]") {
  OpenList list(3, 1);
  REQUIRE(list.Update(0));
  CHECK(list.state.step == 2);
  REQUIRE(list.host.placements.size() == 1);
  const fw::RowPlacement& placement = list.host.placements[0];
  CHECK(placement.row == 21);
  CHECK(placement.x == 320);
  CHECK(placement.y == 34);
  CHECK(placement.w == 151);
  CHECK(list.host.row_texts[0] == list.host.storage[1].name.data());
}

TEST_CASE("UpdateEquipmentChoices finishes sliding after the last row",
          "[equipment-rows]") {
  OpenList list(3, 2);
  REQUIRE(list.Update(0));
  CHECK(list.host.rows[22].outset_icon == 0);
  CHECK(list.state.step == 3);
  REQUIRE(list.Update(0));
  CHECK(list.state.step == inv::kEquipmentRowsWaiting);
  CHECK(list.host.placements.size() == 1);
}

TEST_CASE("UpdateEquipmentChoices starts a slid row with the record's flags",
          "[equipment-rows]") {
  OpenList list(3, 1);
  list.host.storage[1].flags = Flags(1, 104, 6, 0, 2);
  REQUIRE(list.Update(0));
  const fw::MenuRow& row = list.host.rows[21];
  CHECK(row.state == fw::RowState::kSlideX);
  CHECK(row.target0 == 169);
  CHECK(row.unselectable == 1);
  CHECK(row.row_icon == 2);
  CHECK(row.outset_icon == 4);
  CHECK(row.material == 6);
}

TEST_CASE(
    "UpdateEquipmentChoices marks a long list scrollable after the sixth row",
    "[equipment-rows]") {
  OpenList long_list(7, 5);
  REQUIRE(long_list.Update(0));
  CHECK(long_list.state.step == inv::kEquipmentRowsWaiting);
  CHECK(long_list.host.rows[25].is_scrollable == 2);
  OpenList full_list(6, 5);
  REQUIRE(full_list.Update(0));
  CHECK(full_list.state.step == inv::kEquipmentRowsWaiting);
  CHECK(full_list.host.rows[25].is_scrollable == 0);
}

TEST_CASE("UpdateEquipmentChoices waits for idle rows before taking input",
          "[equipment-rows]") {
  OpenList list(8, inv::kEquipmentRowsWaiting);
  list.state.first_row = 1;
  list.state.selected_row = 2;
  REQUIRE(list.Update(kCross));
  CHECK(list.state.step == inv::kEquipmentRowsWaiting);
  REQUIRE(list.host.messages.size() == 1);
  CHECK(list.host.messages[0] == list.host.storage[3].description.data());
  list.host.idle = true;
  REQUIRE(list.Update(kCross));
  CHECK(list.state.step == inv::kEquipmentRowsInput);
  CHECK(list.host.log.empty());
}

TEST_CASE("UpdateEquipmentChoices closes the list for Triangle",
          "[equipment-rows]") {
  OpenList back(3);
  REQUIRE(back.Update(kTriangle | kCircle));
  CHECK(back.host.log == std::vector<std::string>{"leave", "free"});
  CHECK(back.state.selected_row == inv::kEquipmentChoiceBack);
  CHECK(back.state.text.data() == nullptr);
  OpenList silent(3);
  REQUIRE(silent.Update(kTriangle, 0, 2));
  CHECK(silent.host.log == std::vector<std::string>{"free"});
}

TEST_CASE("UpdateEquipmentChoices cancels the list for Cross",
          "[equipment-rows]") {
  OpenList cancel(3);
  cancel.host.rows[20].selected = 1;
  REQUIRE(cancel.Update(kCross));
  CHECK(cancel.host.log == std::vector<std::string>{"leave", "free"});
  CHECK(cancel.state.selected_row == inv::kEquipmentChoiceCancel);
  CHECK(cancel.state.text.data() == nullptr);
  CHECK(cancel.host.rows[20].selected == 0);
}

TEST_CASE("UpdateEquipmentChoices confirms the selected row with Circle",
          "[equipment-rows]") {
  OpenList first(3);
  REQUIRE(first.Update(kCircle));
  CHECK(first.host.log == std::vector<std::string>{"free", "confirm 34"});
  CHECK(first.state.selected_row == 0);
  CHECK(first.state.text.data() == nullptr);
  OpenList scrolled(9);
  scrolled.state.first_row = 2;
  scrolled.state.selected_row = 3;
  REQUIRE(scrolled.Update(kCircle | kCross));
  CHECK(scrolled.host.log == std::vector<std::string>{"free", "confirm 33"});
  CHECK(scrolled.state.selected_row == 5);
}

TEST_CASE("UpdateEquipmentChoices confirms silently without menu sounds",
          "[equipment-rows]") {
  OpenList quiet(3);
  quiet.state.selected_row = 1;
  REQUIRE(quiet.Update(kCircle, 0, 0));
  CHECK(quiet.host.log == std::vector<std::string>{"free"});
  CHECK(quiet.state.selected_row == 1);
}

TEST_CASE(
    "UpdateEquipmentChoices rejects an unselectable row with the invalid sound",
    "[equipment-rows]") {
  OpenList list(3);
  list.state.selected_row = 1;
  list.host.rows[21].unselectable = 1;
  list.state.cursor_animation = 4;
  REQUIRE(list.Update(kCircle));
  CHECK(list.host.log ==
        std::vector<std::string>{
            "invalid", "cursor 4 " + std::to_string((26 << 16) | 0x9B)});
  CHECK(list.state.text.data() != nullptr);
  CHECK(list.host.rows[21].selected == 1);
  CHECK(list.state.cursor_animation == 7);
}

TEST_CASE("UpdateEquipmentChoices moves the cursor down a short list",
          "[equipment-rows]") {
  OpenList list(4);
  list.host.storage[1].flags = Flags(0, 0, 0, 77, 0);
  REQUIRE(list.Update(0, kDown));
  CHECK(list.state.selected_row == 1);
  CHECK(list.state.equipped_item == 77);
  CHECK(list.host.rows[21].selected == 1);
  CHECK(list.host.log ==
        std::vector<std::string>{
            "change", "cursor 0 " + std::to_string((26 << 16) | 0x9B)});
  list.state.selected_row = 3;
  REQUIRE(list.Update(0, kDown));
  CHECK(list.state.selected_row == 3);
}

TEST_CASE("UpdateEquipmentChoices moves the cursor up a short list",
          "[equipment-rows]") {
  OpenList list(4);
  list.state.selected_row = 1;
  list.state.cursor_animation = 7;
  REQUIRE(list.Update(0, kUp));
  CHECK(list.state.selected_row == 0);
  CHECK(list.host.log.back() ==
        "cursor 7 " + std::to_string(((10 << 16) | 0x9B) - 14));
  list.host.log.clear();
  REQUIRE(list.Update(0, kUp));
  CHECK(list.state.selected_row == 0);
  CHECK_FALSE(Has(list.host.log, "change"));
}

// A long list scrolled down one row by Down from row 4 (16C.c:242-273).
struct ScrolledList {
  OpenList list{8};
  ScrolledList() {
    list.state.selected_row = 4;
    for (std::size_t i = 1; i < 6; ++i) {
      list.host.rows[20 + i].gradient_state = static_cast<std::uint8_t>(10 + i);
    }
    list.host.storage[3].flags = Flags(1, 101, 2, 3, 5);
    REQUIRE(list.Update(0, kDown));
  }
};

TEST_CASE("UpdateEquipmentChoices scrolls a long list down one row",
          "[equipment-rows]") {
  ScrolledList scrolled;
  const OpenList& list = scrolled.list;
  CHECK(list.state.first_row == 1);
  CHECK(list.state.selected_row == 4);
  CHECK(list.state.equipped_item == 5);
  CHECK(list.host.rows[21].is_scrollable == 1);
  CHECK(list.host.rows[25].is_scrollable == 2);
  CHECK(list.host.rows[24].selected == 1);
}

TEST_CASE("UpdateEquipmentChoices redraws rows one to five after scrolling",
          "[equipment-rows]") {
  ScrolledList scrolled;
  const OpenList& list = scrolled.list;
  REQUIRE(list.host.placements.size() == 5);
  for (std::size_t i = 1; i < 6; ++i) {
    const fw::RowPlacement& placement = list.host.placements[i - 1];
    CHECK(placement.row == static_cast<int>(20 + i));
    CHECK(placement.x == 169);
    CHECK(placement.y == static_cast<int>(18 + 16 * i));
    CHECK(list.host.row_texts[i - 1] == list.host.storage[i + 1].name.data());
  }
}

TEST_CASE("UpdateEquipmentChoices keeps row gradients and flags when scrolling",
          "[equipment-rows]") {
  ScrolledList scrolled;
  const OpenList& list = scrolled.list;
  CHECK(list.host.rows[21].gradient_state == 12);
  CHECK(list.host.rows[24].gradient_state == 15);
  CHECK(list.host.rows[25].gradient_state == 0);
  CHECK(list.host.rows[22].unselectable == 1);
  CHECK(list.host.rows[22].outset_icon == 1);
  CHECK(list.host.rows[22].row_icon == 5);
  CHECK(list.host.rows[22].material == 2);
}

TEST_CASE("UpdateEquipmentChoices stops scrolling at either end of the list",
          "[equipment-rows]") {
  ScrolledList scrolled;
  OpenList& list = scrolled.list;
  list.state.first_row = 2;
  list.host.placements.clear();
  REQUIRE(list.Update(0, kDown));
  CHECK(list.state.selected_row == 5);
  CHECK(list.host.placements.empty());
  list.state.selected_row = 2;
  REQUIRE(list.Update(0, kUp));
  CHECK(list.state.first_row == 1);
  CHECK(list.host.rows[25].is_scrollable == 2);
}

TEST_CASE("UpdateEquipmentChoices refuses positions outside the list",
          "[equipment-rows]") {
  OpenList past(3);
  past.state.selected_row = 3;
  CHECK_FALSE(past.Update(0));
  OpenList beyond(8);
  beyond.state.first_row = 3;
  CHECK_FALSE(beyond.Update(0));
  OpenList negative(3);
  negative.state.selected_row = -1;
  CHECK_FALSE(negative.Update(0));
  OpenList short_text(3);
  short_text.state.text = short_text.state.text.first(2);
  CHECK_FALSE(short_text.Update(0));
  OpenList sliding(2, 3);
  CHECK_FALSE(sliding.Update(0));
  CHECK(sliding.state.step == 3);
  CHECK(past.host.log.empty());
  CHECK(past.host.messages.empty());
}

class RecordingAccessoryHost final : public inv::AccessoryPreviewHost {
 public:
  std::int32_t slots = 4;
  inv::AccessoryPreview stats;
  std::optional<inv::AccessoryStatColumns> columns;
  std::vector<std::string> log;

  bool HasAccessory(std::int32_t slot) override {
    return slot >= 0 && slot < slots;
  }
  void ResetStats() override { log.emplace_back("reset"); }
  inv::AccessoryPreview LoadAccessory(std::int32_t slot) override {
    log.emplace_back("load " + std::to_string(slot));
    return stats;
  }
  void StoreStatColumns(const inv::AccessoryStatColumns& stored) override {
    columns = stored;
  }
  void SetPrimaryStats(std::int32_t strength, std::int32_t intelligence,
                       std::int32_t agility, std::int32_t known) override {
    log.emplace_back("primary " + std::to_string(strength) + " " +
                     std::to_string(intelligence) + " " +
                     std::to_string(agility) + " " + std::to_string(known));
  }
  void SetStatusItem(std::uint8_t subtype, std::uint8_t item) override {
    log.emplace_back("item " + std::to_string(subtype) + " " +
                     std::to_string(item));
  }
  void SetStatPages(std::int32_t pages) override {
    log.emplace_back("pages " + std::to_string(pages));
  }
};

// The columns spread from classes 1-8, affinities 20-27 and types 100, 101,
// 102 and 0xFFFF; the type columns past the four types stay zero (#372).
void CheckSpreadColumns(const inv::AccessoryStatColumns& columns) {
  for (std::size_t i = 0; i < 16; ++i) {
    CHECK(columns.classes[i] == static_cast<std::int16_t>((i & 7) + 1));
    CHECK(columns.affinities[i] == static_cast<std::int16_t>(20 + (i & 7)));
  }
  CHECK(columns.types[0] == 100);
  CHECK(columns.types[1] == 101);
  CHECK(columns.types[3] == -1);
  for (std::size_t i = 4; i < 16; ++i) {
    CHECK(columns.types[i] == 0);
  }
}

TEST_CASE("SetAccessoryPreviewStats spreads the accessory's stat columns",
          "[equipment-rows]") {
  RecordingAccessoryHost host;
  host.stats.strength = -3;
  host.stats.intelligence = 4;
  host.stats.agility = 5;
  host.stats.types = {100, 101, 102, 0xFFFF};
  for (std::size_t i = 0; i < 8; ++i) {
    host.stats.classes[i] = static_cast<std::uint16_t>(i + 1);
    host.stats.affinities[i] = static_cast<std::uint16_t>(20 + i);
  }
  REQUIRE(inv::SetAccessoryPreviewStats(host, 4));
  CHECK(host.log == std::vector<std::string>{"reset", "load 3",
                                             "primary -3 4 5 2", "item 32 4",
                                             "pages 7"});
  REQUIRE(host.columns.has_value());
  CheckSpreadColumns(*host.columns);
}

TEST_CASE("SetAccessoryPreviewStats with no accessory only resets the panel",
          "[equipment-rows]") {
  RecordingAccessoryHost host;
  REQUIRE(inv::SetAccessoryPreviewStats(host, 0));
  CHECK(host.log == std::vector<std::string>{"reset", "item 32 0", "pages 7"});
  CHECK_FALSE(host.columns.has_value());
}

TEST_CASE("SetAccessoryPreviewStats refuses slots outside the armor table",
          "[equipment-rows]") {
  RecordingAccessoryHost host;
  CHECK_FALSE(inv::SetAccessoryPreviewStats(host, 5));
  CHECK_FALSE(inv::SetAccessoryPreviewStats(host, -1));
  CHECK(host.log.empty());
}

class RecordingModelHost final : public inv::EquipmentModelHost {
 public:
  bool actor = true;
  std::uint8_t sequence_model = 0;
  std::uint8_t animation = 0;
  std::array<bool, 4> weapon_models{};
  std::array<std::optional<AssetAddress>, 2> slots{AssetAddress{0x1000},
                                                   AssetAddress{0x2000}};
  bool busy = false;
  std::vector<ActorAssetRequest> requests;
  std::vector<std::string> log;

  bool HasPreviewActor() override { return actor; }
  std::uint8_t PreviewSequenceModel() override { return sequence_model; }
  std::uint8_t PreviewAnimation() override { return animation; }
  bool HasWeaponModel(int slot) override {
    return weapon_models.at(static_cast<std::size_t>(slot));
  }
  std::optional<AssetAddress> EquipmentModelSlot(int index) override {
    return slots.at(static_cast<std::size_t>(index));
  }
  void ReleaseWeaponModel(int slot) override {
    log.emplace_back("release " + std::to_string(slot));
  }
  void QueueWeaponAnimations(std::int32_t sequence) override {
    log.emplace_back("animations " + std::to_string(sequence));
  }
  void QueueAsset(const ActorAssetRequest& request) override {
    requests.push_back(request);
    log.emplace_back("queue");
  }
  void SetWeaponDrawn(bool drawn) override {
    log.emplace_back(drawn ? "drawn" : "sheathed");
  }
  bool AssetQueueBusy() override { return busy; }
  void ResetEquipmentPose() override { log.emplace_back("equipment pose"); }
  void ReleasePreview() override { log.emplace_back("release preview"); }
  void ClonePreview(int source) override {
    log.emplace_back("clone " + std::to_string(source));
  }
  void ResetPreviewPose() override { log.emplace_back("preview pose"); }
  void SheathePreviewWeapon() override { log.emplace_back("sheathe"); }
  void PlayPreviewAnimation(int played) override {
    log.emplace_back("play " + std::to_string(played));
  }
  void AdvancePreviewSkeleton() override { log.emplace_back("advance"); }
  void CopyPreviewPoseToBlend() override { log.emplace_back("blend"); }
  void ClearWeaponModelFlag(int slot) override {
    log.emplace_back("clear " + std::to_string(slot));
  }
};

TEST_CASE("ReloadEquipmentModels swaps a shield and queues its model",
          "[equipment-rows]") {
  RecordingModelHost host;
  host.sequence_model = 3;
  host.weapon_models[3] = true;
  std::uint8_t step = 0;
  const inv::EquipmentModelChoice shield{0, 100, 2};
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kLoading);
  CHECK(step == 1);
  CHECK(host.log == std::vector<std::string>{"release 1", "release 0",
                                             "animations 10", "queue",
                                             "sheathed"});
  REQUIRE(host.requests.size() == 1);
  CHECK(host.requests[0].target_slot == 1);
}

TEST_CASE("ReloadEquipmentModels waits for the assets and the idle preview",
          "[equipment-rows]") {
  RecordingModelHost host;
  std::uint8_t step = 1;
  const inv::EquipmentModelChoice shield{0, 100, 2};
  host.busy = true;
  host.animation = 1;
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kLoading);
  host.busy = false;
  host.animation = 2;
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kLoading);
  CHECK(step == 1);
  host.animation = 1;
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kLoading);
  CHECK(step == 2);
  CHECK(host.log == std::vector<std::string>{"equipment pose"});
}

TEST_CASE("ReloadEquipmentModels rebuilds the preview and waits until settled",
          "[equipment-rows]") {
  RecordingModelHost host;
  host.weapon_models[3] = true;
  std::uint8_t step = 2;
  const inv::EquipmentModelChoice shield{0, 100, 2};
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kLoading);
  CHECK(step == 3);
  CHECK(host.log == std::vector<std::string>{
                        "release preview", "clone 0", "preview pose", "sheathe",
                        "play 1", "advance", "blend", "clear 3", "drawn"});
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kLoading);
  host.animation = inv::kPreviewSettledAnimation;
  CHECK(inv::ReloadEquipmentModels(step, host, shield) ==
        inv::ModelReload::kDone);
  CHECK(step == 0);
}

TEST_CASE("ReloadEquipmentModels keeps a one-handed weapon for a shield",
          "[equipment-rows]") {
  RecordingModelHost host;
  host.sequence_model = 2;
  std::uint8_t step = 0;
  CHECK(inv::ReloadEquipmentModels(step, host, {0, 100, 0}) ==
        inv::ModelReload::kLoading);
  CHECK(host.log == std::vector<std::string>{"release 1", "queue", "sheathed"});
  RecordingModelHost removed;
  removed.sequence_model = 3;
  step = 0;
  CHECK(inv::ReloadEquipmentModels(step, removed, {0, 0, 0}) ==
        inv::ModelReload::kLoading);
  CHECK(removed.log == std::vector<std::string>{"release 1", "sheathed"});
}

TEST_CASE("ReloadEquipmentModels releases the shield for a two-handed weapon",
          "[equipment-rows]") {
  RecordingModelHost host;
  host.sequence_model = 2;
  host.weapon_models[3] = true;
  std::uint8_t step = 0;
  CHECK(inv::ReloadEquipmentModels(step, host, {8, 7, 1}) ==
        inv::ModelReload::kLoading);
  CHECK(host.log == std::vector<std::string>{"animations 8", "release 0",
                                             "release 1", "queue", "sheathed"});
  RecordingModelHost same;
  same.sequence_model = 4;
  same.weapon_models[3] = true;
  step = 0;
  CHECK(inv::ReloadEquipmentModels(step, same, {4, 7, 1}) ==
        inv::ModelReload::kLoading);
  CHECK(same.log == std::vector<std::string>{"release 0", "queue", "sheathed"});
}

TEST_CASE("ReloadEquipmentModels refuses without a preview actor or model slot",
          "[equipment-rows]") {
  RecordingModelHost host;
  host.actor = false;
  std::uint8_t step = 2;
  CHECK(inv::ReloadEquipmentModels(step, host, {1, 5, 0}) ==
        inv::ModelReload::kRefused);
  CHECK(step == 2);
  RecordingModelHost missing;
  missing.slots[0] = std::nullopt;
  step = 0;
  CHECK(inv::ReloadEquipmentModels(step, missing, {1, 5, 0}) ==
        inv::ModelReload::kRefused);
  CHECK(step == 0);
  CHECK(missing.log.empty());
  CHECK(host.log.empty());
}

TEST_CASE("IsTwoHandedWeapon classifies weapon categories",
          "[equipment-rows]") {
  CHECK_FALSE(inv::IsTwoHandedWeapon(-5));
  CHECK_FALSE(inv::IsTwoHandedWeapon(1));
  CHECK_FALSE(inv::IsTwoHandedWeapon(2));
  CHECK(inv::IsTwoHandedWeapon(3));
  CHECK_FALSE(inv::IsTwoHandedWeapon(4));
  CHECK(inv::IsTwoHandedWeapon(7));
  CHECK(inv::IsTwoHandedWeapon(8));
  CHECK(inv::IsTwoHandedWeapon(9));
  CHECK_FALSE(inv::IsTwoHandedWeapon(10));
}

TEST_CASE("QueueEquipmentModel requests the weapon or shield model",
          "[equipment-rows]") {
  RecordingModelHost host;
  REQUIRE(inv::QueueEquipmentModel(host, 95, 0x105));
  REQUIRE(inv::QueueEquipmentModel(host, 96, 3));
  REQUIRE(host.requests.size() == 2);
  ActorAssetRequest weapon{};
  weapon.kind = 3;
  weapon.target_slot = 0;
  weapon.model_id = 95;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  weapon.destination = AssetAddress{0x1000};
  weapon.texture_slot = 240;
  weapon.material = 5;
  CHECK(host.requests[0] == weapon);
  ActorAssetRequest shield = weapon;
  shield.target_slot = 1;
  shield.model_id = 96;
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  shield.destination = AssetAddress{0x2000};
  shield.texture_slot = 241;
  shield.material = 3;
  CHECK(host.requests[1] == shield);
}

TEST_CASE("QueueEquipmentModel skips model zero and refuses a missing slot",
          "[equipment-rows]") {
  RecordingModelHost host;
  host.slots = {std::nullopt, std::nullopt};
  CHECK(inv::QueueEquipmentModel(host, 0, 1));
  CHECK_FALSE(inv::QueueEquipmentModel(host, 5, 1));
  CHECK_FALSE(inv::QueueEquipmentModel(host, -4, 1));
  CHECK(host.requests.empty());
}

// The live list globals, rows and a game heap for the list text.
struct LiveRowsFixture {
  Restore<char*> saved_text{Menu_Inventory_itemsText};
  Restore<int> saved_selected{Menu_Inventory_selectedItemRow};
  Restore<u_char> saved_step{Menu_Inventory_equipListStep};
  Restore<char> saved_count{Menu_Inventory_menuRowCount};
  Restore<char> saved_first{Menu_Inventory_itemsFirstRow};
  Restore<char> saved_equipped{Menu_Inventory_equippedItem};
  Restore<Menu_Framework_MenuRow*> saved_rows{Menu_Framework_menuItems};
  Restore<decltype(Input_buttonsPressed)> saved_pressed{Input_buttonsPressed};
  Restore<int> saved_repeat{Input_buttonRepeat};
  GlobalArrayRestore<Menu_textBoxes> saved_text_boxes;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 256> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<Menu_Framework_MenuRow, 40> rows{};
  Strings first{1};
  Strings second{60};
  std::array<u_short*, 4> texts{};
  std::array<int, 2> flags{Flags(0, 0, 3, 1, 4), Flags(1, 0, 0, 2, 0)};

  LiveRowsFixture() {
    Menu_Framework_menuItems = rows.data();
    Menu_Inventory_itemsText = nullptr;
    Input_buttonsPressed.allButtons = 0;
    Input_buttonRepeat = 0;
    texts[0] = reinterpret_cast<u_short*>(first.name.data());
    texts[1] = reinterpret_cast<u_short*>(first.description.data());
    texts[2] = reinterpret_cast<u_short*>(second.name.data());
    texts[3] = nullptr;
    inv::ResetEquipmentRowsLive();
  }
  ~LiveRowsFixture() {
    if (Menu_Inventory_itemsText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(Menu_Inventory_itemsText);
    }
    inv::ResetEquipmentRowsLive();
  }
  LiveRowsFixture(const LiveRowsFixture&) = delete;
  LiveRowsFixture& operator=(const LiveRowsFixture&) = delete;
};

TEST_CASE("LiveCreateEquipmentRows allocates the list text from the game heap",
          "[equipment-rows-live]") {
  LiveRowsFixture fixture;
  Menu_Inventory_itemsFirstRow = 3;
  inv::LiveCreateEquipmentRows(2, fixture.texts.data(), fixture.flags.data());
  REQUIRE(Menu_Inventory_itemsText != nullptr);
  CHECK(VS::Core::Memory::LivePayloadContaining(Menu_Inventory_itemsText)
            .has_value());
  CHECK(Menu_Inventory_menuRowCount == 2);
  CHECK(Menu_Inventory_itemsFirstRow == 0);
  CHECK(Menu_Inventory_selectedItemRow == 0);
  CHECK(Menu_Inventory_equipListStep == 1);
  CHECK(Menu_Inventory_equippedItem == 1);
  CHECK(inv::LiveSelectedEquipmentRow() == -1);
}

TEST_CASE("LiveCreateEquipmentRows copies the records and sets the title row",
          "[equipment-rows-live]") {
  LiveRowsFixture fixture;
  inv::LiveCreateEquipmentRows(2, fixture.texts.data(), fixture.flags.data());
  REQUIRE(Menu_Inventory_itemsText != nullptr);
  const auto* text =
      reinterpret_cast<const fw::ItemText*>(Menu_Inventory_itemsText);
  CHECK(text[0].name[0] == 1);
  CHECK(text[1].name[0] == 60);
  CHECK(text[1].description[0] == 0xE7);
  CHECK(text[1].flags == fixture.flags[1]);
  const fw::MenuRow& row = fw::LiveMenuRow(20);
  CHECK(row.row_icon == 4);
  CHECK(row.material == 3);
  CHECK(row.selected == 1);
}

TEST_CASE("LiveCreateEquipmentRows refuses counts outside the list capacity",
          "[equipment-rows-live]") {
  LiveRowsFixture fixture;
  Menu_Inventory_equipListStep = 9;
  const std::uint64_t before = inv::LiveEquipmentRowsRefusalCount();
  inv::LiveCreateEquipmentRows(0, fixture.texts.data(), fixture.flags.data());
  inv::LiveCreateEquipmentRows(65, fixture.texts.data(), fixture.flags.data());
  inv::LiveCreateEquipmentRows(2, nullptr, fixture.flags.data());
  CHECK(inv::LiveEquipmentRowsRefusalCount() == before + 3);
  CHECK(Menu_Inventory_itemsText == nullptr);
  CHECK(Menu_Inventory_equipListStep == 9);
}

TEST_CASE("LiveSelectedEquipmentRow reports the choice once the text is freed",
          "[equipment-rows-live]") {
  LiveRowsFixture fixture;
  Menu_Inventory_selectedItemRow = -2;
  CHECK(inv::LiveSelectedEquipmentRow() == -2);
  Menu_Inventory_selectedItemRow = 4;
  CHECK(inv::LiveSelectedEquipmentRow() == 4);
}

TEST_CASE("UpdateLiveEquipmentChoices slides rows in and closes on Cross",
          "[equipment-rows-live]") {
  LiveRowsFixture fixture;
  inv::LiveCreateEquipmentRows(2, fixture.texts.data(), fixture.flags.data());
  inv::UpdateLiveEquipmentChoices(0);
  CHECK(Menu_Inventory_equipListStep == 2);
  const fw::MenuRow& row = fw::LiveMenuRow(21);
  CHECK(row.state == fw::RowState::kSlideX);
  CHECK(row.target0 == 169);
  CHECK(row.unselectable == 1);
  inv::UpdateLiveEquipmentChoices(0);
  CHECK(Menu_Inventory_equipListStep == inv::kEquipmentRowsWaiting);
  Menu_Inventory_equipListStep = inv::kEquipmentRowsInput;
  Input_buttonsPressed.allButtons = kCross;
  inv::UpdateLiveEquipmentChoices(0);
  CHECK(Menu_Inventory_itemsText == nullptr);
  CHECK(Menu_Inventory_selectedItemRow == inv::kEquipmentChoiceCancel);
  CHECK(inv::LiveSelectedEquipmentRow() == inv::kEquipmentChoiceCancel);
}

TEST_CASE("UpdateLiveEquipmentChoices refuses text outside the game heap",
          "[equipment-rows-live]") {
  LiveRowsFixture fixture;
  alignas(fw::ItemText) std::array<std::byte, 2 * sizeof(fw::ItemText)>
      foreign{};
  Menu_Inventory_itemsText = reinterpret_cast<char*>(foreign.data());
  Menu_Inventory_menuRowCount = 2;
  Menu_Inventory_equipListStep = 1;
  const std::uint64_t before = inv::LiveEquipmentRowsRefusalCount();
  inv::UpdateLiveEquipmentChoices(0);
  CHECK(inv::LiveEquipmentRowsRefusalCount() == before + 1);
  CHECK(Menu_Inventory_equipListStep == 1);
  CHECK(fw::LiveMenuRow(21).state == fw::RowState::kInactive);
  Menu_Inventory_itemsText = nullptr;
}

struct LiveAccessoryFixture {
  Restore<Items_InventoryArmor*> saved_armor{Menu_Framework_armor};
  Restore<Items_Inventory> saved_inventory{Audio_Seq_unusedInventory};
  Restore<char> saved_pages{Menu_Framework_enabledStatPages};
  Restore<char> saved_page{Menu_Framework_itemStatPage};
  Restore<char> saved_subtype{Menu_Framework_equipmentSubtype};
  Restore<char> saved_item{Menu_Framework_currentStatusViewItem};
  Restore<u_char> saved_selection{Menu_Framework_selectedStatusViewElement};
  GlobalArrayRestore<Menu_Framework_equipmentStats> saved_stats;
  GlobalArrayRestore<Menu_Framework_strIntAgi> saved_primary;
  GlobalArrayRestore<Menu_Framework_rangeRiskData> saved_range;
  GlobalArrayRestore<Menu_Framework_comparisonStatDeltas> saved_deltas;
  GlobalArrayRestore<Menu_Framework_comparisonStatTotals> saved_totals;
  GlobalArrayRestore<Menu_Framework_comparisonAccessoryStats> saved_accessory;
};

TEST_CASE("SetLiveAccessoryPreviewStats spreads Ashley's accessory",
          "[equipment-rows-live]") {
  LiveAccessoryFixture fixture;
  Items_InventoryArmor& accessory = Audio_Seq_unusedInventory.armor[1];
  accessory = {};
  accessory.id = 1;
  accessory.classes[0] = 6;
  accessory.affinities[2] = -4;
  accessory.types[1] = -7;
  Menu_Framework_armor = Audio_Seq_unusedInventory.armor;
  inv::SetLiveAccessoryPreviewStats(2);
  CHECK(Menu_Framework_equipmentStats[0] == 6);
  CHECK(Menu_Framework_equipmentStats[8] == 6);
  CHECK(Menu_Framework_equipmentStats[16 + 2] == -4);
  CHECK(Menu_Framework_equipmentStats[32 + 1] == -7);
  CHECK(Menu_Framework_equipmentStats[32 + 4] == 0);
  CHECK(Menu_Framework_equipmentSubtype == 32);
  CHECK(Menu_Framework_currentStatusViewItem == 2);
}

TEST_CASE("SetLiveAccessoryPreviewStats refuses an unbound armor table",
          "[equipment-rows-live]") {
  LiveAccessoryFixture fixture;
  std::array<Items_InventoryArmor, 4> foreign{};
  Menu_Framework_armor = foreign.data();
  Menu_Framework_equipmentSubtype = 5;
  const std::uint64_t before = inv::LiveEquipmentRowsRefusalCount();
  inv::SetLiveAccessoryPreviewStats(1);
  Menu_Framework_armor = Audio_Seq_unusedInventory.armor;
  inv::SetLiveAccessoryPreviewStats(17);
  CHECK(inv::LiveEquipmentRowsRefusalCount() == before + 2);
  CHECK(Menu_Framework_equipmentSubtype == 5);
}

struct LiveModelFixture {
  Restore<Actors_ActorRecord*> saved_actor{Actors_actorRecords[1]};
  Restore<char> saved_step{Menu_Inventory_equipmentModelLoadStep};
  Restore<Battle_Actor*> saved_character{Battle_characterState};
  std::unique_ptr<Actors_ActorRecord> actor =
      std::make_unique<Actors_ActorRecord>();
};

TEST_CASE("ReloadLiveEquipmentModels waits for the settled preview animation",
          "[equipment-rows-live]") {
  LiveModelFixture fixture;
  Actors_actorRecords[1] = fixture.actor.get();
  Menu_Inventory_equipmentModelLoadStep = 3;
  fixture.actor->animationId = 1;
  CHECK(inv::ReloadLiveEquipmentModels(1, 5, 0) == 1);
  CHECK(Menu_Inventory_equipmentModelLoadStep == 3);
  fixture.actor->animationId = 101;
  CHECK(inv::ReloadLiveEquipmentModels(1, 5, 0) == 0);
  CHECK(Menu_Inventory_equipmentModelLoadStep == 0);
}

TEST_CASE("ReloadLiveEquipmentModels refuses without a preview actor",
          "[equipment-rows-live]") {
  LiveModelFixture fixture;
  Actors_actorRecords[1] = nullptr;
  Menu_Inventory_equipmentModelLoadStep = 0;
  const std::uint64_t before = inv::LiveEquipmentRowsRefusalCount();
  CHECK(inv::ReloadLiveEquipmentModels(1, 5, 0) == 0);
  Actors_actorRecords[1] = fixture.actor.get();
  Battle_characterState = nullptr;
  CHECK(inv::ReloadLiveEquipmentModels(1, 5, 0) == 0);
  CHECK(inv::LiveEquipmentRowsRefusalCount() == before + 2);
  CHECK(Menu_Inventory_equipmentModelLoadStep == 0);
}

TEST_CASE("QueueLiveEquipmentModel refuses without Ashley's model slots",
          "[equipment-rows-live]") {
  LiveModelFixture fixture;
  Battle_characterState = nullptr;
  const std::uint64_t before = inv::LiveEquipmentRowsRefusalCount();
  inv::QueueLiveEquipmentModel(0, 1);
  CHECK(inv::LiveEquipmentRowsRefusalCount() == before);
  inv::QueueLiveEquipmentModel(5, 1);
  CHECK(inv::LiveEquipmentRowsRefusalCount() == before + 1);
}

TEST_CASE("LiveIsTwoHandedWeapon returns the original's int result",
          "[equipment-rows-live][F02638]") {
  for (int category = -1; category <= 10; ++category) {
    CAPTURE(category);
    CHECK(inv::LiveIsTwoHandedWeapon(category) ==
          (inv::IsTwoHandedWeapon(category) ? 1 : 0));
  }
}

TEST_CASE("Equipment rows registry binds the MENU3 addresses",
          "[equipment-rows-live][F02626][F02627][F02628][F02629][F02637]"
          "[F02638][F02639]") {
  struct Entry {
    unsigned address;
    const void* target;
  };
  const std::array entries = {
      Entry{0x80106390,
            reinterpret_cast<const void*>(&inv::LiveCreateEquipmentRows)},
      Entry{0x80106574,
            reinterpret_cast<const void*>(&inv::LiveSelectedEquipmentRow)},
      Entry{0x8010659C,
            reinterpret_cast<const void*>(&inv::UpdateLiveEquipmentChoices)},
      Entry{0x80106BB4,
            reinterpret_cast<const void*>(&inv::SetLiveAccessoryPreviewStats)},
      Entry{0x801090C4,
            reinterpret_cast<const void*>(&inv::ReloadLiveEquipmentModels)},
      Entry{0x80109320,
            reinterpret_cast<const void*>(&inv::LiveIsTwoHandedWeapon)},
      Entry{0x80109344,
            reinterpret_cast<const void*>(&inv::QueueLiveEquipmentModel)},
  };
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const Entry& entry : entries) {
    CAPTURE(entry.address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU3.PRG",
                                            entry.address) == entry.target);
  }
}
}  // namespace
