#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Targeting.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Menu/BreakArts.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Options.h"
#include "Menu/Setup/Assembly.h"
#include "Menu/Setup/Combination.h"
#include "Menu/Setup/SetupScreen.h"
#include "Menu/TextBox.h"
#include "State/StateFlags.h"
}

namespace {
struct CursorFixture {
  State_Settings saved_settings = Input_settings;
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  Menu_Framework_MenuMemory saved_memory = Menu_Framework_menuMemory;
  Menu_Framework_MenuRow* saved_items = Menu_Framework_menuItems;
  u_short* saved_text = Menu_Framework_itemsText;
  int saved_selected = Menu_Framework_selectedRow;
  int saved_animation = Menu_Framework_mainMenuCursorAnimation;
  char saved_root = Menu_Framework_submenuStates[0];
  char saved_page = Menu_Framework_subMenuPage;
  char saved_row_info = Menu_Framework_subMenuRowInfo;
  char saved_row_y = Menu_Framework_menuRowYOffset;
  char saved_list_row = Menu_Framework_itemsListRow;
  u_char saved_id = Menu_Framework_subMenuId;
  u_short saved_count = Menu_Framework_subMenuRowCount;
  std::array<Core_HeapBlockHeader, 512> heap{};
  struct {
    std::array<Menu_Framework_MenuRow, 40> rows{};
    std::array<std::byte, 0x124> scratch{};
  } workspace;
  std::array<char*, 40> strings{};
  std::array<int, 20> row_types{};

  CursorFixture() {
    Core_Memory_InitializeHeap(heap.data(), sizeof(heap));
    Menu_Framework_menuItems = workspace.rows.data();
    Menu_Framework_itemsText = nullptr;
    Menu_Framework_menuMemory = {};
    Input_settings.menuFlags = 0x3FF;
  }

  ~CursorFixture() {
    if (Menu_Framework_itemsText != nullptr) {
      Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);
    }
    Input_settings = saved_settings;
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
    Menu_Framework_menuMemory = saved_memory;
    Menu_Framework_menuItems = saved_items;
    Menu_Framework_itemsText = saved_text;
    Menu_Framework_selectedRow = saved_selected;
    Menu_Framework_mainMenuCursorAnimation = saved_animation;
    Menu_Framework_submenuStates[0] = saved_root;
    Menu_Framework_subMenuPage = saved_page;
    Menu_Framework_subMenuRowInfo = saved_row_info;
    Menu_Framework_menuRowYOffset = saved_row_y;
    Menu_Framework_itemsListRow = saved_list_row;
    Menu_Framework_subMenuId = saved_id;
    Menu_Framework_subMenuRowCount = saved_count;
  }

  void OpenRows(int count) {
    Menu_Framework_CreateMenuRows(count, 0x100, strings.data(),
                                  row_types.data());
  }
};

void CheckNativeHeapAllocation(void* allocation, std::size_t units) {
  const auto* header = static_cast<Core_HeapBlockHeader*>(allocation) - 1;
  CHECK(header->blockSize == units);
  CHECK(reinterpret_cast<std::uintptr_t>(allocation) %
            alignof(Core_HeapBlockHeader) ==
        0);
}

struct PuzzleOptionsFixture {
  CursorFixture rows;
  std::array<u_short, 1070> saved_table =
      std::to_array(Menu_Options_menuStrings);
  char saved_disabled = Audio_Seq_unusedStateFlags.puzzleModeDisabled;

  PuzzleOptionsFixture() {
    std::fill_n(Menu_Options_menuStrings, saved_table.size(), 0xE7E7);
    std::fill_n(Menu_Options_menuStrings, 18, 64);
  }

  ~PuzzleOptionsFixture() {
    std::copy(saved_table.begin(), saved_table.end(), Menu_Options_menuStrings);
    Audio_Seq_unusedStateFlags.puzzleModeDisabled = saved_disabled;
  }
};

struct BreakArtMenuFixture {
  CursorFixture rows;
  std::array<u_short, 2151> saved_strings =
      std::to_array(Menu_BreakArts_strings);
  std::array<Battle_ActionDefinition, 4> saved_actions{};
  Battle_Actor* saved_character = Battle_characterState;
  Battle_Actor* saved_actor = Battle_actors[0];
  decltype(Menu_Framework_shortcutInvoked) saved_shortcut =
      Menu_Framework_shortcutInvoked;
  decltype(Menu_Framework_actionRestrictions) saved_restrictions =
      Menu_Framework_actionRestrictions;
  decltype(Menu_Framework_displayActionCost) saved_cost =
      Menu_Framework_displayActionCost;
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  int category;
  int first_action;
  u_char saved_arts;

  explicit BreakArtMenuFixture(int weapon_category)
      : category(weapon_category),
        first_action(Battle_ActionId_DaggerArtOne + (category - 1) * 4),
        saved_arts(Audio_Seq_unusedArtsStatus.artsLearned[category % 10]) {
    Menu_BreakArts_ResetMenu1Statics();
    std::fill_n(Menu_BreakArts_strings, saved_strings.size(), 0xE7E7);
    std::fill_n(Menu_BreakArts_strings, 72, 96);
    Audio_Seq_unusedArtsStatus.artsLearned[category % 10] = 4;
    Battle_characterState = Battle_actors[0] = &actor;
    actor.stats = &stats;
    actor.equippedWeaponCategory = static_cast<u_char>(category);
    actor.weaponDrawn = 1;
    stats.currentHp = 999;
    Menu_Framework_actionRestrictions = 0;
    Menu_Framework_shortcutInvoked = 0;
    for (int index = 0; index < 4; ++index) {
      auto& action = Battle_actionDefinitions[first_action + index];
      saved_actions[index] = action;
      action = {};
      action.type = Battle_ActionType_BreakArt;
      action.unlocked = 1;
      std::fill_n(action.name, std::size(action.name), static_cast<char>(0xE7));
    }
  }

  ~BreakArtMenuFixture() {
    Menu_BreakArts_ResetMenu1Statics();
    std::copy(saved_strings.begin(), saved_strings.end(),
              Menu_BreakArts_strings);
    std::copy(saved_actions.begin(), saved_actions.end(),
              &Battle_actionDefinitions[first_action]);
    Battle_characterState = saved_character;
    Battle_actors[0] = saved_actor;
    Menu_Framework_shortcutInvoked = saved_shortcut;
    Menu_Framework_actionRestrictions = saved_restrictions;
    Menu_Framework_displayActionCost = saved_cost;
    Audio_Seq_unusedArtsStatus.artsLearned[category % 10] = saved_arts;
  }

