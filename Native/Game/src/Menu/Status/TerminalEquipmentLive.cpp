#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/EquipmentStats.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Status/ComponentDescriptionsLive.hpp"
#include "Menu/Status/DetailTransitions.hpp"
#include "Menu/Status/DetailTransitionsLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status::terminal_live {
namespace {
constexpr int kWeapon = 0;
constexpr int kShield = 1;
constexpr int kFirstArmor = 2;
constexpr std::int32_t kEquipmentListStyle = 0x19142;

[[nodiscard]] std::span<const std::uint8_t> ByteView(const void* data,
                                                     std::size_t count) {
  return {static_cast<const std::uint8_t*>(data), count};
}
[[nodiscard]] std::span<const std::uint8_t> Slice(
    std::span<const std::uint8_t> bank, const void* pointer) {
  const auto start = reinterpret_cast<std::uintptr_t>(bank.data());
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  if (pointer == nullptr || address < start || address - start >= bank.size()) {
    return {};
  }
  return Encoded(bank.subspan(address - start));
}
[[nodiscard]] int LimbRows(const Battle_ActorStats& actor) {
  std::array<std::uint8_t, kStatusLimbSlots> names{};
  for (std::size_t index = 0; index < names.size(); ++index) {
    names[index] = actor.limbs[index].nameIndex;
  }
  return LimbCount(names);
}
[[nodiscard]] std::span<const std::uint8_t> Resolve(
    Framework::TextRef ref, std::span<std::uint8_t> buffer,
    const Battle_ActorStats& actor, Framework::LiveItemRowsHost& host) {
  if (ref.source == Framework::TextSource::kBuffer) {
    return ref.offset < buffer.size() ? Encoded(buffer.subspan(ref.offset))
                                      : std::span<const std::uint8_t>{};
  }
  if (ref.source == Framework::TextSource::kWeaponName) {
    return Encoded(ByteView(actor.weapon.name, sizeof(actor.weapon.name)));
  }
  return Encoded(host.TextBytes(ref));
}

class EquipmentHost final : public HostBase<EquipmentTerminalHost> {
 public:
  std::span<Framework::MenuRow> Rows() override {
    return Workspace()
               ? std::span(&Framework::LiveMenuRow(0), Framework::kRowCount)
               : std::span<Framework::MenuRow>{};
  }
  bool HideHud() override {
    Invoke([] { Battle::SetLiveHudHidden(1); });
    return true;
  }
  bool HoistRow(int row) override {
    if (!Workspace()) {
      return false;
    }
    Invoke([row] { HoistLiveEquipmentRow(row); });
    return true;
  }
  bool InitializeDetail(int row, int pages) override {
    constexpr int kWeaponTitle = 9;
    constexpr int kArmorTitle = 11;
    if (!Workspace() || terminal_live::StatusText(
                            row < 2 ? row + kWeaponTitle : kArmorTitle, true)
                            .empty()) {
      return false;
    }
    Invoke([=] { InitializeLiveDetailPanel(row, pages); });
    return true;
  }
  bool DpPpBars(int command) override {
    Invoke([command] { Framework::UpdateLiveDpPpBars(command); });
    return true;
  }
  bool EquipmentStats(int row) override {
    auto* actor = Selected();
    if (actor == nullptr || !ValidRow(*actor, row)) {
      return false;
    }
    Invoke([=] {
      if (row == kWeapon) {
        Framework::PopulateLiveWeaponStats(&actor->weapon);
      } else if (row == kShield) {
        Framework::PopulateLiveShieldStats(&actor->shield);
      } else if (row - kFirstArmor < LimbRows(*actor)) {
        Framework::PopulateLiveArmorStats(
            &actor->limbs[row - kFirstArmor].armor);
      } else {
        Framework::PopulateLiveAccessoryStats(&actor->accessory);
      }
    });
    return true;
  }
  bool ComponentRow(int equipment, int part, bool slide) override {
    auto* actor = Selected();
    if (actor == nullptr || !ItemText() || !Workspace()) {
      return false;
    }
    constexpr int kWeaponParts = 5;
    if (equipment < 0 || equipment > kShield || part < 1 ||
        part > kWeaponParts) {
      return false;
    }
    const bool absent =
        equipment == kWeapon
            ? part >= 3 && part - 3 >= static_cast<std::uint8_t>(
                                           actor->weapon.grip.gemSlots)
            : part > static_cast<std::uint8_t>(actor->shield.base.gemSlots);
    auto* row = Invoke([=] {
      Framework::LiveItemRowsHost items;
      return equipment == kWeapon
                 ? SetWeaponComponentRow(
                       items, part, Items::Live::Typed(actor->weapon), slide)
                 : SetShieldGemRow(items, part,
                                   Items::Live::Typed(actor->shield), slide);
    });
    return row != nullptr || absent;
  }
  bool Deactivate(int row) override {
    if (!Workspace()) {
      return false;
    }
    Invoke([row] { Framework::DeactivateLiveRow(row); });
    return true;
  }
  std::span<std::uint8_t> DescriptionBuffer() override {
    if (!Workspace()) {
      return {};
    }
    return {reinterpret_cast<std::uint8_t*>(Menu_Framework_stringBuf),
            Framework::kStringBufferBytes};
  }
  std::optional<TerminalRowText> FormatRow(
      int row, std::span<std::uint8_t> buffer) override {
    auto* actor = Selected();
    if (actor == nullptr || !ValidRow(*actor, row) || !ItemText() ||
        buffer.size() < Framework::kRowDescriptionBufferBytes) {
      return std::nullopt;
    }
    Framework::LiveItemRowsHost items;
    const auto text =
        Invoke([&] { return Format(*actor, row, buffer, items); });
    if (!text.name || !text.description) {
      return std::nullopt;
    }
    auto name =
        row == kWeapon
            ? Encoded(ByteView(actor->weapon.name, sizeof(actor->weapon.name)))
            : Resolve(*text.name, buffer, *actor, items);
    auto description = Resolve(*text.description, buffer, *actor, items);
    if (name.empty() || description.empty()) {
      return std::nullopt;
    }
    return TerminalRowText{name, description, text.row_type};
  }
  Framework::MenuRow* CreateRow(const Framework::RowPlacement& placement,
                                std::span<const std::uint8_t> text) override {
    if (!Workspace() || placement.row < 0 ||
        static_cast<std::size_t>(placement.row) >= Framework::kRowCount ||
        Encoded(text).empty()) {
      return nullptr;
    }
    return Invoke(
        [&] { return &Framework::SetLiveMenuItemText(placement, text); });
  }
  bool CreateList(std::span<const TerminalRowText> rows,
                  std::span<const std::int32_t> flags) override {
    constexpr std::size_t kMaxEquipmentRows = 9;
    if (!Workspace() || rows.empty() || rows.size() > kMaxEquipmentRows ||
        rows.size() != flags.size()) {
      return false;
    }
    std::array<char*, kMaxEquipmentRows * 2> strings{};
    std::array<int, kMaxEquipmentRows> types{};
    for (std::size_t index = 0; index < rows.size(); ++index) {
      if (Encoded(rows[index].name).empty() ||
          Encoded(rows[index].description).empty()) {
        return false;
      }
      // The framework copies these synchronous borrows into its owned list.
      strings[index * 2] = reinterpret_cast<char*>(
          const_cast<std::uint8_t*>(rows[index].name.data()));
      strings[index * 2 + 1] = reinterpret_cast<char*>(
          const_cast<std::uint8_t*>(rows[index].description.data()));
      types[index] = flags[index];
    }
    Invoke([&] {
      Framework::CreateLiveMenuRows(static_cast<int>(rows.size()),
                                    kEquipmentListStyle, strings.data(),
                                    types.data());
    });
    return Menu_Framework_itemsText != nullptr;
  }
  std::optional<int> NavigateDetail(std::uint8_t selected,
                                    std::uint32_t flags) override {
    Framework::StatusViewNavigation links{};
    for (std::size_t row = 0; row < links.size(); ++row) {
      std::ranges::copy(Menu_Framework_statusViewNavigationMap[row],
                        links[row].begin());
    }
    return NavigateDetailElements(
        {selected, flags, frame.repeat, frame.stat_page, links});
  }
  std::span<const std::uint8_t> DescribeComponent(int equipment,
                                                  int component) override {
    auto* actor = Selected();
    if (actor == nullptr || !ValidRow(*actor, equipment) || !Workspace() ||
        !ItemText()) {
      return {};
    }
    const char* text = Invoke([=] {
      if (equipment == kWeapon) {
        return SelectLiveWeaponComponent(component, &actor->weapon);
      }
      if (equipment == kShield) {
        return SelectLiveShieldComponent(component, &actor->shield);
      }
      if (equipment - kFirstArmor < LimbRows(*actor)) {
        return LiveArmorDescription(
            &actor->limbs[equipment - kFirstArmor].armor);
      }
      return LiveAccessoryDescription(&actor->accessory);
    });
    if (text == nullptr) {
      return {};
    }
    const auto buffer = DescriptionBuffer();
    auto view = Slice(buffer, text);
    if (view.empty()) {
      view = Slice(ByteView(Menu_Framework_itemHelp, Framework::kItemHelpBytes),
                   text);
    }
    if (view.empty()) {
      Refuse("component returned text outside its current owners");
    }
    return view;
  }
  std::span<const std::uint8_t> HelpText(int index) override {
    if (!ItemText()) {
      return {};
    }
    Framework::LiveItemRowsHost items;
    const auto ref = Framework::ItemHelpEntry(items.HelpText(), index);
    return ref ? Encoded(items.TextBytes(*ref))
               : std::span<const std::uint8_t>{};
  }
  bool Tabs() override {
    if (!RenderSpace()) {
      return false;
    }
    Invoke([] { Framework::DrawLiveTabNavigation(1); });
    return true;
  }
  bool ExitDetail() override {
    if (!Workspace()) {
      return false;
    }
    Invoke([] { ExitLiveEquipmentDetails(1); });
    return true;
  }
  bool ComparisonStats(int actor) override {
    constexpr int kComparedActorBase = 16;
    if (Stats(actor - kComparedActorBase) == nullptr) {
      return false;
    }
    Invoke([actor] { Framework::SetLiveActorComparisonStats(actor); });
    return true;
  }
  std::optional<int> SelectedRow() override {
    if (!Workspace()) {
      return std::nullopt;
    }
    return Invoke([] { return Framework::LiveSelectedRow(); });
  }
  bool StoreCursor() override {
    if (!Workspace()) {
      return false;
    }
    Invoke(
        [] { static_cast<void>(Framework::CloseLiveRowsAndRememberCursor()); });
    return true;
  }

