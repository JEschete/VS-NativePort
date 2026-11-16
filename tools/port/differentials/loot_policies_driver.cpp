// Native side of the loot screen policy differentials
// (loot_policies_common.py, #276). One vector per line: the mode, then the
// mode's inputs in the order the harness lists them. Prints a refusal marker,
// the observed globals and fixture fields, then the recorded host calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/Framework/Framework.hpp"
#include "Menu/Loot/LootPolicies.hpp"

namespace {
namespace loot = VS::Menu::Loot;
using VS::Items::InventoryContents;
using VS::Items::ItemCategory;
using VS::Items::ItemsStatus;
using VS::Menu::Framework::MenuRow;
using VS::Menu::Framework::RowPlacement;
using VS::Menu::Framework::RowState;

enum Mode : int {
    kCapacities,
    kRows,
    kDescription,
    kLeave,
    kOrganize,
};

enum Call : std::int64_t {
    kCallLabel = 1,
    kCallColored,
    kCallCount,
    kCallContained,
    kCallInteger,
    kCallBackground,
    kCallButton,
    kCallGetRow = 10,
    kCallSetRow,
    kCallCopy = 20,
    kCallFormat,
    kCallInfo,
    kCallInitText = 30,
    kCallInitBox,
    kCallIdle,
    kCallFlyOut,
    kCallDismiss,
    kCallLeave,
    kCallChange,
    kCallSelect,
    kCallSfx,
    kCallCursor,
};

constexpr std::int64_t kRefused = -999;
constexpr std::size_t kListSlots = 64;
constexpr std::size_t kInventoryBytes = 0xF00;
constexpr std::size_t kNameBytes = 24;
constexpr std::size_t kNameCount = 512;
constexpr std::int64_t kNamesTextId = 0x10000;
constexpr std::size_t kMenuRows = 40;
constexpr std::size_t kCategories = 7;
constexpr std::array<std::int64_t, kCategories> kOffsets { 0x000, 0x280, 0x540, 0x100,
    0x640, 0x8C0, 0xE00 };
constexpr std::array<std::int64_t, kCategories> kStrides { 0x20, 0x2C, 0x10, 0x30, 0x28,
    0x1C, 0x04 };
constexpr std::int64_t kInventoryMain = 0;
constexpr std::int64_t kInventoryBackup = 1;
constexpr std::int64_t kInventoryStaged = 2;
constexpr std::int64_t kDescriptionStride = 0x100;
constexpr std::int64_t kSfxChannel = 0x7E;
constexpr std::int64_t kSfxDiscard = 0x1C;

using Calls = std::vector<std::vector<std::int64_t>>;
using LootList = std::array<loot::LootRow, kListSlots>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadList(LootList& rows)
{
    for (auto& row : rows) {
        if (!Read(row.category) || !Read(row.item_index) || !Read(row.slide_step)
            || !Read(row.state)) {
            return false;
        }
    }
    return true;
}

bool ReadInventory(InventoryContents& inventory)
{
    static_assert(sizeof(InventoryContents) == kInventoryBytes);
    std::array<std::uint8_t, kInventoryBytes> bytes { };
    for (std::size_t at = 0; at < bytes.size(); at += 4) {
        std::uint32_t word = 0;
        if (!Read(word))
            return false;
        for (std::size_t i = 0; i < 4; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(word >> (i * 8));
        }
    }
    std::memcpy(&inventory, bytes.data(), bytes.size());
    return true;
}

bool ReadRows(std::array<MenuRow, kMenuRows>& rows, int first, int count)
{
    for (int i = 0; i < count; ++i) {
        MenuRow& row = rows.at(static_cast<std::size_t>(first + i));
        int state = 0;
        if (!Read(state) || !Read(row.background_width) || !Read(row.gradient_state)
            || !Read(row.is_scrollable) || !Read(row.selected) || !Read(row.unselectable)
            || !Read(row.row_icon) || !Read(row.font_color) || !Read(row.material)
            || !Read(row.count) || !Read(row.x) || !Read(row.target0)) {
            return false;
        }
        row.state = static_cast<RowState>(state);
    }
    return true;
}

void PrintRows(const std::array<MenuRow, kMenuRows>& rows, int first, int count)
{
    for (int i = 0; i < count; ++i) {
        const MenuRow& row = rows.at(static_cast<std::size_t>(first + i));
        std::cout << ' ' << static_cast<int>(row.state) << ' '
                  << static_cast<int>(row.background_width) << ' '
                  << static_cast<int>(row.gradient_state) << ' '
                  << static_cast<int>(row.is_scrollable) << ' '
                  << static_cast<int>(row.selected) << ' '
                  << static_cast<int>(row.unselectable) << ' '
                  << static_cast<int>(row.row_icon) << ' '
                  << static_cast<int>(row.font_color) << ' '
                  << static_cast<int>(row.material) << ' ' << row.count << ' ' << row.x
                  << ' ' << row.target0;
    }
}

void PrintCalls(const Calls& calls)
{
    std::cout << ' ' << calls.size();
    for (const auto& call : calls) {
        for (const std::int64_t word : call)
            std::cout << ' ' << word;
    }
    std::cout << '\n';
}

class CapacityRecorder final : public loot::CapacityHost {
public:
    Calls calls;
    std::array<std::int32_t, kCategories> counts { };
    std::array<std::int32_t, kCategories> contained { };
    const loot::LootRow* first_row = nullptr;

