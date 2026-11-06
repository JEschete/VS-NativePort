#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/SoundPlaybackLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/OptionsLive.hpp"
#include "Menu/Setup/Assembly.hpp"
#include "Menu/Setup/Combination.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Menu/Setup/SetupScreen.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/TargetingState.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Input/Buttons.h"
#include "Input/ControllerLiveState.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/BreakArts.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/OptionsLiveState.h"
#include "Menu/TextBox.h"
#include "State/StateFlags.h"
}

namespace {
struct CursorFixture {
  State_Settings saved_settings = Input_settings;
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
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 768> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  struct {
    std::array<Menu_Framework_MenuRow, 40> rows{};
    std::array<std::byte, 0x124> scratch{};
  } workspace;
  std::array<char*, 40> strings{};
  std::array<int, 20> row_types{};

  CursorFixture() {
    Menu_Framework_menuItems = workspace.rows.data();
    Menu_Framework_itemsText = nullptr;
    Menu_Framework_menuMemory = {};
    Input_settings.menuFlags = 0x3FF;
  }

  ~CursorFixture() {
    if (Menu_Framework_itemsText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(Menu_Framework_itemsText);
    }
    Input_settings = saved_settings;
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
    VS::Menu::Framework::CreateLiveMenuRows(count, 0x100, strings.data(),
                                            row_types.data());
  }
};

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
    VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
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

namespace setup = VS::Menu::Setup;

// Runs a typed workshop call over the live State and host.
template <typename Call>
int RunSetup(const Call& call) {
  return setup::RunLiveSetup([&call](setup::Context& context) {
    if constexpr (std::is_void_v<
                      std::invoke_result_t<const Call&, setup::Context&>>) {
      call(context);
      return 0;
    } else {
      return static_cast<int>(call(context));
    }
  });
}

struct WorkshopGateFixture {
  CursorFixture rows;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
  u_short* saved_text = Menu_Framework_menu12Text;
  int* saved_types = Menu_Framework_rowTypeBuf;
  Field_MpdEnvironment* saved_environment = Field_roomData.environment;
  u_int saved_environment_length = Field_roomData.header.environmentLength;
  setup::State saved_state = setup::LiveSetupState();
  std::array<u_short, 1024> strings{};
  Field_MpdEnvironment environment{};

  WorkshopGateFixture(int workshop, int weapon_count, bool cursor_memory) {
    setup::LiveSetupState() = {};
    std::fill(strings.begin(), strings.end(), 0xE7E7);
    std::fill_n(strings.begin(), 16, 32);
    Menu_Framework_menu12Text = strings.data();
    Menu_Framework_rowTypeBuf = rows.row_types.data();
    Field_roomData.environment = &environment;
    Field_roomData.header.environmentLength = sizeof environment;
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
    setup::LiveSetupState() = saved_state;
    Audio_Seq_unusedInventory = saved_inventory;
    Menu_Framework_menu12Text = saved_text;
    Menu_Framework_rowTypeBuf = saved_types;
    Field_roomData.environment = saved_environment;
    Field_roomData.header.environmentLength = saved_environment_length;
  }

  const Menu_Framework_ItemText* Open() {
    std::uint8_t state = 2;
    REQUIRE(setup::UpdateLiveSetup(state) == 0);
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
    setup::LiveSetupState().combination.workshop = 6;
    Audio_Seq_unusedInventoryIndices = {};
    FillParts();
    FillProtection();
    for (int category = 1; category <= 5; ++category) {
      VS::Items::Inventory::RebuildLiveDisplayIndices(category);
    }
  }