 private:
  Battle_ActorStats* Selected() { return Stats(frame.selected_actor - 1); }
  static bool ValidRow(const Battle_ActorStats& actor, int row) {
    const int count = LimbRows(actor) + 2 + (actor.accessory.accessory.id != 0);
    return row >= 0 && row < count;
  }
  static Framework::ItemRowText Format(const Battle_ActorStats& actor, int row,
                                       std::span<std::uint8_t> buffer,
                                       Framework::LiveItemRowsHost& items) {
    if (row == kWeapon) {
      return Framework::FormatWeaponRow(items, Items::Live::Typed(actor.weapon),
                                        buffer);
    }
    if (row == kShield) {
      return Framework::FormatShieldRow(items, Items::Live::Typed(actor.shield),
                                        buffer);
    }
    if (row - kFirstArmor < LimbRows(actor)) {
      return Framework::FormatArmorRow(
          items, Items::Live::Typed(actor.limbs[row - kFirstArmor].armor),
          buffer);
    }
    Items::InventoryArmor accessory{};
    Items::StoreInventoryAccessoryStats(accessory,
                                        Items::Live::Typed(actor.accessory));
    return Framework::FormatInventoryArmorRow(items, accessory, buffer);
  }
};
}  // namespace
}  // namespace VS::Menu::Status::terminal_live

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
// decomp: _equipmentDetailScreen (src/MENU/MENU4.PRG/120.c:1482)
int LiveUpdateEquipmentDetails(int row) {
  if (!terminal_live::Workspace()) {
    return 0;
  }
  terminal_live::EquipmentHost host;
  return terminal_live::Report(
      "equipment details",
      UpdateEquipmentDetails(terminal_live::detail_state, host, row));
}
// decomp: _equipmentScreen (src/MENU/MENU4.PRG/120.c:1875)
int LiveUpdateEquipmentList(int element) {
  if (!terminal_live::Workspace()) {
    return 0;
  }
  terminal_live::EquipmentHost host;
  return terminal_live::Report(
      "equipment list",
      UpdateEquipmentList(terminal_live::list_state,
                          terminal_live::detail_state, host, element));
}
}  // namespace VS::Menu::Status
