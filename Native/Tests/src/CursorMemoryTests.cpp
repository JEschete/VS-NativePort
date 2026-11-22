#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/SoundPlaybackLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/TargetingState.hpp"
#include "Content/StringTables.hpp"
#include "Content/WorkshopRecipes.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Input/Actions.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/BreakArtsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/OptionsLive.hpp"
#include "Menu/OptionsLiveState.hpp"
#include "Menu/Setup/Assembly.hpp"
#include "Menu/Setup/Combination.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Menu/Setup/SetupScreen.hpp"
#include "Menu/TextBox.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"
#include "TestSupport/WorkshopRecipeRoot.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::kActionIdDaggerArtOne;
using VS::Battle::kActionTypeBreakArt;
using VS::Content::WorkshopKind;

namespace menu1_strings = VS::Content::text::menu1_prg::strings;

namespace {
struct CursorFixture {
  VS::Input::Settings saved_settings = VS::Input::settings;
  VS::Menu::Framework::MenuMemory saved_memory =
      VS::Menu::Framework::menuMemory;
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  std::uint16_t* saved_text = VS::Menu::Framework::itemsText;
  int saved_selected = VS::Menu::Framework::selectedRow;
  int saved_animation = VS::Menu::Framework::mainMenuCursorAnimation;
  char saved_root = VS::Menu::Framework::submenuStates[0];
  char saved_page = VS::Menu::Framework::subMenuPage;
  char saved_row_info = VS::Menu::Framework::subMenuRowInfo;
  char saved_row_y = VS::Menu::Framework::menuRowYOffset;
  char saved_list_row = VS::Menu::Framework::itemsListRow;
  std::uint8_t saved_id = VS::Menu::Framework::subMenuId;
  std::uint16_t saved_count = VS::Menu::Framework::subMenuRowCount;
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 768> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  struct {
    std::array<VS::Menu::Framework::MenuRow, 40> rows{};
    std::array<std::byte, 0x124> scratch{};
  } workspace;
  std::array<char*, 40> strings{};
  std::array<int, 20> row_types{};

  CursorFixture() {
    VS::Menu::Framework::menuItems = workspace.rows.data();
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::menuMemory = {};
    VS::Input::settings.menu_flags = 0x3FF;
  }

  ~CursorFixture() {
    if (VS::Menu::Framework::itemsText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(VS::Menu::Framework::itemsText);
    }
    VS::Input::settings = saved_settings;
    VS::Menu::Framework::menuMemory = saved_memory;
    VS::Menu::Framework::menuItems = saved_items;
    VS::Menu::Framework::itemsText = saved_text;
    VS::Menu::Framework::selectedRow = saved_selected;
    VS::Menu::Framework::mainMenuCursorAnimation = saved_animation;
    VS::Menu::Framework::submenuStates[0] = saved_root;
    VS::Menu::Framework::subMenuPage = saved_page;
    VS::Menu::Framework::subMenuRowInfo = saved_row_info;
    VS::Menu::Framework::menuRowYOffset = saved_row_y;
    VS::Menu::Framework::itemsListRow = saved_list_row;
    VS::Menu::Framework::subMenuId = saved_id;
    VS::Menu::Framework::subMenuRowCount = saved_count;
  }

  void OpenRows(int count) {
    VS::Menu::Framework::CreateLiveMenuRows(count, 0x100, strings.data(),
                                            row_types.data());
  }
};

struct PuzzleOptionsFixture {
  CursorFixture rows;
  std::array<std::uint16_t, 1070> saved_table =
      std::to_array(VS::Menu::Options::menuStrings);
  std::uint8_t saved_disabled = VS::State::stateFlags.puzzle_mode_disabled;

  PuzzleOptionsFixture() {
    std::fill_n(VS::Menu::Options::menuStrings, saved_table.size(), 0xE7E7);
    std::fill_n(VS::Menu::Options::menuStrings, 18, 64);
  }

  ~PuzzleOptionsFixture() {
    std::copy(saved_table.begin(), saved_table.end(),
              VS::Menu::Options::menuStrings);
    VS::State::stateFlags.puzzle_mode_disabled = saved_disabled;
  }
};

struct BreakArtMenuFixture {
  CursorFixture rows;
  std::array<std::uint16_t, VS::Menu::break_arts::kMenuStringHalfwords>
      saved_strings = std::to_array(VS::Menu::break_arts::strings);
  std::array<ActionDefinition, 4> saved_actions{};
  VS::Battle::Actor* saved_character = VS::Battle::characterState;
  VS::Battle::Actor* saved_actor = VS::Battle::actors[0];
  decltype(VS::Menu::Framework::shortcutInvoked) saved_shortcut =
      VS::Menu::Framework::shortcutInvoked;
  decltype(VS::Menu::Framework::actionRestrictions) saved_restrictions =
      VS::Menu::Framework::actionRestrictions;
  decltype(VS::Menu::Framework::displayActionCost) saved_cost =
      VS::Menu::Framework::displayActionCost;
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};
  int category;
  int first_action;
  std::uint8_t saved_arts;