  ~WorkshopChoicesFixture() {
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Battle_stringContext = saved_context;
    Menu_Framework_itemNames = saved_names;
    Menu_Framework_itemHelp = saved_help;
    Menu_Framework_shields = saved_shields;
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

  using Selector = int (*)(setup::Context&, int);

  void Open(Selector select, int initial, int capacity) {
    REQUIRE(RunSetup([&](setup::Context& c) { return select(c, initial); }) ==
            0);
    for (auto& row : setup.rows.workspace.rows) row.state = 1;
    // InitializeHeap excludes the arena's final unit from the usable span.
    const auto slack =
        std::span(setup.rows.arena).last<VS::Core::Memory::kHeapUnitBytes>();
    const std::vector<std::byte> before(slack.begin(), slack.end());
    REQUIRE(RunSetup([&](setup::Context& c) { return select(c, 0); }) == 0);
    CHECK(std::ranges::equal(before, slack));
    const auto& list = setup::LiveSetupState().item_list;
    REQUIRE(list.count == capacity + 1);
    REQUIRE(list.open);
  }

  static const setup::ItemListState& List() {
    return setup::LiveSetupState().item_list;
  }

  void CheckRows(int capacity) const {
    const auto& items = List().items;
    CHECK((items[0].flags & 1) == 1);
    for (int row = 1; row <= capacity; ++row) {
      CAPTURE(row);
      CHECK(setup::LiveSetupState().available_items[row] == row);
      CHECK((items[row].flags & 1) == 0);
      CHECK(items[row].name[0] == 0x0A);
      CHECK(items[row].name[1] == 0xE7);
    }
  }
};

struct WorkshopRepairFixture {
  WorkshopChoicesFixture setup;
  decltype(Input_buttonsPressed) saved_pressed = Input_buttonsPressed;
  int saved_repeat = Input_buttonRepeat;
  Menu_TextBoxState saved_box = Menu_textBoxes[7];
  Battle_Actor* saved_character = Battle_characterState;
  Battle_Actor* saved_actor = Battle_actors[0];
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  std::array<u_int, 0x180> sfx_offsets{};
  std::array<u_short, 0x180> sfx_selectors{};
  VS::Audio::Playback::ScopedLiveEffectTables sfx_tables{
      {.program_offsets = sfx_offsets,
       .bank_selectors = sfx_selectors,
       .program_data = {}}};

  WorkshopRepairFixture(int missing_dp, int pp) {
    sfx_offsets.fill(UINT32_MAX);
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
      VS::Items::Inventory::RebuildLiveDisplayIndices(category);
    }
    Input_buttonRepeat = 0;
  }

  ~WorkshopRepairFixture() {
    Input_buttonsPressed = saved_pressed;
    Input_buttonRepeat = saved_repeat;
    Menu_textBoxes[7] = saved_box;
    Battle_characterState = saved_character;
    Battle_actors[0] = saved_actor;
  }

  int Tick() {
    for (auto& row : setup.setup.rows.workspace.rows) row.state = 1;
    return RunSetup(
        [](setup::Context& c) { return setup::UpdateRepair(c, false); });
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
    VS::Items::Inventory::RebuildLiveDisplayIndices(0);
    VS::Items::EquipLiveAshleyWeapon(&inventory.weapons[0]);
    VS::Items::EquipLiveAshleyShield(&inventory.shields[0]);
    VS::Items::EquipLiveAshleyArmor(0, &inventory.armor[0]);
  }

  void Prompt(int button) {
    REQUIRE(RunSetup([](setup::Context& c) {
              return setup::UpdateRepair(c, true);
            }) == 0);
    Input_buttonsPressed.allButtons = static_cast<u_short>(button);
    for (int step = 0; step < 5; ++step) {
      CAPTURE(step);
      REQUIRE(Tick() == 0);
    }
  }
};

struct WorkshopRecipeFixture {
  std::array<std::uint8_t, 14000> bytes{};
  setup::MaterialBonuses bonuses{};
  static constexpr std::uint32_t kCombinations = 12;
  static constexpr std::uint32_t kMaterials = 10000;
  static constexpr std::uint32_t kRecords = 12000;

  WorkshopRecipeFixture() {
    const std::array<std::uint32_t, 3> header{kCombinations, kMaterials,
                                              kRecords};
    for (std::size_t table = 0; table < header.size(); ++table) {
      for (std::size_t byte = 0; byte < sizeof(std::uint32_t); ++byte) {
        bytes[(table * sizeof(std::uint32_t)) + byte] =
            static_cast<std::uint8_t>(header[table] >> (byte * 8));
      }
    }
    bonuses[5] = {.strength = 3, .intelligence = -2, .agility = -4};
  }

  [[nodiscard]] static setup::RecipeBook Decode(
      setup::RecipeKind kind, std::span<const std::uint8_t> file) {
    auto book = setup::DecodeRecipeFile(kind, file);
    REQUIRE(book.has_value());
    return *std::move(book);
  }