  void FinishRowAnimations() {
    for (auto& row : rows.workspace.rows) row.state = 1;
  }

  void Open() {
    REQUIRE(Menu_BreakArts_UpdateArtList(category) == 0);
    FinishRowAnimations();
    REQUIRE(Menu_BreakArts_UpdateArtList(0) == 0);
  }

  int SelectRow(int row) {
    Menu_Framework_selectedRow = row;
    Menu_Framework_CloseRowsAndRememberCursor();
    REQUIRE(Menu_BreakArts_UpdateArtList(0) == 0);
    FinishRowAnimations();
    return Menu_BreakArts_UpdateArtList(0);
  }
};

struct EquipmentGateFixture {
  CursorFixture rows;
  decltype(std::to_array(Menu_Inventory_menuText)) saved_strings =
      std::to_array(Menu_Inventory_menuText);
  decltype(Audio_Seq_unusedStateFlags) saved_flags = Audio_Seq_unusedStateFlags;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
  decltype(Audio_Seq_unusedInventoryIndices) saved_indices =
      Audio_Seq_unusedInventoryIndices;
  Battle_Actor* saved_character = Battle_characterState;
  decltype(Menu_Framework_actionRestrictions) saved_restrictions =
      Menu_Framework_actionRestrictions;
  Battle_Actor actor{};
  Battle_ActorStats stats{};

  EquipmentGateFixture() {
    Menu_Inventory_ItemList_ResetMenu3Statics();
    std::fill_n(Menu_Inventory_menuText, saved_strings.size(), 0xE7E7);
    std::fill_n(Menu_Inventory_menuText, 128, 128);
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    actor.stats = &stats;
    Battle_characterState = &actor;
  }

  ~EquipmentGateFixture() {
    Menu_Inventory_ItemList_ResetMenu3Statics();
    std::copy(saved_strings.begin(), saved_strings.end(),
              Menu_Inventory_menuText);
    Audio_Seq_unusedStateFlags = saved_flags;
    Audio_Seq_unusedInventory = saved_inventory;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Battle_characterState = saved_character;
    Menu_Framework_actionRestrictions = saved_restrictions;
  }
};

struct WorkshopGateFixture {
  CursorFixture rows;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
  u_short* saved_text = Menu_Framework_menu12Text;
  int* saved_types = Menu_Framework_rowTypeBuf;
  Field_MpdEnvironment* saved_environment = Field_roomData.environment;
  u_char saved_workshop = Menu_Setup_currentWorkshop;
  std::array<u_short, 1024> strings{};
  Field_MpdEnvironment environment{};

  WorkshopGateFixture(int workshop, int weapon_count, bool cursor_memory) {
    Menu_Setup_SetupScreen_ResetMenucStatics();
    std::fill(strings.begin(), strings.end(), 0xE7E7);
    std::fill_n(strings.begin(), 16, 32);
    Menu_Framework_menu12Text = strings.data();
    Menu_Framework_rowTypeBuf = rows.row_types.data();
    Field_roomData.environment = &environment;
    Audio_Seq_unusedInventory = {};
    environment.isWorkshop = static_cast<char>(workshop);
    Input_settings.cursorMemory = static_cast<char>(cursor_memory);
    for (int index = 0; index < weapon_count; ++index) {
      Audio_Seq_unusedInventory.weapons[index].index =
          static_cast<char>(index + 1);
      Audio_Seq_unusedInventory.weapons[index].blade =
          static_cast<u_char>(index + 1);
    }
  }

  ~WorkshopGateFixture() {
    Menu_Setup_SetupScreen_ResetMenucStatics();
    Audio_Seq_unusedInventory = saved_inventory;
    Menu_Framework_menu12Text = saved_text;
    Menu_Framework_rowTypeBuf = saved_types;
    Field_roomData.environment = saved_environment;
    Menu_Setup_currentWorkshop = saved_workshop;
  }

  const Menu_Framework_ItemText* Open() {
    u_char state = 2;
    REQUIRE(Menu_Setup_Update(&state) == 0);
    REQUIRE(state == 3);
    REQUIRE(Menu_Framework_subMenuRowCount == 6);
    const auto* items = reinterpret_cast<const Menu_Framework_ItemText*>(
        Menu_Framework_itemsText);
    REQUIRE(items != nullptr);
    return items;
  }
};

struct WorkshopChoicesFixture {
  WorkshopGateFixture setup{6, 0, false};
  decltype(Audio_Seq_unusedInventoryIndices) saved_indices =
      Audio_Seq_unusedInventoryIndices;
  decltype(Battle_stringContext) saved_context = Battle_stringContext;
  decltype(Menu_Framework_itemNames) saved_names = Menu_Framework_itemNames;
  u_short* saved_help = Menu_Framework_itemHelp;
  decltype(Menu_Framework_shields) saved_shields = Menu_Framework_shields;
  u_short* saved_list = Menu_Setup_itemsList;
  char saved_count = Menu_Setup_itemCount;
  int saved_selection = Menu_Setup_itemsListSelection;
  char saved_state = Menu_Setup_itemsListState;
  char saved_window = Menu_Setup_itemsListWindow;
  char saved_info = Menu_Setup_rowInfoUnused;
  u_char saved_combining_item = Menu_Setup_combiningItem;
  decltype(std::to_array(Menu_Setup_availableItems)) saved_available =
      std::to_array(Menu_Setup_availableItems);
  decltype(std::to_array(Menu_Setup_combiningBladeIds)) saved_blades =
      std::to_array(Menu_Setup_combiningBladeIds);
  decltype(std::to_array(Menu_Setup_combiningArmorIds)) saved_armor =
      std::to_array(Menu_Setup_combiningArmorIds);
  decltype(std::to_array(Menu_Setup_itemsToCombine)) saved_combining =
      std::to_array(Menu_Setup_itemsToCombine);
  std::array<u_short, 2048> strings{};
  std::array<u_short, 16384> help{};
  char names[512][24]{};
  std::array<int, 49> types{};

  WorkshopChoicesFixture() {
    std::fill(strings.begin(), strings.end(), 0xE7E7);
    std::fill_n(strings.begin(), 128, 128);
    std::fill(help.begin(), help.end(), 0xE7E7);
    std::fill_n(help.begin(), 768, 1024);
    for (auto& name : names) {
      std::fill_n(name, std::size(name), static_cast<char>(0xE7));
      name[0] = 10;
    }
    Menu_Framework_menu12Text = strings.data();
    Menu_Framework_itemHelp = help.data();
    Menu_Framework_itemNames = names;
    Menu_Framework_rowTypeBuf = types.data();
    Menu_Framework_shields = Audio_Seq_unusedInventory.shields;
    Menu_Setup_itemsList = nullptr;
    Menu_Setup_currentWorkshop = 6;
    Audio_Seq_unusedInventoryIndices = {};
    std::fill_n(Menu_Setup_combiningBladeIds, saved_blades.size(), 0);
    std::fill_n(Menu_Setup_combiningArmorIds, saved_armor.size(), 0);
    std::fill_n(Menu_Setup_itemsToCombine, saved_combining.size(), 0);
    FillParts();
    FillProtection();
    for (int category = 1; category <= 5; ++category) {
      Items_Inventory_RebuildDisplayIndices(category);
    }
  }