  explicit BreakArtMenuFixture(int weapon_category)
      : category(weapon_category),
        first_action(kActionIdDaggerArtOne + (category - 1) * 4),
        saved_arts(VS::State::artsStatus.arts_learned[category % 10]) {
    VS::Menu::break_arts::ResetLiveStatics();
    std::fill_n(VS::Menu::break_arts::strings, saved_strings.size(), 0xE7E7);
    std::fill_n(VS::Menu::break_arts::strings, 72, 96);
    VS::Menu::break_arts::strings[96] = 0;
    VS::Menu::break_arts::strings[menu1_strings::offset::kViewArts] = 0;
    VS::Menu::break_arts::strings[menu1_strings::offset::kViewArtsDesc] = 0;
    VS::State::artsStatus.arts_learned[category % 10] = 4;
    VS::Battle::characterState = VS::Battle::actors[0] = &actor;
    actor.stats = &stats;
    actor.equipped_weapon_category = static_cast<std::uint8_t>(category);
    actor.weapon_drawn = 1;
    stats.current_hp = 999;
    VS::Menu::Framework::actionRestrictions = 0;
    VS::Menu::Framework::shortcutInvoked = 0;
    for (int index = 0; index < 4; ++index) {
      auto& action = VS::Battle::actionDefinitions[first_action + index];
      saved_actions[index] = action;
      action = {};
      action.type = kActionTypeBreakArt;
      action.unlocked = 1;
      std::fill_n(action.name.begin(), action.name.size(),
                  static_cast<char>(0xE7));
      action.name[0] = '\0';
    }
  }

  ~BreakArtMenuFixture() {
    VS::Menu::break_arts::ResetLiveStatics();
    std::copy(saved_strings.begin(), saved_strings.end(),
              VS::Menu::break_arts::strings);
    std::copy(saved_actions.begin(), saved_actions.end(),
              &VS::Battle::actionDefinitions[first_action]);
    VS::Battle::characterState = saved_character;
    VS::Battle::actors[0] = saved_actor;
    VS::Menu::Framework::shortcutInvoked = saved_shortcut;
    VS::Menu::Framework::actionRestrictions = saved_restrictions;
    VS::Menu::Framework::displayActionCost = saved_cost;
    VS::State::artsStatus.arts_learned[category % 10] = saved_arts;
  }

  void FinishRowAnimations() {
    for (auto& row : rows.workspace.rows) {
      row.state = VS::Menu::Framework::RowState::kIdle;
    }
  }

  void Open() {
    REQUIRE(VS::Menu::break_arts::UpdateArtListLive(category) == 0);
    FinishRowAnimations();
    REQUIRE(VS::Menu::break_arts::UpdateArtListLive(0) == 0);
  }

  int SelectRow(int row) {
    VS::Menu::Framework::selectedRow = row;
    VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
    REQUIRE(VS::Menu::break_arts::UpdateArtListLive(0) == 0);
    FinishRowAnimations();
    return VS::Menu::break_arts::UpdateArtListLive(0);
  }
};

