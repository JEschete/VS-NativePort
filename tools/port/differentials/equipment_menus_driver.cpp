// Native side of the equipment menu differentials (equipment_menus_common.py,
// #276). One vector per line: the mode, then the mode's inputs in the order the
// harness lists them. Prints a refusal marker, the result, the menu statics, the
// inventory bytes the menus write, the globals and title row, then the recorded
// host calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/Inventory/EquipmentMenus.hpp"

namespace {
namespace inv = VS::Menu::Inventory;
using VS::Menu::Framework::MenuRow;
using VS::Menu::Framework::RowPlacement;
using VS::Menu::Framework::RowState;

enum Mode : int {
    kArmorMode,
    kWeaponMode,
    kShieldMode,
    kAccessoryMode,
    kDispatchMode,
    kTopMode,
};

enum Call : std::int64_t {
    kCallPreview = 1,
    kCallPages,
    kCallBars,
    kCallPanel,
    kCallIdle,
    kCallCount,
    kCallFormat,
    kCallPopulate,
    kCallChoices,
    kCallSelected,
    kCallClear,
    kCallFlyOut,
    kCallUnequipWeapon,
    kCallUnequipShield,
    kCallEquipWeapon,
    kCallEquipShield,
    kCallEquipArmor,
    kCallEquipAccessory,
    kCallReload,
    kCallSetItem,
    kCallTopRows,
    kCallInvalid,
    kCallSelectedRow,
    kCallHighlightedRow,
    kCallStart,
    kCallContinue,
    kCallDispatch,
    kCallHandler,
};

constexpr std::int64_t kRefused = -999;
constexpr std::int64_t kMarker = 0x100000;
constexpr std::size_t kRows = 64;
constexpr std::size_t kTitleRow = 31;
constexpr std::size_t kFirstText = 12;
constexpr std::size_t kTexts = 10;
constexpr std::size_t kIdleCalls = 4;
constexpr std::uint32_t kSquare = 0x80;
constexpr std::int64_t kNoItem = -1;

using Calls = std::vector<std::vector<std::int64_t>>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read)) {
        return false;
    }
    value = static_cast<Value>(read);
    return true;
}

template <typename Array> bool ReadAll(Array& items)
{
    for (auto& item : items) {
        if (!Read(item)) {
            return false;
        }
    }
    return true;
}

struct Statics {
    std::int32_t selected = 0;
    std::uint8_t step = 0;
    std::array<std::uint8_t, 3> extra { };
};

class MenusHost final : public inv::EquipmentMenusHost {
public:
    Calls calls;
    std::int32_t count = 0;
    std::array<std::uint8_t, inv::kEquipmentWeaponSlots> weapon_list { };
    std::array<std::uint8_t, inv::kEquipmentShieldSlots> shield_list { };
    std::array<std::uint8_t, inv::kEquipmentArmorSlots> armor_list { };
    std::array<std::uint8_t, inv::kEquipmentWeaponSlots> weapon_equipped { };
    std::array<std::uint8_t, inv::kEquipmentWeaponSlots> weapon_blade { };
    std::array<std::uint8_t, inv::kEquipmentBladeSlots> blade_category { };
    std::array<std::uint8_t, inv::kEquipmentBladeSlots> blade_model { };
    std::array<std::uint8_t, inv::kEquipmentBladeSlots> blade_material { };
    std::array<std::uint8_t, inv::kEquipmentShieldSlots> shield_equipped { };
    std::array<std::uint8_t, inv::kEquipmentShieldSlots> shield_model { };
    std::array<std::uint8_t, inv::kEquipmentShieldSlots> shield_material { };
    std::array<std::uint8_t, inv::kEquipmentArmorSlots> armor_category { };
    std::array<std::uint8_t, inv::kEquipmentArmorSlots> armor_limb { };
    inv::HeldBlade held;
    std::array<std::uint8_t, inv::kEquipmentWeaponCategories> hands { };
    std::array<std::uint16_t, kTexts> texts { };
    std::uint8_t before = 0;
    std::uint8_t after = 0;
    bool choices_updated = false;
    std::int32_t choice_row = 0;
    std::array<std::int32_t, kIdleCalls> idle { };
    std::size_t idle_calls = 0;
    std::array<std::array<std::int32_t, inv::kEquipmentArmorSlots>, 3> flags { };
    std::int32_t busy = 0;
    std::int32_t top_row = 0;
    std::int32_t highlighted_row = 0;
    std::uint32_t buttons = 0;
    std::int32_t dispatch_result = 0;
    std::int32_t render_state = 0;
    std::uint8_t icons = 0;
    std::uint8_t status = 0;
    std::uint8_t icon_on_top = 0;
    std::array<MenuRow, kRows> rows { };
    std::array<std::array<std::int64_t, 2>, inv::kEquipmentChoiceRows> row_text { };