  ~WorkshopChoicesFixture() {
    if (Menu_Setup_itemsList != nullptr) {
      Core_Memory_FreeFromHighEnd(Menu_Setup_itemsList);
    }
    Menu_Setup_itemsList = saved_list;
    Menu_Setup_itemCount = saved_count;
    Menu_Setup_itemsListSelection = saved_selection;
    Menu_Setup_itemsListState = saved_state;
    Menu_Setup_itemsListWindow = saved_window;
    Menu_Setup_rowInfoUnused = saved_info;
    Menu_Setup_combiningItem = saved_combining_item;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Battle_stringContext = saved_context;
    Menu_Framework_itemNames = saved_names;
    Menu_Framework_itemHelp = saved_help;
    Menu_Framework_shields = saved_shields;
    std::copy(saved_available.begin(), saved_available.end(),
              Menu_Setup_availableItems);
    std::copy(saved_blades.begin(), saved_blades.end(),
              Menu_Setup_combiningBladeIds);
    std::copy(saved_armor.begin(), saved_armor.end(),
              Menu_Setup_combiningArmorIds);
    std::copy(saved_combining.begin(), saved_combining.end(),
              Menu_Setup_itemsToCombine);
  }

  void FillParts() {
    for (int index = 0; index < 16; ++index) {
      auto& blade = Audio_Seq_unusedInventory.blades[index];
      blade.id = 1;
      blade.index = static_cast<char>(index + 1);
      blade.category = blade.damageType = 1;
      blade.material = 3;
      auto& grip = Audio_Seq_unusedInventory.grips[index];
      grip.id = 97;
      grip.index = static_cast<u_short>(index + 1);
      grip.category = 1;
    }
    for (int index = 0; index < 48; ++index) {
      auto& gem = Audio_Seq_unusedInventory.gems[index];
      gem.id = 261;
      gem.index = static_cast<int16_t>(index + 1);
    }
  }

  void FillProtection() {
    for (int index = 0; index < 16; ++index) {
      auto& armor = Audio_Seq_unusedInventory.armor[index];
      armor.id = 161;
      armor.index = static_cast<u_char>(index + 1);
      armor.category = 1;
      armor.material = 3;
      armor.maxDp = 1000;
      if (index < 8) {
        auto& shield = Audio_Seq_unusedInventory.shields[index];
        shield.base.id = 132;
        shield.base.material = 3;
        shield.base.maxDp = 1000;
      }
    }
  }

  std::array<std::byte, sizeof(Items_Inventory)> InventoryBytes() const {
    std::array<std::byte, sizeof(Items_Inventory)> bytes{};
    std::memcpy(bytes.data(), &Audio_Seq_unusedInventory, bytes.size());
    return bytes;
  }

  void Open(int (*select)(int), int initial, int capacity) {
    REQUIRE(select(initial) == 0);
    for (auto& row : setup.rows.workspace.rows) row.state = 1;
    // InitializeHeap excludes its final header from the usable span.
    std::array<std::byte, sizeof(Core_HeapBlockHeader)> before{}, after{};
    std::memcpy(before.data(), &setup.rows.heap.back(), before.size());
    REQUIRE(select(0) == 0);
    std::memcpy(after.data(), &setup.rows.heap.back(), after.size());
    CHECK(before == after);
    REQUIRE(Menu_Setup_itemCount == capacity + 1);
    REQUIRE(Menu_Setup_itemsList != nullptr);
  }

  void CheckRows(int capacity) const {
    const auto* items =
        reinterpret_cast<const Menu_Framework_ItemText*>(Menu_Setup_itemsList);
    CHECK((items[0].menuRowFlags & 1) == 1);
    for (int row = 1; row <= capacity; ++row) {
      CAPTURE(row);
      CHECK(Menu_Setup_availableItems[row] == row);
      CHECK((items[row].menuRowFlags & 1) == 0);
      CHECK(items[row].name[0] == 0xE70A);
    }
  }
};

struct WorkshopRepairFixture {
  WorkshopChoicesFixture setup;
  u_int* saved_sfx_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_sfx_selectors = Audio_Playback_soundEffectBankSelectors;
  decltype(Input_buttonsPressed) saved_pressed = Input_buttonsPressed;
  int saved_repeat = Input_buttonRepeat;
  Menu_TextBoxState saved_box = Menu_textBoxes[7];
  Battle_Actor* saved_character = Battle_characterState;
  Battle_Actor* saved_actor = Battle_actors[0];
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  std::array<u_int, 0x180> sfx_offsets{};
  std::array<u_short, 0x180> sfx_selectors{};

  WorkshopRepairFixture(int missing_dp, int pp) {
    sfx_offsets.fill(UINT32_MAX);
    Audio_Playback_soundEffectProgramOffsets = sfx_offsets.data();
    Audio_Playback_soundEffectBankSelectors = sfx_selectors.data();
    auto& inventory = Audio_Seq_unusedInventory;
    inventory = {};
    auto initialize = [missing_dp, pp](auto& item, int id) {
      item.id = static_cast<decltype(item.id)>(id);
      item.maxDp = 10000;
      item.currentDp = static_cast<u_short>(10000 - missing_dp);
      item.maxPp = 200;
      item.currentPp = static_cast<u_short>(pp);
    };
    initialize(inventory.blades[0], 14);
    inventory.blades[0].index = 1;
    initialize(inventory.shields[0].base, 132);
    inventory.shields[0].index = 1;
    initialize(inventory.armor[0], 161);
    inventory.armor[0].index = 1;
    inventory.armor[0].category = 3;
    initialize(inventory.armor[1], 223);
    inventory.armor[1].index = 2;
    inventory.armor[1].category = 7;
    for (int category : {1, 3, 4}) {
      Items_Inventory_RebuildDisplayIndices(category);
    }
    Input_buttonRepeat = 0;
  }

