// Hand-maintained (tools/port/owned.csv).
#include "Menu/MagicCostsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Field/RoomStateLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Magic.hpp"
#include "Menu/MagicCosts.hpp"
#include "State/StateFlags.hpp"

namespace battle_menu_strings = VS::Content::text::battle_prg::menu_strings;
namespace menu0_teleportation = VS::Content::text::menu0_prg::teleportation;

namespace VS::Menu::Magic {
namespace {
constexpr std::size_t kTeleportCursorRow = 22;
constexpr std::size_t kTeleportCursorScroll = 23;
constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
TeleportMenuState g_teleport_state{};
std::uint32_t g_refusals = 0;

char ToStorageChar(std::uint8_t value) { return std::bit_cast<char>(value); }

std::optional<std::span<const std::uint8_t>> NullTerminated(
    std::span<const std::uint16_t> strings, std::size_t offset) {
  if (offset >= strings.size()) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> bytes(
      reinterpret_cast<const std::uint8_t*>(strings.data() + offset),
      (strings.size() - offset) * sizeof(strings[0]));
  const auto terminator = std::ranges::find(bytes, std::uint8_t{0});
  if (terminator == bytes.end()) {
    return std::nullopt;
  }
  return bytes.first(static_cast<std::size_t>(terminator - bytes.begin()) + 1);
}

// The row builder's char* view of a terminated teleportation string.
bool ResolveRowText(std::span<const std::uint16_t> table, std::uint16_t offset,
                    char*& text) {
  const auto bytes = NullTerminated(table, offset);
  if (!bytes) {
    return false;
  }
  text = reinterpret_cast<char*>(const_cast<std::uint8_t*>(bytes->data()));
  return true;
}

class LiveHost final : public Host {
 public:
  void Refuse(std::string_view reason) override {
    ++g_refusals;
    if (g_refusals <= kRefusalLogLimit ||
        g_refusals % kRefusalLogInterval == 0) {
      VS::Core::LogWarn("menu.magic", "{} (refusal {})", reason, g_refusals);
    }
  }

  std::optional<std::uint8_t> ActionCost(int action_id) override {
    if (ValidAction(action_id)) {
      return Battle_actionDefinitions[action_id].cost;
    }
    return std::nullopt;
  }
  std::optional<std::uint32_t> ActionRestrictionFlags(int action_id) override {
    if (ValidAction(action_id) && Battle_characterState != nullptr) {
      // The spell's restriction bits for the player (actor slot 0).
      return static_cast<std::uint32_t>(
          VS::Battle::LiveActorActionRestrictionFlags(0, action_id));
    }
    return std::nullopt;
  }
  void SetActionCostText(int index, std::string_view text, int x,
                         int disabled) override {
    const std::string terminated(text);
    Framework::SetLiveActionCostText(index, terminated.c_str(), x, disabled);
  }

  int CurrentRoomId() override { return VS::Field::LiveCurrentRoomId(); }
  const SavePointTable& SavePoints() override { return OriginalSavePoints(); }
  std::span<const std::uint8_t> SavePointStates() override {
    return {reinterpret_cast<const std::uint8_t*>(
                VS::State::stateFlags.save_point_states),
            std::size(VS::State::stateFlags.save_point_states)};
  }
  std::optional<std::int16_t> CurrentMp() override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      return std::nullopt;
    }
    return Battle_characterState->stats->currentMp;
  }
  std::span<const std::uint16_t> TeleportationStrings() override {
    return teleportationStrings;
  }
  std::uint16_t LocationsIndex() const override {
    return menu0_teleportation::index::kLocations;
  }
  std::uint16_t DisabledDescriptionOffset() const override {
    return menu0_teleportation::offset::kTeleportationDisabled;
  }

