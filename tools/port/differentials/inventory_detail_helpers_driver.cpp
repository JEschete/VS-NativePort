#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "Input/Pad.hpp"
#include "Menu/Inventory/DetailsPolicies.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using Byte = std::uint8_t;
constexpr std::size_t kMemoryBytes = 0xF2;
constexpr std::size_t kRowBytes = 64;
constexpr std::size_t kRows = 40;
constexpr std::array<std::size_t, 8> kCapacities { 8, 16, 16, 8, 16, 48, 64, 2 };
constexpr char kBorrowedText[] = "borrowed";
enum Operation {
    kCount = 1,
    kInvalid,
    kClear,
    kRow,
    kContinue,
    kPages,
    kPrimary,
    kLeave,
    kDismiss,
    kBars
};

std::int64_t Read()
{
    std::int64_t value = 0;
    if (!(std::cin >> value))
        std::exit(2);
    return value;
}

template <std::size_t Size> void ReadBytes(std::array<Byte, Size>& bytes)
{
    for (auto& byte : bytes)
        byte = static_cast<Byte>(Read());
}

std::uint32_t Word(
    const std::array<Byte, kRowBytes>& bytes, std::size_t offset, std::size_t size)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < size; ++i)
        value |= std::uint32_t { bytes[offset + i] } << (i * 8);
    return value;
}

void Put(std::array<Byte, kRowBytes>& bytes, std::size_t offset, std::uint32_t value,
    std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i)
        bytes[offset + i] = static_cast<Byte>(value >> (i * 8));
}

struct RowImage {
    std::array<Byte, kRowBytes> original { };
    fw::MenuRow native { };

    void ReadInput()
    {
        ReadBytes(original);
        native = { .state = static_cast<fw::RowState>(original[0]),
            .w = original[1],
            .background_width = original[2],
            .animation_step = original[3],
            .gradient_state = original[4],
            .is_scrollable = original[5],
            .selected = original[6],
            .unselectable = original[7],
            .button_id = original[8],
            .row_icon = original[9],
            .font_color = original[10],
            .invert_gradient = original[11],
            .material = original[12],
            .outset_icon = original[13],
            .item_page = static_cast<std::uint16_t>(Word(original, 14, 2)),
            .count = static_cast<std::uint16_t>(Word(original, 16, 2)),
            .max = static_cast<std::uint16_t>(Word(original, 18, 2)),
            .x = std::bit_cast<std::int16_t>(
                static_cast<std::uint16_t>(Word(original, 20, 2))),
            .y = std::bit_cast<std::int16_t>(
                static_cast<std::uint16_t>(Word(original, 22, 2))),
            .target0 = std::bit_cast<std::int16_t>(
                static_cast<std::uint16_t>(Word(original, 24, 2))),
            .target1 = std::bit_cast<std::int16_t>(
                static_cast<std::uint16_t>(Word(original, 26, 2))),
            .sub_text = Word(original, 60, 4) != 0 ? kBorrowedText : nullptr };
        for (std::size_t i = 0; i < native.text.size(); ++i)
            native.text[i] = original[28 + i];
    }

    void Print() const
    {
        auto bytes = original;
        const std::array<Byte, 14> head { static_cast<Byte>(native.state), native.w,
            native.background_width, native.animation_step, native.gradient_state,
            native.is_scrollable, native.selected, native.unselectable, native.button_id,
            native.row_icon, native.font_color, native.invert_gradient, native.material,
            native.outset_icon };
        for (std::size_t i = 0; i < head.size(); ++i)
            bytes[i] = head[i];
        Put(bytes, 14, native.item_page, 2);
        Put(bytes, 16, native.count, 2);
        Put(bytes, 18, native.max, 2);
        Put(bytes, 20, static_cast<std::uint16_t>(native.x), 2);
        Put(bytes, 22, static_cast<std::uint16_t>(native.y), 2);
        Put(bytes, 24, static_cast<std::uint16_t>(native.target0), 2);
        Put(bytes, 26, static_cast<std::uint16_t>(native.target1), 2);
        for (std::size_t i = 0; i < native.text.size(); ++i)
            bytes[28 + i] = native.text[i];
        Put(bytes, 60, native.sub_text == nullptr ? 0 : Word(original, 60, 4), 4);
        for (const auto byte : bytes)
            std::cout << ' ' << static_cast<unsigned>(byte);
    }
};

class World final : public inventory::DetailsWorld {
public:
    int kind = 0;
    std::int32_t first = 0, second = 0, count = 0;
    std::uint32_t held = 0, repeat = 0, pressed = 0, after_repeat = 0, after_pressed = 0;
    int mode = 0, owner = 0, row_reads = 0;
    inventory::PanelVisibility panels { };
    std::int32_t preview = 0;
    Byte header = 0;
    std::int32_t exit_intent = 0;
    std::array<Byte, kMemoryBytes> memory { };
    std::array<Byte, 64> order { };
    std::array<std::array<RowImage, kRows>, 2> rows { };
    std::vector<std::array<std::int64_t, 9>> calls;

