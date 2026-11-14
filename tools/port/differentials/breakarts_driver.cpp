#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Menu/BreakArts.hpp"

namespace {
namespace break_arts = VS::Menu::break_arts;

struct CostText {
    int index;
    std::string text;
    int x;
    int disabled;
};

struct Event {
    int code;
    std::vector<int> values;
};

class Host final : public break_arts::Host {
public:
    int action_id = 0;
    std::uint8_t cost = 0;
    std::uint32_t restrictions = 0;
    std::array<std::uint16_t, break_arts::kMenuStringHalfwords> menu_strings { };
    std::array<std::uint16_t, 32> battle_strings { };
    std::array<int, 12> learned { };
    std::array<int, 10> kills { };
    std::array<std::array<int, 4>, 15> requirements { };
    std::vector<CostText> cost_text;
    std::vector<std::pair<std::string, std::uint32_t>> drawings;
    std::vector<Event> events;
    std::uint16_t view_name_offset = 0x472;
    std::uint16_t header_index = 5;
    bool rows_idle = true;
    std::optional<int> equipped = 1;
    std::uint8_t current_menu = 7;

    void Refuse(std::string_view) override { }
    std::span<const std::uint16_t> MenuStrings() override { return menu_strings; }
    std::span<const std::uint16_t> BattleMenuStrings() override { return battle_strings; }
    std::uint16_t DaggerArtDescriptionIndex() const override { return 0; }
    std::uint16_t ViewArtsNameOffset() const override { return view_name_offset; }
    std::uint16_t ViewArtsDescriptionOffset() const override { return 0; }
    std::uint16_t BreakArtsHeaderIndex() const override { return header_index; }
    std::int32_t DaggerArtOneId() const override { return 184; }
    std::optional<std::uint8_t> ActionCost(int id) override
    {
        return id == action_id ? std::optional<std::uint8_t>(cost) : std::nullopt;
    }
    std::optional<std::uint32_t> ActionRestrictionFlags(int id) override
    {
        return id == action_id ? std::optional<std::uint32_t>(restrictions)
                               : std::nullopt;
    }
    std::optional<bool> ActionUnlocked(int) override { return false; }
    bool ActionNameAvailable(int) override { return false; }
    std::optional<int> EquippedWeaponCategory() override
    {
        events.push_back({ 5, { equipped.value_or(-1) } });
        return equipped;
    }
    std::optional<int> ArtsLearned(int category) override
    {
        if (category < 0 || category >= static_cast<int>(learned.size())) {
            return std::nullopt;
        }
        return learned[static_cast<std::size_t>(category)];
    }
    std::optional<int> CategoryKills(int category) override
    {
        if (category < 0 || category >= static_cast<int>(kills.size())) {
            return std::nullopt;
        }
        return kills[static_cast<std::size_t>(category)];
    }
    std::optional<int> PointRequirement(int category, int art) override
    {
        if (category < 0 || category >= static_cast<int>(requirements.size()) || art < 0
            || art >= static_cast<int>(requirements[0].size())) {
            return std::nullopt;
        }
        return requirements[static_cast<std::size_t>(category)]
                           [static_cast<std::size_t>(art)];
    }
    std::uint8_t CursorMemory() override { return 0; }
    void SetCursorMemory(std::uint8_t) override { }
    std::uint32_t ActionRestrictions() override { return 0; }
    bool ShortcutInvoked() override { return false; }
    bool RowsIdle() override
    {
        events.push_back({ 4, { rows_idle } });
        return rows_idle;
    }
    int SelectedRow() override { return -1; }
    int HighlightedRow() override { return 0; }
    std::span<const std::uint8_t> AnimationIndices() override { return { }; }
    std::span<const std::int8_t> RowAnimationSteps() override { return { }; }
    bool SetMenuItem(
        const break_arts::RowPlacement& placement, break_arts::TextRef text) override
    {
        events.push_back({ 1, { placement.row, placement.x, placement.y, placement.width,
                                  placement.background_width, static_cast<int>(text.kind),
                                  text.value, placement.icon } });
        return true;
    }
    bool CreateRows(int, std::span<const break_arts::Row>) override { return false; }
    void SetActionCostText(int index, std::string_view text, int x, int disabled) override
    {
        cost_text.push_back({ index, std::string(text), x, disabled });
    }
    void DrawCompactText(std::string_view text, std::uint32_t position) override
    {
        drawings.emplace_back(text, position);
    }
    void SetDisplayActionCost(int) override { }
    void ClearRowsExcept(int) override { }
    void InitializeInformationBox() override { events.push_back({ 3, { } }); }
    void SetCommandLabel(int command) override { events.push_back({ 2, { command } }); }
    void SetActionSelection(std::uint16_t action_id) override
    {
        events.push_back({ 6, { action_id } });
    }
    void SetExecuteAbilityType(std::uint8_t type) override
    {
        events.push_back({ 7, { type } });
    }
    std::uint8_t CurrentMenu() override
    {
        events.push_back({ 8, { current_menu } });
        return current_menu;
    }
    void SetCurrentMenu(std::uint8_t menu) override { events.push_back({ 9, { menu } }); }
    void SetReturnState(std::uint8_t state) override
    {
        events.push_back({ 10, { state } });
    }
    void SetSelectionKind(std::int16_t kind) override
    {
        events.push_back({ 11, { kind } });
    }
    void DismissInformationBox() override { events.push_back({ 12, { } }); }
};

void PrintEvents(const Host& host)
{
    std::printf("%zu", host.events.size());
    for (const Event& event : host.events) {
        std::printf(" %d %zu", event.code, event.values.size());
        for (int value : event.values) {
            std::printf(" %d", value);
        }
    }
}
} // namespace

