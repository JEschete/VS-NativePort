#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "Menu/Score/GazetteStatistics.hpp"

namespace {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace render = VS::Render;
using Call = std::vector<std::int64_t>;
constexpr std::size_t kRows = 33;
constexpr std::size_t kMenuBytes = 60;
constexpr std::uint32_t kText = 0x80300000;
constexpr std::uint32_t kTextStride = 16;
constexpr std::uint32_t kPrimitives = 0x80303000;
constexpr std::size_t kGuardWords = 4;

template <typename Value> bool Read(Value& value)
{
    std::int64_t input = 0;
    if (!(std::cin >> input))
        return false;
    value = static_cast<Value>(input);
    return true;
}

template <typename Range> bool ReadAll(Range& values)
{
    for (auto& value : values) {
        if (!Read(value))
            return false;
    }
    return true;
}

void PrintCalls(const std::vector<Call>& calls)
{
    std::cout << ' ' << calls.size();
    for (const auto& call : calls) {
        for (const auto value : call)
            std::cout << ' ' << value;
    }
    std::cout << '\n';
}

void AppendText(Call& call, score::GazetteText text)
{
    const auto end = std::find(text.begin(), text.end(), std::uint8_t { 0xE7 });
    const std::size_t length = static_cast<std::size_t>(end - text.begin())
        + (end == text.end() ? 0U : 1U);
    call.push_back(static_cast<std::int64_t>(length));
    for (const auto byte : text.first(length))
        call.push_back(byte);
}

class Host final : public score::GazetteRecordHost {
public:
    std::array<std::array<std::uint8_t, 2>, 132> texts { };
    std::array<std::uint32_t, 132> addresses { };
    std::array<std::array<std::uint8_t, kMenuBytes>, 16> menu_responses { };
    std::array<std::int32_t, 8> measure_responses { };
    fw::MenuRow menu;
    std::int32_t cursor_reply = 0;
    std::vector<Call> calls;
    std::size_t created = 0;
    std::size_t measured = 0;

    Host()
    {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            texts[index] = { static_cast<std::uint8_t>(index), 0xE7 };
            addresses[index] = kText + static_cast<std::uint32_t>(index) * kTextStride;
        }
    }
    std::uint32_t Address(score::GazetteText text) const
    {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            if (text.data() == texts[index].data())
                return addresses[index];
        }
        return 0;
    }
    fw::MenuRow& Row() override
    {
        calls.push_back({ 1, 0 });
        return menu;
    }
    fw::MenuRow& CreateRow(
        const fw::RowPlacement& placement, score::GazetteText text) override
    {
        Call call { 2, placement.row, placement.x, placement.y, placement.w,
            placement.background_width };
        AppendText(call, text);
        calls.push_back(call);
        std::memcpy(&menu, menu_responses.at(created++).data(), kMenuBytes);
        return menu;
    }
    void SetInformation(score::GazetteText description) override
    {
        Call call { 3, Address(description) };
        AppendText(call, description);
        calls.push_back(call);
    }
    std::int32_t DrawCursor(std::uint8_t step, int x, int y) override
    {
        calls.push_back({ 4, step, fw::PackXy(x, y) });
        return cursor_reply;
    }
    void DrawNumber(const score::TitleNumber& number) override
    {
        calls.push_back({ 5, number.x, number.y, number.value, number.dimmed ? 1 : 0 });
    }
    void DrawRow(bool scrolling) override
    {
        Call call { scrolling ? 7 : 6 };
        for (const auto byte : std::as_bytes(std::span(&menu, 1)).first(kMenuBytes))
            call.push_back(std::to_integer<int>(byte));
        calls.push_back(call);
    }
    std::int32_t MeasureText(score::GazetteText text) override
    {
        Call call { 8 };
        AppendText(call, text);
        calls.push_back(call);
        return measure_responses.at(measured++);
    }
    void Refuse(std::string_view) override { calls.push_back({ 90 }); }
};

struct Input {
    std::int32_t step = 0;
    score::GazetteRecordCursor cursor;
    score::GazetteRecordAnimation animation;
    std::uint32_t titles = 0;
    std::array<score::GazetteRecordRow, kRows> rows { };
    score::GazetteStatisticsValues values;
};

