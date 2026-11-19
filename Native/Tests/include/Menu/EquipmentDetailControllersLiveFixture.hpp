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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Battle/TargetingState.h"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/TextBox.h"
#include "Render/Display.hpp"
#include "Render/EncodedText.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/Sprites.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/Text/VariableFont.hpp"
#include "Render/UiPrimitives.hpp"

namespace equipment_detail_tests {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
constexpr std::size_t kIndexedCategories = VS::Items::kItemCategoryCount - 1;
constexpr std::size_t kPrimitiveBytes = 0x20000;
constexpr std::size_t kOrderingEntries = 32;
constexpr std::size_t kSceneOrderingEntry = 18;
constexpr std::size_t kHelpEntries = 700;
constexpr int kPreviewOffset = 5;
constexpr int kMaxDp = 1000;
constexpr int kMaxPp = 100;
constexpr int kCurrentDp = 500;
constexpr int kCurrentPp = 40;
constexpr int kBladeStrength = 7;
constexpr int kSecondBladeStrength = 9;
constexpr int kBladeCost = 4;
constexpr int kBladeRange = 21;
constexpr int kBladeClass = -3;
constexpr int kBladeAffinity = 4;
constexpr int kGripStrength = 5;
constexpr int kShieldCategory = 6;
constexpr int kSelectedRow = 20;
constexpr int kWaitFrames = 11;
constexpr int kCountIcon = 0x116;
constexpr int kCountIconX = 256;
constexpr int kCountIconY = 32;
constexpr int kCountTextX = 280;
constexpr int kCountTextY = 36;
constexpr std::size_t kPacketVisitLimit = 1024;
constexpr std::uint32_t kDrawModeCommand = 0xE1;
constexpr std::uint32_t kRgbMask = 0xFFFFFF;
constexpr std::uint32_t kCompactTop = 76;
constexpr std::uint32_t kCompactHeight = 10;
constexpr std::uint32_t kCompactClut = 0x37F6;
constexpr std::uint32_t kCompactColumnMask = 0xFE;
constexpr std::uint32_t kWidthMask = 15;
constexpr std::uint32_t kNibbleBits = 4;
enum CompactWord : std::size_t {
  kCompactTag,
  kCompactColor,
  kCompactPosition,
  kCompactTexture,
  kCompactSize,
  kCompactWords,
};

template <typename T>
struct ValueRestore {
  T& target;
  T previous;
  explicit ValueRestore(T& value) : target(value), previous(value) {}
  ~ValueRestore() { target = previous; }
};

template <typename T, std::size_t Count>
struct ArrayRestore {
  std::span<T, Count> target;
  std::array<T, Count> previous{};
  explicit ArrayRestore(T* value) : target(value, Count) {
    std::copy(target.begin(), target.end(), previous.begin());
  }
  ~ArrayRestore() {
    std::copy(previous.begin(), previous.end(), target.begin());
  }
};

struct DrawnSprite {
  std::uint32_t color = 0;
  std::uint32_t position = 0;
  std::uint32_t texture = 0;
  std::uint32_t size = 0;
  std::uint32_t draw_mode = 0;
  bool compact = false;
  friend bool operator==(DrawnSprite, DrawnSprite) = default;
};

struct LiveFixture {
  ValueRestore<fw::MenuRow*> saved_rows{VS::Menu::Framework::menuItems};
  ValueRestore<VS::Items::InventoryContents> saved_inventory{
      VS::Items::inventory};
  ValueRestore<VS::Items::InventoryIndices> saved_indices{
      VS::Items::inventoryIndices};
  ValueRestore<Menu_Framework_MenuMemory> saved_memory{
      VS::Menu::Framework::menuMemory};
  ValueRestore<decltype(VS::Menu::Inventory::categoryOrders)> saved_orders{
      VS::Menu::Inventory::categoryOrders};
  ValueRestore<int> saved_held{VS::Input::buttonsState};
  ValueRestore<int> saved_repeat{VS::Input::buttonRepeat};
  ValueRestore<decltype(VS::Input::buttonsPressed)> saved_pressed{
      VS::Input::buttonsPressed};
  ValueRestore<int> saved_preview{Menu_Inventory_ashleyRenderState};
  ValueRestore<int> saved_y{Menu_Inventory_ashleyYOffset};
  ValueRestore<int> saved_return{Menu_Inventory_exitToBattle};
  ValueRestore<char> saved_title{Menu_Inventory_secondaryTitleEnabled};
  ValueRestore<char> saved_categories{Menu_Inventory_itemCategoryIconsEnabled};
  ValueRestore<char> saved_command{Menu_Inventory_statusCommandState};
  ValueRestore<char> saved_header{Menu_Inventory_selectedItemCategoryIconOnTop};
  ValueRestore<char> saved_pages{VS::Menu::Framework::enabledStatPages};
  ValueRestore<char> saved_page{VS::Menu::Framework::itemStatPage};
  ValueRestore<char> saved_subtype{VS::Menu::Framework::equipmentSubtype};
  ValueRestore<char> saved_item{VS::Menu::Framework::currentStatusViewItem};
  ValueRestore<u_char> saved_selection{
      VS::Menu::Framework::selectedStatusViewElement};
  ArrayRestore<int16_t, fw::kEquipmentStatCount> saved_stats{
      VS::Menu::Framework::equipmentStats};
  ArrayRestore<int16_t, fw::kPrimaryStatCount> saved_primary{
      VS::Menu::Framework::strIntAgi};
  ArrayRestore<char, fw::kRangeRiskBytes> saved_range{
      VS::Menu::Framework::rangeRiskData};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_deltas{
      VS::Menu::Framework::comparisonStatDeltas};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_totals{
      VS::Menu::Framework::comparisonStatTotals};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_accessory{
      VS::Menu::Framework::comparisonAccessoryStats};
  ValueRestore<VS::Items::InventoryWeapon*> saved_weapons{
      VS::Menu::Framework::weapons};
  ValueRestore<VS::Items::InventoryBlade*> saved_blades{
      VS::Menu::Framework::blades};
  ValueRestore<VS::Items::InventoryGrip*> saved_grips{
      VS::Menu::Framework::grips};
  ValueRestore<VS::Items::InventoryShield*> saved_shields{
      VS::Menu::Framework::shields};
  ValueRestore<VS::Items::InventoryArmor*> saved_armor{
      VS::Menu::Framework::armor};
  ValueRestore<VS::Items::InventoryGem*> saved_gems{VS::Menu::Framework::gems};
  ValueRestore<u_short*> saved_help{VS::Menu::Framework::itemHelp};
  ValueRestore<decltype(VS::Menu::Framework::itemNames)> saved_names{
      VS::Menu::Framework::itemNames};
  ValueRestore<char*> saved_buffer{VS::Menu::Framework::stringBuf};
  ValueRestore<decltype(Battle_stringContext)> saved_arguments{
      Battle_stringContext};
  ArrayRestore<Menu_TextBoxState, std::size(Menu_textBoxes)> saved_text_boxes{
      Menu_textBoxes};
  ValueRestore<VS::Render::FrameScratch> saved_frame{
      VS::Render::FrameScratchpad()};
  ValueRestore<void*> saved_primitive{VS::Render::legacyPrimitiveBuffers[0]};
  ValueRestore<void*> saved_ordering{VS::Render::legacyOrderingTables[0]};
  ValueRestore<int> saved_framebuffer{VS::Render::frameBuf};
  ValueRestore<VS::Render::FontStyle> saved_font{VS::Render::LiveFontStyle()};
  ArrayRestore<u_char, sizeof VS::Render::compactGlyphLeftTrim>
      saved_compact_trim{VS::Render::compactGlyphLeftTrim};
  ArrayRestore<u_char, sizeof VS::Render::compactGlyphWidths>
      saved_compact_widths{VS::Render::compactGlyphWidths};
  ArrayRestore<u_char, sizeof VS::Render::compactGlyphIndices>
      saved_compact_indices{VS::Render::compactGlyphIndices};
  ArrayRestore<std::uint8_t, sizeof VS::Render::variableFontGlyphWidths>
      saved_variable_widths{VS::Render::variableFontGlyphWidths};