    std::int32_t CountItems(std::int32_t category) override
    {
        calls.push_back({ kCallCount, category, 0 });
        return count;
    }
    std::uint8_t ListedSlot(inv::EquipmentTable table, std::int32_t position) override
    {
        const auto at = static_cast<std::size_t>(position);
        switch (table) {
        case inv::EquipmentTable::kWeapons:
            return weapon_list.at(at);
        case inv::EquipmentTable::kShields:
            return shield_list.at(at);
        case inv::EquipmentTable::kArmor:
            return armor_list.at(at);
        }
        return 0;
    }
    bool IsEquipped(inv::EquipmentTable table, std::int32_t slot) override
    {
        const auto at = static_cast<std::size_t>(slot);
        return (table == inv::EquipmentTable::kWeapons ? weapon_equipped.at(at)
                                                       : shield_equipped.at(at))
            != 0;
    }
    void MarkEquipped(inv::EquipmentTable table, std::int32_t slot) override
    {
        const auto at = static_cast<std::size_t>(slot);
        (table == inv::EquipmentTable::kWeapons ? weapon_equipped.at(at)
                                                : shield_equipped.at(at)) = 1;
    }
    std::uint8_t ArmorLimb(std::int32_t slot) override
    {
        return armor_limb.at(static_cast<std::size_t>(slot));
    }
    std::uint8_t ArmorCategory(std::int32_t slot) override
    {
        return armor_category.at(static_cast<std::size_t>(slot));
    }
    void SetArmorLimb(std::int32_t slot, std::uint8_t limb) override
    {
        armor_limb.at(static_cast<std::size_t>(slot)) = limb;
    }
    std::uint8_t WeaponBlade(std::int32_t slot) override
    {
        return weapon_blade.at(static_cast<std::size_t>(slot));
    }
    inv::BladeModel Blade(std::int32_t slot) override
    {
        const auto at = static_cast<std::size_t>(slot);
        return { .category = blade_category.at(at),
            .model_id = blade_model.at(at),
            .material = blade_material.at(at) };
    }
    inv::ShieldModel Shield(std::int32_t slot) override
    {
        const auto at = static_cast<std::size_t>(slot);
        return { .model_id = shield_model.at(at), .material = shield_material.at(at) };
    }
    std::optional<inv::HeldBlade> HeldWeapon() override { return held; }
    bool IsTwoHanded(std::uint8_t category) override
    {
        return hands.at(static_cast<std::size_t>(category - 1)) != 0;
    }