struct EquipmentGateFixture {
  CursorFixture rows;
  decltype(std::to_array(VS::Menu::Inventory::menuText)) saved_strings =
      std::to_array(VS::Menu::Inventory::menuText);
  decltype(VS::State::stateFlags) saved_flags = VS::State::stateFlags;
  decltype(VS::Items::inventory) saved_inventory = VS::Items::inventory;
  decltype(VS::Items::inventoryIndices) saved_indices =
      VS::Items::inventoryIndices;
  VS::Battle::Actor* saved_character = VS::Battle::characterState;
  decltype(VS::Menu::Framework::actionRestrictions) saved_restrictions =
      VS::Menu::Framework::actionRestrictions;
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};

  EquipmentGateFixture() {
    VS::Menu::Inventory::ResetInventoryTerminalSession();
    std::fill_n(VS::Menu::Inventory::menuText, saved_strings.size(), 0xE7E7);
    std::fill_n(VS::Menu::Inventory::menuText, 128, 128);
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    actor.stats = &stats;
    VS::Battle::characterState = &actor;
  }

  ~EquipmentGateFixture() {
    VS::Menu::Inventory::ResetInventoryTerminalSession();
    std::copy(saved_strings.begin(), saved_strings.end(),
              VS::Menu::Inventory::menuText);
    VS::State::stateFlags = saved_flags;
    VS::Items::inventory = saved_inventory;
    VS::Items::inventoryIndices = saved_indices;
    VS::Battle::characterState = saved_character;
    VS::Menu::Framework::actionRestrictions = saved_restrictions;
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
  decltype(VS::Items::inventory) saved_inventory = VS::Items::inventory;
  std::uint16_t* saved_text = VS::Menu::Framework::menu12Text;
  int* saved_types = VS::Menu::Framework::rowTypeBuf;
  VS::Field::MpdEnvironment* saved_environment =
      VS::Field::roomData.environment;
  std::uint32_t saved_environment_length =
      VS::Field::roomData.header.environment_length;
  setup::State saved_state = setup::LiveSetupState();
  std::array<std::uint16_t, 1024> strings{};
  VS::Field::MpdEnvironment environment{};

  WorkshopGateFixture(int workshop, int weapon_count, bool cursor_memory) {
    setup::LiveSetupState() = {};
    std::fill(strings.begin(), strings.end(), 0xE7E7);
    std::fill_n(strings.begin(), 16, 32);
    VS::Menu::Framework::menu12Text = strings.data();
    VS::Menu::Framework::rowTypeBuf = rows.row_types.data();
    VS::Field::roomData.environment = &environment;
    VS::Field::roomData.header.environment_length = sizeof environment;
    VS::Items::inventory = {};
    environment.is_workshop = static_cast<char>(workshop);
    VS::Input::settings.cursor_memory = static_cast<char>(cursor_memory);
    for (int index = 0; index < weapon_count; ++index) {
      VS::Items::inventory.weapons[index].index = static_cast<char>(index + 1);
      VS::Items::inventory.weapons[index].blade =
          static_cast<std::uint8_t>(index + 1);
    }
  }

  ~WorkshopGateFixture() {
    setup::LiveSetupState() = saved_state;
    VS::Items::inventory = saved_inventory;
    VS::Menu::Framework::menu12Text = saved_text;
    VS::Menu::Framework::rowTypeBuf = saved_types;
    VS::Field::roomData.environment = saved_environment;
    VS::Field::roomData.header.environment_length = saved_environment_length;
  }

  const VS::Menu::Framework::ItemText* Open() {
    std::uint8_t state = 2;
    REQUIRE(setup::UpdateLiveSetup(state) == 0);
    REQUIRE(state == 3);
    REQUIRE(VS::Menu::Framework::subMenuRowCount == 6);
    const auto* items = reinterpret_cast<const VS::Menu::Framework::ItemText*>(
        VS::Menu::Framework::itemsText);
    REQUIRE(items != nullptr);
    return items;
  }
};