  ~WorkshopRepairFixture() {
    Audio_Playback_soundEffectProgramOffsets = saved_sfx_offsets;
    Audio_Playback_soundEffectBankSelectors = saved_sfx_selectors;
    Input_buttonsPressed = saved_pressed;
    Input_buttonRepeat = saved_repeat;
    Menu_textBoxes[7] = saved_box;
    Battle_characterState = saved_character;
    Battle_actors[0] = saved_actor;
  }

  int Tick() {
    for (auto& row : setup.setup.rows.workspace.rows) row.state = 1;
    return Menu_Setup_UpdateRepair(0);
  }

  void Equip() {
    auto& inventory = Audio_Seq_unusedInventory;
    Battle_characterState = Battle_actors[0] = &actor;
    actor.stats = &stats;
    inventory.weapons[0].index = 1;
    inventory.weapons[0].blade = inventory.weapons[0].grip = 1;
    inventory.weapons[0].isEquipped = 1;
    inventory.blades[0].assembledWeaponIndex = 1;
    inventory.blades[0].category = 2;
    inventory.grips[0].id = 96;
    inventory.grips[0].category = 1;
    inventory.grips[0].index = inventory.grips[0].assembledWeaponIndex = 1;
    inventory.shields[0].isEquipped = 1;
    inventory.armor[0].limb = 1;
    Items_Inventory_RebuildDisplayIndices(0);
    Items_EquipAshleyWeapon(&inventory.weapons[0]);
    Items_EquipAshleyShield(&inventory.shields[0]);
    Items_EquipAshleyArmor(0, &inventory.armor[0]);
  }

  void Prompt(int button) {
    REQUIRE(Menu_Setup_UpdateRepair(1) == 0);
    Input_buttonsPressed.allButtons = static_cast<u_short>(button);
    for (int step = 0; step < 5; ++step) {
      CAPTURE(step);
      REQUIRE(Tick() == 0);
    }
  }
};

struct WorkshopRecipeFixture {
  char* saved_combinations = Menu_Setup_combinationResults;
  char* saved_materials = Menu_Setup_materialResults;
  void* saved_blades = Menu_Setup_bladeCombinationInitData;
  Menu_Setup_CombinedArmorRecord* saved_shields =
      Menu_Setup_shieldCombinationInitData;
  Menu_Setup_CombinedArmorRecord* saved_armor = Menu_Setup_combinationInitData;
  std::array<char, sizeof(Audio_Seq_unusedMaterialModifiers)> saved_modifiers{};
  alignas(Menu_Setup_SydHeader) std::array<unsigned char, 14000> bytes{};
  static constexpr int kCombinations = 12;
  static constexpr int kMaterials = 10000;
  static constexpr int kRecords = 12000;

  WorkshopRecipeFixture() {
    std::memcpy(saved_modifiers.data(), Audio_Seq_unusedMaterialModifiers,
                saved_modifiers.size());
    std::memset(Audio_Seq_unusedMaterialModifiers, 0, saved_modifiers.size());
    Menu_Setup_SydHeader header{};
    header.combinationsOffset = kCombinations;
    header.materialsOffset = kMaterials;
    header.initDataOffset = kRecords;
    std::memcpy(bytes.data(), &header, sizeof(header));
    Audio_Seq_unusedMaterialModifiers[5][24] = 3;
    Audio_Seq_unusedMaterialModifiers[5][26] = -2;
    Audio_Seq_unusedMaterialModifiers[5][28] = -4;
  }

  ~WorkshopRecipeFixture() {
    Menu_Setup_combinationResults = saved_combinations;
    Menu_Setup_materialResults = saved_materials;
    Menu_Setup_bladeCombinationInitData = saved_blades;
    Menu_Setup_shieldCombinationInitData = saved_shields;
    Menu_Setup_combinationInitData = saved_armor;
    std::memcpy(Audio_Seq_unusedMaterialModifiers, saved_modifiers.data(),
                saved_modifiers.size());
  }

  void ArmorRecipe(bool shield, int second_material) {
    const int combination = shield ? 6 * 17 + 7 : (65 - 16) * 65 + 66 - 16;
    const int material = shield ? 3 * 8 + second_material
                                : 3 * 128 + 3 * 32 + second_material * 4 + 2;
    bytes[kCombinations + combination] = 12;
    bytes[kMaterials + material] = 5;
    const Menu_Setup_CombinedArmorRecord record{
        0, 9, 4, 3, 10, static_cast<char>(-5), 2, 0};
    std::memcpy(bytes.data() + kRecords + 12 * sizeof(record), &record,
                sizeof(record));
  }

  void CheckArmor(const Items_InventoryArmor& result, bool shield) const {
    const std::array<int, 16> actual{
        result.id,       result.subId,     result.material,
        result.category, result.strength,  result.intelligence,
        result.agility,  result.maxDp,     result.currentDp,
        result.maxPp,    result.currentPp, result.gemSlots,
        result.wepId,    result.index,     result.limb,
        result.types[0]};
    const std::array<int, 16> expected{138,
                                       12,
                                       5,
                                       4,
                                       13,
                                       -7,
                                       -2,
                                       1501,
                                       1501,
                                       shield ? 151 : 101,
                                       shield ? 61 : 41,
                                       shield ? 3 : 0,
                                       shield ? 9 : 0,
                                       7,
                                       2,
                                       7};
    for (std::size_t index = 0; index < actual.size(); ++index) {
      CAPTURE(index);
      CHECK(actual[index] == expected[index]);
    }
  }

  void CheckBlade(const Items_InventoryBlade& result,
                  const std::array<unsigned char, 16>& record) const {
    const std::array<int, 18> actual{
        result.id,
        result.subId,
        result.wepId,
        result.category,
        result.material,
        result.strength,
        result.intelligence,
        result.agility,
        static_cast<unsigned char>(result.damageType) & 3,
        static_cast<unsigned char>(result.costType) & 7,
        result.cost,
        result.opaque12,
        result.maxDp,
        result.currentDp,
        result.maxPp,
        result.currentPp,
        result.index,
        result.assembledWeaponIndex};
    const std::array<int, 18> expected{31, 31, 35, 4,    5,    13,  4,  -2, 2,
                                       5,  7,  9,  1501, 1501, 151, 61, 7,  2};
    for (std::size_t index = 0; index < actual.size(); ++index) {
      CAPTURE(index);
      CHECK(actual[index] == expected[index]);
    }
    CHECK(std::memcmp(&result.range, record.data() + 12,
                      sizeof(result.range)) == 0);
  }
};