  std::array<fw::MenuRow, fw::kRowCount> rows{};
  std::array<char, fw::kStringBufferBytes> description{};
  std::array<u_short, fw::kItemHelpBytes / 2> help{};
  std::array<std::array<char, fw::kItemNameRowBytes>,
             fw::kItemNamesBytes / fw::kItemNameRowBytes>
      names{};
  std::vector<u_long> primitives =
      std::vector<u_long>(kPrimitiveBytes / sizeof(u_long) + 2);
  std::array<u_long, kOrderingEntries> ordering{};

  LiveFixture() {
    inventory::ResetLiveEquipmentDetailControllers();
    fw::ResetLiveEquipmentDetailsStatics();
    BindInventory();
    BuildText();
    LoadFonts();
    ResetFrame();
  }
  void LoadFonts() {
    std::ifstream stream(R"(Native\data\disc\BATTLE\BATTLE.PRG)",
                         std::ios::binary);
    if (!stream) {
      throw std::runtime_error(
          "equipment integration requires the extracted original BATTLE.PRG");
    }
    const std::vector<unsigned char> image{
        std::istreambuf_iterator<char>(stream), {}};
    const std::array targets{
        static_cast<void*>(VS::Render::compactGlyphLeftTrim),
        static_cast<void*>(VS::Render::compactGlyphWidths),
        static_cast<void*>(VS::Render::compactGlyphIndices),
        static_cast<void*>(VS::Render::variableFontGlyphWidths)};
    const auto entries = VS::Content::ModuleDataEntries();
    for (void* target : targets) {
      const auto entry = std::ranges::find(
          entries, target, &VS::Content::ModuleDataEntry::target);
      if (entry == entries.end() ||
          std::string_view(entry->module) != "BATTLE/BATTLE.PRG" ||
          !VS::Content::CopyModuleDataEntry(*entry, image)) {
        throw std::runtime_error(
            "equipment integration could not load an original font table");
      }
    }
    VS::Render::SetLiveMenuFontStyle(0);
  }
  ~LiveFixture() {
    inventory::ResetLiveEquipmentDetailControllers();
    fw::ResetLiveEquipmentDetailsStatics();
  }

