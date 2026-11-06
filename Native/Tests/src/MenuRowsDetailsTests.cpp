// Hand-maintained tests for the typed menu item rows and equipment details
// (#276).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/UiPackets.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Items/Inventory.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
}

namespace {
namespace fw = VS::Menu::Framework;
namespace items = VS::Items;
using fw::TextRef;
using fw::TextSource;

constexpr std::uint8_t kTerminator = 0xE7;
constexpr std::uint8_t kPrintString = 0xFF;
// The fake ITEMHELP offset table: entry i names the text at halfword 2000 + i.
constexpr std::size_t kHelpEntries = 700;
constexpr std::size_t kFirstEntryText = 2000;
constexpr std::size_t kHandCategories = 10;

constexpr TextRef Help(std::size_t halfword) {
  return {.source = TextSource::kHelp, .offset = halfword * 2};
}

// The text the fake's ITEMHELP entry `index` names.
constexpr TextRef Entry(std::size_t index) {
  return Help(kFirstEntryText + index);
}

constexpr TextRef NameRow(std::size_t row) {
  return {.source = TextSource::kNames, .offset = row * fw::kItemNameRowBytes};
}

constexpr TextRef kBufferText{.source = TextSource::kBuffer};
constexpr TextRef kEmptyText{.source = TextSource::kEmpty};
constexpr TextRef kWeaponNameText{.source = TextSource::kWeaponName};

// "name(a,b)" in decimal, the form the fakes log their calls in.
std::string Call(std::string_view name,
                 std::initializer_list<std::int64_t> args) {
  std::string text(name);
  text += '(';
  bool first = true;
  for (const std::int64_t arg : args) {
    if (!first) {
      text += ',';
    }
    text += std::to_string(arg);
    first = false;
  }
  return text + ')';
}

constexpr std::size_t kStringSlots = VS::Render::kEncodedTextArgumentCount;
using StringSlots = std::array<std::optional<TextRef>, kStringSlots>;

// The texts, tables and rows the item rows read; every string argument and
// row request lands in the fake's records.
class FakeRows final : public fw::ItemRowsHost {
 public:
  FakeRows()
      : help(fw::kItemHelpBytes, kTerminator),
        names(fw::kItemNamesBytes, kTerminator) {
    for (std::size_t index = 0; index < kHelpEntries; ++index) {
      const std::size_t text = kFirstEntryText + index;
      help[index * 2] = static_cast<std::uint8_t>(text & 0xFF);
      help[(index * 2) + 1] = static_cast<std::uint8_t>(text >> 8);
    }
  }

  void SetText(TextRef where, std::initializer_list<std::uint8_t> bytes) {
    std::vector<std::uint8_t>& table =
        where.source == TextSource::kHelp ? help : names;
    std::size_t offset = where.offset;
    for (const std::uint8_t byte : bytes) {
      table.at(offset++) = byte;
    }
  }

  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  std::span<const std::uint8_t> HelpText() override {
    return loaded ? std::span<const std::uint8_t>(help)
                  : std::span<const std::uint8_t>{};
  }
  std::span<const std::uint8_t> NameText() override {
    return loaded ? std::span<const std::uint8_t>(names)
                  : std::span<const std::uint8_t>{};
  }
  std::span<const std::uint8_t> WeaponHands() override { return hands; }
  VS::Render::EncodedTextArguments TextArguments() override { return {}; }
  void SetStringArgument(std::size_t slot,
                         std::span<const std::uint8_t> text) override {
    strings.at(slot) = Locate(text);
  }
  std::optional<items::WeaponLoadout> InventoryWeaponLoadout(
      const items::InventoryWeapon& /*weapon*/) override {
    return weapon_loadout;
  }
  std::optional<items::ShieldLoadout> InventoryShieldLoadout(
      const items::InventoryShield& /*shield*/) override {
    return shield_loadout;
  }
  std::optional<items::InventoryWeapon> Weapon(int reference) override {
    return Lookup(weapons, reference);
  }
  std::optional<items::InventoryBlade> Blade(int reference) override {
    return Lookup(blades, reference);
  }
  std::optional<items::InventoryGrip> Grip(int reference) override {
    return Lookup(grips, reference);
  }
  std::optional<items::InventoryShield> Shield(int reference) override {
    return Lookup(shields, reference);
  }
  std::optional<items::InventoryArmor> Armor(int reference) override {
    return Lookup(armor, reference);
  }
  std::optional<items::InventoryGem> Gem(int reference) override {
    return Lookup(gems, reference);
  }
  std::span<std::uint8_t> StringBuffer() override { return buffer; }
  void DeactivateRow(int row_index) override {
    placements.push_back(Call("Deactivate", {row_index}));
  }
  fw::MenuRow* SetMenuItem(const fw::RowPlacement& placement,
                           TextRef text) override {
    placements.push_back(
        Call("Place", {placement.row, placement.x, placement.y, placement.w,
                       placement.background_width}));
    row_texts.push_back(text);
    if (refuse_rows) {
      return nullptr;
    }
    row = {};
    return &row;
  }

  std::vector<std::uint8_t> help;
  std::vector<std::uint8_t> names;
  bool loaded = true;
  std::array<std::uint8_t, kHandCategories> hands{0, 0, 1, 0, 1, 0, 1, 1, 1, 0};
  StringSlots strings{};
  std::optional<items::WeaponLoadout> weapon_loadout;
  std::optional<items::ShieldLoadout> shield_loadout;
  std::vector<items::InventoryWeapon> weapons;
  std::vector<items::InventoryBlade> blades;
  std::vector<items::InventoryGrip> grips;
  std::vector<items::InventoryShield> shields;
  std::vector<items::InventoryArmor> armor;
  std::vector<items::InventoryGem> gems;
  std::array<std::uint8_t, fw::kRowDescriptionBufferBytes> buffer{};
  std::vector<std::string> placements;
  std::vector<TextRef> row_texts;
  fw::MenuRow row;
  bool refuse_rows = false;
  std::vector<std::string> refusals;

 private:
  template <typename Record>
  static std::optional<Record> Lookup(const std::vector<Record>& table,
                                      int reference) {
    if (reference < 1 || static_cast<std::size_t>(reference) > table.size()) {
      return std::nullopt;
    }
    return table[static_cast<std::size_t>(reference) - 1];
  }

  static std::optional<TextRef> Find(std::span<const std::uint8_t> text,
                                     const std::vector<std::uint8_t>& table,
                                     TextSource source) {
    const std::less<const std::uint8_t*> before;
    const std::uint8_t* begin = table.data();
    const std::uint8_t* end = begin + table.size();
    if (text.empty() || before(text.data(), begin) ||
        !before(text.data(), end)) {
      return std::nullopt;
    }
    return TextRef{.source = source,
                   .offset = static_cast<std::size_t>(text.data() - begin)};
  }

  [[nodiscard]] std::optional<TextRef> Locate(
      std::span<const std::uint8_t> text) const {
    if (auto found = Find(text, help, TextSource::kHelp)) {
      return found;
    }
    return Find(text, names, TextSource::kNames);
  }
};

class FakeTextLoad final : public fw::ItemTextLoadHost {
 public:
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  void StartItemTextLoad() override { calls.emplace_back("Start"); }
  std::optional<bool> ItemTextLoaded() override {
    calls.emplace_back("Loaded");
    return loaded;
  }
  void ReleaseItemTextRequest() override { calls.emplace_back("Release"); }
  bool ItemTextAllocated() override { return allocated; }
  void FreeItemText() override {
    calls.emplace_back("Free");
    allocated = false;
  }

  std::optional<bool> loaded;
  bool allocated = true;
  std::vector<std::string> calls;
  std::vector<std::string> refusals;
};

// A weapon whose greatest total is class 1 (60, rank 1) and whose runner-up
// is affinity 2 (25, rank 2).
items::ClassAffinityCurrent RankedTotals() {
  items::ClassAffinityCurrent totals;
  totals.classes[1][1] = 60;
  totals.affinities[1][2] = 25;
  return totals;
}

items::WeaponLoadout SampleWeapon() {
  items::WeaponLoadout weapon;
  weapon.blade.category = 2;
  weapon.blade.material = 2;
  weapon.blade.damage_type = 1;
  weapon.class_affinity = RankedTotals();
  return weapon;
}

items::ShieldLoadout SampleShield() {
  items::ShieldLoadout shield;
  shield.base.id = 70;
  shield.base.material = 5;
  shield.class_affinity = RankedTotals();
  return shield;
}

// The class template prints string 9 (the material name); the description
// template adds one glyph.
void SetWeaponTemplates(FakeRows& fake) {
  fake.SetText(Help(fw::item_text::kClassTemplateOffset),
               {kPrintString, 9, kTerminator});
  fake.SetText(Help(fw::item_text::kWeaponDescTemplateOffset),
               {0x10, kTerminator});
  fake.SetText(NameRow(255), {0x20, 0x21, kTerminator});
}

StringSlots Strings(
    std::initializer_list<std::pair<std::size_t, TextRef>> set) {
  StringSlots slots{};
  for (const auto& [slot, text] : set) {
    slots.at(slot) = text;
  }
  return slots;
}

// ---------------------------------------------------------------- texts

TEST_CASE("Item help texts stay inside the text", "[menu][menu-rows-details]") {
  const std::array<std::uint8_t, 10> help{3, 0, 5, 0, 0xE7, 0, 0, 0, 0, 0};
  CHECK(fw::ItemHelpText(help, 4) ==
        TextRef{.source = TextSource::kHelp, .offset = 8});
  CHECK(fw::ItemHelpText(help, 5) == std::nullopt);
  CHECK(fw::ItemHelpEntry(help, 0) ==
        TextRef{.source = TextSource::kHelp, .offset = 6});
  CHECK(fw::ItemHelpEntry(help, 1) == std::nullopt);
  CHECK(fw::ItemHelpEntry(help, -1) == std::nullopt);
  CHECK(fw::ItemHelpEntry(help, 5) == std::nullopt);
  CHECK(fw::ItemHelpEntry({}, 0) == std::nullopt);
}

TEST_CASE("Equipment title ranks rank the greatest and runner-up totals",
          "[menu][menu-rows-details]") {
  CHECK(fw::EquipmentTitleRanks(RankedTotals()) == 0x10102);
  items::ClassAffinityCurrent affinity;
  affinity.classes[1][0] = 95;
  affinity.affinities[1][3] = 100;
  CHECK(fw::EquipmentTitleRanks(affinity) == 0x90209);
  items::ClassAffinityCurrent tied;
  tied.classes[1][0] = 40;
  tied.classes[1][3] = 40;
  CHECK(fw::EquipmentTitleRanks(tied) == 4);
  items::ClassAffinityCurrent saturated;
  saturated.classes[1][2] = 32767;
  CHECK(fw::EquipmentTitleRanks(saturated) == 0x20000);
}

TEST_CASE("Equipment title ranks ignore the first totals and negatives",
          "[menu][menu-rows-details]") {
  CHECK(fw::EquipmentTitleRanks({}) == 0);
  items::ClassAffinityCurrent first_row;
  first_row.classes[0][1] = 99;
  first_row.affinities[0][1] = 99;
  CHECK(fw::EquipmentTitleRanks(first_row) == 0);
  items::ClassAffinityCurrent negative;
  negative.classes[1][1] = -5;
  negative.affinities[1][6] = -40;
  CHECK(fw::EquipmentTitleRanks(negative) == 0);
}

// ---------------------------------------------------------------- weapons

TEST_CASE("Weapon rows format the class template into the buffer",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  SetWeaponTemplates(fake);
  const fw::ItemRowText row =
      fw::FormatWeaponRow(fake, SampleWeapon(), fake.buffer);
  CHECK_FALSE(row.name.has_value());
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x08020000);
  CHECK(fake.strings == Strings({{0, Entry(374)},
                                 {1, Entry(560)},
                                 {6, Entry(412)},
                                 {7, Entry(409)},
                                 {8, Entry(400)},
                                 {9, NameRow(255)}}));
  CHECK(
      std::vector<std::uint8_t>(fake.buffer.begin(), fake.buffer.begin() + 4) ==
      std::vector<std::uint8_t>{0x20, 0x21, 0x10, kTerminator});
  CHECK(fake.refusals.empty());
}