    void ReadInput(int operation)
    {
        kind = operation;
        first = static_cast<std::int32_t>(Read());
        second = static_cast<std::int32_t>(Read());
        count = static_cast<std::int32_t>(Read());
        held = static_cast<std::uint32_t>(Read());
        repeat = static_cast<std::uint32_t>(Read());
        pressed = static_cast<std::uint32_t>(Read());
        after_repeat = static_cast<std::uint32_t>(Read());
        after_pressed = static_cast<std::uint32_t>(Read());
        mode = static_cast<int>(Read());
        panels = { static_cast<Byte>(Read()), static_cast<Byte>(Read()),
            static_cast<Byte>(Read()) };
        preview = static_cast<std::int32_t>(Read());
        header = static_cast<Byte>(Read());
        exit_intent = static_cast<std::int32_t>(Read());
        if (kind == 4) {
            ReadBytes(memory);
            ReadBytes(order);
        }
        if (kind == 2) {
            for (auto& table : rows)
                for (auto& row : table)
                    row.ReadInput();
        }
    }

    void Record(int operation, std::int32_t argument = 0, std::int32_t extra = 0)
    {
        calls.push_back({ operation, argument, extra, panels.secondary_title,
            panels.category_icons, panels.status_command, preview, header, exit_intent });
    }
    std::uint32_t HeldButtons() override { return held; }
    std::uint32_t RepeatButtons() override { return repeat; }
    std::uint32_t PressedButtons() override { return pressed; }
    std::int32_t ItemCount() override
    {
        Record(kCount, first);
        repeat = after_repeat;
        pressed = after_pressed;
        return count;
    }
    void RememberPage(inventory::CursorMemory cursor) override
    {
        const auto at = 0x3C + static_cast<std::size_t>(first) * 2;
        memory.at(at) = cursor.row;
        memory.at(at + 1) = cursor.page;
    }
    std::span<const Byte> DisplayOrder() override
    {
        return { order.data(), kCapacities.at(static_cast<std::size_t>(first)) };
    }
    [[noreturn]] void Refuse(std::string_view reason) override
    {
        std::cerr << reason;
        std::exit(3);
    }
    void ClearRows() override
    {
        Record(kClear, 40);
        if (kind == 2 && mode == 1)
            owner = 1;
    }
    fw::MenuRow& Row(inventory::DetailRowId id) override
    {
        Record(kRow, id.index);
        auto& value = rows.at(static_cast<std::size_t>(owner))
                          .at(static_cast<std::size_t>(id.index))
                          .native;
        ++row_reads;
        if (mode == 2 && row_reads == 1)
            owner = 1;
        return value;
    }
    void SetPreviewLayout(inventory::PreviewLayout layout) override
    {
        preview = static_cast<int>(layout);
    }
    void SetCategoryVisibility(Byte visibility) override
    {
        panels.category_icons = visibility;
    }
    void SetCommandVisibility(Byte visibility) override
    {
        panels.status_command = visibility;
    }
    void ContinueTransition(inventory::DetailRowId id) override
    {
        Record(kContinue, id.index);
    }
    void ShowStatPages(std::int32_t pages) override { Record(kPages, pages); }
    void MovePrimaryStats(inventory::StatPanelMotion motion) override
    {
        Record(kPrimary, static_cast<int>(motion));
        if (mode == 1)
            pressed = after_pressed;
        else if (mode == 2) {
            panels.category_icons = 0x93;
            panels.status_command = 0xA7;
        }
    }
    void PlayLeaveSound() override { Record(kLeave); }
    void PlayInvalidSound() override { Record(kInvalid); }
    void DismissCommandRow() override { Record(kDismiss, -1); }
    void SetHeaderPlacement(inventory::CategoryHeaderPlacement placement) override
    {
        header = static_cast<Byte>(placement);
    }
    void SetBattleReturnIntent(inventory::BattleReturnIntent intent) override
    {
        exit_intent =
            intent == inventory::BattleReturnIntent::kBattle ? VS::Input::Triangle : 0;
    }
    void HideDpPpBars() override { Record(kBars, 4); }

    void Print(std::int32_t result) const
    {
        std::cout << result << ' ' << static_cast<unsigned>(panels.secondary_title) << ' '
                  << static_cast<unsigned>(panels.category_icons) << ' '
                  << static_cast<unsigned>(panels.status_command) << ' ' << preview << ' '
                  << static_cast<unsigned>(header) << ' ' << exit_intent;
        if (kind == 4)
            for (const auto byte : memory)
                std::cout << ' ' << static_cast<unsigned>(byte);
        if (kind == 2)
            for (const auto& table : rows)
                for (const auto& row : table)
                    row.Print();
        std::cout << ' ' << calls.size();
        for (const auto& call : calls)
            for (const auto value : call)
                std::cout << ' ' << value;
        std::cout << '\n';
    }
};

void Execute(World& world)
{
    std::int32_t result = 0;
    switch (world.kind) {
    case 0:
        inventory::SetPanelVisibility(world.panels, static_cast<Byte>(world.first));
        break;
    case 1:
        result = inventory::PollDetailsPaging(world, { world.second }).index;
        break;
    case 2:
        inventory::BeginDetailsTransition(world, { world.first });
        break;
    case 3:
        inventory::InitializeDetailsLayout(world, { world.first }, world.second);
        break;
    case 4:
        result = inventory::SelectDetailsItem(world, { world.second }).one_based;
        break;
    case 5:
        inventory::ExitDetailsView(world, world.first != 0);
        break;
    default:
        std::exit(4);
    }
    world.Print(result);
}
} // namespace

int main()
{
    int kind = 0;
    while (std::cin >> kind) {
        World world;
        world.ReadInput(kind);
        Execute(world);
    }
}