    bool RowsIdle() override
    {
        calls.push_back({ kCallIdle });
        return idle_calls < kIdleCalls && idle.at(idle_calls++) != 0;
    }
    void SetRowText(
        std::int32_t row, std::uint16_t name, std::uint16_t description) override
    {
        row_text.at(static_cast<std::size_t>(row)) = { name, description };
    }
    std::uint16_t MenuTextEntry(std::size_t index) override
    {
        return index >= kFirstText && index < kFirstText + kTexts
                 ? texts.at(index - kFirstText)
                 : static_cast<std::uint16_t>(0xFFFF);
    }
    std::int32_t FormatRow(
        inv::EquipmentTable table, std::int32_t slot, std::int32_t row) override
    {
        const auto which = static_cast<std::int64_t>(table);
        calls.push_back({ kCallFormat, which, slot, row, row, row });
        const std::int64_t marker = kMarker + which * 0x100 + std::int64_t { slot } * 2;
        row_text.at(static_cast<std::size_t>(row)) = { marker, marker + 1 };
        return flags.at(static_cast<std::size_t>(which))
            .at(static_cast<std::size_t>(slot));
    }
    void CreateChoiceRows(std::span<const std::int32_t> row_flags) override
    {
        std::vector<std::int64_t> call { kCallPopulate,
            static_cast<std::int64_t>(row_flags.size()) };
        for (std::size_t row = 0; row < row_flags.size(); ++row) {
            call.push_back(row_flags[row]);
            call.push_back(row_text.at(row)[0]);
            call.push_back(row_text.at(row)[1]);
        }
        calls.push_back(call);
    }
    void UpdateChoices(std::int32_t has_sfx) override
    {
        calls.push_back({ kCallChoices, has_sfx });
        choices_updated = true;
    }
    std::int32_t SelectedChoice() override
    {
        calls.push_back({ kCallSelected });
        return choice_row;
    }
    std::uint8_t HighlightedItem() override { return choices_updated ? after : before; }

    void PreviewItem(inv::EquipmentMenu menu, std::int32_t item) override
    {
        calls.push_back({ kCallPreview, static_cast<std::int64_t>(menu), item });
    }
    void UpdateStatPages(std::int32_t command) override
    {
        calls.push_back({ kCallPages, command });
    }
    void UpdateDpPpBars(std::int32_t bar_flags) override
    {
        calls.push_back({ kCallBars, bar_flags });
    }
    void UpdatePrimaryStatPanel(std::int32_t animate) override
    {
        calls.push_back({ kCallPanel, animate });
    }
    void ClearMenuExcept(std::int32_t row) override
    {
        calls.push_back({ kCallClear, row });
    }
    void FlyOutRowLeft(std::int32_t row) override
    {
        calls.push_back({ kCallFlyOut, row });
    }

    void UnequipWeapon() override { calls.push_back({ kCallUnequipWeapon }); }
    void UnequipShield() override { calls.push_back({ kCallUnequipShield }); }
    void EquipWeapon(std::int32_t slot) override
    {
        calls.push_back({ kCallEquipWeapon, slot });
    }
    void EquipShield(std::int32_t slot) override
    {
        calls.push_back({ kCallEquipShield, slot });
    }
    void EquipArmor(std::int32_t part, std::optional<std::int32_t> slot) override
    {
        calls.push_back({ kCallEquipArmor, part, slot.value_or(kNoItem) });
    }
    void EquipAccessory(std::optional<std::int32_t> slot) override
    {
        calls.push_back({ kCallEquipAccessory, slot.value_or(kNoItem) });
    }
    bool ReloadModels(
        std::int32_t category, std::int32_t model_id, std::int32_t material) override
    {
        calls.push_back({ kCallReload, category, model_id, material });
        return busy != 0;
    }