TEST_CASE("Unranked weapon rows show the category's base title",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  items::WeaponLoadout weapon;
  weapon.blade.category = 3;
  weapon.blade.material = 1;
  weapon.blade.damage_type = 2;
  const fw::ItemRowText row = fw::FormatWeaponRow(fake, weapon, fake.buffer);
  CHECK(row.row_type == 0x0C010000);
  CHECK(fake.strings == Strings({{0, Entry(372)},
                                 {1, Entry(385)},
                                 {6, Entry(413)},
                                 {7, Entry(410)},
                                 {8, Entry(401)},
                                 {9, NameRow(254)}}));
  CHECK(fake.refusals.empty());
}

TEST_CASE("Weapon rows before the text load are refused and emptied",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.loaded = false;
  fake.buffer[0] = 0x55;
  const fw::ItemRowText row =
      fw::FormatWeaponRow(fake, SampleWeapon(), fake.buffer);
  CHECK(fake.refusals ==
        std::vector<std::string>{"item text: ITEMHELP entry past the text"});
  CHECK_FALSE(row.name.has_value());
  CHECK(row.description == kEmptyText);
  CHECK(fake.buffer[0] == kTerminator);
  CHECK(fake.strings == StringSlots{});
}

TEST_CASE("Weapon rows refuse a blade category past the hand table",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  items::WeaponLoadout weapon;
  weapon.blade.category = 11;
  weapon.blade.material = 1;
  weapon.blade.damage_type = 1;
  const fw::ItemRowText row = fw::FormatWeaponRow(fake, weapon, fake.buffer);
  CHECK(fake.refusals == std::vector<std::string>{
                             "item text: blade category past the hand table"});
  CHECK(row.description == kEmptyText);
  weapon.blade.category = 0;
  fake.refusals.clear();
  (void)fw::FormatWeaponRow(fake, weapon, fake.buffer);
  CHECK(fake.refusals == std::vector<std::string>{
                             "item text: blade category past the hand table"});
}

TEST_CASE("Weapon rows refuse templates that do not fit the buffer",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  SetWeaponTemplates(fake);
  std::array<std::uint8_t, 2> row_only{};
  (void)fw::FormatWeaponRow(fake, SampleWeapon(), row_only);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "item text: the row template does not fit the buffer"});
  CHECK(row_only[0] == kTerminator);
  fake.refusals.clear();
  std::array<std::uint8_t, 3> no_description{};
  const fw::ItemRowText row =
      fw::FormatWeaponRow(fake, SampleWeapon(), no_description);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "item text: the description template does not fit the buffer"});
  CHECK(row.description == kEmptyText);
}

TEST_CASE("Inventory weapon rows name the weapon and format its stats",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.weapon_loadout = SampleWeapon();
  const fw::ItemRowText row =
      fw::FormatInventoryWeaponRow(fake, items::InventoryWeapon{}, fake.buffer);
  CHECK(row.name == kWeaponNameText);
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x08020000);
  CHECK(fake.refusals.empty());
}

TEST_CASE("Inventory weapon rows without stats are refused",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.buffer[0] = 0x55;
  const fw::ItemRowText row =
      fw::FormatInventoryWeaponRow(fake, items::InventoryWeapon{}, fake.buffer);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "item text: no stats for the weapon's inventory table"});
  CHECK(row.name == kWeaponNameText);
  CHECK(row.description == kEmptyText);
  CHECK(row.row_type == 0);
  CHECK(fake.buffer[0] == kTerminator);
}

// ---------------------------------------------------------------- parts

TEST_CASE("Blade rows format the blade's description",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.SetText(Entry(4), {0x30, kTerminator});
  fake.SetText(Help(fw::item_text::kWeaponDescTemplateOffset),
               {0x10, kTerminator});
  items::InventoryBlade blade;
  blade.id = 5;
  blade.category = 1;
  blade.type_bits = 0x23;
  blade.material = 3;
  const fw::ItemRowText row = fw::FormatBladeRow(fake, blade, fake.buffer);
  CHECK(row.name == NameRow(5));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x04030000);
  CHECK(fake.strings == Strings({{6, Entry(412)},
                                 {7, Entry(411)},
                                 {8, Entry(399)},
                                 {9, NameRow(256)}}));
  CHECK(
      std::vector<std::uint8_t>(fake.buffer.begin(), fake.buffer.begin() + 3) ==
      std::vector<std::uint8_t>{0x30, 0x10, kTerminator});
}

TEST_CASE("Grip rows format the grip's description",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.SetText(Entry(94), {0x31, kTerminator});
  fake.SetText(Help(fw::item_text::kGripTemplateOffset), {0x11, kTerminator});
  items::InventoryGrip grip;
  grip.id = 100;
  grip.category = 3;
  const fw::ItemRowText row = fw::FormatGripRow(fake, grip, fake.buffer);
  CHECK(row.name == NameRow(100));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x34000000);
  CHECK(fake.strings == Strings({{5, Entry(643)}}));
  CHECK(
      std::vector<std::uint8_t>(fake.buffer.begin(), fake.buffer.begin() + 3) ==
      std::vector<std::uint8_t>{0x31, 0x11, kTerminator});
}

TEST_CASE("Shield rows format the armour template",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  const fw::ItemRowText row =
      fw::FormatShieldRow(fake, SampleShield(), fake.buffer);
  CHECK(row.name == NameRow(70));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x3C050000);
  CHECK(fake.strings == Strings({{0, Entry(374)},
                                 {1, Entry(586)},
                                 {8, Help(fw::item_text::kArmorCategoryOffset)},
                                 {9, NameRow(258)}}));
  CHECK(fake.refusals.empty());
  fake.strings = {};
  items::ShieldLoadout unranked = SampleShield();
  unranked.class_affinity = {};
  (void)fw::FormatShieldRow(fake, unranked, fake.buffer);
  CHECK(fake.strings.at(1) == Help(fw::item_text::kArmorBasePrestigeOffset));
}

TEST_CASE("Inventory shield rows need the shield's stats",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  const fw::ItemRowText refused =
      fw::FormatInventoryShieldRow(fake, items::InventoryShield{}, fake.buffer);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "item text: no stats for the shield's inventory table"});
  CHECK(refused.name == kEmptyText);
  CHECK(refused.description == kEmptyText);
  CHECK(refused.row_type == 0);
  fake.refusals.clear();
  fake.shield_loadout = SampleShield();
  const fw::ItemRowText row =
      fw::FormatInventoryShieldRow(fake, items::InventoryShield{}, fake.buffer);
  CHECK(row.name == NameRow(70));
  CHECK(row.row_type == 0x3C050000);
  CHECK(fake.refusals.empty());
}

TEST_CASE("Armor rows format the armour template",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  items::ArmorLoadout armor;
  armor.armor.id = 30;
  armor.armor.category = 2;
  armor.armor.material = 4;
  const fw::ItemRowText row = fw::FormatArmorRow(fake, armor, fake.buffer);
  CHECK(row.name == NameRow(30));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x40040000);
  CHECK(fake.strings == Strings({{0, Entry(372)},
                                 {1, Entry(393)},
                                 {8, Entry(646)},
                                 {9, NameRow(257)}}));
  CHECK(fake.refusals.empty());
}

TEST_CASE("Accessory rows show the accessory's own description",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  items::ArmorLoadout accessory;
  accessory.armor.id = 225;
  accessory.armor.category = 7;
  const fw::ItemRowText row = fw::FormatArmorRow(fake, accessory, fake.buffer);
  CHECK(row.name == NameRow(225));
  CHECK(row.description == Entry(612));
  CHECK(row.row_type == 0x54000000);
  CHECK(fake.strings == StringSlots{});
}

TEST_CASE("Inventory armor rows assemble the armour's stats",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  items::InventoryArmor armor;
  armor.id = 30;
  armor.category = 2;
  armor.material = 4;
  const fw::ItemRowText row =
      fw::FormatInventoryArmorRow(fake, armor, fake.buffer);
  CHECK(row.name == NameRow(30));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x40040000);
  CHECK(fake.strings.at(1) == Entry(393));
  CHECK(fake.strings.at(9) == NameRow(257));
}

TEST_CASE("Gem rows copy the gem's description", "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.SetText(Entry(122), {0x41, 0x42});
  items::InventoryGem gem;
  gem.id = 262;
  const fw::ItemRowText row = fw::FormatGemRow(fake, gem, fake.buffer);
  CHECK(row.name == NameRow(262));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0x58000000);
  CHECK(fake.buffer[0] == 0x41);
  CHECK(fake.buffer[1] == 0x42);
  CHECK(fake.buffer[95] == kTerminator);
}

TEST_CASE("Gem rows refuse a copy past the buffer",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  items::InventoryGem gem;
  gem.id = 262;
  std::array<std::uint8_t, 10> small{};
  const fw::ItemRowText row = fw::FormatGemRow(fake, gem, small);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "item text: a description copy past the text or the buffer"});
  CHECK(row.name == kEmptyText);
  CHECK(row.description == kEmptyText);
  CHECK(small[0] == kTerminator);
}

TEST_CASE("Gem rows refuse a description copy past the text",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  // Entry 122 names halfword 14300: 72 bytes before the end of the text.
  fake.help[244] = 0xDC;
  fake.help[245] = 0x37;
  items::InventoryGem gem;
  gem.id = 262;
  (void)fw::FormatGemRow(fake, gem, fake.buffer);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "item text: a description copy past the text or the buffer"});
}

TEST_CASE("Misc item rows copy the description and count",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  fake.SetText(Entry(184), {0x43});
  items::InventoryMisc misc;
  misc.id = 324;
  misc.count = 7;
  const fw::ItemRowText row = fw::FormatMiscItemRow(fake, misc, fake.buffer);
  CHECK(row.name == NameRow(324));
  CHECK(row.description == kBufferText);
  CHECK(row.row_type == 0xE00);
  CHECK(fake.buffer[0] == 0x43);
  misc.id = 600;
  const fw::ItemRowText refused =
      fw::FormatMiscItemRow(fake, misc, fake.buffer);
  CHECK(fake.refusals ==
        std::vector<std::string>{"item text: ITEMNAME row past the names"});
  CHECK(refused.name == kEmptyText);
  CHECK(refused.description == kEmptyText);
}

// ---------------------------------------------------------------- detail rows

// Weapon 1: blade 1 (id 5), grip 1 (id 100, two gem slots) and gem 1 (id 262)
// in its first slot.
void AddSampleWeapon(FakeRows& fake) {
  items::InventoryWeapon weapon;
  weapon.blade = 1;
  weapon.grip = 1;
  weapon.gems = {1, 0, 0, 0};
  fake.weapons.push_back(weapon);
  items::InventoryBlade blade;
  blade.id = 5;
  blade.category = 1;
  blade.type_bits = 0x23;
  blade.material = 3;
  fake.blades.push_back(blade);
  items::InventoryGrip grip;
  grip.id = 100;
  grip.category = 3;
  grip.gem_slots = 2;
  fake.grips.push_back(grip);
  items::InventoryGem gem;
  gem.id = 262;
  fake.gems.push_back(gem);
}

TEST_CASE("Weapon component rows place the blade and grip",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  CHECK(fw::CreateWeaponComponentRow(fake, 1, 1, false) == &fake.row);
  CHECK(fw::CreateWeaponComponentRow(fake, 2, 1, true) == &fake.row);
  CHECK(fake.placements ==
        std::vector<std::string>{"Deactivate(11)", "Place(11,162,34,158,0)",
                                 "Deactivate(12)", "Place(12,162,50,158,0)"});
  CHECK(fake.row_texts == std::vector<TextRef>{NameRow(5), NameRow(100)});
  CHECK(fake.refusals.empty());
}

TEST_CASE("Weapon component rows style the blade row",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  REQUIRE(fw::CreateWeaponComponentRow(fake, 1, 1, false) == &fake.row);
  CHECK(fake.row.row_icon == 1);
  CHECK(fake.row.material == 3);
  CHECK(fake.row.unselectable == 0);
  CHECK(fake.row.state == fw::RowState::kInactive);
}