    void DrawLabel(loot::CapacityLabel label, std::uint32_t xy) override
    {
        calls.push_back({ kCallLabel, static_cast<std::int64_t>(label), 0, xy });
    }
    void DrawColoredLabel(loot::CapacityLabel label, std::uint8_t category,
        std::uint32_t xy, std::uint32_t color) override
    {
        const std::int64_t shown = label == loot::CapacityLabel::kCategory ? category : 0;
        calls.push_back(
            { kCallColored, static_cast<std::int64_t>(label), shown, xy, color });
    }
    std::int32_t CountItems(std::uint8_t category) override
    {
        calls.push_back({ kCallCount, category });
        return counts.at(category);
    }
    std::int32_t CountContained(std::uint8_t category, const loot::LootRow& row) override
    {
        calls.push_back({ kCallContained, category, &row - first_row });
        return contained.at(category);
    }
    void DrawInteger(std::int32_t value, std::uint32_t xy, std::uint32_t color) override
    {
        calls.push_back({ kCallInteger, value, xy, color });
    }
    void DrawButtonBackground(
        std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h) override
    {
        calls.push_back({ kCallBackground, x, y, w, h });
    }
    void DrawButton(loot::PromptButton button, std::int32_t x, std::int32_t y) override
    {
        calls.push_back({ kCallButton, static_cast<std::int64_t>(button), x, y });
    }
};

class RowsRecorder final : public loot::LootRowsHost {
public:
    Calls calls;
    std::array<MenuRow, kMenuRows> rows { };
    const InventoryContents* inventory = nullptr;
    std::vector<std::uint8_t> names = std::vector<std::uint8_t>(kNameBytes * kNameCount);

    [[nodiscard]] std::int64_t TextId(const std::uint8_t* text) const
    {
        const auto* base = reinterpret_cast<const std::uint8_t*>(inventory);
        if (text >= base && text < base + kInventoryBytes)
            return text - base;
        if (text >= names.data() && text < names.data() + names.size()) {
            return kNamesTextId + (text - names.data());
        }
        return -1;
    }

    MenuRow& Row(int row) override
    {
        calls.push_back({ kCallGetRow, row });
        return rows.at(static_cast<std::size_t>(row));
    }
    MenuRow& SetRow(
        const RowPlacement& placement, std::span<const std::uint8_t> text) override
    {
        calls.push_back({ kCallSetRow, placement.row, placement.x, placement.y,
            placement.w, placement.background_width, TextId(text.data()) });
        return rows.at(static_cast<std::size_t>(placement.row));
    }
    std::span<const std::uint8_t> ItemName(std::uint16_t id) override
    {
        return std::span<const std::uint8_t>(names).subspan(id * kNameBytes, kNameBytes);
    }
};

class DescriptionRecorder final : public loot::DescriptionHost {
public:
    Calls calls;
    std::array<std::uint8_t, kDescriptionStride * kCategories> text { };

    void BackupInventory() override
    {
        calls.push_back({ kCallCopy, kInventoryBackup, kInventoryMain,
            static_cast<std::int64_t>(kInventoryBytes) });
    }
    void LoadStagedInventory() override
    {
        calls.push_back({ kCallCopy, kInventoryMain, kInventoryStaged,
            static_cast<std::int64_t>(kInventoryBytes) });
    }
    std::span<const std::uint8_t> FormatDescription(
        ItemCategory category, std::uint8_t index) override
    {
        const auto at = static_cast<std::size_t>(category);
        calls.push_back({ kCallFormat, static_cast<std::int64_t>(at),
            kOffsets.at(at) + (index * kStrides.at(at)) });
        return std::span<const std::uint8_t>(text).subspan(
            (at * kDescriptionStride) + index, 1);
    }
    void ShowInformation(std::span<const std::uint8_t> message) override
    {
        calls.push_back({ kCallInfo, message.data() - text.data() });
    }
    void RestoreInventory() override
    {
        calls.push_back({ kCallCopy, kInventoryMain, kInventoryBackup,
            static_cast<std::int64_t>(kInventoryBytes) });
    }
};

class PromptRecorder final : public loot::PromptHost {
public:
    Calls calls;
    std::array<MenuRow, kMenuRows> rows { };
    bool idle = false;
    std::uint8_t cursor_return = 0;
    std::int64_t string_item = -1;

