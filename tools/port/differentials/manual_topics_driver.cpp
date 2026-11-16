// Native driver for the MENUE manual topic and page text differentials (manual_topics_*.py).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/ManualTopics.hpp"

namespace {
namespace manual = VS::Menu::manual;

// _menuText's offset in MENUE.PRG and its halfword count.
constexpr std::size_t kTopicTableOffset = 0x2654;
constexpr std::size_t kTopicTableHalfwords = 516;

template <typename T> bool Read(T& value)
{
    long long raw = 0;
    if (!(std::cin >> raw)) {
        return false;
    }
    value = static_cast<T>(raw);
    return true;
}

template <typename T> bool ReadList(std::vector<T>& values)
{
    std::size_t count = 0;
    if (!Read(count)) {
        return false;
    }
    values.resize(count);
    for (T& value : values) {
        if (!Read(value)) {
            return false;
        }
    }
    return true;
}

template <typename T> void PrintList(const std::vector<T>& values)
{
    std::printf(" %zu", values.size());
    for (const T& value : values) {
        std::printf(" %lld", static_cast<long long>(value));
    }
}

class Host final : public manual::TopicMenuHost {
public:
    std::uint8_t row = 0;
    std::uint8_t scroll = 0;
    std::uint8_t setting = 0;
    std::uint8_t setting_while_creating = 0;
    int creations = 0;
    std::int32_t created_count = 0;
    std::int32_t created_info = 0;
    std::vector<std::uint32_t> offsets;
    std::vector<std::int32_t> types;
    std::int32_t next_row = -1;
    std::vector<std::int32_t> cleared;
    std::vector<std::int32_t> fly_outs;

    void SetTopicCursor(manual::TopicCursor cursor) override
    {
        row = cursor.row;
        scroll = cursor.scroll;
    }
    [[nodiscard]] std::uint8_t TopicCursorRow() const override { return row; }
    [[nodiscard]] std::uint8_t CursorMemorySetting() const override { return setting; }
    void SetCursorMemorySetting(std::uint8_t value) override { setting = value; }
    void CreateRows(std::int32_t row_count, std::int32_t row_info, std::span<const std::uint32_t> text_offsets,
        std::span<const std::int32_t> row_types) override
    {
        ++creations;
        setting_while_creating = setting;
        created_count = row_count;
        created_info = row_info;
        offsets.assign(text_offsets.begin(), text_offsets.end());
        types.assign(row_types.begin(), row_types.end());
    }
    [[nodiscard]] std::int32_t SelectedRow() override { return next_row; }
    void ClearMenuExcept(std::int32_t kept) override { cleared.push_back(kept); }
    void FlyOutAndHoistSelection(std::int32_t hoisted, std::int32_t offset) override
    {
        fly_outs.push_back(hoisted);
        fly_outs.push_back(offset);
    }

    // The cursor bytes, the setting and the created rows.
    void PrintRows() const
    {
        std::printf(" %u %u %d %u %u %d %d", unsigned { row }, unsigned { scroll }, creations,
            unsigned { setting_while_creating }, unsigned { setting }, created_count, created_info);
        PrintList(offsets);
        PrintList(types);
    }
    void PrintSelection() const
    {
        PrintList(cleared);
        PrintList(fly_outs);
    }
};

template <typename E> int Status(const E& result)
{
    return result.has_value() ? 0 : 1 + static_cast<int>(result.error());
}

std::vector<std::uint16_t> LoadTopicTable(const char* module)
{
    std::vector<std::uint16_t> table(kTopicTableHalfwords);
    std::ifstream file(module, std::ios::binary);
    file.seekg(static_cast<std::streamoff>(kTopicTableOffset));
    std::array<char, 2> bytes {};
    for (std::uint16_t& value : table) {
        if (!file.read(bytes.data(), bytes.size())) {
            return {};
        }
        value = static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[0])
            | (static_cast<std::uint8_t>(bytes[1]) << 8U));
    }
    return table;
}

bool SetRows()
{
    std::int32_t count = 0;
    std::int32_t highlighted = 0;
    Host host;
    std::vector<std::uint32_t> offsets;
    if (!Read(count) || !Read(highlighted) || !Read(host.setting) || !Read(host.row) || !Read(host.scroll)
        || !ReadList(offsets)) {
        return false;
    }
    std::printf("%d", Status(manual::SetTopicRows(host, count, highlighted, offsets)));
    host.PrintRows();
    return true;
}

bool SelectRow()
{
    Host host;
    if (!Read(host.next_row) || !Read(host.row)) {
        return false;
    }
    std::printf("%d", manual::GetSelectedTopicRow(host));
    host.PrintSelection();
    return true;
}

bool UpdateMenu(const std::vector<std::uint16_t>& image_table)
{
    std::int32_t init = 0;
    manual::TopicMenuState state;
    std::int32_t current = 0;
    Host host;
    std::vector<std::int32_t> patches;
    if (!Read(init) || !Read(state.step) || !Read(state.selected_row) || !Read(current) || !Read(host.next_row)
        || !Read(host.row) || !Read(host.scroll) || !Read(host.setting) || !ReadList(patches)) {
        return false;
    }
    std::vector<std::uint16_t> table = image_table;
    for (std::size_t index = 0; index + 1 < patches.size(); index += 2) {
        table.at(static_cast<std::size_t>(patches[index])) = static_cast<std::uint16_t>(patches[index + 1]);
    }
    const auto result = manual::UpdateTopicMenu(state, init != 0, table, current, host);
    std::printf("%d %d %d %d", Status(result), result.has_value() ? *result : 0, state.step, state.selected_row);
    host.PrintRows();
    host.PrintSelection();
    return true;
}

bool CopyLine()
{
    std::size_t capacity = 0;
    std::vector<std::uint8_t> source;
    if (!Read(capacity) || !ReadList(source)) {
        return false;
    }
    std::vector<std::uint8_t> destination(capacity);
    const auto written = manual::CopyEncodedLine(source, destination);
    std::printf("%d", Status(written));
    if (written.has_value()) {
        destination.resize(*written);
        PrintList(destination);
    }
    return true;
}

bool ComposePage()
{
    std::int32_t scroll = 0;
    std::vector<std::uint8_t> help;
    if (!Read(scroll) || !ReadList(help)) {
        return false;
    }
    std::array<std::uint8_t, manual::kPageTextCapacity> page {};
    const auto length = manual::ComposePageText(help, scroll, page);
    std::printf("%d", Status(length));
    if (length.has_value()) {
        const std::size_t shown = std::max<std::size_t>(*length, manual::kPageHeaderBytes + 1);
        PrintList(std::vector<std::uint8_t>(page.begin(), page.begin() + static_cast<std::ptrdiff_t>(shown)));
    }
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        return 2;
    }
    const std::vector<std::uint16_t> table = LoadTopicTable(argv[1]);
    if (table.empty()) {
        return 2;
    }
    int operation = 0;
    while (std::cin >> operation) {
        bool read = false;
        if (operation == 1) {
            read = SetRows();
        } else if (operation == 2) {
            read = SelectRow();
        } else if (operation == 3) {
            read = UpdateMenu(table);
        } else if (operation == 4) {
            std::uint32_t count = 0;
            read = Read(count);
            std::printf("%d", manual::LastPageLineOffset(static_cast<std::uint16_t>(count)));
        } else if (operation == 5) {
            read = CopyLine();
        } else if (operation == 6) {
            read = ComposePage();
        }
        if (!read) {
            return 2;
        }
        std::printf("\n");
    }
    return 0;
}