TEST_CASE("Weapon component rows slide the grip row in",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  REQUIRE(fw::CreateWeaponComponentRow(fake, 2, 1, true) == &fake.row);
  CHECK(fake.row.row_icon == 13);
  CHECK(fake.row.x == 320);
  CHECK(fake.row.state == fw::RowState::kSlideX);
  CHECK(fake.row.target0 == 162);
}

TEST_CASE("Weapon component rows place the gem slots",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  CHECK(fw::CreateWeaponComponentRow(fake, 3, 1, false) == &fake.row);
  CHECK(fw::CreateWeaponComponentRow(fake, 4, 1, false) == &fake.row);
  CHECK(fw::CreateWeaponComponentRow(fake, 5, 1, false) == nullptr);
  CHECK(fake.placements ==
        std::vector<std::string>{"Deactivate(13)", "Place(13,169,66,151,0)",
                                 "Deactivate(14)", "Place(14,169,82,151,0)",
                                 "Deactivate(15)"});
  CHECK(fake.row_texts ==
        std::vector<TextRef>{NameRow(262), Help(fw::item_text::kNoneOffset)});
  CHECK(fake.refusals.empty());
}

TEST_CASE("Weapon component rows style filled and empty gem slots",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  REQUIRE(fw::CreateWeaponComponentRow(fake, 3, 1, false) == &fake.row);
  CHECK(fake.row.row_icon == 0x16);
  CHECK(fake.row.unselectable == 0);
  REQUIRE(fw::CreateWeaponComponentRow(fake, 4, 1, false) == &fake.row);
  CHECK(fake.row.row_icon == 0x16);
  CHECK(fake.row.unselectable == 1);
}

TEST_CASE("Weapon component rows refuse missing records",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  CHECK(fw::CreateWeaponComponentRow(fake, 1, 2, false) == nullptr);
  fake.weapons[0].blade = 2;
  CHECK(fw::CreateWeaponComponentRow(fake, 1, 1, false) == nullptr);
  fake.weapons[0].grip = 2;
  CHECK(fw::CreateWeaponComponentRow(fake, 2, 1, false) == nullptr);
  fake.weapons[0].grip = 1;
  fake.weapons[0].gems = {5, 0, 0, 0};
  CHECK(fw::CreateWeaponComponentRow(fake, 3, 1, false) == nullptr);
  fake.grips[0].gem_slots = 6;
  CHECK(fw::CreateWeaponComponentRow(fake, 7, 1, false) == nullptr);
  CHECK(fw::CreateWeaponComponentRow(fake, 0, 1, false) == nullptr);
  CHECK(fake.refusals ==
        std::vector<std::string>{
            "weapon component row: no such weapon",
            "weapon component row: the weapon's blade is missing",
            "weapon component row: the weapon's grip is missing",
            "weapon component row: the slot's gem is missing",
            "weapon component row: a gem slot past the weapon",
            "weapon component row: a gem slot past the weapon"});
  CHECK(fake.placements.empty());
}

TEST_CASE("Weapon component rows return no row the menu refused",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleWeapon(fake);
  fake.refuse_rows = true;
  CHECK(fw::CreateWeaponComponentRow(fake, 1, 1, true) == nullptr);
  CHECK(fake.placements ==
        std::vector<std::string>{"Deactivate(11)", "Place(11,162,34,158,0)"});
}

void AddSampleShield(FakeRows& fake) {
  items::InventoryShield shield;
  shield.base.gem_slots = 2;
  shield.gems = {1, 0, 0, 0};
  fake.shields.push_back(shield);
  items::InventoryGem gem;
  gem.id = 262;
  fake.gems.push_back(gem);
}

TEST_CASE("Shield gem rows place the shield's gem slots",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleShield(fake);
  CHECK(fw::CreateShieldGemRow(fake, 1, 1, false) == &fake.row);
  CHECK(fw::CreateShieldGemRow(fake, 2, 1, true) == &fake.row);
  CHECK(fake.row.state == fw::RowState::kSlideX);
  CHECK(fake.row.target0 == 169);
  CHECK(fw::CreateShieldGemRow(fake, 3, 1, false) == nullptr);
  CHECK(fake.placements ==
        std::vector<std::string>{"Deactivate(11)", "Place(11,169,34,151,0)",
                                 "Deactivate(12)", "Place(12,169,50,151,0)",
                                 "Deactivate(13)"});
  CHECK(fake.row_texts ==
        std::vector<TextRef>{NameRow(262), Help(fw::item_text::kNoneOffset)});
}

TEST_CASE("Shield gem rows style filled and empty gem slots",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleShield(fake);
  REQUIRE(fw::CreateShieldGemRow(fake, 1, 1, false) == &fake.row);
  CHECK(fake.row.row_icon == 0x16);
  CHECK(fake.row.unselectable == 0);
  REQUIRE(fw::CreateShieldGemRow(fake, 2, 1, true) == &fake.row);
  CHECK(fake.row.row_icon == 0x16);
  CHECK(fake.row.unselectable == 1);
}

TEST_CASE("Shield gem rows refuse missing records",
          "[menu][menu-rows-details]") {
  FakeRows fake;
  AddSampleShield(fake);
  CHECK(fw::CreateShieldGemRow(fake, 0, 1, false) == nullptr);
  CHECK(fw::CreateShieldGemRow(fake, 1, 2, false) == nullptr);
  fake.shields[0].gems = {9, 0, 0, 0};
  CHECK(fw::CreateShieldGemRow(fake, 1, 1, false) == nullptr);
  fake.shields[0].base.gem_slots = 6;
  CHECK(fw::CreateShieldGemRow(fake, 5, 1, false) == nullptr);
  CHECK(fake.refusals ==
        std::vector<std::string>{"shield gem row: a gem slot past the shield",
                                 "shield gem row: no such shield",
                                 "shield gem row: the slot's gem is missing",
                                 "shield gem row: a gem slot past the shield"});
  CHECK(fake.placements.empty());
}

// ---------------------------------------------------------------- text load

TEST_CASE("Item text loads start and poll the read",
          "[menu][menu-rows-details]") {
  FakeTextLoad fake;
  fw::ItemTextLoadState state;
  CHECK(fw::LoadItemText(state, fake, false) == 1);
  CHECK(fw::LoadItemText(state, fake, true) == 0);
  CHECK(state.loading);
  fake.loaded = false;
  CHECK(fw::LoadItemText(state, fake, false) == 0);
  CHECK(state.loading);
  fake.loaded = true;
  CHECK(fw::LoadItemText(state, fake, false) == 1);
  CHECK_FALSE(state.loading);
  CHECK(fake.calls ==
        std::vector<std::string>{"Start", "Loaded", "Loaded", "Release"});
  CHECK(fake.refusals.empty());
}

TEST_CASE("Item text polls with no queued read are refused",
          "[menu][menu-rows-details]") {
  FakeTextLoad fake;
  fw::ItemTextLoadState state{.loading = true};
  CHECK(fw::LoadItemText(state, fake, false) == 1);
  CHECK_FALSE(state.loading);
  CHECK(fake.refusals ==
        std::vector<std::string>{"item text: polled with no queued read"});
}

TEST_CASE("Item text unloads wait for a pending read",
          "[menu][menu-rows-details]") {
  FakeTextLoad fake;
  fw::ItemTextLoadState state{.loading = true};
  fake.loaded = false;
  CHECK(fw::UnloadItemTextWhenReady(state, fake) == 0);
  CHECK(fake.allocated);
  fake.loaded = true;
  CHECK(fw::UnloadItemTextWhenReady(state, fake) == 1);
  CHECK_FALSE(fake.allocated);
  CHECK(fw::UnloadItemTextWhenReady(state, fake) == 1);
  CHECK(fake.calls ==
        std::vector<std::string>{"Loaded", "Loaded", "Release", "Free"});
}

TEST_CASE("Item text unloads free an idle block", "[menu][menu-rows-details]") {
  FakeTextLoad fake;
  fw::ItemTextLoadState state;
  CHECK(fw::UnloadItemTextWhenReady(state, fake) == 1);
  CHECK(fake.calls == std::vector<std::string>{"Free"});
}

// ---------------------------------------------------------------- live rows

TEST_CASE("Live item rows refuse a null record", "[menu][menu-rows-details]") {
  VS::test_support::ChannelWarnings warnings{"menu.framework"};
  std::array<char*, 2> menu_text{};
  int row_type = 0;
  fw::FormatLiveGemRow(nullptr, menu_text.data(), &row_type, nullptr);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "item rows: a null record, text array or row type"});
  CHECK(menu_text[0] == nullptr);
}

TEST_CASE("Live gem rows point into the loaded text block",
          "[menu][menu-rows-details]") {
  std::vector<std::uint16_t> block(
      (fw::kItemNamesBytes + fw::kItemHelpBytes) / 2, 0xE7E7);
  auto* const saved_names = Menu_Framework_itemNames;
  auto* const saved_help = Menu_Framework_itemHelp;
  Menu_Framework_itemNames =
      reinterpret_cast<decltype(Menu_Framework_itemNames)>(block.data());
  Menu_Framework_itemHelp = block.data() + (fw::kItemNamesBytes / 2);
  Menu_Framework_itemHelp[122] = 2000;
  Items_InventoryGem gem{};
  gem.id = 262;
  std::array<char*, 2> menu_text{};
  int row_type = 0;
  std::array<std::uint8_t, fw::kRowDescriptionBufferBytes> buffer{};
  fw::FormatLiveGemRow(&gem, menu_text.data(), &row_type, buffer.data());
  Menu_Framework_itemNames = saved_names;
  Menu_Framework_itemHelp = saved_help;
  CHECK(menu_text[0] ==
        reinterpret_cast<char*>(block.data()) + (262 * fw::kItemNameRowBytes));
  CHECK(menu_text[1] == reinterpret_cast<char*>(buffer.data()));
  CHECK(row_type == 0x58000000);
  CHECK(buffer[0] == kTerminator);
}

TEST_CASE("Live item text unloads with no block report it freed",
          "[menu][menu-rows-details]") {
  auto* const saved_names = Menu_Framework_itemNames;
  Menu_Framework_itemNames = nullptr;
  CHECK(fw::UnloadLiveItemTextWhenReady() == 1);
  CHECK(Menu_Framework_itemNames == nullptr);
  Menu_Framework_itemNames = saved_names;
}

TEST_CASE("Live item row statics reset the hand table",
          "[menu][menu-rows-details]") {
  auto* const saved_names = Menu_Framework_itemNames;
  auto* const saved_help = Menu_Framework_itemHelp;
  auto* const saved_menu12 = Menu_Framework_menu12Text;
  const std::vector<u_char> saved_hands(std::begin(Menu_Framework_weaponHands),
                                        std::end(Menu_Framework_weaponHands));
  Menu_Framework_weaponHands[0] = 9;
  fw::ResetLiveItemRowsStatics();
  const std::vector<u_char> hands(std::begin(Menu_Framework_weaponHands),
                                  std::end(Menu_Framework_weaponHands));
  CHECK(hands == std::vector<u_char>{0, 0, 1, 0, 1, 0, 1, 1, 1, 0});
  CHECK(Menu_Framework_itemNames == nullptr);
  Menu_Framework_itemNames = saved_names;
  Menu_Framework_itemHelp = saved_help;
  Menu_Framework_menu12Text = saved_menu12;
  std::ranges::copy(saved_hands, std::begin(Menu_Framework_weaponHands));
}

// ---------------------------------------------------------------- details

using Stats = std::array<std::int16_t, fw::kEquipmentStatCount>;
using Primary = std::array<std::int16_t, fw::kPrimaryStatCount>;
using RangeRisk = std::array<std::uint8_t, fw::kRangeRiskBytes>;
using Comparison = std::array<std::int16_t, fw::kComparisonStatCount>;