std::vector<unsigned char> ReadWorkshopRecipe(std::string_view file) {
  for (auto directory = std::filesystem::current_path();
       directory != directory.parent_path();
       directory = directory.parent_path()) {
    const auto path = directory / "Native" / "data" / "disc" / "MENU" / file;
    if (!std::filesystem::exists(path)) continue;
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    std::vector<unsigned char> bytes(std::filesystem::file_size(path));
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    REQUIRE(input.good());
    return bytes;
  }
  SKIP("Extracted original workshop recipe is unavailable");
}
}  // namespace

TEST_CASE(
    "Heap allocation splitting uses native header units in both directions",
    "[memory][heap]") {
  CursorFixture fixture;
  const bool from_high = GENERATE(false, true);
  const auto allocate = from_high ? Core_Memory_AllocateFromHighEnd
                                  : Core_Memory_AllocateFromLowEnd;
  const auto release =
      from_high ? Core_Memory_FreeFromHighEnd : Core_Memory_FreeFromLowEnd;
  constexpr std::size_t kUnit = sizeof(Core_HeapBlockHeader);
  constexpr u_int kBytes = static_cast<u_int>(kUnit + 1);
  constexpr std::size_t kAllocationUnits = (kBytes + kUnit - 1) / kUnit + 1;
  const auto available = Core_legacyHeapList.next->blockSize;
  void* first = allocate(kBytes);
  void* second = allocate(kBytes);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CheckNativeHeapAllocation(first, kAllocationUnits);
  CheckNativeHeapAllocation(second, kAllocationUnits);
  CHECK(Core_legacyHeapList.next->blockSize ==
        available - 2 * kAllocationUnits);
  release(first);
  release(second);
  CHECK(Core_legacyHeapList.next == fixture.heap.data());
  CHECK(Core_legacyHeapList.prev == fixture.heap.data());
  CHECK(Core_legacyHeapList.next->blockSize == available);
}

TEST_CASE("Heap setup and allocation do not manufacture initialized payloads",
          "[memory][heap]") {
  CursorFixture fixture;
  auto* payload = reinterpret_cast<std::byte*>(fixture.heap.data() + 1);
  std::fill_n(payload, sizeof(Core_HeapBlockHeader), std::byte{0xCD});
  void* allocation = Core_Memory_AllocateFromLowEnd(1);
  REQUIRE(allocation == payload);
  CHECK(*static_cast<std::byte*>(allocation) == std::byte{0xCD});
  Core_Memory_FreeFromLowEnd(allocation);
}

TEST_CASE("Workshop repair restores DP with original rounded PP cost",
          "[menu][equipment][workshop][repair]") {
  const int missing = GENERATE(0, 1, 99, 100, 101, 9999, 10000);
  const int pp = GENERATE(0, 1, 50, 200);
  CAPTURE(missing, pp);
  WorkshopRepairFixture fixture(missing, pp);
  const auto original = Audio_Seq_unusedInventory;
  fixture.Prompt(PADRdown);
  CHECK(std::memcmp(&original, &Audio_Seq_unusedInventory, sizeof(original)) ==
        0);
  REQUIRE(fixture.Tick() == -1);
  auto expected = original;
  auto repaired = [missing, pp](auto& item) {
    item.currentDp = item.maxDp;
    item.currentPp = static_cast<u_short>(std::max(0, pp - missing / 100));
  };
  repaired(expected.blades[0]);
  repaired(expected.shields[0].base);
  repaired(expected.armor[0]);
  for (int repeat = 0; repeat < 2; ++repeat) {
    fixture.Prompt(PADRright);
    REQUIRE(fixture.Tick() == 0);
    CHECK(std::memcmp(&expected, &Audio_Seq_unusedInventory,
                      sizeof(expected)) == 0);
    REQUIRE(fixture.Tick() == 0);
    REQUIRE(fixture.Tick() == -1);
  }
}

TEST_CASE("Workshop armor recipes retain original ownership and result rules",
          "[menu][equipment][workshop][recipes]") {
  const bool shield = GENERATE(false, true);
  const int second_material = GENERATE(3, 4);
  CAPTURE(shield, second_material);
  WorkshopRecipeFixture fixture;
  fixture.ArmorRecipe(shield, second_material);
  Items_InventoryArmor first{}, second{}, result{};
  first.id = 191;
  first.subId = static_cast<char>(shield ? 6 : 65);
  first.category = 5;
  first.material = 3;
  first.index = 7;
  first.limb = 2;
  first.maxDp = 1001;
  first.currentDp = 123;
  first.maxPp = 101;
  first.currentPp = 41;
  first.types[0] = 7;
  second.subId = static_cast<char>(shield ? 7 : 66);
  second.category = 4;
  second.material = static_cast<u_short>(second_material);
  second.maxDp = 2002;
  second.maxPp = 202;
  second.currentPp = 82;
  const auto saved_first = first, saved_second = second;
  const auto build =
      shield ? Menu_Setup_BuildCombinedShield : Menu_Setup_BuildCombinedArmor;
  REQUIRE(build(&first, &second, &result, fixture.bytes.data()) == &result);
  fixture.CheckArmor(result, shield);
  CHECK(std::memcmp(&first, &saved_first, sizeof(first)) == 0);
  CHECK(std::memcmp(&second, &saved_second, sizeof(second)) == 0);
}

TEST_CASE("Workshop repair refreshes equipped runtime DP and PP",
          "[menu][equipment][workshop][repair]") {
  const int missing = GENERATE(101, 9999);
  const int pp = GENERATE(0, 50, 200);
  WorkshopRepairFixture fixture(missing, pp);
  fixture.Equip();
  REQUIRE(fixture.stats.weapon.currentDp == 10000 - missing);
  REQUIRE(fixture.stats.shield.currentDp == 10000 - missing);
  REQUIRE(fixture.stats.limbs[0].armor.currentDp == 10000 - missing);
  fixture.Prompt(PADRright);
  REQUIRE(fixture.Tick() == 0);
  const std::array<int, 5> actual{
      fixture.stats.weapon.currentDp, fixture.stats.shield.currentDp,
      fixture.stats.limbs[0].armor.currentDp, fixture.stats.weapon.currentPp,
      fixture.stats.shield.currentPp};
  const int remaining_pp = std::max(0, pp - missing / 100);
  const std::array<int, 5> expected{10000, 10000, 10000, remaining_pp,
                                    remaining_pp};
  CHECK(actual == expected);
  CHECK(Audio_Seq_unusedInventory.weapons[0].isEquipped == 1);
  CHECK(Audio_Seq_unusedInventory.shields[0].isEquipped == 1);
  CHECK(Audio_Seq_unusedInventory.armor[0].limb == 1);
}