  void FlyOutRowsAndHoistSelection(int row, int offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  std::uint32_t ActionRestrictions() override {
    return static_cast<std::uint32_t>(VS::Menu::Framework::actionRestrictions);
  }
  void SetTeleportCursorMemory(std::uint8_t row, std::uint8_t scroll) override {
    VS::Menu::Framework::menuMemory.cursor_memories[kTeleportCursorRow] = row;
    VS::Menu::Framework::menuMemory.cursor_memories[kTeleportCursorScroll] =
        scroll;
  }
  std::uint8_t CursorMemorySetting() override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  }
  void SetCursorMemorySetting(std::uint8_t value) override {
    VS::Input::settings.cursor_memory = ToStorageChar(value);
  }
  void CreateRows(int row_info, std::span<const TeleportRow> rows) override {
    if (rows.size() > kSavePointCount) {
      Refuse("Magic teleport row count exceeds the live row buffer");
      return;
    }
    std::array<char*, kSavePointCount * 2> strings{};
    std::array<int, kSavePointCount> types{};
    const std::span<const std::uint16_t> table = TeleportationStrings();
    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (!ResolveRowText(table, rows[i].name_offset, strings[i * 2]) ||
          !ResolveRowText(table, rows[i].description_offset,
                          strings[(i * 2) + 1])) {
        Refuse("Magic teleport row text is unavailable or unterminated");
        return;
      }
      types[i] = rows[i].type;
    }
    Framework::CreateLiveMenuRows(static_cast<int>(rows.size()), row_info,
                                  strings.data(), types.data());
  }
  int SelectedRow() override { return Framework::LiveSelectedRow(); }
  int HighlightedRow() override { return Framework::LiveHighlightedRow(); }
  void SetDisplayActionCost(int enabled) override {
    VS::Menu::Framework::displayActionCost = static_cast<char>(enabled);
  }
  void ClearRowsExcept(int row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void SetCommandLabel(Framework::MenuCommand command) override {
    Framework::SetLiveCommandLabel(command);
  }

  std::span<const std::uint16_t> BattleMenuStrings() override {
    return VS::Menu::menuStrings;
  }
  std::uint16_t MagicHeaderIndex() const override {
    return battle_menu_strings::index::kMagic;
  }
  void SetMenuItem(const MenuItem& item) override {
    const auto text = NullTerminated(VS::Menu::menuStrings, item.text_offset);
    if (!text) {
      Refuse("Magic header text is unavailable or unterminated");
      return;
    }
    Framework::MenuRow& row = Framework::SetLiveMenuItemText(
        {.row = item.row,
         .x = item.x,
         .y = item.y,
         .w = item.width,
         .background_width = item.background_width},
        *text);
    row.state = static_cast<Framework::RowState>(item.state);
    row.target0 = item.target_position;
    row.selected = item.selected;
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }

 private:
  static bool ValidAction(int action_id) {
    return action_id >= 0 && static_cast<std::size_t>(action_id) < kActionCount;
  }
};
}  // namespace

// decomp: MENU0.PRG _teleportMenu (src/MENU/MENU0.PRG/15AC.c:22).
int UpdateTeleportMenuLive(int init) {
  LiveHost host;
  const TeleportMenuResult result =
      UpdateTeleportMenu(init, g_teleport_state, host);
  if (result.destination) {
    return PackTeleportDestination(*result.destination);
  }
  return result.selection;
}

// decomp: MENU0.PRG _setMenuHeader (src/MENU/MENU0.PRG/15AC.c:148).
void CreateHeaderLive() {
  LiveHost host;
  CreateHeader(host);
}

// decomp: MENU0.PRG _setMPCost (src/MENU/MENU0.PRG/84.c:18).
void SetSpellMpCostLive(int id) {
  LiveHost host;
  SetSpellMpCost(id, host);
}

// decomp: MENU0.PRG _setMPCostDirect (src/MENU/MENU0.PRG/84.c:64).
void SetTeleportMpCostLive(int cost_decimal, int disabled) {
  LiveHost host;
  SetTeleportMpCost(cost_decimal, disabled, host);
}

// decomp: MENU0.PRG _getTeleportCost (src/MENU/MENU0.PRG/84.c:117).
int TeleportMpCostLive(int target_save_point) {
  LiveHost host;
  const std::optional<int> cost = TeleportMpCost(
      target_save_point, host.CurrentRoomId(), host.SavePoints());
  if (!cost) {
    host.Refuse("Magic teleport cost target exceeds the save-point table");
    return 0;
  }
  return *cost;
}

void ResetTeleportMenuLiveState() { g_teleport_state = {}; }
}  // namespace VS::Menu::Magic