constexpr std::uint32_t kGray = fw::kStatTextGray;
constexpr std::uint32_t kDim = fw::kStatTextDim;
constexpr std::uint32_t kGain = fw::kStatTextGain;
constexpr int kStatSlot = fw::detail_layer::kStatPage;
constexpr int kPanelSlot = fw::detail_layer::kPrimaryPanel;
constexpr TextRef kNoGems = Help(fw::item_text::kNoGemsOffset);
// The shape of vs_mainMenu_rowAnimationSteps: a slide's offset by step.
constexpr std::array<std::uint8_t, 16> kSlideSteps{
    0, 1, 2, 4, 8, 16, 24, 32, 48, 64, 80, 96, 112, 128, 144, 160,
};

constexpr std::uint32_t Xy(int x, int y) { return fw::PackXy(x, y); }

std::string TextCall(const char* text, std::uint32_t xy, std::uint32_t color,
                     int slot) {
  return Call(std::string("Text:") + (text == nullptr ? "(null)" : text),
              {xy, color, slot});
}

class FakeQueue final : public VS::Render::PacketQueue {
 public:
  [[nodiscard]] std::uint32_t ChainLink() const override { return 0; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
    return true;
  }

  std::vector<std::vector<std::uint32_t>> packets;
};

// The rows, equipment, actors, input and draws the details reach; every call
// lands in the fake's logs.
class FakeDetails final : public fw::EquipmentDetailsHost {
 public:
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  fw::ItemRowsHost& Rows() override { return rows; }
  std::optional<items::WeaponLoadout> EquippedWeapon() override {
    calls.emplace_back("EquippedWeapon()");
    return equipped_weapon;
  }
  std::optional<items::ShieldLoadout> EquippedShield() override {
    calls.emplace_back("EquippedShield()");
    return equipped_shield;
  }
  std::optional<items::ArmorLoadout> EquippedArmor(int limb) override {
    calls.push_back(Call("EquippedArmor", {limb}));
    return armor_loadout;
  }
  std::optional<items::ArmorLoadout> InventoryArmorLoadout(
      const items::InventoryArmor& /*armor*/) override {
    calls.emplace_back("InventoryArmor()");
    return armor_loadout;
  }
  std::optional<fw::ActorComparisonStats> ActorStats(int actor) override {
    calls.push_back(Call("ActorStats", {actor}));
    if (actor < 0 || static_cast<std::size_t>(actor) >= actors.size()) {
      return std::nullopt;
    }
    return actors[static_cast<std::size_t>(actor)];
  }
  fw::Buttons Input() override { return input; }
  std::span<const std::uint8_t> RowAnimationSteps() override { return steps; }
  void PlaySound(fw::MenuSound sound) override {
    calls.push_back(Call("Sound", {static_cast<int>(sound)}));
  }
  void PlayPageSound() override { calls.emplace_back("PageSound()"); }
  void SetRowSelected(int row, bool selected) override {
    calls.push_back(Call("RowSelected", {row, selected ? 1 : 0}));
  }
  void SetInformationMessage(TextRef text) override {
    messages.push_back(text);
  }
  int DrawCursor(std::uint32_t step, std::uint32_t position) override {
    calls.push_back(Call("Cursor", {step, position}));
    return static_cast<int>(step) + 1;
  }
  VS::Render::PacketQueue& SceneQueue(int slot) override {
    calls.push_back(Call("Queue", {slot}));
    return queue;
  }
  std::uint32_t DrawComparedInteger(int value, std::uint32_t xy, int threshold,
                                    int slot) override {
    draws.push_back(Call("Compared", {value, xy, threshold, slot}));
    return xy + 8;
  }
  void DrawColoredInteger(int value, std::uint32_t xy, std::uint32_t color,
                          int slot) override {
    draws.push_back(Call("Colored", {value, xy, color, slot}));
  }
  void DrawNeutralInteger(int value, std::uint32_t xy, int slot) override {
    draws.push_back(Call("Neutral", {value, xy, slot}));
  }
  void DrawCompactText(const char* text, std::uint32_t xy, std::uint32_t color,
                       int slot) override {
    draws.push_back(TextCall(text, xy, color, slot));
  }
  std::uint32_t DrawRightAlignedInteger(int font, std::uint32_t xy, int value,
                                        int slot) override {
    draws.push_back(Call("Right", {font, xy, value, slot}));
    return xy - 6;
  }
  void DrawStatGauge(int color, int fill_width, std::uint32_t xy,
                     int slot) override {
    draws.push_back(Call("Gauge", {color, fill_width, xy, slot}));
  }
  void DrawButtonPromptBackground(int x, int y, int w, int h) override {
    draws.push_back(Call("Prompt", {x, y, w, h}));
  }
  void DrawButtonIcon(int button, int x, int y, int slot) override {
    draws.push_back(Call("Icon", {button, x, y, slot}));
  }
  void SetFontStyle(int palette_offset) override {
    styles.push_back(palette_offset);
  }
  int DrawGlyph(std::uint8_t glyph, int x, int y, int slot) override {
    glyphs.push_back(Call("Glyph", {glyph, x, y, slot}));
    return x + 1;
  }

  FakeRows rows;
  std::optional<items::WeaponLoadout> equipped_weapon;
  std::optional<items::ShieldLoadout> equipped_shield;
  std::optional<items::ArmorLoadout> armor_loadout;
  std::vector<fw::ActorComparisonStats> actors;
  fw::Buttons input;
  std::vector<std::uint8_t> steps{kSlideSteps.begin(), kSlideSteps.end()};
  FakeQueue queue;
  std::vector<std::string> calls;
  std::vector<std::string> draws;
  std::vector<std::string> glyphs;
  std::vector<int> styles;
  std::vector<TextRef> messages;
  std::vector<std::string> refusals;
};

// A detail screen over the fakes.
struct Details {
  FakeDetails host;
  fw::EquipmentDetailsState state;
  fw::EquipmentDetailsContext ctx{.state = state, .host = host};
};

std::vector<std::int16_t> StatRange(const fw::EquipmentDetailsState& state,
                                    std::size_t first, std::size_t count) {
  const auto stats = std::span(state.stats).subspan(first, count);
  return {stats.begin(), stats.end()};
}

// The shown current DP, max DP, current PP and max PP.
fw::DpPpValues DpPp(const fw::EquipmentDetailsState& state) {
  return {state.current_dp, state.max_dp, state.current_pp, state.max_pp};
}

// The packet's words after its tag.
std::vector<std::uint32_t> Body(const std::vector<std::uint32_t>& packet) {
  return {std::next(packet.begin()), packet.end()};
}

items::LoadoutAttributes Attributes(std::array<std::int16_t, 6> values) {
  return {.current_strength = values[0],
          .base_strength = static_cast<std::uint16_t>(values[1]),
          .current_intelligence = values[2],
          .base_intelligence = static_cast<std::uint16_t>(values[3]),
          .current_agility = values[4],
          .base_agility = static_cast<std::uint16_t>(values[5])};
}

// A weapon loadout with every stat the details copy set.
items::WeaponLoadout StatWeapon() {
  items::WeaponLoadout weapon = SampleWeapon();
  weapon.grip.types = {1, 2, 3, 4};
  weapon.damage_type = 2;
  weapon.current_dp = 250;
  weapon.max_dp = 1000;
  weapon.current_pp = 7;
  weapon.max_pp = 20;
  weapon.attributes = Attributes({11, 21, 12, 22, 13, 23});
  weapon.range.range = 5;
  weapon.risk = 9;
  return weapon;
}

items::ShieldLoadout StatShield() {
  items::ShieldLoadout shield = SampleShield();
  shield.types = {5, 6, 7, 8};
  shield.current_dp = 120;
  shield.max_dp = 200;
  shield.current_pp = 3;
  shield.max_pp = 4;
  shield.attributes = Attributes({1, 2, 3, 4, 5, 6});
  return shield;
}

items::ArmorLoadout StatArmor(std::uint8_t category) {
  items::ArmorLoadout armor;
  armor.armor.category = category;
  armor.current_dp = 500;
  armor.max_dp = 800;
  armor.attributes = Attributes({4, 1, 5, 2, 6, 3});
  armor.types = {1, 2, 3, 4};
  armor.class_affinity = RankedTotals();
  return armor;
}

// Inventory armour 1 shown on the detail screen, the cursor on the item.
void ShowArmor(Details& details) {
  items::InventoryArmor armor;
  armor.id = 30;
  armor.category = 2;
  armor.material = 4;
  details.host.rows.armor.push_back(armor);
  details.state.subtype = fw::equipment_subtype::kArmor;
  details.state.item = 1;
  details.state.selected = fw::kFirstDetailElement;
}

// One detail frame's calls: `before`, the component rows' deselection, then
// `after`.
std::vector<std::string> DetailFrame(std::vector<std::string> before,
                                     const std::vector<std::string>& after) {
  for (int row = 11; row <= 15; ++row) {
    before.push_back(Call("RowSelected", {row, 0}));
  }
  before.insert(before.end(), after.begin(), after.end());
  return before;
}

std::string ReachableElements(fw::EquipmentDetailsContext& ctx) {
  std::string marks;
  for (int element = 0; element < static_cast<int>(fw::kStatusViewElementCount);
       ++element) {
    marks += fw::IsItemDetailElementReachable(ctx, element) ? 'T' : 'F';
  }
  return marks;
}

TEST_CASE("Equipment stats reset the stats", "[menu][menu-rows-details]") {
  fw::EquipmentDetailsState state;
  state.stats.fill(5);
  state.range_risk.fill(4);
  state.max_dp = 9;
  fw::ResetEquipmentStats(state);
  CHECK(state.stats == Stats{});
  CHECK(state.primary == Primary{0, 0, 0, 1, 0, 0, 0, 1});
  // The original leaves the comparison half of the range data (C48.c:973).
  CHECK(state.range_risk == RangeRisk{0, 0, 0, 0, 4, 4, 4, 4});
  CHECK(state.max_dp == 0);
}

TEST_CASE("Equipment stat setters set the primary, range and DP/PP stats",
          "[menu][menu-rows-details]") {
  fw::EquipmentDetailsState state;
  state.range_risk.fill(4);
  fw::SetPrimaryStats(state, {11, 12, 13, 1});
  CHECK(state.primary == Primary{11, 12, 13, 1, 11, 12, 13, 1});
  fw::SetRangeAndRisk(state, {300, 5, 9, 1});
  CHECK(state.range_risk == RangeRisk{44, 5, 9, 1, 4, 4, 4, 4});
  fw::SetDpPp(state, {250, 1000, 7, 20});
  CHECK(DpPp(state) == fw::DpPpValues{3, 10, 7, 20});
  fw::ResetItemDetailCursor(state);
  CHECK(state.selected == 9);
}

TEST_CASE("Stat pages cycle to the next enabled page",
          "[menu][menu-rows-details]") {
  fw::EquipmentDetailsState state;
  state.enabled_pages = fw::stat_page::kAllEnabled;
  CHECK(fw::NextEnabledStatPage(state, 0) == 1);
  CHECK(fw::NextEnabledStatPage(state, 2) == 0);
  CHECK(fw::NextEnabledStatPage(state, 3) == 0);
  state.enabled_pages = fw::stat_page::kTypeEnabled;
  CHECK(fw::NextEnabledStatPage(state, 2) == 2);
  state.enabled_pages = 8;
  CHECK(fw::NextEnabledStatPage(state, 0) == 3);
  state.enabled_pages = 0;
  CHECK(fw::NextEnabledStatPage(state, 1) == std::nullopt);
}

TEST_CASE("Enabling stat pages leaves a page that is not enabled",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.page = fw::stat_page::kType;
  fw::SetEnabledStatPages(d.ctx, 3);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.state.page == fw::stat_page::kClass);
  d.state.page = fw::stat_page::kType;
  fw::SetEnabledStatPages(d.ctx, fw::stat_page::kTypeEnabled);
  CHECK(d.state.page == fw::stat_page::kType);
  fw::SetEnabledStatPages(d.ctx, 0);
  CHECK(d.state.enabled_pages == 0);
  CHECK(d.state.page == fw::stat_page::kType);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"stat pages with none enabled"});
}

TEST_CASE("Stat bar coordinates shift by the scanline",
          "[menu][menu-rows-details]") {
  CHECK(fw::OffsetStatBarCoordinate(10, 5, 97) == 102);
  CHECK(fw::OffsetStatBarCoordinate(0, 200, 97) == 65433);
}