    MenuRow& SetRow(const RowPlacement& placement, loot::PromptText text) override
    {
        calls.push_back({ kCallSetRow, placement.row, placement.x, placement.y,
            placement.w, placement.background_width, static_cast<std::int64_t>(text) });
        return rows.at(static_cast<std::size_t>(placement.row));
    }
    MenuRow& Row(int row) override
    {
        calls.push_back({ kCallGetRow, row });
        return rows.at(static_cast<std::size_t>(row));
    }
    void InitInformationTextBox() override { calls.push_back({ kCallInitText, 1 }); }
    void InitInformationBox() override { calls.push_back({ kCallInitBox }); }
    void ShowInformation(loot::PromptText text) override
    {
        calls.push_back({ kCallInfo, static_cast<std::int64_t>(text) });
    }
    bool RowsIdle() override
    {
        calls.push_back({ kCallIdle });
        return idle;
    }
    void FlyOutRight(int row) override { calls.push_back({ kCallFlyOut, row }); }
    void DismissInformationBox() override { calls.push_back({ kCallDismiss }); }
    void PlayLeaveSound() override { calls.push_back({ kCallLeave }); }
    void PlayChangeSound() override { calls.push_back({ kCallChange }); }
    void PlaySelectSound() override { calls.push_back({ kCallSelect }); }
    void PlayDiscardSound() override
    {
        calls.push_back({ kCallSfx, kSfxChannel, kSfxDiscard });
    }
    std::uint8_t DrawCursor(std::uint8_t step, int position) override
    {
        calls.push_back({ kCallCursor, step, position });
        return cursor_return;
    }
    void SetStringItem(std::uint16_t item_id) override
    {
        string_item = static_cast<std::int64_t>(item_id) * kNameBytes;
    }
};

int RunCapacities()
{
    std::int32_t edge = 0;
    loot::CapacityPanel panel;
    std::array<std::uint8_t, kCategories> capacities { };
    CapacityRecorder host;
    LootList rows { };
    if (!Read(edge) || !Read(panel.previewed) || !Read(panel.controls_enabled)
        || !Read(panel.cursor.count) || !Read(panel.cursor.offset)) {
        return 1;
    }
    for (auto& item : capacities) {
        if (!Read(item))
            return 1;
    }
    for (auto& item : host.counts) {
        if (!Read(item))
            return 1;
    }
    for (auto& item : host.contained) {
        if (!Read(item))
            return 1;
    }
    if (!ReadList(rows))
        return 1;
    panel.rows = rows;
    panel.capacities = capacities;
    host.first_row = rows.data();
    const bool drawn = loot::RenderBagCapacities(edge, panel, host);
    std::cout << (drawn ? 0 : kRefused);
    PrintCalls(host.calls);
    return 0;
}

int RunRows()
{
    std::int32_t x = 0;
    LootList rows { };
    InventoryContents inventory { };
    loot::LootRowsView view;
    RowsRecorder host;
    if (!Read(x) || !Read(view.cursor.count) || !Read(view.cursor.offset)
        || !Read(view.gradient_state_index) || !ReadList(rows)
        || !ReadInventory(inventory)
        || !ReadRows(host.rows, loot::kFirstLootMenuRow, loot::kVisibleLootRows)) {
        return 1;
    }
    view.rows = rows;
    host.inventory = &inventory;
    const ItemsStatus status = loot::RenderLootRows(x, view, inventory, host);
    std::cout << (status == ItemsStatus::kOk ? 0 : kRefused) << ' '
              << static_cast<int>(view.gradient_state_index);
    for (const auto& row : rows) {
        std::cout << ' ' << static_cast<int>(row.slide_step);
    }
    PrintRows(host.rows, loot::kFirstLootMenuRow, loot::kVisibleLootRows);
    PrintCalls(host.calls);
    return 0;
}

int RunDescription()
{
    std::size_t index = 0;
    LootList rows { };
    if (!Read(index) || !ReadList(rows))
        return 1;
    DescriptionRecorder host;
    const ItemsStatus status = loot::ShowLootDescription(rows.at(index), host);
    std::cout << (status == ItemsStatus::kOk ? 0 : kRefused);
    PrintCalls(host.calls);
    return 0;
}

int RunLeaveInit()
{
    loot::LeaveConfirmation prompt;
    loot::LeaveContext context;
    LootList rows { };
    InventoryContents inventory { };
    int claimed = 0;
    int entry = 0;
    if (!Read(context.count) || !Read(claimed) || !Read(entry) || !Read(prompt.state)
        || !Read(prompt.choice) || !Read(prompt.kept_item) || !ReadList(rows)
        || !ReadInventory(inventory)) {
        return 1;
    }
    context.rows = rows;
    context.claimed = claimed != 0;
    context.single_entry = entry != 0;
    const ItemsStatus status = loot::InitLeaveConfirmation(prompt, context, inventory);
    std::cout << (status == ItemsStatus::kOk ? 0 : kRefused) << " 0 "
              << static_cast<int>(prompt.state) << ' ' << static_cast<int>(prompt.choice)
              << ' ' << static_cast<std::int64_t>(prompt.kept_item) * kNameBytes;
    PrintCalls({ });
    return 0;
}

int RunLeaveUpdate()
{
    loot::LeaveConfirmation prompt;
    loot::PromptInput input;
    PromptRecorder host;
    int claimed = 0;
    int information = 0;
    int ready = 0;
    if (!Read(prompt.state) || !Read(prompt.choice) || !Read(prompt.kept_item)
        || !Read(claimed) || !Read(input.pressed) || !Read(input.repeat)
        || !Read(information) || !Read(ready) || !Read(prompt.cursor_step)
        || !Read(host.cursor_return) || !ReadRows(host.rows, loot::kFirstPromptRow, 2)) {
        return 1;
    }
    input.information_shown = information != 0;
    host.idle = ready != 0;
    const auto result = loot::UpdateLeaveConfirmation(prompt, claimed != 0, input, host);
    std::cout << (result ? 0 : kRefused) << ' ' << result.value_or(0) << ' '
              << static_cast<int>(prompt.state) << ' ' << static_cast<int>(prompt.choice)
              << ' ' << static_cast<int>(prompt.cursor_step) << ' ' << host.string_item;
    PrintRows(host.rows, loot::kFirstPromptRow, 2);
    PrintCalls(host.calls);
    return 0;
}

int RunOrganize()
{
    int init = 0;
    loot::LootRow row;
    loot::OrganizeMenu menu;
    loot::PromptInput input;
    InventoryContents inventory { };
    PromptRecorder host;
    int ready = 0;
    if (!Read(init))
        return 1;
    if (init != 0 && (!Read(row.category) || !Read(row.item_index)))
        return 1;
    if (!Read(menu.state) || !Read(menu.category) || !Read(menu.selected_row)
        || !Read(menu.action) || !Read(menu.cursor_step)) {
        return 1;
    }
    if (init != 0) {
        if (!ReadInventory(inventory))
            return 1;
    } else if (!Read(input.pressed) || !Read(input.repeat) || !Read(ready)
               || !Read(host.cursor_return)) {
        return 1;
    }
    if (!ReadRows(host.rows, loot::kFirstPromptRow, 2))
        return 1;
    host.idle = ready != 0;
    std::optional<std::int32_t> result = 0;
    if (init != 0) {
        if (loot::InitOrganizeMenu(menu, row, inventory, host) != ItemsStatus::kOk) {
            result = std::nullopt;
        }
    } else {
        result = loot::UpdateOrganizeMenu(menu, input, host);
    }
    std::cout << (result ? 0 : kRefused) << ' ' << result.value_or(0) << ' '
              << static_cast<int>(menu.state) << ' ' << static_cast<int>(menu.category)
              << ' ' << static_cast<int>(menu.selected_row) << ' '
              << static_cast<int>(menu.action) << ' '
              << static_cast<int>(menu.cursor_step);
    PrintRows(host.rows, loot::kFirstPromptRow, 2);
    PrintCalls(host.calls);
    return 0;
}

int RunVector(int mode)
{
    switch (mode) {
    case kCapacities:
        return RunCapacities();
    case kRows:
        return RunRows();
    case kDescription:
        return RunDescription();
    case kLeave: {
        int init = 0;
        if (!Read(init))
            return 1;
        return init != 0 ? RunLeaveInit() : RunLeaveUpdate();
    }
    case kOrganize:
        return RunOrganize();
    default:
        return 1;
    }
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int mode = 0;
    while (Read(mode)) {
        if (RunVector(mode) != 0)
            return 1;
    }
    return 0;
}