TEST_CASE("Workshop blade recipes use ordered IDs materials and native stats",
          "[menu][equipment][workshop][recipes]") {
  const int second_material = GENERATE(3, 4);
  WorkshopRecipeFixture fixture;
  fixture.bytes[fixture.kCombinations + 14 * 96 + 26] = 31;
  fixture.bytes[fixture.kMaterials + 3 * 50 + (second_material - 3) * 10 + 4] =
      5;
  const std::array<unsigned char, 16> record{31, 35, 4, 2, 5, 7, 9, 0,
                                             10, 6,  2, 0, 1, 2, 3, 4};
  std::copy(record.begin(), record.end(),
            fixture.bytes.data() + fixture.kRecords + 31 * record.size());
  Items_InventoryBlade first{}, second{}, result{};
  first.id = 14;
  first.category = 3;
  first.material = 3;
  first.index = 7;
  first.assembledWeaponIndex = 2;
  first.maxDp = 1001;
  first.currentDp = 123;
  first.maxPp = 101;
  first.currentPp = 41;
  second.id = 26;
  second.category = 4;
  second.material = static_cast<u_short>(second_material);
  second.maxDp = 2002;
  second.maxPp = 202;
  second.currentPp = 82;
  const auto saved_first = first, saved_second = second;
  REQUIRE(Menu_Setup_BuildCombinedBlade(&first, &second, &result,
                                        fixture.bytes.data()) == &result);
  fixture.CheckBlade(result, record);
  CHECK(std::memcmp(&first, &saved_first, sizeof(first)) == 0);
  CHECK(std::memcmp(&second, &saved_second, sizeof(second)) == 0);
}

TEST_CASE("Workshop stat fusion preserves extrema truncation and bounds",
          "[menu][equipment][workshop][recipes]") {
  const int different = GENERATE(0, 1);
  const int affinities = GENERATE(0, 1);
  const std::array<signed char, 7> values{100, 99, -99, -100, 4, -4, 3};
  auto first = values, second = values;
  std::array<signed char, 8> result{};
  result.fill(77);
  Menu_Setup_CombineClassOrAffinityValues(first.data(), second.data(),
                                          result.data(), different, affinities);
  const int scaled = different ? 5 : 6;
  const std::array<int, 8> expected{
      100, 100, -100, -100, scaled, -scaled, affinities ? 4 : 77, 77};
  for (std::size_t index = 0; index < result.size(); ++index) {
    CAPTURE(index);
    CHECK(result[index] == expected[index]);
  }
  CHECK(first == values);
  CHECK(second == values);
}

TEST_CASE("Original blade recipe produces the inspected ordered result",
          "[menu][equipment][workshop][recipes][disc]") {
  WorkshopRecipeFixture fixture;
  auto bytes = ReadWorkshopRecipe("BLADE.SYD");
  REQUIRE(bytes.size() == 13264);
  Items_InventoryBlade first{}, second{}, result{};
  first.id = 14;
  first.category = 2;
  first.material = 3;
  first.maxDp = 1001;
  second.id = 25;
  second.category = 3;
  second.material = 4;
  second.maxDp = 2002;
  REQUIRE(Menu_Setup_BuildCombinedBlade(&first, &second, &result,
                                        bytes.data()) == &result);
  const std::array<int, 10> actual{
      result.id,       result.subId,    result.wepId,        result.category,
      result.material, result.strength, result.intelligence, result.agility,
      result.maxDp,    result.currentDp};
  const std::array<int, 10> expected{13, 13, 14, 2, 4, 5, 0, -1, 1501, 1501};
  CHECK(actual == expected);
}

TEST_CASE(
    "Original shield and armor recipes use their distinct material tables",
    "[menu][equipment][workshop][recipes][disc]") {
  const bool shield = GENERATE(false, true);
  WorkshopRecipeFixture fixture;
  auto bytes = ReadWorkshopRecipe(shield ? "SHIELD.SYD" : "ARMOR.SYD");
  REQUIRE(bytes.size() == (shield ? 504 : 5912));
  Items_InventoryArmor first{}, second{}, result{};
  first.subId = static_cast<u_char>(shield ? 6 : 65);
  second.subId = static_cast<u_char>(shield ? 7 : 66);
  first.category = second.category = 5;
  first.material = static_cast<u_short>(shield ? 3 : 2);
  second.material = static_cast<u_short>(shield ? 4 : 3);
  first.maxDp = 1001;
  second.maxDp = 2002;
  const auto build =
      shield ? Menu_Setup_BuildCombinedShield : Menu_Setup_BuildCombinedArmor;
  REQUIRE(build(&first, &second, &result, bytes.data()) == &result);
  const std::array<int, 9> actual{
      result.id,       result.subId,    result.category,
      result.material, result.strength, result.intelligence,
      result.agility,  result.maxDp,    result.currentDp};
  const std::array<int, 9> expected{shield ? 134 : 192,
                                    shield ? 8 : 66,
                                    shield ? 1 : 5,
                                    shield ? 5 : 3,
                                    shield ? 17 : 2,
                                    shield ? 11 : 4,
                                    shield ? -6 : 0,
                                    1501,
                                    1501};
  CHECK(actual == expected);
}

TEST_CASE("Workshop physical fusion preserves sentinel type and signed bounds",
          "[menu][equipment][workshop][recipes]") {
  const int different = GENERATE(0, 1);
  const int same_type = GENERATE(0, 1);
  std::array<signed char, 4> types1{7, 100, -90, 9};
  std::array<signed char, 4> types2{8, -90, 100, -4};
  if (same_type) types2 = types1;
  std::array<signed char, 4> types_result{77, 77, 77, 77};
  Menu_Setup_CombinePhysicalTypeValues(types1.data(), types2.data(),
                                       types_result.data(), different);
  CHECK(types_result[0] == 77);
  const int factor = different ? (same_type ? 6 : 4) : (same_type ? 8 : 6);
  for (int index = 1; index < 4; ++index) {
    const int expected =
        std::clamp((types1[index] + types2[index]) * factor / 10, -100, 100);
    CHECK(types_result[index] == expected);
  }
}