// ---------------------------------------------------------------- setters

// Inventory weapon 1 with StatWeapon's loadout, set from the affinity page.
void ShowWeaponStats(Details& details) {
  AddSampleWeapon(details.host.rows);
  details.host.rows.weapon_loadout = StatWeapon();
  details.state.page = fw::stat_page::kAffinity;
  fw::SetWeaponStats(details.ctx, 1);
}

TEST_CASE("Weapon stats copy the inventory weapon's loadout",
          "[menu][menu-rows-details]") {
  Details d;
  ShowWeaponStats(d);
  CHECK(d.state.stats[9] == 60);
  CHECK(d.state.stats[26] == 25);
  CHECK(StatRange(d.state, 32, 5) == std::vector<std::int16_t>{1, 2, 3, 4, 2});
  CHECK(DpPp(d.state) == fw::DpPpValues{3, 10, 7, 20});
  CHECK(d.state.primary == Primary{11, 12, 13, 1, 21, 22, 23, 1});
  CHECK(d.state.range_risk == RangeRisk{5, 9, 0, 1, 0, 0, 0, 0});
}

TEST_CASE("Weapon stats show every page of the inventory weapon",
          "[menu][menu-rows-details]") {
  Details d;
  ShowWeaponStats(d);
  CHECK(d.state.subtype == fw::equipment_subtype::kWeapon);
  CHECK(d.state.item == 1);
  CHECK(d.state.enabled_pages == fw::stat_page::kAllEnabled);
  CHECK(d.state.page == fw::stat_page::kAffinity);
  CHECK(d.host.calls.empty());
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Weapon stats read an equipped weapon from Ashley",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleWeapon(d.host.rows);
  d.host.rows.weapons[0].is_equipped = 1;
  d.host.equipped_weapon = StatWeapon();
  fw::SetWeaponStats(d.ctx, 1);
  CHECK(d.host.calls == std::vector<std::string>{"EquippedWeapon()"});
  CHECK(d.state.primary == Primary{11, 12, 13, 1, 21, 22, 23, 1});
  d.state.stats[0] = 5;
  fw::SetWeaponStats(d.ctx, 0);
  CHECK(d.state.stats == Stats{});
  CHECK(d.state.primary == Primary{0, 0, 0, 1, 0, 0, 0, 1});
  CHECK(d.state.range_risk == RangeRisk{0, 0, 0, 1, 0, 0, 0, 0});
  CHECK(d.state.item == 0);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Weapon stats refuse a missing weapon or loadout",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleWeapon(d.host.rows);
  d.host.rows.weapons[0].is_equipped = 1;
  fw::SetWeaponStats(d.ctx, 1);
  fw::SetWeaponStats(d.ctx, 2);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"weapon stats without the weapon's loadout",
                                 "weapon stats of a weapon past the table"});
  CHECK(d.state.primary == Primary{0, 0, 0, 1, 0, 0, 0, 1});
  CHECK(d.state.range_risk == RangeRisk{0, 0, 0, 1, 0, 0, 0, 0});
  CHECK(d.state.subtype == fw::equipment_subtype::kWeapon);
  CHECK(d.state.item == 2);
}

// Inventory shield 1 with StatShield's loadout over a stale range.
void ShowShieldStats(Details& details) {
  AddSampleShield(details.host.rows);
  details.host.rows.shield_loadout = StatShield();
  details.state.range_risk[0] = 9;
  fw::SetShieldStats(details.ctx, 1);
}

TEST_CASE("Shield stats copy the shield's loadout",
          "[menu][menu-rows-details]") {
  Details d;
  ShowShieldStats(d);
  CHECK(d.state.stats[9] == 60);
  CHECK(StatRange(d.state, 32, 5) == std::vector<std::int16_t>{5, 6, 7, 8, 0});
  CHECK(DpPp(d.state) == fw::DpPpValues{2, 2, 3, 4});
  CHECK(d.state.primary == Primary{1, 3, 5, 1, 2, 4, 6, 1});
  CHECK(d.state.range_risk == RangeRisk{});
}

TEST_CASE("Shield stats show every page of the inventory shield",
          "[menu][menu-rows-details]") {
  Details d;
  ShowShieldStats(d);
  CHECK(d.state.subtype == fw::equipment_subtype::kShield);
  CHECK(d.state.item == 1);
  CHECK(d.state.enabled_pages == fw::stat_page::kAllEnabled);
  CHECK(d.host.calls.empty());
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Shield stats read an equipped shield and refuse missing ones",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleShield(d.host.rows);
  d.host.rows.shields[0].is_equipped = 1;
  fw::SetShieldStats(d.ctx, 1);
  d.host.equipped_shield = StatShield();
  fw::SetShieldStats(d.ctx, 1);
  CHECK(d.state.primary == Primary{1, 3, 5, 1, 2, 4, 6, 1});
  fw::SetShieldStats(d.ctx, 3);
  CHECK(d.host.calls ==
        std::vector<std::string>{"EquippedShield()", "EquippedShield()"});
  CHECK(d.host.refusals ==
        std::vector<std::string>{"shield stats without the shield's loadout",
                                 "shield stats of a shield past the table"});
  CHECK(d.state.item == 3);
  CHECK(d.state.primary == Primary{0, 0, 0, 1, 0, 0, 0, 1});
}

// Inventory armour 1 for limb 2, whose equipped armour is StatArmor(2).
void ShowArmorStats(Details& details) {
  items::InventoryArmor armor;
  armor.limb = 2;
  details.host.rows.armor.push_back(armor);
  details.host.armor_loadout = StatArmor(2);
  fw::SetArmorStats(details.ctx, 1);
}

TEST_CASE("Armor stats read the limb's equipped armour",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmorStats(d);
  CHECK(d.host.calls == std::vector<std::string>{"EquippedArmor(1)"});
  CHECK(d.state.stats[26] == 25);
  CHECK(StatRange(d.state, 32, 4) == std::vector<std::int16_t>{1, 2, 3, 4});
  CHECK(DpPp(d.state) == fw::DpPpValues{5, 8, 0, 0});
  CHECK(d.state.primary == Primary{4, 5, 6, 1, 1, 2, 3, 1});
}

TEST_CASE("Armor stats show every page of the armour",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmorStats(d);
  CHECK(d.state.subtype == fw::equipment_subtype::kArmor);
  CHECK(d.state.item == 1);
  CHECK(d.state.enabled_pages == fw::stat_page::kAllEnabled);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Accessory stats read the inventory loadout",
          "[menu][menu-rows-details]") {
  Details d;
  d.host.rows.armor.emplace_back();
  d.host.armor_loadout = StatArmor(7);
  fw::SetArmorStats(d.ctx, 1);
  CHECK(d.host.calls == std::vector<std::string>{"InventoryArmor()"});
  CHECK(d.state.primary == Primary{4, 5, 6, 2, 1, 2, 3, 2});
  CHECK(d.state.subtype == fw::equipment_subtype::kAccessory);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Armor stats refuse a missing armour or loadout",
          "[menu][menu-rows-details]") {
  Details d;
  d.host.rows.armor.emplace_back();
  fw::SetArmorStats(d.ctx, 1);
  fw::SetArmorStats(d.ctx, 2);
  fw::SetArmorStats(d.ctx, 0);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"armor stats without the armor's loadout",
                                 "armor stats of an armor past the table"});
  CHECK(d.host.calls == std::vector<std::string>{"InventoryArmor()"});
  CHECK(d.state.subtype == fw::equipment_subtype::kArmor);
  CHECK(d.state.item == 0);
}

// A blade with a class and an affinity for every stat row.
items::InventoryBlade StatBlade() {
  items::InventoryBlade blade;
  blade.classes = {1, 2, 3, 4, 5, 6, 7, 8};
  blade.affinities = {-1, -2, -3, -4, -5, -6, -7, -8};
  blade.current_dp = 150;
  blade.max_dp = 300;
  blade.current_pp = 4;
  blade.max_pp = 8;
  blade.strength = 3;
  blade.intelligence = -2;
  blade.agility = 1;
  blade.range.range = 6;
  blade.cost = 12;
  return blade;
}

// Inventory blade 1 set from the type page, whose stat 33 the copy keeps.
void ShowBladeStats(Details& details) {
  details.host.rows.blades.push_back(StatBlade());
  details.state.page = fw::stat_page::kType;
  details.state.stats[33] = 9;
  fw::SetBladeStats(details.ctx, 1);
}

TEST_CASE("Blade stats copy the blade's parts", "[menu][menu-rows-details]") {
  Details d;
  ShowBladeStats(d);
  CHECK(StatRange(d.state, 0, 16) == std::vector<std::int16_t>{1, 2, 3, 4, 5, 6,
                                                               7, 8, 1, 2, 3, 4,
                                                               5, 6, 7, 8});
  CHECK(StatRange(d.state, 16, 16) ==
        std::vector<std::int16_t>{-1, -2, -3, -4, -5, -6, -7, -8, -1, -2, -3,
                                  -4, -5, -6, -7, -8});
  CHECK(d.state.stats[33] == 9);
  CHECK(DpPp(d.state) == fw::DpPpValues{2, 3, 4, 8});
  CHECK(d.state.primary == Primary{3, -2, 1, 1, 3, -2, 1, 1});
  CHECK(d.state.range_risk == RangeRisk{6, 12, 0, 1, 0, 0, 0, 0});
}

TEST_CASE("Blade stats show the class and affinity pages",
          "[menu][menu-rows-details]") {
  Details d;
  ShowBladeStats(d);
  CHECK(d.state.subtype == fw::equipment_subtype::kBlade);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.state.page == fw::stat_page::kClass);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Grip stats copy the grip's types", "[menu][menu-rows-details]") {
  Details d;
  items::InventoryGrip grip;
  grip.types = {1, -2, 3, 0};
  grip.strength = 4;
  grip.agility = -1;
  d.host.rows.grips.push_back(grip);
  fw::SetGripStats(d.ctx, 1);
  CHECK(StatRange(d.state, 32, 4) == std::vector<std::int16_t>{1, -2, 3, 0});
  CHECK(d.state.primary == Primary{4, 0, -1, 1, 4, 0, -1, 1});
  CHECK(d.state.subtype == fw::equipment_subtype::kGrip);
  CHECK(d.state.enabled_pages == fw::stat_page::kTypeEnabled);
  CHECK(d.state.page == fw::stat_page::kType);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Gem stats copy the gem's classes and affinities",
          "[menu][menu-rows-details]") {
  Details d;
  items::InventoryGem gem;
  gem.classes = {2, 0, 0, 0, 0, 0, 0, 0};
  gem.affinities = {0, 3, 0, 0, 0, 0, 0, 0};
  gem.strength = 1;
  gem.intelligence = 2;
  gem.agility = 3;
  d.host.rows.gems.push_back(gem);
  d.state.page = fw::stat_page::kType;
  fw::SetGemStats(d.ctx, 1);
  const std::array<std::int16_t, 4> copied{
      d.state.stats[0], d.state.stats[8], d.state.stats[17], d.state.stats[25]};
  CHECK(copied == std::array<std::int16_t, 4>{2, 2, 3, 3});
  CHECK(d.state.primary == Primary{1, 2, 3, 1, 1, 2, 3, 1});
  CHECK(d.state.subtype == fw::equipment_subtype::kGem);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.state.page == fw::stat_page::kClass);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Part stats refuse parts past their tables",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.subtype = fw::equipment_subtype::kArmor;
  fw::SetBladeStats(d.ctx, 1);
  fw::SetGripStats(d.ctx, 1);
  fw::SetGemStats(d.ctx, 1);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"blade stats of a blade past the table",
                                 "grip stats of a grip past the table",
                                 "gem stats of a gem past the table"});
  CHECK(d.state.subtype == fw::equipment_subtype::kArmor);
}

// ---------------------------------------------------------------- detail
// screen