    MenuRow& SetMenuItem(const RowPlacement& placement, std::uint16_t text) override
    {
        calls.push_back({ kCallSetItem, placement.row, placement.x, placement.y,
            placement.w, placement.background_width, text });
        return rows.at(static_cast<std::size_t>(placement.row));
    }
    void CreateTopRows(std::span<const std::int32_t, inv::kEquipmentTopRows> row_flags,
        std::int32_t row_info) override
    {
        std::vector<std::int64_t> call { kCallTopRows,
            static_cast<std::int64_t>(row_flags.size()), row_info };
        for (std::size_t row = 0; row < row_flags.size(); ++row) {
            call.push_back(row_text.at(row)[0]);
            call.push_back(row_text.at(row)[1]);
        }
        call.insert(call.end(), row_flags.begin(), row_flags.end());
        calls.push_back(call);
    }
    bool SquarePressed() override { return (buttons & kSquare) != 0; }
    void PlayInvalidSound() override { calls.push_back({ kCallInvalid }); }
    std::int32_t SelectedRow() override
    {
        calls.push_back({ kCallSelectedRow });
        return top_row;
    }
    std::int32_t HighlightedRow() override
    {
        calls.push_back({ kCallHighlightedRow });
        return highlighted_row;
    }
    void StartTransition(std::int32_t row) override
    {
        calls.push_back({ kCallStart, row });
    }
    void ContinueTransition(std::int32_t row) override
    {
        calls.push_back({ kCallContinue, row });
    }
    void SetPreviewState(std::int32_t state) override { render_state = state; }
    void SetCategoryIconsEnabled(std::uint8_t enabled) override { icons = enabled; }
    void SetStatusCommandState(std::uint8_t state) override { status = state; }
    void SetCategoryIconOnTop(std::int32_t icon) override
    {
        icon_on_top = static_cast<std::uint8_t>(icon);
    }
    std::int32_t Dispatch(std::int32_t row) override
    {
        calls.push_back({ kCallDispatch, row });
        return dispatch_result;
    }
};

bool ReadHost(MenusHost& host, Statics& statics, std::int32_t& initialize)
{
    MenuRow& title = host.rows.at(kTitleRow);
    std::uint8_t row_state = 0;
    if (!Read(initialize) || !Read(statics.selected) || !Read(statics.step)
        || !ReadAll(statics.extra) || !Read(host.count) || !ReadAll(host.weapon_list)
        || !ReadAll(host.shield_list) || !ReadAll(host.armor_list)
        || !ReadAll(host.weapon_equipped) || !ReadAll(host.weapon_blade)
        || !ReadAll(host.blade_category) || !ReadAll(host.blade_model)
        || !ReadAll(host.blade_material) || !ReadAll(host.shield_equipped)
        || !ReadAll(host.shield_model) || !ReadAll(host.shield_material)
        || !ReadAll(host.armor_category) || !ReadAll(host.armor_limb)
        || !Read(host.held.id) || !Read(host.held.category) || !ReadAll(host.hands)
        || !ReadAll(host.texts) || !Read(host.before) || !Read(host.after)
        || !Read(host.choice_row) || !ReadAll(host.idle)) {
        return false;
    }
    for (std::size_t table = 0; table < host.flags.size(); ++table) {
        const std::size_t size =
            table == 2 ? inv::kEquipmentArmorSlots : inv::kEquipmentWeaponSlots;
        for (std::size_t slot = 0; slot < size; ++slot) {
            if (!Read(host.flags.at(table).at(slot))) {
                return false;
            }
        }
    }
    if (!Read(host.busy) || !Read(host.top_row) || !Read(host.highlighted_row)
        || !Read(host.buttons) || !Read(host.dispatch_result) || !Read(host.render_state)
        || !Read(host.icons) || !Read(host.status) || !Read(host.icon_on_top)
        || !Read(row_state) || !Read(title.selected) || !Read(title.target0)) {
        return false;
    }
    title.state = static_cast<RowState>(row_state);
    return true;
}

void PrintCalls(const Calls& calls)
{
    std::cout << ' ' << calls.size();
    for (const auto& call : calls) {
        for (const std::int64_t word : call) {
            std::cout << ' ' << word;
        }
    }
    std::cout << '\n';
}

void PrintResult(const std::optional<std::int32_t>& result)
{
    std::cout << (result ? 0 : kRefused) << ' ' << result.value_or(0);
}