  [[nodiscard]] setup::RecipeBook Book(setup::RecipeKind kind) const {
    return Decode(kind, bytes);
  }

  void ArmorRecipe(bool shield, int second_material) {
    const int combination = shield ? 6 * 17 + 7 : (65 - 16) * 65 + 66 - 16;
    const int material = shield ? 3 * 8 + second_material
                                : 3 * 128 + 3 * 32 + second_material * 4 + 2;
    bytes[kCombinations + combination] = 12;
    bytes[kMaterials + material] = 5;
    const std::array<std::uint8_t, setup::kArmorRecipeBytes> record{
        0, 9, 4, 3, 10, static_cast<std::uint8_t>(-5), 2, 0};
    std::copy(record.begin(), record.end(),
              bytes.begin() + kRecords + (12 * record.size()));
  }

  void CheckArmor(const VS::Items::InventoryArmor& result, bool shield) const {
    const std::array<int, 16> actual{result.id,
                                     result.sub_id,
                                     result.material,
                                     result.category,
                                     result.strength,
                                     result.intelligence,
                                     result.agility,
                                     result.max_dp,
                                     result.current_dp,
                                     result.max_pp,
                                     result.current_pp,
                                     result.gem_slots,
                                     result.weapon_model_id,
                                     result.index,
                                     result.limb,
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

  void CheckBlade(const VS::Items::InventoryBlade& result,
                  const std::array<unsigned char, 16>& record) const {
    const std::array<int, 18> actual{
        result.id,           result.sub_id,     result.weapon_model_id,
        result.category,     result.material,   result.strength,
        result.intelligence, result.agility,    result.DamageType(),
        result.CostType(),   result.cost,       result.opaque12,
        result.max_dp,       result.current_dp, result.max_pp,
        result.current_pp,   result.index,      result.assembled_weapon_index};
    const std::array<int, 18> expected{31, 31, 35, 4,    5,    13,  4,  -2, 2,
                                       5,  7,  9,  1501, 1501, 151, 61, 7,  2};
    for (std::size_t index = 0; index < actual.size(); ++index) {
      CAPTURE(index);
      CHECK(actual[index] == expected[index]);
    }
    const VS::Items::WeaponRange range{.range = record[12],
                                       .second_range = record[13],
                                       .third_range = record[14],
                                       .shape_bits = record[15]};
    CHECK(result.range == range);
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

TEST_CASE("Workshop repair restores DP with original rounded PP cost",
          "[menu][equipment][workshop][repair][menu-setup]") {
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
          "[menu][equipment][workshop][recipes][menu-setup]") {
  const bool shield = GENERATE(false, true);
  const int second_material = GENERATE(3, 4);
  CAPTURE(shield, second_material);
  WorkshopRecipeFixture fixture;
  fixture.ArmorRecipe(shield, second_material);
  VS::Items::InventoryArmor first{}, second{};
  first.id = 191;
  first.sub_id = static_cast<std::uint8_t>(shield ? 6 : 65);
  first.category = 5;
  first.material = 3;
  first.index = 7;
  first.limb = 2;
  first.max_dp = 1001;
  first.current_dp = 123;
  first.max_pp = 101;
  first.current_pp = 41;
  first.types[0] = 7;
  second.sub_id = static_cast<std::uint8_t>(shield ? 7 : 66);
  second.category = 4;
  second.material = static_cast<std::uint16_t>(second_material);
  second.max_dp = 2002;
  second.max_pp = 202;
  second.current_pp = 82;
  const auto build =
      shield ? setup::BuildCombinedShield : setup::BuildCombinedArmor;
  const auto book = fixture.Book(shield ? setup::RecipeKind::kShield
                                        : setup::RecipeKind::kArmor);
  const auto result = build(first, second, book, fixture.bonuses);
  REQUIRE(result.has_value());
  fixture.CheckArmor(*result, shield);
}

TEST_CASE("Workshop repair refreshes equipped runtime DP and PP",
          "[menu][equipment][workshop][repair][menu-setup]") {
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
          "[menu][equipment][workshop][recipes][menu-setup]") {
  const int second_material = GENERATE(3, 4);
  WorkshopRecipeFixture fixture;
  fixture.bytes[fixture.kCombinations + 14 * 96 + 26] = 31;
  fixture.bytes[fixture.kMaterials + 3 * 50 + (second_material - 3) * 10 + 4] =
      5;
  const std::array<unsigned char, 16> record{31, 35, 4, 2, 5, 7, 9, 0,
                                             10, 6,  2, 0, 1, 2, 3, 4};
  std::copy(record.begin(), record.end(),
            fixture.bytes.data() + fixture.kRecords + 31 * record.size());
  VS::Items::InventoryBlade first{}, second{};
  first.id = 14;
  first.category = 3;
  first.material = 3;
  first.index = 7;
  first.assembled_weapon_index = 2;
  first.max_dp = 1001;
  first.current_dp = 123;
  first.max_pp = 101;
  first.current_pp = 41;
  second.id = 26;
  second.category = 4;
  second.material = static_cast<std::uint16_t>(second_material);
  second.max_dp = 2002;
  second.max_pp = 202;
  second.current_pp = 82;
  const auto result = setup::BuildCombinedBlade(
      first, second, fixture.Book(setup::RecipeKind::kBlade), fixture.bonuses);
  REQUIRE(result.has_value());
  fixture.CheckBlade(*result, record);
}

TEST_CASE("Workshop stat fusion preserves extrema truncation and bounds",
          "[menu][equipment][workshop][recipes][menu-setup]") {
  const int different = GENERATE(0, 1);
  const int affinities = GENERATE(0, 1);
  const VS::Items::PartClasses values{100, 99, -99, -100, 4, -4, 3, 0};
  VS::Items::PartClasses result{};
  result.fill(77);
  setup::CombineClassOrAffinityValues(values, values, result, different != 0,
                                      affinities != 0);
  const int scaled = different ? 5 : 6;
  const std::array<int, 8> expected{
      100, 100, -100, -100, scaled, -scaled, affinities ? 4 : 77, 77};
  for (std::size_t index = 0; index < result.size(); ++index) {
    CAPTURE(index);
    CHECK(result[index] == expected[index]);
  }
}

TEST_CASE("Original blade recipe produces the inspected ordered result",
          "[menu][equipment][workshop][recipes][disc][menu-setup]") {
  WorkshopRecipeFixture fixture;
  auto bytes = ReadWorkshopRecipe("BLADE.SYD");
  REQUIRE(bytes.size() == 13264);
  VS::Items::InventoryBlade first{}, second{};
  first.id = 14;
  first.category = 2;
  first.material = 3;
  first.max_dp = 1001;
  second.id = 25;
  second.category = 3;
  second.material = 4;
  second.max_dp = 2002;
  const auto book =
      WorkshopRecipeFixture::Decode(setup::RecipeKind::kBlade, bytes);
  const auto result =
      setup::BuildCombinedBlade(first, second, book, fixture.bonuses);
  REQUIRE(result.has_value());
  const std::array<int, 10> actual{
      result->id,           result->sub_id,   result->weapon_model_id,
      result->category,     result->material, result->strength,
      result->intelligence, result->agility,  result->max_dp,
      result->current_dp};
  const std::array<int, 10> expected{13, 13, 14, 2, 4, 5, 0, -1, 1501, 1501};
  CHECK(actual == expected);
}

TEST_CASE(
    "Original shield and armor recipes use their distinct material tables",
    "[menu][equipment][workshop][recipes][disc][menu-setup]") {
  const bool shield = GENERATE(false, true);
  WorkshopRecipeFixture fixture;
  auto bytes = ReadWorkshopRecipe(shield ? "SHIELD.SYD" : "ARMOR.SYD");
  REQUIRE(bytes.size() == (shield ? 504 : 5912));
  VS::Items::InventoryArmor first{}, second{};
  first.sub_id = static_cast<std::uint8_t>(shield ? 6 : 65);
  second.sub_id = static_cast<std::uint8_t>(shield ? 7 : 66);
  first.category = second.category = 5;
  first.material = static_cast<std::uint16_t>(shield ? 3 : 2);
  second.material = static_cast<std::uint16_t>(shield ? 4 : 3);
  first.max_dp = 1001;
  second.max_dp = 2002;
  const auto build =
      shield ? setup::BuildCombinedShield : setup::BuildCombinedArmor;
  const auto book = WorkshopRecipeFixture::Decode(
      shield ? setup::RecipeKind::kShield : setup::RecipeKind::kArmor, bytes);
  const auto result = build(first, second, book, fixture.bonuses);
  REQUIRE(result.has_value());
  const std::array<int, 9> actual{
      result->id,       result->sub_id,   result->category,
      result->material, result->strength, result->intelligence,
      result->agility,  result->max_dp,   result->current_dp};
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
          "[menu][equipment][workshop][recipes][menu-setup]") {
  const int different = GENERATE(0, 1);
  const int same_type = GENERATE(0, 1);
  VS::Items::PartTypes types1{7, 100, -90, 9};
  VS::Items::PartTypes types2{8, -90, 100, -4};
  if (same_type) types2 = types1;
  VS::Items::PartTypes types_result{77, 77, 77, 77};
  setup::CombinePhysicalTypeValues(types1, types2, types_result,
                                   different != 0);
  CHECK(types_result[0] == 77);
  const int factor = different ? (same_type ? 6 : 4) : (same_type ? 8 : 6);
  for (int index = 1; index < 4; ++index) {
    const int expected =
        std::clamp((types1[index] + types2[index]) * factor / 10, -100, 100);
    CHECK(types_result[index] == expected);
  }
}

TEST_CASE("Assembly and disassembly preserve parts and release gem ownership",
          "[menu][equipment][workshop][ownership][menu-setup]") {
  const int slots = GENERATE(0, 1, 2, 3);
  WorkshopChoicesFixture fixture;
  auto& inventory = Audio_Seq_unusedInventory;
  inventory.weapons[2].index = 3;
  inventory.grips[0].gemSlots = static_cast<char>(slots);
  setup::LiveSetupState().combining_item = 2;
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
  constexpr std::array<std::uint8_t, 4> kGems{1, 2, 3, 0};
  for (int repeat = 0; repeat < 2; ++repeat) {
    RunSetup([&](setup::Context& c) { setup::AssembleWeapon(c, 0, 0, kGems); });
    CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
  }
  expected.weapons[2] = {};
  expected.weapons[2].index = 3;
  expected.blades[0].assembledWeaponIndex = 0;
  expected.grips[0].assembledWeaponIndex = 0;
  for (int index = 0; index < slots; ++index) {
    expected.gems[index].setItemIndex = 0;
  }
  for (int repeat = 0; repeat < 2; ++repeat) {
    RunSetup([](setup::Context& c) { setup::DisassembleWeapon(c, 2); });
    CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
  }
}

TEST_CASE("Shield disassembly releases only its attached gem ownership",
          "[menu][equipment][workshop][ownership][menu-setup]") {
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
  RunSetup([](setup::Context& c) { setup::DetachShieldGems(c, 4); });
  CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
}

TEST_CASE("Workshop combination choices enforce original material masks",
          "[menu][equipment][workshop][choices][menu-setup]") {
  const int workshop = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int material = GENERATE(1, 2, 3, 4, 5, 6, 7);
  const int category = GENERATE(0, 1, 2);
  CAPTURE(workshop, material, category);
  WorkshopChoicesFixture fixture;
  setup::LiveSetupState().combination.workshop =
      static_cast<std::uint8_t>(workshop);
  for (auto& blade : Audio_Seq_unusedInventory.blades) {
    blade.material = static_cast<u_short>(material);
  }
  for (auto& shield : Audio_Seq_unusedInventory.shields) {
    shield.base.material = static_cast<u_short>(material);
  }
  for (auto& armor : Audio_Seq_unusedInventory.armor) {
    armor.material = static_cast<u_short>(material);
  }
  constexpr std::array<WorkshopChoicesFixture::Selector, 3> kSelectors{
      setup::SelectCombinationBlade, setup::SelectCombinationShield,
      setup::SelectCombinationArmor};
  const int capacity = category == 1 ? 8 : 16;
  const auto original = fixture.InventoryBytes();
  fixture.Open(kSelectors[category], 1, capacity);
  constexpr std::array kMasks{0, 0x0E, 0x18, 0x38, 0xC0, 0x3E, 0xFE};
  const bool disabled = (kMasks[workshop] & (1 << material)) == 0;
  const auto& rows = WorkshopChoicesFixture::List().items;
  for (int row = 1; row <= capacity; ++row) {
    CHECK((rows[row].flags & 1) == disabled);
  }
  CHECK(original == fixture.InventoryBytes());
}

TEST_CASE("Combination choices exclude the other ingredient and accessories",
          "[menu][equipment][workshop][choices][menu-setup]") {
  const int category = GENERATE(0, 1, 2);
  WorkshopChoicesFixture fixture;
  auto& combination = setup::LiveSetupState().combination;
  combination.blade_ids = combination.shield_ids =
      combination.armor_ids = {1, 2};
  Audio_Seq_unusedInventory.armor[15].category = 7;
  const int capacity = category == 1 ? 6 : (category == 2 ? 13 : 14);
  constexpr std::array<WorkshopChoicesFixture::Selector, 3> kSelectors{
      setup::SelectCombinationBlade, setup::SelectCombinationShield,
      setup::SelectCombinationArmor};
  const auto original = fixture.InventoryBytes();
  fixture.Open(kSelectors[category], 1, capacity);
  const auto& available = setup::LiveSetupState().available_items;
  CHECK(available[0] == 1);
  CHECK((WorkshopChoicesFixture::List().items[0].flags & 1) == 0);
  for (int row = 1; row <= capacity; ++row) {
    CHECK(available[row] == row + 2);
  }
  CHECK(original == fixture.InventoryBytes());
}

TEST_CASE("Workshop selectors copy full stock-capacity component lists",
          "[menu][equipment][workshop][choices][menu-setup]") {
  struct Selector {
    WorkshopChoicesFixture::Selector select;
    int initial;
    int capacity;
  };
  constexpr std::array kSelectors{
      Selector{setup::SelectAssemblyBlade, 0x80, 16},
      Selector{setup::SelectAssemblyGrip, 0x80, 16},
      Selector{setup::SelectGem, 11, 48},
      Selector{setup::SelectCombinationBlade, 1, 16},
      Selector{setup::SelectCombinationShield, 1, 8},
      Selector{setup::SelectCombinationArmor, 1, 16}};
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
          "[menu][equipment][workshop][menu-setup]") {
  const int workshop = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int weapon_count = GENERATE(0, 1, 7, 8);
  const bool cursor_memory = GENERATE(false, true);
  CAPTURE(workshop, weapon_count, cursor_memory);
  WorkshopGateFixture fixture(workshop, weapon_count, cursor_memory);
  REQUIRE(VS::Items::Inventory::CountLiveItems(0, nullptr) == weapon_count);
  const auto* rows = fixture.Open();
  REQUIRE(setup::LiveSetupState().combination.workshop == workshop);
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
    CHECK(VS::Menu::Framework::UpdateLiveMainMenuSelection(2) == 0);
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
  CHECK(VS::Menu::Framework::LiveHighlightedRow() == (enabled ? 8 : 0));
  Menu_Framework_selectedRow = 4;
  Menu_Framework_subMenuPage = 6;
  VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
  CHECK(Menu_Framework_itemsText == nullptr);
  CHECK(Menu_Framework_menuMemory.cursorMemories[0] == 4);
  CHECK(Menu_Framework_menuMemory.cursorMemories[1] == 6);
  fixture.OpenRows(20);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(VS::Menu::Framework::LiveHighlightedRow() == (enabled ? 10 : 0));
}

TEST_CASE("Cursor Memory clamps a remembered selection after rows shrink",
          "[menu][options][cursor]") {
  CursorFixture fixture;
  Input_settings.cursorMemory = 1;
  Menu_Framework_menuMemory.cursorMemories[0] = 4;
  Menu_Framework_menuMemory.cursorMemories[1] = 6;
  fixture.OpenRows(3);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(VS::Menu::Framework::LiveHighlightedRow() == 0);
}

TEST_CASE("Options retain the real Puzzle Mode availability gate",
          "[menu][options][puzzle]") {
  const char disabled = GENERATE(char{0}, char{1});
  PuzzleOptionsFixture fixture;
  Audio_Seq_unusedStateFlags.puzzleModeDisabled = disabled;
  Input_settings.cursorMemory = 0;
  std::uint8_t state = 4;
  CHECK(VS::Menu::Options::UpdateLiveOptions(state) == 0);
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
  REQUIRE(VS::Menu::Framework::LiveHighlightedRow() == (cursor_memory ? 3 : 0));
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