TEST_CASE("Detail descriptions format the shown part",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleWeapon(d.host.rows);
  items::InventoryArmor armor;
  armor.id = 30;
  armor.category = 2;
  armor.material = 4;
  d.host.rows.armor.push_back(armor);
  d.state.item = 1;
  CHECK(fw::BladeDetailDescription(d.ctx) == kBufferText);
  CHECK(fw::GripDetailDescription(d.ctx) == kBufferText);
  CHECK(fw::ArmorDetailDescription(d.ctx) == kBufferText);
  CHECK(fw::GemDetailDescription(d.ctx) == kBufferText);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Detail descriptions refuse parts past their tables",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.item = 1;
  CHECK(fw::BladeDetailDescription(d.ctx) == std::nullopt);
  CHECK(fw::GripDetailDescription(d.ctx) == std::nullopt);
  CHECK(fw::ArmorDetailDescription(d.ctx) == std::nullopt);
  CHECK(fw::GemDetailDescription(d.ctx) == std::nullopt);
  CHECK(d.host.refusals == std::vector<std::string>{
                               "detail description of a blade past the table",
                               "detail description of a grip past the table",
                               "detail description of an armor past the table",
                               "detail description of a gem past the table"});
}

// Inventory weapon 1 and its loadout on the detail screen.
void ShowWeaponDetails(Details& details) {
  AddSampleWeapon(details.host.rows);
  details.host.rows.weapon_loadout = SampleWeapon();
  details.state.item = 1;
}

TEST_CASE("Weapon detail rows show the weapon and its blade",
          "[menu][menu-rows-details]") {
  Details d;
  ShowWeaponDetails(d);
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 0) == kBufferText);
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 1) == kBufferText);
  CHECK(d.state.subtype == fw::equipment_subtype::kWeapon);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.state.bars_target == 3);
  CHECK(d.host.calls ==
        std::vector<std::string>{"RowSelected(10,1)", "RowSelected(11,1)"});
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Weapon detail rows show the grip's types and the gem's pages",
          "[menu][menu-rows-details]") {
  Details d;
  ShowWeaponDetails(d);
  d.state.bars_target = 3;
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 2) == kBufferText);
  CHECK(d.state.enabled_pages == fw::stat_page::kTypeEnabled);
  CHECK(d.state.bars_target == 0);
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 3) == kBufferText);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.host.calls ==
        std::vector<std::string>{"RowSelected(12,1)", "RowSelected(13,1)"});
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Weapon detail rows show no gems in empty slots",
          "[menu][menu-rows-details]") {
  Details d;
  ShowWeaponDetails(d);
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 4) == kNoGems);
  CHECK(d.state.primary == Primary{0, 0, 0, 1, 0, 0, 0, 1});
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 6) == kNoGems);
  CHECK(d.state.subtype == fw::equipment_subtype::kWeapon);
  CHECK(d.state.item == 1);
  CHECK(d.host.calls ==
        std::vector<std::string>{"RowSelected(14,1)", "RowSelected(16,1)"});
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Weapon detail rows refuse missing weapons and parts",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleWeapon(d.host.rows);
  d.state.item = 3;
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 0) == kNoGems);
  CHECK(d.state.item == 3);
  d.state.item = 1;
  d.host.rows.weapons[0].blade = 9;
  d.host.rows.weapons[0].grip = 9;
  d.host.rows.weapons[0].gems = {9, 0, 0, 0};
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 1) == kNoGems);
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 2) == kNoGems);
  CHECK(fw::SelectWeaponDetailRow(d.ctx, 3) == kNoGems);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"detail row of a weapon past the table",
                                 "detail row of a blade past the table",
                                 "detail row of a grip past the table",
                                 "detail row of a gem past the table"});
}

// Inventory shield 1 and its loadout on the detail screen.
void ShowShieldDetails(Details& details) {
  AddSampleShield(details.host.rows);
  details.host.rows.shield_loadout = SampleShield();
  details.state.item = 1;
}

TEST_CASE("Shield detail rows show the shield and its first gem",
          "[menu][menu-rows-details]") {
  Details d;
  ShowShieldDetails(d);
  CHECK(fw::SelectShieldDetailRow(d.ctx, 0) == kBufferText);
  CHECK(fw::SelectShieldDetailRow(d.ctx, 1) == kBufferText);
  CHECK(d.state.subtype == fw::equipment_subtype::kShield);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.state.bars_state == 2);
  CHECK(d.host.calls ==
        std::vector<std::string>{"RowSelected(10,1)", "RowSelected(11,1)"});
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Shield detail rows show no gems in empty slots or missing shields",
          "[menu][menu-rows-details]") {
  Details d;
  ShowShieldDetails(d);
  CHECK(fw::SelectShieldDetailRow(d.ctx, 2) == kNoGems);
  CHECK(fw::SelectShieldDetailRow(d.ctx, 4) == kNoGems);
  d.state.item = 2;
  CHECK(fw::SelectShieldDetailRow(d.ctx, 0) == kNoGems);
  CHECK(d.state.item == 2);
  CHECK(d.host.calls == std::vector<std::string>{"RowSelected(12,1)",
                                                 "RowSelected(14,1)",
                                                 "RowSelected(10,1)"});
  CHECK(d.host.refusals ==
        std::vector<std::string>{"detail row of a shield past the table"});
}

TEST_CASE("Weapon details reach the grip's gem slots",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleWeapon(d.host.rows);
  d.state.subtype = fw::equipment_subtype::kWeapon;
  d.state.item = 1;
  CHECK(ReachableElements(d.ctx) == "TTTTTTTTFTTTTTFFTTTTT");
  d.state.page = fw::stat_page::kAffinity;
  CHECK(fw::IsItemDetailElementReachable(d.ctx, 8));
  d.state.element15_enabled = 1;
  CHECK(fw::IsItemDetailElementReachable(d.ctx, 15));
  CHECK_FALSE(fw::IsItemDetailElementReachable(d.ctx, 21));
  CHECK_FALSE(fw::IsItemDetailElementReachable(d.ctx, -1));
  d.state.item = 2;
  CHECK(fw::IsItemDetailElementReachable(d.ctx, 11));
  CHECK_FALSE(fw::IsItemDetailElementReachable(d.ctx, 12));
  CHECK(d.host.refusals ==
        std::vector<std::string>{"detail element of a weapon past the tables"});
}

TEST_CASE("Shield details reach the shield's gem slots",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleShield(d.host.rows);
  d.host.rows.shields[0].base.gem_slots = 1;
  d.state.subtype = fw::equipment_subtype::kShield;
  d.state.item = 1;
  d.state.page = fw::stat_page::kType;
  CHECK(ReachableElements(d.ctx) == "TTTTTFFFFTTFFFFFFFTTT");
  d.state.item = 2;
  CHECK_FALSE(fw::IsItemDetailElementReachable(d.ctx, 10));
  CHECK(d.host.refusals ==
        std::vector<std::string>{"detail element of a shield past the table"});
  d.state.subtype = 0;
  d.state.item = 0;
  CHECK(ReachableElements(d.ctx) == "FFTTTFFFFTFFFFFFFFTTT");
}

TEST_CASE("Item details keep a selection the input cannot move",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmor(d);
  d.host.input.repeat = 0x1000;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.state.selected == 9);
  CHECK(d.state.previous_item == 1);
  CHECK(d.state.bars_state == 2);
  CHECK(d.state.bars_target == 1);
  CHECK(d.state.cursor_step == 1);
  CHECK(d.host.messages == std::vector<TextRef>{kBufferText});
  CHECK(d.host.calls == DetailFrame({}, {Call("Cursor", {0, Xy(141, 10)})}));
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Item details skip unreachable elements and play the move",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmor(d);
  d.state.previous_item = 1;
  d.host.input.repeat = 0x4000;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.state.selected == 18);
  CHECK(d.host.messages == std::vector<TextRef>{Entry(661)});
  CHECK(d.host.calls ==
        DetailFrame({"Sound(0)"}, {Call("Cursor", {0, Xy(152, 146)})}));
}

TEST_CASE("Item details step off an unreachable selection",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmor(d);
  d.state.selected = 10;
  d.state.previous_item = 1;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.state.selected == 9);
  CHECK(d.host.calls ==
        DetailFrame({"Sound(0)"}, {Call("Cursor", {0, Xy(141, 10)})}));
}

TEST_CASE("Item detail help follows the selected element",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleWeapon(d.host.rows);
  d.host.rows.weapon_loadout = SampleWeapon();
  d.state.subtype = fw::equipment_subtype::kWeapon;
  d.state.item = 1;
  d.state.page = fw::stat_page::kAffinity;
  for (const int element : {0, 1, 2, 16, 19}) {
    d.state.selected = static_cast<std::uint8_t>(element);
    fw::UpdateItemDetails(d.ctx);
  }
  CHECK(d.host.messages == std::vector<TextRef>{Entry(415), Entry(414),
                                                Entry(422), Entry(432),
                                                Entry(659)});
  CHECK(d.state.bars_target == 3);
  CHECK(d.state.page == fw::stat_page::kAffinity);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Item detail help names the stat page's row",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmor(d);
  d.state.selected = 3;
  for (const std::uint8_t page :
       {fw::stat_page::kClass, fw::stat_page::kAffinity,
        fw::stat_page::kType}) {
    d.state.page = page;
    fw::UpdateItemDetails(d.ctx);
  }
  CHECK(d.host.messages ==
        std::vector<TextRef>{Entry(417), Entry(423), Entry(430)});
  d.state.page = fw::stat_page::kNone;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.host.messages.size() == 3);
  CHECK(d.host.refusals == std::vector<std::string>{
                               "detail help of a stat page past the type page",
                               "item details without a help text"});
}

TEST_CASE("Item detail cursor offsets the unused element",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmor(d);
  d.state.selected = 15;
  d.state.element15_enabled = 1;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.host.messages ==
        std::vector<TextRef>{Help(fw::item_text::kEndsSetupOffset)});
  CHECK(d.host.calls.back() == Call("Cursor", {0, Xy(180, 24) + (1U << 20)}));
  CHECK(d.state.element15_enabled == 0);
}

TEST_CASE("Shield details describe the selected gem slot",
          "[menu][menu-rows-details]") {
  Details d;
  AddSampleShield(d.host.rows);
  d.host.rows.shield_loadout = SampleShield();
  d.state.subtype = fw::equipment_subtype::kShield;
  d.state.item = 1;
  d.state.selected = 10;
  d.state.previous_item = 1;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.host.messages == std::vector<TextRef>{kBufferText});
  CHECK(d.host.calls ==
        DetailFrame(
            {}, {"RowSelected(11,1)", Call("Cursor", {0, Xy(148, 26) + 7})}));
  CHECK(d.state.subtype == fw::equipment_subtype::kShield);
  CHECK(d.state.item == 1);
  CHECK(d.state.bars_target == 0);
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Item details refuse selections and maps past the elements",
          "[menu][menu-rows-details]") {
  Details d;
  ShowArmor(d);
  d.state.selected = 21;
  fw::UpdateItemDetails(d.ctx);
  d.state.selected = 9;
  d.state.navigation[9][0] = 30;
  d.host.input.repeat = 0x1000;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.state.selected == 9);
  d.state.navigation[9][0] = 10;
  d.state.navigation[10][0] = 11;
  d.state.navigation[11][0] = 10;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.host.refusals ==
        std::vector<std::string>{
            "item details with a selection past the elements",
            "detail navigation to an element past the map",
            "detail navigation found nothing reachable"});
  CHECK(d.host.calls.empty());
  CHECK(d.host.messages.empty());
}

TEST_CASE("Item details without a subtype have no help text",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.selected = 9;
  fw::UpdateItemDetails(d.ctx);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"detail description without an item subtype",
                                 "item details without a help text"});
  CHECK(d.host.calls == DetailFrame({}, {Call("Cursor", {0, Xy(141, 10)})}));
}

// ---------------------------------------------------------------- stat page

// The class page with the first class at 50 (40 equipped) and the second at
// -50.
void DrawClassPage(Details& details) {
  details.state.enabled_pages = 3;
  details.state.stats[0] = 50;
  details.state.stats[8] = 40;
  details.state.stats[1] = -50;
  fw::DrawEquipmentStatPage(details.ctx, 0);
}

