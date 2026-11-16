// Native driver for the MENU0 MP cost helper differentials (magic_costs_*.py).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Menu/MagicCosts.hpp"

namespace {
namespace magic = VS::Menu::Magic;

enum Event : int {
    kFlyOut = 1,
    kSetMenuRows = 2,
    kClearMenuExcept = 3,
    kDismissInformationBox = 4,
    kSetMenuCommand = 5,
    kSetActionCost = 6,
    kInitializeInformationBox = 7,
    kSetMenuItem = 8,
};

class Host final : public magic::Host {
public:
    std::uint8_t cost = 0;
    std::uint32_t flags = 0;
    int room_id = 0;
    magic::SavePointTable save_points = magic::OriginalSavePoints();
    std::array<std::uint8_t, magic::kSavePointCount> states { };
    std::int16_t character_mp = 0;
    std::vector<std::uint16_t> teleportation_strings;
    bool rows_idle = true;
    std::uint32_t restrictions = 0;
    std::uint8_t cursor_row = 0;
    std::uint8_t cursor_scroll = 0;
    std::uint8_t cursor_setting = 0;
    int display_action_cost = 0;
    int selected_row = 0;
    int highlighted_row = 0;
    std::vector<std::uint16_t> battle_strings;
    int item_state = 0;
    int item_target = 0;
    int item_selected = 0;
    std::vector<std::vector<int>> events;
    int refusals = 0;

    void Refuse(std::string_view) override { ++refusals; }
    std::optional<std::uint8_t> ActionCost(int) override { return cost; }
    std::optional<std::uint32_t> ActionRestrictionFlags(int) override { return flags; }
    void SetActionCostText(int index, std::string_view text, int x, int disabled) override
    {
        std::vector<int> event { kSetActionCost, index, x, disabled, static_cast<int>(text.size()) };
        for (const char ch : text) {
            event.push_back(static_cast<unsigned char>(ch));
        }
        events.push_back(std::move(event));
    }
    int CurrentRoomId() override { return room_id; }
    const magic::SavePointTable& SavePoints() override { return save_points; }
    std::span<const std::uint8_t> SavePointStates() override { return states; }
    std::optional<std::int16_t> CurrentMp() override { return character_mp; }
    std::span<const std::uint16_t> TeleportationStrings() override { return teleportation_strings; }
    std::uint16_t LocationsIndex() const override { return 3; }
    std::uint16_t DisabledDescriptionOffset() const override { return 144; }
    void FlyOutRowsAndHoistSelection(int row, int offset) override
    {
        events.push_back({ kFlyOut, row, offset });
    }
    bool RowsIdle() override { return rows_idle; }
    std::uint32_t ActionRestrictions() override { return restrictions; }
    void SetTeleportCursorMemory(std::uint8_t row, std::uint8_t scroll) override
    {
        cursor_row = row;
        cursor_scroll = scroll;
    }
    std::uint8_t CursorMemorySetting() override { return cursor_setting; }
    void SetCursorMemorySetting(std::uint8_t value) override { cursor_setting = value; }
    void CreateRows(int row_info, std::span<const magic::TeleportRow> rows) override
    {
        std::vector<int> event { kSetMenuRows, static_cast<int>(rows.size()), row_info,
            cursor_setting };
        for (const magic::TeleportRow& row : rows) {
            event.push_back(row.name_offset);
            event.push_back(row.description_offset);
            event.push_back(row.type);
        }
        events.push_back(std::move(event));
    }
    int SelectedRow() override { return selected_row; }
    int HighlightedRow() override { return highlighted_row; }
    void SetDisplayActionCost(int enabled) override { display_action_cost = enabled; }
    void ClearRowsExcept(int row) override { events.push_back({ kClearMenuExcept, row }); }
    void DismissInformationBox() override { events.push_back({ kDismissInformationBox }); }
    void SetCommandLabel(int command) override { events.push_back({ kSetMenuCommand, command }); }
    std::span<const std::uint16_t> BattleMenuStrings() override { return battle_strings; }
    std::uint16_t MagicHeaderIndex() const override { return 0; }
    void SetMenuItem(const magic::MenuItem& item) override
    {
        events.push_back({ kSetMenuItem, item.row, item.x, item.y, item.width,
            item.background_width, item.text_offset });
        item_state = item.state;
        item_target = item.target_position;
        item_selected = item.selected;
    }
    void InitializeInformationBox() override { events.push_back({ kInitializeInformationBox }); }
};

void PrintEvents(const Host& host)
{
    std::printf(" %zu", host.events.size());
    for (const std::vector<int>& event : host.events) {
        std::printf(" %zu", event.size());
        for (const int value : event) {
            std::printf(" %d", value);
        }
    }
}

template <typename T> bool Read(T& value)
{
    long long raw = 0;
    if (!(std::cin >> raw)) {
        return false;
    }
    value = static_cast<T>(raw);
    return true;
}

bool ReadTeleportMenu(Host& host, int& init, magic::TeleportMenuState& state)
{
    int step = 0;
    if (!Read(init) || !Read(step) || !Read(state.selected_row) || !Read(state.current_mp)) {
        return false;
    }
    state.step = static_cast<magic::TeleportStep>(step);
    for (std::uint8_t& save_point : state.save_points) {
        if (!Read(save_point)) {
            return false;
        }
    }
    for (std::uint16_t& cost : state.mp_costs) {
        if (!Read(cost)) {
            return false;
        }
    }
    for (std::uint8_t& value : host.states) {
        if (!Read(value)) {
            return false;
        }
    }
    int rows_idle = 0;
    std::size_t strings = 0;
    if (!Read(host.room_id) || !Read(host.character_mp) || !Read(host.restrictions)
        || !Read(host.cursor_setting) || !Read(host.cursor_row) || !Read(host.cursor_scroll)
        || !Read(host.display_action_cost) || !Read(rows_idle) || !Read(host.selected_row)
        || !Read(host.highlighted_row) || !Read(strings)) {
        return false;
    }
    host.rows_idle = rows_idle != 0;
    host.teleportation_strings.resize(strings);
    for (std::uint16_t& value : host.teleportation_strings) {
        if (!Read(value)) {
            return false;
        }
    }
    return true;
}
} // namespace