void PrintHost(const MenusHost& host)
{
    for (const std::uint8_t item : host.weapon_equipped) {
        std::cout << ' ' << static_cast<int>(item);
    }
    for (const std::uint8_t item : host.shield_equipped) {
        std::cout << ' ' << static_cast<int>(item);
    }
    for (const std::uint8_t item : host.armor_limb) {
        std::cout << ' ' << static_cast<int>(item);
    }
    const MenuRow& title = host.rows.at(kTitleRow);
    std::cout << ' ' << host.render_state << ' ' << static_cast<int>(host.icons) << ' '
              << static_cast<int>(host.status) << ' '
              << static_cast<int>(host.icon_on_top) << ' '
              << static_cast<int>(title.state) << ' ' << static_cast<int>(title.selected)
              << ' ' << title.target0;
    PrintCalls(host.calls);
}

template <typename... Bytes> void PrintStatics(std::int32_t selected, Bytes... bytes)
{
    std::cout << ' ' << selected;
    ((std::cout << ' ' << static_cast<int>(bytes)), ...);
}

int RunMenu(int mode)
{
    MenusHost host;
    Statics statics;
    std::int32_t initialize = 0;
    if (!ReadHost(host, statics, initialize)) {
        return 1;
    }
    const auto [first, second, third] = statics.extra;
    switch (mode) {
    case kAccessoryMode: {
        inv::AccessoryMenuState state { statics.selected, statics.step, first };
        PrintResult(inv::UpdateAccessoryMenu(state, host, initialize));
        PrintStatics(state.selected, state.step, state.equipped_accessory);
        break;
    }
    case kWeaponMode: {
        inv::WeaponMenuState state { statics.selected, statics.step, first, second,
            third };
        PrintResult(inv::UpdateWeaponMenu(state, host, initialize));
        PrintStatics(
            state.selected, state.step, state.category, state.model_id, state.material);
        break;
    }
    case kShieldMode: {
        inv::ShieldMenuState state { statics.selected, statics.step, first, second };
        PrintResult(inv::UpdateShieldMenu(state, host, initialize));
        PrintStatics(state.selected, state.step, state.model_id, state.material);
        break;
    }
    case kArmorMode: {
        inv::ArmorMenuState state { statics.selected, statics.step, first, second };
        PrintResult(inv::UpdateArmorMenu(state, host, initialize));
        PrintStatics(state.selected, state.step, state.armor_index, state.body_part);
        break;
    }
    default: {
        inv::EquipmentMenuState state { statics.selected, statics.step };
        PrintResult(inv::UpdateEquipmentMenu(state, host, initialize));
        PrintStatics(state.selected_row, state.step);
        break;
    }
    }
    PrintHost(host);
    return 0;
}

// The slot menus _dispatchEquipMenu's handler table holds, recording each call.
Calls* g_handler_calls = nullptr;
std::int32_t g_handler_result = 0;

template <inv::EquipmentMenu Menu> int RecordHandler(int argument)
{
    g_handler_calls->push_back(
        { kCallHandler, static_cast<std::int64_t>(Menu), argument });
    return g_handler_result;
}

constexpr inv::EquipmentMenuHandlers kHandlers {
    RecordHandler<inv::EquipmentMenu::kArmor>,
    RecordHandler<inv::EquipmentMenu::kWeapon>,
    RecordHandler<inv::EquipmentMenu::kShield>,
    RecordHandler<inv::EquipmentMenu::kAccessory>,
};

int RunDispatch()
{
    std::int32_t row = 0;
    std::uint8_t current = 0;
    if (!Read(row) || !Read(current) || !Read(g_handler_result)) {
        return 1;
    }
    Calls calls;
    g_handler_calls = &calls;
    PrintResult(inv::DispatchEquipmentMenu(current, kHandlers, row));
    std::cout << ' ' << static_cast<int>(current);
    PrintCalls(calls);
    g_handler_calls = nullptr;
    return 0;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int mode = 0;
    while (Read(mode)) {
        if ((mode == kDispatchMode ? RunDispatch() : RunMenu(mode)) != 0) {
            return 1;
        }
    }
    return 0;
}