TEST_CASE("Class stat pages draw each row's bars",
          "[menu][menu-rows-details]") {
  Details d;
  DrawClassPage(d);
  CHECK(d.host.calls == std::vector<std::string>{"Queue(-3)"});
  const auto& packets = d.host.queue.packets;
  REQUIRE(packets.size() == 186);
  CHECK(Body(packets[0]) ==
        std::vector<std::uint32_t>{0x40D0E030, Xy(116, 0x42), Xy(116, 0x45)});
  CHECK(Body(packets[1]) ==
        std::vector<std::uint32_t>{0x40978918, Xy(116, 64), Xy(116, 0x47)});
  CHECK(Body(packets[2]) ==
        std::vector<std::uint32_t>{0x40817318, Xy(116, 66), Xy(128, 66)});
  CHECK(Body(packets[6]) ==
        std::vector<std::uint32_t>{0x40330500, Xy(95, 66), Xy(140, 66)});
}

TEST_CASE("Class stat pages draw the background and frame",
          "[menu][menu-rows-details]") {
  Details d;
  DrawClassPage(d);
  const auto& packets = d.host.queue.packets;
  REQUIRE(packets.size() == 186);
  CHECK(Body(packets[18]) == std::vector<std::uint32_t>{0xE1000000});
  CHECK(Body(packets[19]) == std::vector<std::uint32_t>{0x50330500, Xy(0, 64),
                                                        0x5D3200, Xy(96, 64)});
  CHECK(Body(packets[30]) == std::vector<std::uint32_t>{0xE1000200});
  CHECK(Body(packets[33]) ==
        std::vector<std::uint32_t>{0x40693F6E, Xy(116, 85), Xy(104, 85)});
}

TEST_CASE("Class stat pages draw each row's value and name",
          "[menu][menu-rows-details]") {
  Details d;
  DrawClassPage(d);
  const auto& draws = d.host.draws;
  REQUIRE(draws.size() == 14);
  CHECK(draws[0] == Call("Compared", {50, Xy(88, 64), 40, kStatSlot}));
  CHECK(draws[1] == TextCall("#HUMAN", Xy(64, 64), kGray, kStatSlot));
  CHECK(draws[11] == TextCall("#EVIL", Xy(64, 144), kGray, kStatSlot));
  CHECK(draws[12] == Call("Prompt", {24, 48, 135, 12}));
  CHECK(draws[13] == Call("Icon", {1, 16, 46, fw::detail_layer::kButtonIcon}));
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Affinity stat pages compare and colour a weapon's totals",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.subtype = fw::equipment_subtype::kWeapon;
  d.state.enabled_pages = fw::stat_page::kAllEnabled;
  d.state.page = fw::stat_page::kAffinity;
  d.state.stats[63] = 1;
  d.state.stats[16] = 10;
  d.state.stats[24] = 10;
  d.state.stats[56] = 4;
  d.state.stats[17] = 30;
  d.state.stats[25] = 30;
  d.state.stats[57] = 30;
  d.state.stats[18] = 20;
  d.state.stats[26] = 15;
  d.state.stats[58] = 25;
  fw::DrawEquipmentStatPage(d.ctx, 0);
  REQUIRE(d.host.draws.size() >= 11);
  const std::vector<std::string> first(d.host.draws.begin(),
                                       d.host.draws.begin() + 11);
  CHECK(first == std::vector<std::string>{
                     Call("Compared", {6, Xy(164, 64), 0, kStatSlot}),
                     TextCall("#+", Xy(173, 64), kGain, kStatSlot),
                     Call("Colored", {10, Xy(88, 64), kDim, kStatSlot}),
                     TextCall("#PHYSICAL", Xy(64, 64), kDim, kStatSlot),
                     Call("Compared", {0, Xy(164, 80), 0, kStatSlot}),
                     TextCall("#*", Xy(173, 80), kGray, kStatSlot),
                     Call("Colored", {30, Xy(88, 80), kGray, kStatSlot}),
                     TextCall("#AIR", Xy(64, 80), kGray, kStatSlot),
                     Call("Compared", {-5, Xy(164, 96), 0, kStatSlot}),
                     Call("Colored", {20, Xy(88, 96), kGain, kStatSlot}),
                     TextCall("#FIRE", Xy(64, 96), kDim, kStatSlot)});
  CHECK(d.host.queue.packets.size() == std::size_t{7} * 31);
}

TEST_CASE("Type stat pages highlight the weapon's damage type",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.subtype = fw::equipment_subtype::kWeapon;
  d.state.enabled_pages = fw::stat_page::kAllEnabled;
  d.state.page = fw::stat_page::kType;
  d.state.stats[33] = 1;
  d.state.stats[34] = 2;
  d.state.stats[35] = 3;
  d.state.stats[36] = 2;
  fw::DrawEquipmentStatPage(d.ctx, 0);
  CHECK(d.host.draws ==
        std::vector<std::string>{
            Call("Colored", {1, Xy(88, 64), kDim, kStatSlot}),
            TextCall("#BLUNT", Xy(64, 64), kDim, kStatSlot),
            Call("Colored", {2, Xy(88, 80), kGray, kStatSlot}),
            TextCall("#EDGED", Xy(64, 80), kGray, kStatSlot),
            Call("Colored", {3, Xy(88, 96), kDim, kStatSlot}),
            TextCall("#PIERCING", Xy(64, 96), kDim, kStatSlot),
            Call("Prompt", {24, 48, 135, 12}),
            Call("Icon", {1, 16, 46, fw::detail_layer::kButtonIcon})});
  CHECK(d.host.queue.packets.size() == 93);
}

TEST_CASE("Stat pages refuse a page past the type page",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.page = fw::stat_page::kNone;
  fw::DrawEquipmentStatPage(d.ctx, 0);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"stat page past the type page"});
  CHECK(d.host.calls.empty());
  CHECK(d.host.draws.empty());
}

TEST_CASE("Stat page tabs style the enabled and shown pages",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.enabled_pages = 3;
  fw::DrawEquipmentStatPage(d.ctx, 0);
  CHECK(d.host.styles == std::vector<int>{5, 4, 4, 4, 7});
  REQUIRE(d.host.glyphs.size() == 19);
  CHECK(d.host.glyphs.front() == Call("Glyph", {12, 34, 48, -7}));
  CHECK(d.host.glyphs.back() == Call("Glyph", {14, 52, 48, -7}));
  Details shown;
  shown.state.enabled_pages = fw::stat_page::kAllEnabled;
  shown.state.page = fw::stat_page::kAffinity;
  fw::DrawEquipmentStatPage(shown.ctx, 0);
  CHECK(shown.host.styles == std::vector<int>{4, 4, 5, 4, 4});
}

TEST_CASE("Stat page panels start sliding in", "[menu][menu-rows-details]") {
  Details d;
  d.state.page = fw::stat_page::kType;
  fw::UpdateEquipmentStatPages(d.ctx, 3);
  CHECK(d.state.page == fw::stat_page::kClass);
  CHECK(d.state.enabled_pages == 3);
  CHECK(d.state.panel_direction == 3);
  CHECK(d.state.panel_slide == 10);
  CHECK(d.host.draws.empty());
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Stat page panels slide in by a step", "[menu][menu-rows-details]") {
  Details d;
  fw::UpdateEquipmentStatPages(d.ctx, 3);
  fw::UpdateEquipmentStatPages(d.ctx, 0);
  CHECK(d.state.panel_slide == 9);
  REQUIRE_FALSE(d.host.draws.empty());
  CHECK(d.host.draws[0] == Call("Compared", {0, Xy(8, 64), 0, kStatSlot}));
}

TEST_CASE("Stat page panels slide out by a step", "[menu][menu-rows-details]") {
  Details d;
  fw::UpdateEquipmentStatPages(d.ctx, 3);
  fw::UpdateEquipmentStatPages(d.ctx, -1);
  CHECK(d.state.panel_direction == -1);
  CHECK(d.state.panel_slide == 10);
  fw::UpdateEquipmentStatPages(d.ctx, 0);
  CHECK(d.state.panel_slide == 9);
  REQUIRE_FALSE(d.host.draws.empty());
  CHECK(d.host.draws[0] == Call("Compared", {0, Xy(64, 64), 0, kStatSlot}));
}

TEST_CASE("Stat page panels draw nothing once slid out",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.panel_direction = -1;
  d.state.panel_slide = 0;
  fw::UpdateEquipmentStatPages(d.ctx, 0);
  CHECK(d.host.draws.empty());
  CHECK(d.host.queue.packets.empty());
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Stat page panels turn to the next enabled page",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.enabled_pages = 3;
  d.state.panel_direction = 3;
  d.host.input.pressed = 0x80;
  fw::UpdateEquipmentStatPages(d.ctx, 0);
  CHECK(d.state.page == fw::stat_page::kAffinity);
  CHECK(d.host.calls == std::vector<std::string>{"PageSound()", "Queue(-3)"});
  REQUIRE(d.host.draws.size() > 1);
  CHECK(d.host.draws[1] == TextCall("#HUMAN", Xy(64, 64), kGray, kStatSlot));
  Details single;
  single.state.enabled_pages = fw::stat_page::kClassEnabled;
  single.state.panel_direction = 3;
  single.host.input.pressed = 0x80;
  fw::UpdateEquipmentStatPages(single.ctx, 0);
  CHECK(single.state.page == fw::stat_page::kClass);
  CHECK(single.host.calls == std::vector<std::string>{"Sound(3)", "Queue(-3)"});
}

TEST_CASE("Stat page panels refuse turns and slides they cannot make",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.panel_direction = 3;
  d.host.input.pressed = 0x80;
  fw::UpdateEquipmentStatPages(d.ctx, 0);
  CHECK(d.host.calls == std::vector<std::string>{"Queue(-3)"});
  d.host.steps = {0, 1};
  d.state.panel_slide = 5;
  fw::UpdateEquipmentStatPages(d.ctx, 0);
  CHECK(d.state.panel_slide == 5);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"stat page turn with none enabled",
                                 "stat page slide past the animation steps"});
  CHECK(d.host.calls.size() == 1);
}

// ---------------------------------------------------------------- panels

fw::ActorComparisonStats ComparedActor() {
  fw::ActorComparisonStats actor;
  actor.accessory = {1, 2, 3};
  actor.base = {50, 60, 70};
  actor.totals = {10, 20, 30};
  return actor;
}

TEST_CASE("Comparison stats subtract a battle actor's equipment",
          "[menu][menu-rows-details]") {
  Details d;
  d.host.actors = {ComparedActor(), ComparedActor(), ComparedActor()};
  d.state.subtype = fw::equipment_subtype::kShield;
  fw::SetActorComparisonStats(d.ctx, 1);
  CHECK(d.state.comparison_deltas == Comparison{40, 40, 40, 1});
  CHECK(d.state.comparison_accessory == Comparison{1, 2, 3, 0});
  CHECK(d.state.comparison_totals == Comparison{});
  CHECK(d.state.primary == Primary{10, 20, 30, 0, 10, 20, 30, 0});
  CHECK(d.state.subtype == 0);
  CHECK(d.host.calls == std::vector<std::string>{"ActorStats(1)"});
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Comparison stats know Ashley and keep other actors' totals",
          "[menu][menu-rows-details]") {
  Details d;
  d.host.actors = {ComparedActor(), ComparedActor(), ComparedActor()};
  fw::SetActorComparisonStats(d.ctx, 0);
  CHECK(d.state.primary[3] == 1);
  fw::SetActorComparisonStats(d.ctx, 18);
  CHECK(d.state.comparison_deltas == Comparison{50, 60, 70, 2});
  CHECK(d.state.comparison_totals == Comparison{10, 20, 30, 0});
  CHECK(d.host.calls ==
        std::vector<std::string>{"ActorStats(0)", "ActorStats(2)"});
}

TEST_CASE("Comparison stats refuse an absent actor",
          "[menu][menu-rows-details]") {
  Details d;
  d.host.actors = {ComparedActor(), ComparedActor(), ComparedActor()};
  d.state.comparison_deltas = {50, 60, 70, 2};
  fw::SetActorComparisonStats(d.ctx, 5);
  CHECK(d.state.comparison_deltas == Comparison{50, 60, 70, 2});
  CHECK(d.host.calls == std::vector<std::string>{"ActorStats(5)"});
  CHECK(d.host.refusals ==
        std::vector<std::string>{"comparison stats of an absent actor"});
}