TEST_CASE("Assembly and disassembly preserve parts and release gem ownership",
          "[menu][equipment][workshop][ownership]") {
  const int slots = GENERATE(0, 1, 2, 3);
  WorkshopChoicesFixture fixture;
  auto& inventory = Audio_Seq_unusedInventory;
  inventory.weapons[2].index = 3;
  inventory.grips[0].gemSlots = static_cast<char>(slots);
  Menu_Setup_combiningItem = 2;
  auto expected = inventory;
  auto& weapon = expected.weapons[2];
  weapon.blade = weapon.grip = 1;
  std::fill_n(weapon.name, std::size(weapon.name), static_cast<char>(0xE7));
  expected.blades[0].assembledWeaponIndex = 3;
  expected.grips[0].assembledWeaponIndex = 3;
  for (int index = 0; index < slots; ++index) {
    weapon.gems[index] = static_cast<u_char>(index + 1);
    expected.gems[index].setItemIndex = 3;
  }
  for (int repeat = 0; repeat < 2; ++repeat) {
    Menu_Setup_AssembleWeapon(0, 0, 0x030201);
    CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
  }
  expected.weapons[2] = {};
  expected.weapons[2].index = 3;
  expected.blades[0].assembledWeaponIndex = 0;
  expected.grips[0].assembledWeaponIndex = 0;
  for (int index = 0; index < slots; ++index) {
    expected.gems[index].setItemIndex = 0;
  }
  Menu_Setup_DisassembleWeapon(2);
  CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
  Menu_Setup_DisassembleWeapon(2);
  CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
}

TEST_CASE("Shield disassembly releases only its attached gem ownership",
          "[menu][equipment][workshop][ownership]") {
  const int slots = GENERATE(0, 1, 2, 3);
  WorkshopChoicesFixture fixture;
  auto& inventory = Audio_Seq_unusedInventory;
  inventory.shields[4].base.gemSlots = static_cast<char>(slots);
  for (int index = 0; index < slots; ++index) {
    inventory.shields[4].gems[index] = static_cast<u_char>(index + 1);
    inventory.gems[index].setItemIndex = 0x85;
  }
  auto expected = inventory;
  std::fill_n(expected.shields[4].gems, slots, 0);
  for (int index = 0; index < slots; ++index) {
    expected.gems[index].setItemIndex = 0;
  }
  Menu_Setup_DetachShieldGems(4);
  CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
}

TEST_CASE("Workshop combination choices enforce original material masks",
          "[menu][equipment][workshop][choices]") {
  const int workshop = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int material = GENERATE(1, 2, 3, 4, 5, 6, 7);
  const int category = GENERATE(0, 1, 2);
  CAPTURE(workshop, material, category);
  WorkshopChoicesFixture fixture;
  Menu_Setup_currentWorkshop = static_cast<u_char>(workshop);
  for (auto& blade : Audio_Seq_unusedInventory.blades) {
    blade.material = static_cast<u_short>(material);
  }
  for (auto& shield : Audio_Seq_unusedInventory.shields) {
    shield.base.material = static_cast<u_short>(material);
  }
  for (auto& armor : Audio_Seq_unusedInventory.armor) {
    armor.material = static_cast<u_short>(material);
  }
  constexpr std::array kSelectors{Menu_Setup_SelectCombinationBlade,
                                  Menu_Setup_SelectCombinationShield,
                                  Menu_Setup_SelectCombinationArmor};
  const int capacity = category == 1 ? 8 : 16;
  const auto original = fixture.InventoryBytes();
  fixture.Open(kSelectors[category], 1, capacity);
  constexpr std::array kMasks{0, 0x0E, 0x18, 0x38, 0xC0, 0x3E, 0xFE};
  const bool disabled = (kMasks[workshop] & (1 << material)) == 0;
  const auto* rows =
      reinterpret_cast<const Menu_Framework_ItemText*>(Menu_Setup_itemsList);
  for (int row = 1; row <= capacity; ++row) {
    CHECK((rows[row].menuRowFlags & 1) == disabled);
  }
  CHECK(original == fixture.InventoryBytes());
}

TEST_CASE("Combination choices exclude the other ingredient and accessories",
          "[menu][equipment][workshop][choices]") {
  const int category = GENERATE(0, 1, 2);
  WorkshopChoicesFixture fixture;
  Menu_Setup_combiningBladeIds[0] = Menu_Setup_combiningArmorIds[0] = 1;
  Menu_Setup_itemsToCombine[0] = 1;
  Menu_Setup_combiningBladeIds[1] = Menu_Setup_combiningArmorIds[1] = 2;
  Menu_Setup_itemsToCombine[1] = 2;
  Audio_Seq_unusedInventory.armor[15].category = 7;
  const int capacity = category == 1 ? 6 : (category == 2 ? 13 : 14);
  constexpr std::array kSelectors{Menu_Setup_SelectCombinationBlade,
                                  Menu_Setup_SelectCombinationShield,
                                  Menu_Setup_SelectCombinationArmor};
  const auto original = fixture.InventoryBytes();
  fixture.Open(kSelectors[category], 1, capacity);
  CHECK(Menu_Setup_availableItems[0] == 1);
  const auto* rows =
      reinterpret_cast<const Menu_Framework_ItemText*>(Menu_Setup_itemsList);
  CHECK((rows[0].menuRowFlags & 1) == 0);
  for (int row = 1; row <= capacity; ++row) {
    CHECK(Menu_Setup_availableItems[row] == row + 2);
  }
  CHECK(original == fixture.InventoryBytes());
}

TEST_CASE("Workshop selectors copy full stock-capacity component lists",
          "[menu][equipment][workshop][choices]") {
  struct Selector {
    int (*select)(int);
    int initial;
    int capacity;
  };
  constexpr std::array kSelectors{
      Selector{Menu_Setup_SelectAssemblyBlade, 0x80, 16},
      Selector{Menu_Setup_SelectAssemblyGrip, 0x80, 16},
      Selector{Menu_Setup_SelectGem, 11, 48},
      Selector{Menu_Setup_SelectCombinationBlade, 1, 16},
      Selector{Menu_Setup_SelectCombinationShield, 1, 8},
      Selector{Menu_Setup_SelectCombinationArmor, 1, 16}};
  const auto index = GENERATE(0, 1, 2, 3, 4, 5);
  CAPTURE(index);
  const auto& selector = kSelectors[index];
  WorkshopChoicesFixture fixture;
  const auto inventory = fixture.InventoryBytes();
  fixture.Open(selector.select, selector.initial, selector.capacity);
  fixture.CheckRows(selector.capacity);
  CHECK(inventory == fixture.InventoryBytes());
}

TEST_CASE("Setup actions obey original workshop and weapon-capacity gates",
          "[menu][equipment][workshop]") {
  const int workshop = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int weapon_count = GENERATE(0, 1, 7, 8);
  const bool cursor_memory = GENERATE(false, true);
  CAPTURE(workshop, weapon_count, cursor_memory);
  WorkshopGateFixture fixture(workshop, weapon_count, cursor_memory);
  REQUIRE(Items_Inventory_CountItems(0, nullptr) == weapon_count);
  const auto* rows = fixture.Open();
  REQUIRE(Menu_Setup_currentWorkshop == workshop);
  CHECK(Input_settings.cursorMemory == cursor_memory);
  for (int row = 0; row < 6; ++row) {
    CAPTURE(row);
    const bool disabled = (row == 0 && (workshop == 0 || weapon_count == 8)) ||
                          (row == 3 && weapon_count == 0) ||
                          ((row == 4 || row == 5) && workshop == 0);
    CHECK((rows[row].menuRowFlags & 1) == disabled);
  }
}