int main()
{
    int operation = 0;
    while (std::cin >> operation) {
        Host host;
        if (operation == 1) {
            int action_id = 0;
            if (!Read(action_id) || !Read(host.cost) || !Read(host.flags)) {
                return 2;
            }
            magic::SetSpellMpCost(action_id, host);
            std::printf("%d", host.refusals);
            PrintEvents(host);
        } else if (operation == 2) {
            int cost = 0;
            int disabled = 0;
            if (!Read(cost) || !Read(disabled)) {
                return 2;
            }
            magic::SetTeleportMpCost(cost, disabled, host);
            std::printf("%d", host.refusals);
            PrintEvents(host);
        } else if (operation == 3) {
            int target = 0;
            int room_id = 0;
            if (!Read(target) || !Read(room_id)) {
                return 2;
            }
            magic::SavePointTable table { };
            for (magic::SavePoint& save_point : table) {
                std::uint32_t word = 0;
                if (!Read(word)) {
                    return 2;
                }
                save_point = magic::DecodeSavePoint(word);
            }
            const std::optional<int> cost = magic::TeleportMpCost(target, room_id, table);
            std::printf("%d %d", cost.has_value() ? 1 : 0, cost.value_or(0));
        } else if (operation == 4) {
            int init = 0;
            magic::TeleportMenuState state;
            if (!ReadTeleportMenu(host, init, state)) {
                return 2;
            }
            const magic::TeleportMenuResult result = magic::UpdateTeleportMenu(init, state, host);
            const std::int32_t value = result.destination.has_value()
                ? magic::PackTeleportDestination(*result.destination)
                : result.selection;
            std::printf("%d %d %d %d %d", host.refusals, value, static_cast<int>(state.step),
                state.selected_row, state.current_mp);
            for (const std::uint8_t save_point : state.save_points) {
                std::printf(" %u", static_cast<unsigned>(save_point));
            }
            for (const std::uint16_t cost : state.mp_costs) {
                std::printf(" %u", static_cast<unsigned>(cost));
            }
            std::printf(" %u %u %u %d", static_cast<unsigned>(host.cursor_row),
                static_cast<unsigned>(host.cursor_scroll),
                static_cast<unsigned>(host.cursor_setting), host.display_action_cost);
            PrintEvents(host);
        } else if (operation == 5) {
            std::uint16_t offset = 0;
            std::size_t strings = 0;
            if (!Read(offset) || !Read(strings)) {
                return 2;
            }
            host.battle_strings.assign(strings, 0);
            if (!host.battle_strings.empty()) {
                host.battle_strings[0] = offset;
            }
            magic::CreateHeader(host);
            std::printf("%d %d %d %d", host.refusals, host.item_state, host.item_target,
                host.item_selected);
            PrintEvents(host);
        } else {
            return 2;
        }
        std::printf("\n");
    }
    return 0;
}