TEST_CASE("Primary stat panels start their animations",
          "[menu][menu-rows-details]") {
  Details d;
  fw::UpdatePrimaryStatPanel(d.ctx, 1);
  CHECK(d.state.primary_animation == 1);
  CHECK(d.state.primary_slide == 11);
  d.state.primary_slide = 3;
  fw::UpdatePrimaryStatPanel(d.ctx, 1);
  CHECK(d.state.primary_slide == 3);
  fw::UpdatePrimaryStatPanel(d.ctx, 2);
  CHECK(d.state.primary_animation == 2);
  CHECK(d.state.primary_slide == 7);
  CHECK(d.host.draws.empty());
}

TEST_CASE("Primary stat panels move by a step of their animation",
          "[menu][menu-rows-details]") {
  Details d;
  fw::UpdatePrimaryStatPanel(d.ctx, 2);
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  CHECK(d.state.primary_slide == 6);
  REQUIRE_FALSE(d.host.draws.empty());
  CHECK(d.host.draws[0] ==
        TextCall("ORG/EQP", Xy(284, 146), kGray, kPanelSlot));
}

TEST_CASE("Primary stat panels draw nothing settled or past their steps",
          "[menu][menu-rows-details]") {
  Details d;
  d.state.primary_animation = 2;
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  CHECK(d.host.draws.empty());
  d.state.primary_animation = 1;
  d.state.primary_slide = 11;
  d.host.steps = {0, 1};
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  CHECK(d.host.draws.empty());
  CHECK(d.state.primary_slide == 11);
  CHECK(d.host.refusals == std::vector<std::string>{
                               "primary stat slide past the animation steps"});
}

TEST_CASE("Primary stat panels compare the stats with the equipment",
          "[menu][menu-rows-details]") {
  Details d;
  fw::SetPrimaryStats(d.state, {10, 20, 30, 1});
  d.state.comparison_totals = {1, 2, 3, 0};
  d.state.comparison_deltas = {4, 5, 6, 0};
  d.state.comparison_accessory = {7, 8, 9, 0};
  d.state.primary_animation = 1;
  d.state.primary_slide = 11;
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  CHECK(d.state.primary_slide == 10);
  std::vector<std::string> expected{
      TextCall("ORG/EQP", Xy(348, 146), kGray, kPanelSlot)};
  const std::array<const char*, 3> names{"#STRENGTH", "#INTELLIGENCE",
                                         "#AGILITY"};
  const std::array<std::array<int, 4>, 3> values{
      {{10, 10, 21, 18}, {20, 20, 33, 30}, {30, 30, 45, 42}}};
  for (std::size_t row = 0; row < names.size(); ++row) {
    const std::uint32_t at = Xy(344, 156 + (static_cast<int>(row) * 10));
    const auto& [stat, stat_threshold, equipped, total] = values[row];
    expected.push_back(TextCall(names[row], at, kGray, kPanelSlot));
    expected.push_back(
        Call("Compared", {stat, at + 28, stat_threshold, kPanelSlot}));
    expected.push_back(
        Call("Compared", {equipped, at + 60, total, kPanelSlot}));
  }
  CHECK(d.host.draws == expected);
  CHECK(d.host.calls.empty());
}

// A weapon's range and risk on the panel, compared with actor 2's range.
void ShowRangeAndRisk(Details& d) {
  d.state.subtype = fw::equipment_subtype::kWeapon;
  d.state.primary[3] = 1;
  d.state.range_risk = {5, 9, 0, 1, 0, 0, 0, 0};
  d.state.comparison_deltas[3] = 2;
  d.state.primary_animation = 1;
  fw::ActorComparisonStats actor;
  actor.current_range = 7;
  actor.weapon_range = 4;
  d.host.actors = {{}, {}, actor};
}

TEST_CASE("Primary stat panels compare the range with the actor's",
          "[menu][menu-rows-details]") {
  Details d;
  ShowRangeAndRisk(d);
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  REQUIRE(d.host.draws.size() >= 8);
  const std::vector<std::string> first(d.host.draws.begin(),
                                       d.host.draws.begin() + 8);
  CHECK(first == std::vector<std::string>{
                     TextCall("ORG/EQP", Xy(252, 126), kGray, kPanelSlot),
                     TextCall("#RANGE", Xy(248, 136), kGray, kPanelSlot),
                     Call("Neutral", {5, Xy(276, 136), kPanelSlot}),
                     Call("Neutral", {8, Xy(308, 136), kPanelSlot}),
                     TextCall("#RISK", Xy(248, 146), kGray, kPanelSlot),
                     Call("Neutral", {9, Xy(276, 146), kPanelSlot}),
                     Call("Neutral", {9, Xy(308, 146), kPanelSlot}),
                     TextCall("#ATTACK/STR", Xy(248, 156), kGray, kPanelSlot)});
  CHECK(d.host.calls == std::vector<std::string>{"ActorStats(2)"});
  CHECK(d.host.refusals.empty());
}

TEST_CASE("Primary stat panels refuse an absent actor and unknown names",
          "[menu][menu-rows-details]") {
  Details d;
  ShowRangeAndRisk(d);
  d.state.comparison_deltas[3] = 5;
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  REQUIRE(d.host.draws.size() >= 4);
  CHECK(d.host.draws[2] == Call("Neutral", {5, Xy(276, 136), kPanelSlot}));
  CHECK(d.host.draws[3] == TextCall("#RISK", Xy(248, 146), kGray, kPanelSlot));
  Details mixed;
  ShowRangeAndRisk(mixed);
  mixed.state.subtype =
      fw::equipment_subtype::kWeapon | fw::equipment_subtype::kShield;
  fw::UpdatePrimaryStatPanel(mixed.ctx, 0);
  CHECK(mixed.host.draws.size() == 7);
  CHECK(d.host.refusals ==
        std::vector<std::string>{"range comparison of an absent actor"});
  CHECK(mixed.host.refusals ==
        std::vector<std::string>{"primary stat names past the table"});
}

TEST_CASE("Primary stat panels hide unknown stats",
          "[menu][menu-rows-details]") {
  Details d;
  ShowRangeAndRisk(d);
  d.state.primary[3] = 0;
  fw::UpdatePrimaryStatPanel(d.ctx, 0);
  REQUIRE(d.host.draws.size() >= 4);
  CHECK(d.host.draws[2] == TextCall("#??", Xy(276, 136), kGray, kPanelSlot));
  CHECK(d.host.draws[3] == TextCall("#??", Xy(308, 136), kGray, kPanelSlot));
  CHECK(d.host.calls.empty());
}

TEST_CASE("DP/PP bars draw both bars once static",
          "[menu][menu-rows-details]") {
  Details d;
  fw::SetDpPp(d.state, {250, 1000, 7, 20});
  fw::UpdateDpPpBars(d.ctx, 11);
  CHECK(d.state.bars_state == 2);
  CHECK(d.state.bars_target == 3);
  CHECK(d.host.draws.empty());
  fw::UpdateDpPpBars(d.ctx, 0);
  CHECK(d.state.bars_animation == 5);
  const std::uint32_t y = Xy(0, 32);
  const std::uint32_t label = y + Xy(0, -1);
  const std::uint32_t dp_end = (y | 66U) - 6;
  const std::uint32_t pp_end = (y | 136U) - 6;
  CHECK(d.host.draws ==
        std::vector<std::string>{
            TextCall("DP", label | 10U, kGray, kStatSlot),
            Call("Right", {0, y | 66U, 10, kStatSlot}),
            Call("Right", {2, dp_end, 0, kStatSlot}),
            Call("Right", {1, dp_end + Xy(-7, -2), 3, kStatSlot}),
            TextCall("PP", label | 80U, kGray, kStatSlot),
            Call("Right", {0, y | 136U, 20, kStatSlot}),
            Call("Right", {2, pp_end, 0, kStatSlot}),
            Call("Right", {1, pp_end + Xy(-7, -2), 7, kStatSlot}),
            Call("Gauge", {0, 20, Xy(10, 40), kStatSlot}),
            Call("Gauge", {1, 23, Xy(80, 40), kStatSlot})});
}

TEST_CASE("DP/PP bars slide in and out", "[menu][menu-rows-details]") {
  Details d;
  fw::UpdateDpPpBars(d.ctx, 1);
  CHECK(d.state.bars_state == 0);
  CHECK(d.state.bars_target == 1);
  fw::UpdateDpPpBars(d.ctx, 0);
  CHECK(d.state.bars_animation == 1);
  REQUIRE(d.host.draws.size() == 5);
  CHECK(d.host.draws.back() == Call("Gauge", {0, 0, Xy(10, 8), kStatSlot}));
  d.host.draws.clear();
  fw::UpdateDpPpBars(d.ctx, 6);
  CHECK(d.state.bars_state == 1);
  CHECK(d.state.bars_target == 1);
  fw::UpdateDpPpBars(d.ctx, 0);
  CHECK(d.state.bars_animation == 0);
  CHECK(d.host.draws.empty());
}

// ---------------------------------------------------------------- live

TEST_CASE("Live range, risk and primary stats reach the shared variables",
          "[menu][menu-rows-details]") {
  const std::vector<char> saved_range(std::begin(Menu_Framework_rangeRiskData),
                                      std::end(Menu_Framework_rangeRiskData));
  const std::vector<std::int16_t> saved_primary(
      std::begin(Menu_Framework_strIntAgi), std::end(Menu_Framework_strIntAgi));
  std::ranges::fill(Menu_Framework_rangeRiskData, '\0');
  fw::SetLiveRangeAndRisk(300, 5, 9, 1);
  const std::vector<int> range(std::begin(Menu_Framework_rangeRiskData),
                               std::end(Menu_Framework_rangeRiskData));
  CHECK(range == std::vector<int>{44, 5, 9, 1, 0, 0, 0, 0});
  fw::SetLivePrimaryStats(11, 12, 13, 1);
  const std::vector<int> primary(std::begin(Menu_Framework_strIntAgi),
                                 std::end(Menu_Framework_strIntAgi));
  CHECK(primary == std::vector<int>{11, 12, 13, 1, 11, 12, 13, 1});
  CHECK(std::ranges::all_of(Menu_Framework_rangeRiskData,
                            [](char value) { return value == 0; }));
  std::ranges::copy(saved_range, std::begin(Menu_Framework_rangeRiskData));
  std::ranges::copy(saved_primary, std::begin(Menu_Framework_strIntAgi));
}

TEST_CASE("Live stat pages and the detail cursor reach the shared variables",
          "[menu][menu-rows-details]") {
  const char saved_page = Menu_Framework_itemStatPage;
  const char saved_enabled = Menu_Framework_enabledStatPages;
  const u_char saved_selected = Menu_Framework_selectedStatusViewElement;
  VS::test_support::ChannelWarnings warnings{"menu.framework"};
  Menu_Framework_itemStatPage = 2;
  fw::SetLiveEnabledStatPages(3);
  CHECK(Menu_Framework_enabledStatPages == 3);
  CHECK(Menu_Framework_itemStatPage == 0);
  fw::SetLiveEnabledStatPages(0);
  fw::ResetLiveItemDetailCursor();
  CHECK(Menu_Framework_selectedStatusViewElement == 9);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "equipment details: stat pages with none enabled"});
  Menu_Framework_itemStatPage = saved_page;
  Menu_Framework_enabledStatPages = saved_enabled;
  Menu_Framework_selectedStatusViewElement = saved_selected;
}

TEST_CASE("Live equipment detail statics reset the DP/PP bars",
          "[menu][menu-rows-details]") {
  VS::test_support::ChannelWarnings warnings{"menu.framework"};
  fw::UpdateLiveDpPpBars(11);
  fw::ResetLiveEquipmentDetailsStatics();
  fw::UpdateLiveDpPpBars(0);
  fw::ResetLiveEquipmentDetailsStatics();
  CHECK(warnings.messages.empty());
}

}  // namespace