  void BindInventory() {
    VS::Menu::Framework::menuItems = rows.data();
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    VS::Menu::Framework::weapons = VS::Items::inventory.weapons.data();
    VS::Menu::Framework::blades = VS::Items::inventory.blades.data();
    VS::Menu::Framework::grips = VS::Items::inventory.grips.data();
    VS::Menu::Framework::shields = VS::Items::inventory.shields.data();
    VS::Menu::Framework::armor = VS::Items::inventory.armor.data();
    VS::Menu::Framework::gems = VS::Items::inventory.gems.data();
    const std::array<u_char*, kIndexedCategories> orders{
        VS::Items::inventoryIndices.weapons.data(),
        VS::Items::inventoryIndices.blades.data(),
        VS::Items::inventoryIndices.grips.data(),
        VS::Items::inventoryIndices.shields.data(),
        VS::Items::inventoryIndices.armor.data(),
        VS::Items::inventoryIndices.gems.data(),
        VS::Items::inventoryIndices.misc.data()};
    VS::Menu::Inventory::categoryOrders = orders;
    VS::Input::buttonsState = 0;
    VS::Input::buttonRepeat = 0;
    VS::Input::buttonsPressed = 0;
    Menu_Inventory_selectedItemCategoryIconOnTop = 1;
    Menu_Inventory_exitToBattle = 0;
    Menu_Inventory_ashleyRenderState = 0;
    Menu_Inventory_ashleyYOffset = kPreviewOffset;
  }
  void BuildText() {
    constexpr std::size_t kTextOffset = 2048;
    auto bytes = std::as_writable_bytes(std::span(help));
    std::fill(bytes.begin(), bytes.end(),
              static_cast<std::byte>(VS::Render::kTextTerminator));
    std::fill_n(help.begin(), kHelpEntries, static_cast<u_short>(kTextOffset));
    bytes[kTextOffset * sizeof(u_short)] = std::byte{1};
    for (auto& name : names) {
      name.fill(static_cast<char>(VS::Render::kTextTerminator));
      name[0] = 1;
    }
    VS::Menu::Framework::itemHelp = help.data();
    VS::Menu::Framework::itemNames =
        reinterpret_cast<char (*)[fw::kItemNameRowBytes]>(names.data());
    VS::Menu::Framework::stringBuf = description.data();
  }
  void ResetFrame() {
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = primitives.data() + 1;
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kSceneOrderingEntry;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  void IdleRows() {
    for (auto& row : rows) {
      if (row.state != fw::RowState::kInactive) {
        row.state = fw::RowState::kIdle;
      }
    }
  }
  fw::ItemText FirstItem() const {
    const auto capacity =
        VS::Core::Memory::LivePayloadCapacity(VS::Menu::Framework::itemsText);
    if (!capacity || *capacity < sizeof(fw::ItemText)) {
      throw std::runtime_error("menu did not produce an owned row");
    }
    fw::ItemText result;
    std::memcpy(&result, VS::Menu::Framework::itemsText, sizeof result);
    return result;
  }
  void AddParts() {
    VS::Items::InventoryBlade blade{.id = 1,
                                    .category = 1,
                                    .max_dp = kMaxDp,
                                    .max_pp = kMaxPp,
                                    .current_dp = kCurrentDp,
                                    .current_pp = kCurrentPp,
                                    .strength = kBladeStrength,
                                    .cost = kBladeCost,
                                    .type_bits = 1,
                                    .range = {.range = kBladeRange},
                                    .material = 1};
    blade.classes[0] = kBladeClass;
    blade.affinities[0] = kBladeAffinity;
    VS::Items::inventory.blades[0] = blade;
    blade.id = 2;
    blade.strength = kSecondBladeStrength;
    VS::Items::inventory.blades[VS::Items::kBladeSlots - 1] = blade;
    VS::Items::InventoryGrip grip{
        .id = 1, .gem_slots = 2, .strength = kGripStrength};
    grip.types[1] = -2;
    VS::Items::inventory.grips[0] = grip;
    grip.id = 2;
    grip.gem_slots = VS::Items::kWeaponGemCount;
    VS::Items::inventory.grips[VS::Items::kGripSlots - 1] = grip;
    VS::Items::inventoryIndices.blades[0] = VS::Items::kBladeSlots;
    VS::Items::inventoryIndices.blades[1] = 1;
    VS::Items::inventoryIndices.grips[0] = 1;
    VS::Items::inventoryIndices.grips[1] = VS::Items::kGripSlots;
  }
  void AddWeapon() {
    AddParts();
    VS::Items::InventoryWeapon weapon{.blade = 1, .grip = 1};
    weapon.name.fill(static_cast<char>(VS::Render::kTextTerminator));
    weapon.name[0] = 1;
    VS::Items::inventory.weapons[0] = weapon;
    VS::Items::inventoryIndices.weapons[0] = 1;
  }
  void AddShield() {
    VS::Items::InventoryShield shield{};
    shield.base.id = 1;
    shield.base.category = kShieldCategory;
    shield.base.material = 1;
    shield.base.max_dp = kMaxDp;
    shield.base.max_pp = kMaxPp;
    shield.base.gem_slots = VS::Items::kWeaponGemCount;
    VS::Items::inventory.shields[0] = shield;
    VS::Items::inventoryIndices.shields[0] = 1;
  }
  void Open(int (*entry)(int)) {
    (void)entry((kSelectedRow << fw::kByteShift) | 1);
    IdleRows();
    (void)entry(0);
  }
  void Wait(int (*entry)(int), int ticks = kWaitFrames) {
    for (int tick = 0; tick < ticks; ++tick) {
      (void)entry(0);
    }
  }
  bool Owns(const void* packet) const {
    const auto address = reinterpret_cast<std::uintptr_t>(packet);
    const auto first = reinterpret_cast<std::uintptr_t>(primitives.data() + 1);
    const auto end = reinterpret_cast<std::uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
    const auto table = reinterpret_cast<std::uintptr_t>(ordering.data());
    return (address >= first && address < end) ||
           (address >= table && address < table + sizeof ordering);
  }
  std::vector<DrawnSprite> Sprites() const {
    std::vector<DrawnSprite> sprites;
    const void* packet = &ordering.back();
    std::uint32_t draw_mode = 0;
    for (std::size_t count = 0; packet != nullptr && count < kPacketVisitLimit;
         ++count) {
      if (!Owns(packet)) {
        return {};
      }
      const auto* words = static_cast<const u_long*>(packet);
      const auto length = static_cast<const P_TAG*>(packet)->len;
      if (length == 1 &&
          (words[1] >> VS::Render::kPacketLengthShift) == kDrawModeCommand) {
        draw_mode = words[1];
      } else if (length == VS::Render::kSpritePacketWords - 1 &&
                 (words[VS::Render::kSpriteColorWord] >>
                  VS::Render::kPacketLengthShift) ==
                     VS::Render::kSpriteCommand) {
        sprites.push_back({words[VS::Render::kSpriteColorWord],
                           words[VS::Render::kSpritePositionWord],
                           words[VS::Render::kSpriteTextureWord],
                           words[VS::Render::kSpriteSizeWord],
                           words[VS::Render::kSpriteDrawModeWord], false});
      } else if (length == kCompactWords - 1 &&
                 (words[kCompactColor] >> VS::Render::kPacketLengthShift) ==
                     VS::Render::kSpriteCommand) {
        // GameFont.cpp BuildCompactGlyph omits the per-glyph draw-mode word.
        sprites.push_back({words[kCompactColor], words[kCompactPosition],
                           words[kCompactTexture], words[kCompactSize],
                           draw_mode, true});
      }
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    return sprites;
  }
  std::string CountLabel(std::span<const DrawnSprite> sprites) const {
    std::vector<std::pair<std::int16_t, char>> glyphs;
    for (const auto& sprite : sprites) {
      if (!sprite.compact ||
          sprite.draw_mode != VS::Render::kDefaultSpriteDrawMode ||
          (sprite.position >> fw::kHalfwordShift) != kCountTextY ||
          (sprite.color & kRgbMask) != VS::Render::kCompactTextGray) {
        continue;
      }
      glyphs.emplace_back(static_cast<std::int16_t>(sprite.position),
                          CompactCharacter(sprite));
    }
    std::sort(glyphs.begin(), glyphs.end());
    if (glyphs.empty() || glyphs.front().first != kCountTextX) {
      throw std::runtime_error("equipment count has no positioned font glyphs");
    }
    std::string text;
    for (const auto& [x, character] : glyphs) {
      (void)x;
      text += character;
    }
    return text;
  }
  static char CompactCharacter(const DrawnSprite& sprite) {
    constexpr std::size_t kGlyphsBeforeSkippedBracket = '[' - ' ';
    for (std::size_t glyph = 0; glyph < kGlyphsBeforeSkippedBracket; ++glyph) {
      const std::uint32_t cell = VS::Render::compactGlyphIndices[glyph];
      const std::uint32_t width = (VS::Render::compactGlyphWidths[glyph / 2] >>
                                   ((glyph % 2) * kNibbleBits)) &
                                  kWidthMask;
      const std::uint32_t texture =
          (cell & kCompactColumnMask) |
          ((kCompactTop + (cell & 1) * kCompactHeight) << fw::kByteShift) |
          (kCompactClut << fw::kHalfwordShift);
      if (width != 0 && sprite.texture == texture &&
          sprite.size == (width | (kCompactHeight << fw::kHalfwordShift))) {
        return static_cast<char>(glyph + ' ');
      }
    }
    throw std::runtime_error("equipment count contains an unknown atlas glyph");
  }
};
}  // namespace equipment_detail_tests