struct WorkshopChoicesFixture {
  WorkshopGateFixture setup{6, 0, false};
  decltype(VS::Items::inventoryIndices) saved_indices =
      VS::Items::inventoryIndices;
  decltype(VS::Battle::stringContext) saved_context = VS::Battle::stringContext;
  decltype(VS::Menu::Framework::itemNames) saved_names =
      VS::Menu::Framework::itemNames;
  std::uint16_t* saved_help = VS::Menu::Framework::itemHelp;
  decltype(VS::Menu::Framework::shields) saved_shields =
      VS::Menu::Framework::shields;
  std::array<std::uint16_t, 2048> strings{};
  std::array<std::uint16_t, 16384> help{};
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
    VS::Menu::Framework::menu12Text = strings.data();
    VS::Menu::Framework::itemHelp = help.data();
    VS::Menu::Framework::itemNames = names;
    VS::Menu::Framework::rowTypeBuf = types.data();
    VS::Menu::Framework::shields = VS::Items::inventory.shields.data();
    setup::LiveSetupState().combination.workshop = 6;
    VS::Items::inventoryIndices = {};
    FillParts();
    FillProtection();
    for (int category = 1; category <= 5; ++category) {
      VS::Items::Inventory::RebuildLiveDisplayIndices(category);
    }
  }

  ~WorkshopChoicesFixture() {
    VS::Items::inventoryIndices = saved_indices;
    VS::Battle::stringContext = saved_context;
    VS::Menu::Framework::itemNames = saved_names;
    VS::Menu::Framework::itemHelp = saved_help;
    VS::Menu::Framework::shields = saved_shields;
  }

  void FillParts() {
    for (int index = 0; index < 16; ++index) {
      auto& blade = VS::Items::inventory.blades[index];
      blade.id = 1;
      blade.index = static_cast<char>(index + 1);
      blade.category = 1;
      blade.SetDamageType(1);
      blade.material = 3;
      auto& grip = VS::Items::inventory.grips[index];
      grip.id = 97;
      grip.index = static_cast<std::uint16_t>(index + 1);
      grip.category = 1;
    }
    for (int index = 0; index < 48; ++index) {
      auto& gem = VS::Items::inventory.gems[index];
      gem.id = 261;
      gem.index = static_cast<int16_t>(index + 1);
    }
  }

  void FillProtection() {
    for (int index = 0; index < 16; ++index) {
      auto& armor = VS::Items::inventory.armor[index];
      armor.id = 161;
      armor.index = static_cast<std::uint8_t>(index + 1);
      armor.category = 1;
      armor.material = 3;
      armor.max_dp = 1000;
      if (index < 8) {
        auto& shield = VS::Items::inventory.shields[index];
        shield.base.id = 132;
        shield.base.material = 3;
        shield.base.max_dp = 1000;
      }
    }
  }

  std::array<std::byte, sizeof(VS::Items::InventoryContents)> InventoryBytes()
      const {
    std::array<std::byte, sizeof(VS::Items::InventoryContents)> bytes{};
    std::memcpy(bytes.data(), &VS::Items::inventory, bytes.size());
    return bytes;
  }

  using Selector = int (*)(setup::Context&, int);

  void Open(Selector select, int initial, int capacity) {
    REQUIRE(RunSetup([&](setup::Context& c) { return select(c, initial); }) ==
            0);
    for (auto& row : setup.rows.workspace.rows) {
      row.state = VS::Menu::Framework::RowState::kIdle;
    }
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
  decltype(VS::Input::buttonsPressed) saved_pressed = VS::Input::buttonsPressed;
  int saved_repeat = VS::Input::buttonRepeat;
  VS::Menu::TextBoxState saved_box = VS::Menu::textBoxes[7];
  VS::Battle::Actor* saved_character = VS::Battle::characterState;
  VS::Battle::Actor* saved_actor = VS::Battle::actors[0];
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};
  std::array<std::uint32_t, 0x180> sfx_offsets{};
  std::array<std::uint16_t, 0x180> sfx_selectors{};
  VS::Audio::Playback::ScopedLiveEffectTables sfx_tables{
      {.program_offsets = sfx_offsets,
       .bank_selectors = sfx_selectors,
       .program_data = {}}};

  WorkshopRepairFixture(int missing_dp, int pp) {
    sfx_offsets.fill(UINT32_MAX);
    auto& inventory = VS::Items::inventory;
    inventory = {};
    auto initialize = [missing_dp, pp](auto& item, int id) {
      item.id = static_cast<decltype(item.id)>(id);
      item.max_dp = 10000;
      item.current_dp = static_cast<std::uint16_t>(10000 - missing_dp);
      item.max_pp = 200;
      item.current_pp = static_cast<std::uint16_t>(pp);
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
    VS::Input::buttonRepeat = 0;
  }

  ~WorkshopRepairFixture() {
    VS::Input::buttonsPressed = saved_pressed;
    VS::Input::buttonRepeat = saved_repeat;
    VS::Menu::textBoxes[7] = saved_box;
    VS::Battle::characterState = saved_character;
    VS::Battle::actors[0] = saved_actor;
  }

  int Tick() {
    for (auto& row : setup.setup.rows.workspace.rows) {
      row.state = VS::Menu::Framework::RowState::kIdle;
    }
    return RunSetup(
        [](setup::Context& c) { return setup::UpdateRepair(c, false); });
  }

  void Equip() {
    auto& inventory = VS::Items::inventory;
    VS::Battle::characterState = VS::Battle::actors[0] = &actor;
    actor.stats = &stats;
    inventory.weapons[0].index = 1;
    inventory.weapons[0].blade = inventory.weapons[0].grip = 1;
    inventory.weapons[0].is_equipped = 1;
    inventory.blades[0].assembled_weapon_index = 1;
    inventory.blades[0].category = 2;
    inventory.grips[0].id = 96;
    inventory.grips[0].category = 1;
    inventory.grips[0].index = inventory.grips[0].assembled_weapon_index = 1;
    inventory.shields[0].is_equipped = 1;
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
    VS::Input::buttonsPressed = static_cast<std::uint16_t>(button);
    for (int step = 0; step < 5; ++step) {
      CAPTURE(step);
      REQUIRE(Tick() == 0);
    }
  }
};

