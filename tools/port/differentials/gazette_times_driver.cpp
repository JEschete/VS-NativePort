#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "Menu/Score/GazetteTimes.hpp"

namespace {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
using Call = std::vector<std::int64_t>;
constexpr std::size_t kRows = 9;
constexpr std::size_t kMenuBytes = 60;
constexpr std::uint32_t kText = 0x80300000;
constexpr std::uint32_t kTextStride = 32;
constexpr std::uint32_t kOrdering = 0x8030401C;
constexpr std::uint32_t kTitles = 0x80109598;
constexpr std::uint32_t kDescriptions = 0x8010960C;
constexpr std::uint32_t kMisc = 0x801097DC;

template <typename Value> bool Read(Value& value)
{
    std::int64_t input = 0;
    if (!(std::cin >> input))
        return false;
    value = static_cast<Value>(input);
    return true;
}

void PrintCalls(const std::vector<Call>& calls)
{
    std::cout << ' ' << calls.size();
    for (const Call& call : calls) {
        for (const std::int64_t value : call)
            std::cout << ' ' << value;
    }
    std::cout << '\n';
}

bool Format()
{
    score::FastestClearTime value;
    std::size_t length = 0;
    if (!Read(value.clear_count) || !Read(value.round_index)
        || !Read(value.completion_minutes) || !Read(length)) {
        return false;
    }
    std::vector<std::uint8_t> placeholder(length);
    for (auto& byte : placeholder) {
        if (!Read(byte))
            return false;
    }
    std::array<std::uint8_t, 64> output { };
    output.fill(0xA5);
    static_cast<void>(score::FormatFastestClearTime(output, value, placeholder));
    for (const auto byte : output)
        std::cout << static_cast<int>(byte) << ' ';
    std::cout << "0\n";
    return true;
}

class Host final : public score::TimeTrialHost {
public:
    std::array<std::array<std::uint8_t, 2>, 36> texts { };
    std::array<std::uint32_t, 36> addresses { };
    std::array<fw::MenuRow, score::kTimeTrialCount> menu { };
    std::int32_t cursor_reply = 0;
    std::vector<Call> calls;

    Host()
    {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            texts[index] = { static_cast<std::uint8_t>(index), 0xE7 };
            addresses[index] = kText + static_cast<std::uint32_t>(index) * kTextStride;
        }
    }
    std::uint32_t Address(score::TimeTrialText text) const
    {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            if (text.data() == texts[index].data())
                return addresses[index];
        }
        return 0;
    }
    fw::MenuRow& Row(score::TimeTrialId trial) override
    {
        calls.push_back({ 1, static_cast<int>(trial) });
        return menu.at(static_cast<std::size_t>(trial));
    }
    fw::MenuRow& CreateRow(
        const fw::RowPlacement& placement, score::TimeTrialText title) override
    {
        calls.push_back({ 2, placement.row, placement.x, placement.y, placement.w,
            placement.background_width, Address(title) });
        return menu.at(static_cast<std::size_t>(placement.row));
    }
    void SetInformation(score::TimeTrialText description) override
    {
        calls.push_back({ 3, Address(description) });
    }
    std::int32_t DrawCursor(std::uint8_t step, int x, int y) override
    {
        calls.push_back({ 4, step, fw::PackXy(x, y) });
        return cursor_reply;
    }
    void DrawText(std::string_view text, int x, int y, std::uint32_t color) override
    {
        Call call { 5, fw::PackXy(x, y), color, kOrdering,
            static_cast<std::int64_t>(text.size()) };
        for (const char byte : text)
            call.push_back(static_cast<unsigned char>(byte));
        calls.push_back(call);
    }
    void DrawPanel(const score::PanelRect& rect, int gradient) override
    {
        calls.push_back({ 6, rect.x, rect.y, rect.width, rect.height, gradient });
    }
    void Refuse(std::string_view) override { calls.push_back({ 90 }); }
};