int main()
{
    Host host;
    int operation = 0;
    while (std::cin >> operation) {
        if (operation == 0) {
            int action_id = 0;
            unsigned cost = 0;
            unsigned restrictions = 0;
            if (!(std::cin >> action_id >> cost >> restrictions))
                return 2;
            host.action_id = action_id;
            host.cost = static_cast<std::uint8_t>(cost);
            host.restrictions = restrictions;
            host.cost_text.clear();
            break_arts::SetHpCost(action_id, host);
            std::printf("%zu", host.cost_text.size());
            for (const CostText& item : host.cost_text) {
                std::printf(
                    " %d %d %d %zu", item.index, item.x, item.disabled, item.text.size());
                for (unsigned char ch : item.text) {
                    std::printf(" %u", static_cast<unsigned>(ch));
                }
            }
        } else if (operation == 1) {
            int x_offset = 0;
            int weapon_category = 0;
            int row = 0;
            int learned = 0;
            int kill_count = 0;
            int requirement = 0;
            if (!(std::cin >> x_offset >> weapon_category >> row >> learned >> kill_count
                    >> requirement)) {
                return 2;
            }
            const int category = weapon_category % 10;
            host.learned[static_cast<std::size_t>(category)] = learned;
            host.kills[static_cast<std::size_t>(category)] = kill_count;
            if (learned >= 0 && learned < 4) {
                host.requirements[static_cast<std::size_t>(category)]
                                 [static_cast<std::size_t>(learned)] = requirement;
            }
            host.drawings.clear();
            break_arts::DrawNextUnlockPoints(x_offset, weapon_category, row, host);
            std::printf("%zu", host.drawings.size());
            for (const auto& [text, position] : host.drawings) {
                std::printf(" %u %zu", position, text.size());
                for (unsigned char ch : text) {
                    std::printf(" %u", static_cast<unsigned>(ch));
                }
            }
        } else if (operation == 2) {
            int cursor_memory = 0;
            unsigned menu_offset = 0;
            if (!(std::cin >> cursor_memory >> menu_offset))
                return 2;
            host.events.clear();
            const int weapon_type = cursor_memory & 0xF;
            if (weapon_type < 1 || weapon_type > 10)
                return 2;
            host.menu_strings[static_cast<std::size_t>((weapon_type - 1) * 3)] =
                static_cast<std::uint16_t>(menu_offset);
            break_arts::State state { };
            break_arts::Context context { .state = state, .host = host };
            const int result = break_arts::UpdateArtList(cursor_memory, context);
            std::printf("%d %d %d %d %d %d ", result,
                static_cast<int>(state.art_list.step), state.art_list.weapon_type,
                state.art_list.force_cursor_memory, state.art_list.is_last_row,
                state.art_list.animation_index);
            PrintEvents(host);
        } else if (operation == 3) {
            int initialize = 0;
            if (!(std::cin >> initialize))
                return 2;
            host.events.clear();
            break_arts::State state { };
            break_arts::Context context { .state = state, .host = host };
            const int result = break_arts::UpdateWeaponCategories(initialize, context);
            std::printf("%d %d ", result, static_cast<int>(state.categories.step));
            PrintEvents(host);
        } else if (operation == 4) {
            unsigned header_index = 0;
            if (!(std::cin >> header_index))
                return 2;
            host.events.clear();
            host.header_index = static_cast<std::uint16_t>(header_index);
            break_arts::CreateHeader(host);
            PrintEvents(host);
        } else if (operation == 5) {
            unsigned initial_step = 0;
            int rows_idle = 0;
            int equipped = 0;
            unsigned current_menu = 0;
            if (!(std::cin >> initial_step >> rows_idle >> equipped >> current_menu)) {
                return 2;
            }
            host.events.clear();
            host.rows_idle = rows_idle != 0;
            host.equipped = equipped;
            host.current_menu = static_cast<std::uint8_t>(current_menu);
            break_arts::State state { };
            state.update_weapon_type = equipped;
            break_arts::Context context { .state = state, .host = host };
            std::uint8_t step = static_cast<std::uint8_t>(initial_step);
            const int result = break_arts::Update(context, step);
            std::printf("%d %u ", result, static_cast<unsigned>(step));
            PrintEvents(host);
        } else {
            return 2;
        }
        std::putchar('\n');
    }
}