// Workshop recipe books from tables holding every record of the kind's SYD
// file, with the pairs and material rules each test adds.
struct WorkshopRecipeFixture {
  setup::MaterialBonuses bonuses{};

  WorkshopRecipeFixture() {
    bonuses[5] = {.strength = 3, .intelligence = -2, .agility = -4};
  }

  [[nodiscard]] static setup::RecipeBook Build(
      const VS::Content::WorkshopRecipeTable& table) {
    auto book = setup::BuildRecipeBook(table);
    INFO(book.error_or(""));
    REQUIRE(book.has_value());
    return *std::move(book);
  }

  // Shields 6 and 7 (armor 65 and 66) make record 12; material 3 (category
  // 5) with `second_material` (category 4) gives material 5.
  [[nodiscard]] static setup::RecipeBook ArmorBook(bool shield,
                                                   int second_material) {
    const WorkshopKind kind =
        shield ? WorkshopKind::kShield : WorkshopKind::kArmor;
    auto table = VS::test_support::FullWorkshopTable(kind);
    table.pairs.push_back(
        {.first = VS::test_support::WorkshopResultId(kind, shield ? 6 : 65),
         .second = VS::test_support::WorkshopResultId(kind, shield ? 7 : 66),
         .result = VS::test_support::WorkshopResultId(kind, 12)});
    table.materials.push_back(
        {.first_material = 3,
         .first_category = static_cast<std::uint8_t>(shield ? 0 : 5),
         .second_material = static_cast<std::uint8_t>(second_material),
         .second_category = static_cast<std::uint8_t>(shield ? 0 : 4),
         .material = 5});
    VS::Content::PieceResult& record = table.pieces.at(12);
    record.model = 9;
    record.category = 4;
    record.gem_slots = 3;
    record.stats = {.strength = 10, .intelligence = -5, .agility = 2};
    return Build(table);
  }

  // Blades 14 and 26 make record 31; material 3 (category 3) with
  // `second_material` (category 4) gives material 5.
  [[nodiscard]] static setup::RecipeBook BladeBook(int second_material) {
    constexpr WorkshopKind kBlade = WorkshopKind::kBlade;
    auto table = VS::test_support::FullWorkshopTable(kBlade);
    table.pairs.push_back(
        {.first = VS::test_support::WorkshopResultId(kBlade, 14),
         .second = VS::test_support::WorkshopResultId(kBlade, 26),
         .result = VS::test_support::WorkshopResultId(kBlade, 31)});
    table.materials.push_back(
        {.first_material = 3,
         .first_category = 3,
         .second_material = static_cast<std::uint8_t>(second_material),
         .second_category = 4,
         .material = 5});
    VS::Content::BladeResult& record = table.blades.at(31);
    record.model = 35;
    record.category = 4;
    record.damage_type = 2;
    record.cost_type = 5;
    record.cost = 7;
    record.recovery_delay = 9;
    record.stats = {.strength = 10, .intelligence = 6, .agility = 2};
    record.range = 1;
    record.range_y = 2;
    record.range_z = 3;
    record.attack_shape = 4;
    return Build(table);
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

  void CheckBlade(const VS::Items::InventoryBlade& result) const {
    const std::array<int, 18> actual{
        result.id,           result.sub_id,     result.weapon_model_id,
        result.category,     result.material,   result.strength,
        result.intelligence, result.agility,    result.DamageType(),
        result.CostType(),   result.cost,       result.recovery_delay,
        result.max_dp,       result.current_dp, result.max_pp,
        result.current_pp,   result.index,      result.assembled_weapon_index};
    const std::array<int, 18> expected{31, 31, 35, 4,    5,    13,  4,  -2, 2,
                                       5,  7,  9,  1501, 1501, 151, 61, 7,  2};
    for (std::size_t index = 0; index < actual.size(); ++index) {
      CAPTURE(index);
      CHECK(actual[index] == expected[index]);
    }
    const VS::Items::WeaponRange range{
        .range = 1, .range_y = 2, .range_z = 3, .shape_bits = 4};
    CHECK(result.range == range);
  }
};

// The workshop recipe tables VS.Extract converted from the extracted disc.
VS::Content::WorkshopRecipeCatalog ConvertedWorkshopRecipes() {
  for (auto directory = std::filesystem::current_path();
       directory != directory.parent_path();
       directory = directory.parent_path()) {
    const auto root = directory / "Native" / "data";
    if (!std::filesystem::exists(root / "catalogs" / "menu")) continue;
    auto catalog = VS::Content::LoadWorkshopRecipes(root);
    INFO((catalog ? std::string{} : catalog.error()));
    REQUIRE(catalog.has_value());
    return *std::move(catalog);
  }
  SKIP("Converted workshop recipe tables are unavailable");
  return {};
}
}  // namespace