struct Input {
    std::int32_t step = 0;
    score::TimeTrialId selected { };
    score::TimeTrialAnimation animation;
    std::array<score::TimeTrialRow, kRows> rows { };
    score::TimeTrialScores scores { };
    score::TimeTrialLabels labels;
};

bool ReadInput(Input& input, Host& host)
{
    if (!Read(input.step) || !Read(input.selected) || !Read(input.animation.cursor_step)
        || !Read(host.cursor_reply)) {
        return false;
    }
    for (std::size_t index = 0; index < 18; ++index) {
        std::uint32_t offset = 0;
        if (!Read(offset))
            return false;
        const std::uint32_t base =
            index < 8 ? kTitles : (index < 16 ? kDescriptions : kMisc);
        host.addresses[index < 16 ? index : index + 18] = base + offset * 2;
    }
    for (std::size_t index = 0; index < input.rows.size(); ++index) {
        auto& row = input.rows[index];
        if (!Read(row.selected) || !Read(row.enabled_style) || !Read(row.entrance_delay)
            || !Read(row.disabled) || !Read(row.x) || !Read(row.row_number)) {
            return false;
        }
        row.title = host.texts[16 + index];
        row.description = host.texts[25 + index];
    }
    static_assert(offsetof(fw::MenuRow, text) == 28);
    for (auto& row : host.menu) {
        std::array<std::uint8_t, kMenuBytes> bytes { };
        for (auto& byte : bytes) {
            if (!Read(byte))
                return false;
        }
        std::memcpy(&row, bytes.data(), bytes.size());
    }
    for (auto& trial : input.scores) {
        for (auto& record : trial) {
            std::uint32_t word = 0;
            if (!Read(word))
                return false;
            if ((word & 0xFFFFFFU) != 0x800000U) {
                record.time =
                    score::TimeTrialTime { static_cast<std::uint8_t>(word >> 16),
                        static_cast<std::uint8_t>(word >> 8),
                        static_cast<std::uint8_t>(word) };
            }
            record.round_index = static_cast<std::uint8_t>((word >> 24) & 0x7F);
        }
    }
    for (std::size_t index = 0; index < score::kTimeTrialCount; ++index) {
        input.labels.titles[index] = host.texts[index];
        input.labels.descriptions[index] = host.texts[8 + index];
    }
    input.labels.missing_title = host.texts[34];
    input.labels.missing_description = host.texts[35];
    return true;
}

bool Rows(int mode)
{
    Host host;
    Input input;
    if (!ReadInput(input, host))
        return false;
    bool accepted = false;
    switch (mode) {
    case 1:
        accepted = score::CreateTimeTrialRows(input.rows, input.scores, input.labels);
        break;
    case 2:
        accepted = score::LeaveTimeTrialMenu(input.rows, host);
        break;
    case 3:
        accepted = score::RenderTimeTrialRecords(
            input.rows, input.scores, input.selected, input.step, input.animation, host);
        break;
    default:
        return false;
    }
    if (!accepted)
        host.calls.push_back({ 91 });
    std::cout << static_cast<int>(input.animation.cursor_step);
    for (const auto& row : input.rows) {
        std::cout << ' ' << static_cast<int>(row.selected) << ' '
                  << static_cast<int>(row.enabled_style) << ' '
                  << static_cast<int>(row.entrance_delay) << ' '
                  << static_cast<int>(row.disabled) << ' '
                  << static_cast<std::uint16_t>(row.x) << ' '
                  << static_cast<std::uint16_t>(row.row_number) << ' '
                  << host.Address(row.title) << ' ' << host.Address(row.description);
    }
    for (const auto& row : host.menu) {
        for (const auto byte : std::as_bytes(std::span(&row, 1)).first(kMenuBytes)) {
            std::cout << ' ' << std::to_integer<int>(byte);
        }
    }
    PrintCalls(host.calls);
    return true;
}
} // namespace

int main()
{
    int mode = 0;
    while (Read(mode)) {
        if (!(mode == 0 ? Format() : Rows(mode)))
            return 2;
    }
}