bool ReadInput(Input& input, Host& host, bool statistics)
{
    if (!Read(input.step) || !Read(input.cursor.first_row)
        || !Read(input.cursor.selected_row) || !Read(input.animation.cursor_step)
        || !Read(host.cursor_reply) || !Read(input.titles)) {
        return false;
    }
    for (std::size_t index = 0; index < 66; ++index) {
        std::uint32_t offset = 0;
        if (!Read(offset))
            return false;
        std::uint32_t base = 0x801097DC;
        if (index < 32)
            base = statistics ? 0x80108FDC : 0x801086E4;
        else if (index < 64)
            base = statistics ? 0x80109200 : 0x801089BC;
        host.addresses[index] = base + offset * 2;
    }
    for (std::size_t index = 0; index < input.rows.size(); ++index) {
        auto& row = input.rows[index];
        if (!Read(row.selected) || !Read(row.unlocked) || !Read(row.highlight_step)
            || !Read(row.disabled) || !Read(row.x) || !Read(row.number)) {
            return false;
        }
        row.title = host.texts[66 + index];
        row.description = host.texts[99 + index];
    }
    for (auto& response : host.menu_responses) {
        if (!ReadAll(response))
            return false;
    }
    static_assert(offsetof(fw::MenuRow, text) == 28);
    std::memcpy(&host.menu, host.menu_responses[0].data(), kMenuBytes);
    auto& values = input.values;
    if (!ReadAll(host.measure_responses) || !Read(values.current.clear_count)
        || !Read(values.current.opened_chest_count) || !Read(values.current.map_completion)
        || !Read(values.best_clear_round) || !Read(values.best.completion_time_minutes)
        || !Read(values.best.opened_chest_count) || !Read(values.best.map_completion)
        || !Read(values.best.max_chain) || !ReadAll(values.best.enemy_kills)
        || !ReadAll(values.best.weapon_attacks)) {
        return false;
    }
    values.no_clear = host.texts[64];
    return true;
}

bool Rows(int mode)
{
    Host host;
    Input input;
    if (!ReadInput(input, host, mode >= 3))
        return false;
    bool accepted = false;
    if (mode == 0) {
        score::GazetteTitleLabels labels;
        for (std::size_t index = 0; index < 32; ++index) {
            labels.titles[index] = host.texts[index];
            labels.descriptions[index] = host.texts[index + 32];
        }
        labels.locked_title = host.texts[64];
        labels.locked_description = host.texts[65];
        accepted = score::CreateTitleRows(input.rows, input.titles, labels);
    } else if (mode == 1) {
        accepted = score::RenderTitles(
            input.rows, input.cursor, input.step, input.animation, host);
    } else if (mode == 3) {
        score::GazetteStatisticLabels labels;
        for (std::size_t index = 0; index < 23; ++index) {
            labels.titles[index] = host.texts[index];
            labels.descriptions[index] = host.texts[index + 32];
        }
        accepted = score::CreateStatisticRows(input.rows, labels);
    } else if (mode == 4) {
        accepted = score::RenderStatistics(
            input.rows, input.values, input.cursor, input.step, input.animation, host);
    }
    if (!accepted)
        host.calls.push_back({ 91 });
    std::cout << static_cast<int>(input.animation.cursor_step);
    for (const auto& row : input.rows) {
        std::cout << ' ' << static_cast<int>(row.selected) << ' '
                  << static_cast<int>(row.unlocked) << ' '
                  << static_cast<int>(row.highlight_step) << ' '
                  << static_cast<int>(row.disabled) << ' '
                  << static_cast<std::uint16_t>(row.x) << ' '
                  << static_cast<std::uint16_t>(row.number) << ' '
                  << host.Address(row.title) << ' ' << host.Address(row.description);
    }
    for (const auto byte : std::as_bytes(std::span(&host.menu, 1)).first(kMenuBytes))
        std::cout << ' ' << std::to_integer<int>(byte);
    PrintCalls(host.calls);
    return true;
}

class NumberTarget final : public score::TitleNumberTarget {
public:
    std::array<std::uint32_t, 28> words { };
    std::uint32_t ordering = 0;
    std::size_t cursor = kGuardWords;
    bool DrawQuad(const render::UiTexturedQuad& quad) override
    {
        render::UiTexturedQuadPacket previous { };
        std::copy_n(words.begin() + static_cast<std::ptrdiff_t>(cursor),
            previous.size(), previous.begin());
        const auto packet = render::BuildUiTexturedQuadPacket(quad, previous, ordering);
        std::copy(packet.begin(), packet.end(),
            words.begin() + static_cast<std::ptrdiff_t>(cursor));
        ordering = (ordering & 0xFF000000U)
            | ((kPrimitives + static_cast<std::uint32_t>(cursor) * 4U) & 0xFFFFFFU);
        cursor += packet.size();
        return true;
    }
};

bool Number()
{
    score::TitleNumber number;
    int dimmed = 0;
    NumberTarget target;
    if (!Read(number.x) || !Read(number.y) || !Read(number.value) || !Read(dimmed)
        || !Read(target.ordering) || !ReadAll(target.words)) {
        return false;
    }
    number.dimmed = dimmed != 0;
    if (!score::DrawTitleNumber(number, target))
        return false;
    std::cout << kPrimitives + static_cast<std::uint32_t>(target.cursor) * 4U
              << ' ' << target.ordering;
    for (const auto word : target.words)
        std::cout << ' ' << word;
    std::cout << " 0\n";
    return true;
}
} // namespace

int main()
{
    int mode = 0;
    while (Read(mode)) {
        if (!(mode == 2 ? Number() : Rows(mode)))
            return 2;
    }
}