TEST_CASE("Workshop repair restores DP with original rounded PP cost",
          "[menu][equipment][workshop][repair][menu-setup]") {
  const int missing = GENERATE(0, 1, 99, 100, 101, 9999, 10000);
  const int pp = GENERATE(0, 1, 50, 200);
  CAPTURE(missing, pp);
  WorkshopRepairFixture fixture(missing, pp);
  const auto original = VS::Items::inventory;
  fixture.Prompt(VS::Input::Cross);
  CHECK(std::memcmp(&original, &VS::Items::inventory, sizeof(original)) == 0);
  REQUIRE(fixture.Tick() == -1);
  auto expected = original;
  auto repaired = [missing, pp](auto& item) {
    item.current_dp = item.max_dp;
    item.current_pp =
        static_cast<std::uint16_t>(std::max(0, pp - missing / 100));
  };
  repaired(expected.blades[0]);
  repaired(expected.shields[0].base);
  repaired(expected.armor[0]);
  for (int repeat = 0; repeat < 2; ++repeat) {
    fixture.Prompt(VS::Input::Circle);
    REQUIRE(fixture.Tick() == 0);
    CHECK(std::memcmp(&expected, &VS::Items::inventory, sizeof(expected)) == 0);
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
  const auto book = WorkshopRecipeFixture::ArmorBook(shield, second_material);
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
  REQUIRE(fixture.stats.weapon.current_dp == 10000 - missing);
  REQUIRE(fixture.stats.shield.current_dp == 10000 - missing);
  REQUIRE(fixture.stats.limbs[0].armor.current_dp == 10000 - missing);
  fixture.Prompt(VS::Input::Circle);
  REQUIRE(fixture.Tick() == 0);
  const std::array<int, 5> actual{
      fixture.stats.weapon.current_dp, fixture.stats.shield.current_dp,
      fixture.stats.limbs[0].armor.current_dp, fixture.stats.weapon.current_pp,
      fixture.stats.shield.current_pp};
  const int remaining_pp = std::max(0, pp - missing / 100);
  const std::array<int, 5> expected{10000, 10000, 10000, remaining_pp,
                                    remaining_pp};
  CHECK(actual == expected);
  CHECK(VS::Items::inventory.weapons[0].is_equipped == 1);
  CHECK(VS::Items::inventory.shields[0].is_equipped == 1);
  CHECK(VS::Items::inventory.armor[0].limb == 1);
}

TEST_CASE("Workshop blade recipes use ordered IDs materials and native stats",
          "[menu][equipment][workshop][recipes][menu-setup]") {
  const int second_material = GENERATE(3, 4);
  WorkshopRecipeFixture fixture;
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
      first, second, WorkshopRecipeFixture::BladeBook(second_material),
      fixture.bonuses);
  REQUIRE(result.has_value());
  fixture.CheckBlade(*result);
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
  const auto recipes = ConvertedWorkshopRecipes();
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
      WorkshopRecipeFixture::Build(recipes.Of(WorkshopKind::kBlade));
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
  const auto recipes = ConvertedWorkshopRecipes();
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
  const auto book = WorkshopRecipeFixture::Build(
      recipes.Of(shield ? WorkshopKind::kShield : WorkshopKind::kArmor));
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
  auto& inventory = VS::Items::inventory;
  inventory.weapons[2].index = 3;
  inventory.grips[0].gem_slots = static_cast<char>(slots);
  setup::LiveSetupState().combining_item = 2;
  auto expected = inventory;
  auto& weapon = expected.weapons[2];
  weapon.blade = weapon.grip = 1;
  weapon.name.fill(static_cast<char>(0xE7));
  expected.blades[0].assembled_weapon_index = 3;
  expected.grips[0].assembled_weapon_index = 3;
  for (int index = 0; index < slots; ++index) {
    weapon.gems[index] = static_cast<std::uint8_t>(index + 1);
    expected.gems[index].set_item_index = 3;
  }
  constexpr std::array<std::uint8_t, 4> kGems{1, 2, 3, 0};
  for (int repeat = 0; repeat < 2; ++repeat) {
    RunSetup([&](setup::Context& c) { setup::AssembleWeapon(c, 0, 0, kGems); });
    CHECK(std::memcmp(&inventory, &expected, sizeof(inventory)) == 0);
  }
  expected.weapons[2] = {};
  expected.weapons[2].index = 3;
  expected.blades[0].assembled_weapon_index = 0;
  expected.grips[0].assembled_weapon_index = 0;
  for (int index = 0; index < slots; ++index) {
    expected.gems[index].set_item_index = 0;
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
  auto& inventory = VS::Items::inventory;
  inventory.shields[4].base.gem_slots = static_cast<char>(slots);
  for (int index = 0; index < slots; ++index) {
    inventory.shields[4].gems[index] = static_cast<std::uint8_t>(index + 1);
    inventory.gems[index].set_item_index = 0x85;
  }
  auto expected = inventory;
  std::fill_n(expected.shields[4].gems.begin(), slots, 0);
  for (int index = 0; index < slots; ++index) {
    expected.gems[index].set_item_index = 0;
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
  for (auto& blade : VS::Items::inventory.blades) {
    blade.material = static_cast<std::uint16_t>(material);
  }
  for (auto& shield : VS::Items::inventory.shields) {
    shield.base.material = static_cast<std::uint16_t>(material);
  }
  for (auto& armor : VS::Items::inventory.armor) {
    armor.material = static_cast<std::uint16_t>(material);
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
  VS::Items::inventory.armor[15].category = 7;
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
  CHECK(VS::Input::settings.cursor_memory == cursor_memory);
  for (int row = 0; row < 6; ++row) {
    CAPTURE(row);
    const bool disabled = (row == 0 && (workshop == 0 || weapon_count == 8)) ||
                          (row == 3 && weapon_count == 0) ||
                          ((row == 4 || row == 5) && workshop == 0);
    CHECK((rows[row].flags & 1) == disabled);
  }
}

TEST_CASE("Equipment management obeys progression tarnish and action gates",
          "[menu][equipment][restrictions]") {
  const int intro = GENERATE(0, 1, 2, 3, 4);
  const bool tarnished = GENERATE(false, true);
  const int restrictions = GENERATE(0, 1, 2, 4, 7, 8, 0x80, 0x200);
  EquipmentGateFixture fixture;
  VS::State::stateFlags.intro_state = static_cast<char>(intro);
  fixture.stats.statuses = tarnished ? 0x80000 : 0;
  VS::Menu::Framework::actionRestrictions = restrictions;
  REQUIRE(VS::Menu::Inventory::UpdateLiveItemList(1) == 0);
  REQUIRE(VS::Menu::Inventory::UpdateLiveItemList(0) == 0);
  REQUIRE(VS::Menu::Framework::subMenuRowCount == 2);
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  const auto* rows = reinterpret_cast<const VS::Menu::Framework::ItemText*>(
      VS::Menu::Framework::itemsText);
  const bool disabled = intro < 3 || tarnished || (restrictions & 7) != 0;
  for (int row = 0; row < 2; ++row) {
    CHECK(((rows[row].flags & 1) != 0) == disabled);
  }
}

TEST_CASE("Cursor Memory controls the actual root selection consumer",
          "[menu][options][cursor]") {
  const std::uint8_t enabled = GENERATE(std::uint8_t{0}, std::uint8_t{1});
  const std::uint16_t flags =
      GENERATE(std::uint16_t{0x3FF}, std::uint16_t{0x3F8});
  CursorFixture fixture;
  VS::Input::settings.cursor_memory = enabled;
  VS::Input::settings.menu_flags = flags;
  for (int visit = 0; visit < 3; ++visit) {
    VS::Menu::Framework::submenuStates[0] = 7;
    CHECK(VS::Menu::Framework::UpdateLiveMainMenuSelection(2) == 0);
    CHECK(VS::Menu::Framework::submenuStates[0] ==
          (enabled ? 7 : (flags == 0x3F8 ? 3 : 0)));
    CHECK(VS::Input::settings.cursor_memory == enabled);
  }
}

TEST_CASE("Cursor Memory restores shared row and scroll positions",
          "[menu][options][cursor]") {
  const std::uint8_t enabled = GENERATE(std::uint8_t{0}, std::uint8_t{1});
  CursorFixture fixture;
  VS::Input::settings.cursor_memory = enabled;
  VS::Menu::Framework::menuMemory.cursor_memories[0] = 3;
  VS::Menu::Framework::menuMemory.cursor_memories[1] = 5;
  fixture.OpenRows(20);
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  CHECK(VS::Menu::Framework::LiveHighlightedRow() == (enabled ? 8 : 0));
  VS::Menu::Framework::selectedRow = 4;
  VS::Menu::Framework::subMenuPage = 6;
  VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
  CHECK(VS::Menu::Framework::itemsText == nullptr);
  CHECK(VS::Menu::Framework::menuMemory.cursor_memories[0] == 4);
  CHECK(VS::Menu::Framework::menuMemory.cursor_memories[1] == 6);
  fixture.OpenRows(20);
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  CHECK(VS::Menu::Framework::LiveHighlightedRow() == (enabled ? 10 : 0));
}

TEST_CASE("Cursor Memory clamps a remembered selection after rows shrink",
          "[menu][options][cursor]") {
  CursorFixture fixture;
  VS::Input::settings.cursor_memory = 1;
  VS::Menu::Framework::menuMemory.cursor_memories[0] = 4;
  VS::Menu::Framework::menuMemory.cursor_memories[1] = 6;
  fixture.OpenRows(3);
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  CHECK(VS::Menu::Framework::LiveHighlightedRow() == 0);
}

TEST_CASE("Options retain the real Puzzle Mode availability gate",
          "[menu][options][puzzle]") {
  const char disabled = GENERATE(char{0}, char{1});
  PuzzleOptionsFixture fixture;
  VS::State::stateFlags.puzzle_mode_disabled = disabled;
  VS::Input::settings.cursor_memory = 0;
  std::uint8_t state = 4;
  CHECK(VS::Menu::Options::UpdateLiveOptions(state) == 0);
  CHECK(state == 5);
  CHECK(VS::Input::settings.cursor_memory == 0);
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  const auto* rows = reinterpret_cast<const VS::Menu::Framework::ItemText*>(
      VS::Menu::Framework::itemsText);
  for (int row = 0; row < 9; ++row) {
    CAPTURE(disabled, row);
    CHECK(rows[row].flags ==
          (row == 6 && disabled ? VS::Menu::Framework::kRowDisabled : 0));
  }
}

TEST_CASE(
    "Four learned Break Arts preserve their navigation row and cursor policy",
    "[menu][break-arts][cursor]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const int flags = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const char shortcut = static_cast<char>(flags & 1);
  const bool equipped = (flags & 2) != 0;
  const auto cursor_memory = static_cast<std::uint8_t>((flags >> 2) & 1);
  BreakArtMenuFixture fixture(category);
  VS::Menu::Framework::shortcutInvoked = shortcut;
  if (!equipped) {
    fixture.actor.equipped_weapon_category =
        static_cast<std::uint8_t>(category % 10 + 1);
  }
  VS::Input::settings.cursor_memory = cursor_memory;
  VS::Menu::Framework::menuMemory
      .cursor_memories[static_cast<std::size_t>(category + 11) * 2] = 3;
  fixture.Open();
  REQUIRE(VS::Menu::Framework::itemsText != nullptr);
  REQUIRE(VS::Menu::Framework::subMenuRowCount == (shortcut ? 4 : 5));
  REQUIRE(VS::Menu::Framework::LiveHighlightedRow() == (cursor_memory ? 3 : 0));
  CHECK(VS::Input::settings.cursor_memory == cursor_memory);
  const auto* rows = reinterpret_cast<const VS::Menu::Framework::ItemText*>(
      VS::Menu::Framework::itemsText);
  for (int index = 0; index < 4; ++index) {
    CHECK(rows[index].flags ==
          (equipped ? 0 : VS::Menu::Framework::kRowDisabled));
  }
  if (!shortcut) {
    CHECK(rows[4].flags == 2);
    CHECK(fixture.SelectRow(4) == 0xFFFF);
  }
}

TEST_CASE("The fourth learned Break Art remains an action in every category",
          "[menu][break-arts][cursor]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const char shortcut = GENERATE(char{0}, char{1});
  BreakArtMenuFixture fixture(category);
  VS::Menu::Framework::shortcutInvoked = shortcut;
  fixture.Open();
  CHECK(fixture.SelectRow(3) == fixture.first_action + 3);
}