TEST_CASE("Equipment management obeys progression tarnish and action gates",
          "[menu][equipment][restrictions]") {
  const int intro = GENERATE(0, 1, 2, 3, 4);
  const bool tarnished = GENERATE(false, true);
  const int restrictions = GENERATE(0, 1, 2, 4, 7, 8, 0x80, 0x200);
  EquipmentGateFixture fixture;
  Audio_Seq_unusedStateFlags.introState = static_cast<char>(intro);
  fixture.stats.statuses = tarnished ? 0x80000 : 0;
  Menu_Framework_actionRestrictions = restrictions;
  REQUIRE(Menu_Inventory_UpdateItemList(1) == 0);
  REQUIRE(Menu_Inventory_UpdateItemList(0) == 0);
  REQUIRE(Menu_Framework_subMenuRowCount == 2);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  const auto* rows = reinterpret_cast<const Menu_Framework_ItemText*>(
      Menu_Framework_itemsText);
  const bool disabled = intro < 3 || tarnished || (restrictions & 7) != 0;
  for (int row = 0; row < 2; ++row) {
    CHECK(((rows[row].menuRowFlags & 1) != 0) == disabled);
  }
}

TEST_CASE("Cursor Memory controls the actual root selection consumer",
          "[menu][options][cursor]") {
  const char enabled = GENERATE(char{0}, char{1});
  const u_short flags = GENERATE(u_short{0x3FF}, u_short{0x3F8});
  CursorFixture fixture;
  Input_settings.cursorMemory = enabled;
  Input_settings.menuFlags = flags;
  for (int visit = 0; visit < 3; ++visit) {
    Menu_Framework_submenuStates[0] = 7;
    CHECK(Menu_Framework_UpdateMainMenuSelection(2) == 0);
    CHECK(Menu_Framework_submenuStates[0] ==
          (enabled ? 7 : (flags == 0x3F8 ? 3 : 0)));
    CHECK(Input_settings.cursorMemory == enabled);
  }
}

TEST_CASE("Cursor Memory restores shared row and scroll positions",
          "[menu][options][cursor]") {
  const char enabled = GENERATE(char{0}, char{1});
  CursorFixture fixture;
  Input_settings.cursorMemory = enabled;
  Menu_Framework_menuMemory.cursorMemories[0] = 3;
  Menu_Framework_menuMemory.cursorMemories[1] = 5;
  fixture.OpenRows(20);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(Menu_Framework_HighlightedRow() == (enabled ? 8 : 0));
  Menu_Framework_selectedRow = 4;
  Menu_Framework_subMenuPage = 6;
  Menu_Framework_CloseRowsAndRememberCursor();
  CHECK(Menu_Framework_itemsText == nullptr);
  CHECK(Menu_Framework_menuMemory.cursorMemories[0] == 4);
  CHECK(Menu_Framework_menuMemory.cursorMemories[1] == 6);
  fixture.OpenRows(20);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(Menu_Framework_HighlightedRow() == (enabled ? 10 : 0));
}

TEST_CASE("Cursor Memory clamps a remembered selection after rows shrink",
          "[menu][options][cursor]") {
  CursorFixture fixture;
  Input_settings.cursorMemory = 1;
  Menu_Framework_menuMemory.cursorMemories[0] = 4;
  Menu_Framework_menuMemory.cursorMemories[1] = 6;
  fixture.OpenRows(3);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(Menu_Framework_HighlightedRow() == 0);
}

TEST_CASE("Options retain the real Puzzle Mode availability gate",
          "[menu][options][puzzle]") {
  const char disabled = GENERATE(char{0}, char{1});
  PuzzleOptionsFixture fixture;
  Audio_Seq_unusedStateFlags.puzzleModeDisabled = disabled;
  Input_settings.cursorMemory = 0;
  u_char state = 4;
  CHECK(Menu_Options_Update(&state) == 0);
  CHECK(state == 5);
  CHECK(Input_settings.cursorMemory == 0);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  const auto* rows = reinterpret_cast<const Menu_Framework_ItemText*>(
      Menu_Framework_itemsText);
  for (int row = 0; row < 9; ++row) {
    CAPTURE(disabled, row);
    CHECK(rows[row].menuRowFlags ==
          (row == 6 && disabled ? Menu_Framework_RowTypeFlag_Disabled : 0));
  }
}

TEST_CASE(
    "Four learned Break Arts preserve their navigation row and cursor policy",
    "[menu][break-arts][cursor]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const int flags = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const char shortcut = static_cast<char>(flags & 1);
  const bool equipped = (flags & 2) != 0;
  const char cursor_memory = static_cast<char>((flags >> 2) & 1);
  BreakArtMenuFixture fixture(category);
  Menu_Framework_shortcutInvoked = shortcut;
  if (!equipped) {
    fixture.actor.equippedWeaponCategory =
        static_cast<u_char>(category % 10 + 1);
  }
  Input_settings.cursorMemory = cursor_memory;
  Menu_Framework_menuMemory
      .cursorMemories[static_cast<std::size_t>(category + 11) * 2] = 3;
  fixture.Open();
  REQUIRE(Menu_Framework_itemsText != nullptr);
  REQUIRE(Menu_Framework_subMenuRowCount == (shortcut ? 4 : 5));
  REQUIRE(Menu_Framework_HighlightedRow() == (cursor_memory ? 3 : 0));
  CHECK(Input_settings.cursorMemory == cursor_memory);
  const auto* rows = reinterpret_cast<const Menu_Framework_ItemText*>(
      Menu_Framework_itemsText);
  for (int index = 0; index < 4; ++index) {
    CHECK(rows[index].menuRowFlags ==
          (equipped ? 0 : Menu_Framework_RowTypeFlag_Disabled));
  }
  if (!shortcut) {
    CHECK(rows[4].menuRowFlags == 2);
    CHECK(fixture.SelectRow(4) == 0xFFFF);
  }
}

TEST_CASE("The fourth learned Break Art remains an action in every category",
          "[menu][break-arts][cursor]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const char shortcut = GENERATE(char{0}, char{1});
  BreakArtMenuFixture fixture(category);
  Menu_Framework_shortcutInvoked = shortcut;
  fixture.Open();
  CHECK(fixture.SelectRow(3) == fixture.first_action + 3);
}
