#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ModuleData.h"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Sprites.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/TargetingState.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
}

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
  ValueRestore<Menu_Framework_MenuRow*> saved_rows{Menu_Framework_menuItems};
  ValueRestore<Items_Inventory> saved_inventory{Audio_Seq_unusedInventory};
  ValueRestore<Items_InventoryIndices> saved_indices{
      Audio_Seq_unusedInventoryIndices};
  ValueRestore<Menu_Framework_MenuMemory> saved_memory{
      Menu_Framework_menuMemory};
  ArrayRestore<u_char*, kIndexedCategories> saved_orders{
      Menu_Inventory_inventoryIndices};
  ValueRestore<int> saved_held{Input_buttonsState};
  ValueRestore<int> saved_repeat{Input_buttonRepeat};
  ValueRestore<decltype(Input_buttonsPressed)> saved_pressed{
      Input_buttonsPressed};
  ValueRestore<int> saved_preview{Menu_Inventory_ashleyRenderState};
  ValueRestore<int> saved_y{Menu_Inventory_ashleyYOffset};
  ValueRestore<int> saved_return{Menu_Inventory_exitToBattle};
  ValueRestore<char> saved_title{Menu_Inventory_secondaryTitleEnabled};
  ValueRestore<char> saved_categories{Menu_Inventory_itemCategoryIconsEnabled};
  ValueRestore<char> saved_command{Menu_Inventory_statusCommandState};
  ValueRestore<char> saved_header{Menu_Inventory_selectedItemCategoryIconOnTop};
  ValueRestore<char> saved_pages{Menu_Framework_enabledStatPages};
  ValueRestore<char> saved_page{Menu_Framework_itemStatPage};
  ValueRestore<char> saved_subtype{Menu_Framework_equipmentSubtype};
  ValueRestore<char> saved_item{Menu_Framework_currentStatusViewItem};
  ValueRestore<u_char> saved_selection{
      Menu_Framework_selectedStatusViewElement};
  ArrayRestore<int16_t, fw::kEquipmentStatCount> saved_stats{
      Menu_Framework_equipmentStats};
  ArrayRestore<int16_t, fw::kPrimaryStatCount> saved_primary{
      Menu_Framework_strIntAgi};
  ArrayRestore<char, fw::kRangeRiskBytes> saved_range{
      Menu_Framework_rangeRiskData};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_deltas{
      Menu_Framework_comparisonStatDeltas};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_totals{
      Menu_Framework_comparisonStatTotals};
  ArrayRestore<int16_t, fw::kComparisonStatCount> saved_accessory{
      Menu_Framework_comparisonAccessoryStats};
  ValueRestore<Items_InventoryWeapon*> saved_weapons{Menu_Framework_weapons};
  ValueRestore<Items_InventoryBlade*> saved_blades{Menu_Framework_blades};
  ValueRestore<Items_InventoryGrip*> saved_grips{Menu_Framework_grips};
  ValueRestore<Items_InventoryShield*> saved_shields{Menu_Framework_shields};
  ValueRestore<Items_InventoryArmor*> saved_armor{Menu_Framework_armor};
  ValueRestore<Items_InventoryGem*> saved_gems{Menu_Framework_gems};
  ValueRestore<u_short*> saved_help{Menu_Framework_itemHelp};
  ValueRestore<decltype(Menu_Framework_itemNames)> saved_names{
      Menu_Framework_itemNames};
  ValueRestore<char*> saved_buffer{Menu_Framework_stringBuf};
  ValueRestore<decltype(Battle_stringContext)> saved_arguments{
      Battle_stringContext};
  ArrayRestore<Menu_TextBoxState, std::size(Menu_textBoxes)> saved_text_boxes{
      Menu_textBoxes};
  ValueRestore<Render_FrameScratch> saved_frame{Render_frameScratch};
  ValueRestore<void*> saved_primitive{Render_legacyPrimitiveBuffers[0]};
  ValueRestore<void*> saved_ordering{Render_legacyOrderingTables[0]};
  ValueRestore<int> saved_framebuffer{Render_frameBuf};
  ValueRestore<VS::Render::FontStyle> saved_font{VS::Render::LiveFontStyle()};
  ArrayRestore<u_char, sizeof Render_compactGlyphLeftTrim> saved_compact_trim{
      Render_compactGlyphLeftTrim};
  ArrayRestore<u_char, sizeof Render_compactGlyphWidths> saved_compact_widths{
      Render_compactGlyphWidths};
  ArrayRestore<u_char, sizeof Render_compactGlyphIndices> saved_compact_indices{
      Render_compactGlyphIndices};
  ArrayRestore<char, sizeof Render_variableFontGlyphWidths>
      saved_variable_widths{Render_variableFontGlyphWidths};

  std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{};
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
        static_cast<void*>(Render_compactGlyphLeftTrim),
        static_cast<void*>(Render_compactGlyphWidths),
        static_cast<void*>(Render_compactGlyphIndices),
        static_cast<void*>(Render_variableFontGlyphWidths)};
    const auto entries = std::span(Content_moduleData, Content_moduleDataCount);
    for (void* target : targets) {
      const auto entry =
          std::ranges::find(entries, target, &Content_ModuleDataEntry::target);
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
    Menu_Framework_menuItems = rows.data();
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    Menu_Framework_weapons = Audio_Seq_unusedInventory.weapons;
    Menu_Framework_blades = Audio_Seq_unusedInventory.blades;
    Menu_Framework_grips = Audio_Seq_unusedInventory.grips;
    Menu_Framework_shields = Audio_Seq_unusedInventory.shields;
    Menu_Framework_armor = Audio_Seq_unusedInventory.armor;
    Menu_Framework_gems = Audio_Seq_unusedInventory.gems;
    const std::array<u_char*, kIndexedCategories> orders{
        Audio_Seq_unusedInventoryIndices.weapons,
        Audio_Seq_unusedInventoryIndices.blades,
        Audio_Seq_unusedInventoryIndices.grips,
        Audio_Seq_unusedInventoryIndices.shields,
        Audio_Seq_unusedInventoryIndices.armor,
        Audio_Seq_unusedInventoryIndices.gems,
        Audio_Seq_unusedInventoryIndices.misc};
    std::copy(orders.begin(), orders.end(), Menu_Inventory_inventoryIndices);
    Input_buttonsState = 0;
    Input_buttonRepeat = 0;
    Input_buttonsPressed.allButtons = 0;
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
    Menu_Framework_itemHelp = help.data();
    Menu_Framework_itemNames =
        reinterpret_cast<char (*)[fw::kItemNameRowBytes]>(names.data());
    Menu_Framework_stringBuf = description.data();
  }
  void ResetFrame() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = primitives.data() + 1;
    Render_frameScratch.primitiveCursor = primitives.data() + 1;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    Render_frameScratch.orderingTable = ordering.data() + kSceneOrderingEntry;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  void IdleRows() {
    for (auto& row : rows) {
      if (row.state != Menu_Framework_MenuItemState_Inactive) {
        row.state = Menu_Framework_MenuItemState_Idle;
      }
    }
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
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.blades[0], blade);
    blade.id = 2;
    blade.strength = kSecondBladeStrength;
    VS::Items::Live::Commit(
        Audio_Seq_unusedInventory.blades[VS::Items::kBladeSlots - 1], blade);
    VS::Items::InventoryGrip grip{
        .id = 1, .gem_slots = 2, .strength = kGripStrength};
    grip.types[1] = -2;
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.grips[0], grip);
    grip.id = 2;
    grip.gem_slots = VS::Items::kWeaponGemCount;
    VS::Items::Live::Commit(
        Audio_Seq_unusedInventory.grips[VS::Items::kGripSlots - 1], grip);
    Audio_Seq_unusedInventoryIndices.blades[0] = VS::Items::kBladeSlots;
    Audio_Seq_unusedInventoryIndices.blades[1] = 1;
    Audio_Seq_unusedInventoryIndices.grips[0] = 1;
    Audio_Seq_unusedInventoryIndices.grips[1] = VS::Items::kGripSlots;
  }
  void AddWeapon() {
    AddParts();
    VS::Items::InventoryWeapon weapon{.blade = 1, .grip = 1};
    weapon.name.fill(static_cast<char>(VS::Render::kTextTerminator));
    weapon.name[0] = 1;
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.weapons[0], weapon);
    Audio_Seq_unusedInventoryIndices.weapons[0] = 1;
  }
  void AddShield() {
    VS::Items::InventoryShield shield{};
    shield.base.id = 1;
    shield.base.category = kShieldCategory;
    shield.base.material = 1;
    shield.base.max_dp = kMaxDp;
    shield.base.max_pp = kMaxPp;
    shield.base.gem_slots = VS::Items::kWeaponGemCount;
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.shields[0], shield);
    Audio_Seq_unusedInventoryIndices.shields[0] = 1;
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
    const auto end =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
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
      const std::uint32_t cell = Render_compactGlyphIndices[glyph];
      const std::uint32_t width = (Render_compactGlyphWidths[glyph / 2] >>
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
