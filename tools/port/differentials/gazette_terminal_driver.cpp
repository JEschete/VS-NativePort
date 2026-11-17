#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/GazetteTerminalFixture.hpp"

namespace {
namespace fixture = VS::tests::gazette;
namespace score = VS::Menu::Score;
namespace render = VS::Render;
constexpr std::size_t kEntries = score::kEncyclopediaEnemies + 1;
constexpr std::size_t kEntryBytes = 44;
constexpr std::size_t kNameOffset = 16;
constexpr std::size_t kGuardWords = 4;
constexpr std::uint32_t kPrimitives = 0x80303000;
constexpr std::uint32_t kEnemyFile = 0x80310000;
constexpr std::uint32_t kRequest = 0x80307000;
constexpr std::uint32_t kDescriptionTable = kEnemyFile + 150 * kEntryBytes;
enum Mode {
    kDescription,
    kNumber,
    kInitialize,
    kList,
    kViewer,
    kNavigation,
    kController,
    kScreen
};

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

template <typename Range> void Print(const Range& values)
{
    for (const auto value : values)
        std::cout << ' ' << +value;
}

void PrintCalls(const fixture::Calls& calls)
{
    std::cout << ' ' << calls.size();
    for (const auto& call : calls)
        Print(call);
    std::cout << '\n';
}

using RawEntries = std::array<std::array<std::uint8_t, kEntryBytes>, kEntries>;

std::int16_t Half(const std::array<std::uint8_t, kEntryBytes>& bytes, std::size_t offset)
{
    return std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(static_cast<unsigned>(bytes[offset])
                                   | (static_cast<unsigned>(bytes[offset + 1]) << 8)));
}

void SetHalf(
    std::array<std::uint8_t, kEntryBytes>& bytes, std::size_t offset, std::int16_t value)
{
    const auto bits = static_cast<std::uint16_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(bits);
    bytes[offset + 1] = static_cast<std::uint8_t>(bits >> 8);
}

bool ReadEnemies(fixture::EnemyData& data, RawEntries& raw)
{
    for (std::size_t index = 0; index < raw.size(); ++index) {
        if (!ReadAll(raw[index]))
            return false;
        auto& enemy = data.enemies[index];
        enemy.model_id = Half(raw[index], 0);
        enemy.class_id = Half(raw[index], 2);
        enemy.first_defeat_bit = Half(raw[index], 4);
        enemy.defeat_bit_count = Half(raw[index], 6);
        enemy.selected = Half(raw[index], 8);
        enemy.highlight_step = Half(raw[index], 10);
        enemy.unlocked = raw[index][12];
        enemy.previous = raw[index][13];
        enemy.next = raw[index][14];
        enemy.name = std::span(raw[index]).subspan(kNameOffset);
    }
    return true;
}

void PrintEnemies(const fixture::EnemyData& data, RawEntries& raw)
{
    for (std::size_t index = 0; index < raw.size(); ++index) {
        const auto& enemy = data.enemies[index];
        SetHalf(raw[index], 0, enemy.model_id);
        SetHalf(raw[index], 2, enemy.class_id);
        SetHalf(raw[index], 4, enemy.first_defeat_bit);
        SetHalf(raw[index], 6, enemy.defeat_bit_count);
        SetHalf(raw[index], 8, enemy.selected);
        SetHalf(raw[index], 10, enemy.highlight_step);
        raw[index][12] = enemy.unlocked;
        raw[index][13] = enemy.previous;
        raw[index][14] = enemy.next;
        Print(raw[index]);
    }
}

bool ReadMenus(fixture::EncyclopediaHost& host)
{
    for (auto& menu : host.menu_replies) {
        if (!ReadAll(menu))
            return false;
    }
    std::memcpy(&host.menu, host.menu_replies[0].data(), fixture::kObservedMenuBytes);
    return true;
}

void PrintMenu(const fixture::EncyclopediaHost& host)
{
    const auto bytes = fixture::MenuBytes(host.menu, 0);
    Print(std::span(bytes).subspan(1));
}

bool DescriptionOrInitialize(int mode)
{
    fixture::EnemyData data;
    fixture::EncyclopediaHost host;
    RawEntries raw { };
    std::int32_t selected = 0;
    if (!Read(selected) || !ReadEnemies(data, raw) || !ReadAll(data.flags)) {
        return false;
    }
    const bool accepted = mode == kDescription
                            ? score::ShowEnemyDescription(
                                  data.enemies, selected, data.locked_description, host)
                            : score::InitializeEnemyList(data.enemies, data.flags);
    if (!accepted)
        return false;
    PrintEnemies(data, raw);
    PrintCalls(host.calls);
    return true;
}

bool List()
{
    fixture::EnemyData data;
    fixture::EncyclopediaHost host;
    RawEntries raw { };
    score::GazetteRecordCursor cursor;
    score::GazetteRecordAnimation animation;
    int step = 0;
    if (!Read(step) || !Read(cursor.first_row) || !Read(cursor.selected_row)
        || !Read(animation.cursor_step) || !Read(host.cursor_reply)
        || !ReadEnemies(data, raw) || !ReadMenus(host)) {
        return false;
    }
    if (!score::RenderEnemyList(data.enemies, cursor, step, animation, data.locked_name,
            data.locked_description, host)) {
        return false;
    }
    std::cout << +animation.cursor_step;
    PrintEnemies(data, raw);
    PrintMenu(host);
    PrintCalls(host.calls);
    return true;
}

bool Viewer()
{
    fixture::EnemyData data;
    fixture::EncyclopediaHost host;
    RawEntries raw { };
    score::EnemyViewerPresentation presentation;
    if (!Read(presentation.selected_enemy) || !Read(presentation.step)
        || !Read(presentation.transition) || !Read(presentation.hide_ui)
        || !Read(presentation.rotation_color) || !Read(presentation.zoom_color)
        || !Read(host.projection.x) || !Read(host.projection.y)) {
        return false;
    }
    for (auto& row : host.camera.rotation.m) {
        if (!ReadAll(row))
            return false;
    }
    if (!Read(host.camera.translation.x) || !Read(host.camera.translation.y)
        || !Read(host.camera.translation.z) || !ReadEnemies(data, raw)
        || !ReadMenus(host)) {
        return false;
    }
    if (!score::RenderEnemyViewer(data.enemies, presentation, host))
        return false;
    std::cout << host.projection.x << ' ' << host.projection.y;
    PrintEnemies(data, raw);
    PrintMenu(host);
    PrintCalls(host.calls);
    return true;
}

bool Primitives(int mode)
{
    fixture::EncyclopediaHost host;
    score::EnemyNumber number;
    int direction = 0;
    int step = 0;
    bool hidden = false;
    if (mode == kNumber) {
        if (!Read(number.x) || !Read(number.y) || !Read(number.value)
            || !Read(number.dimmed)) {
            return false;
        }
    } else if (!Read(direction) || !Read(step) || !Read(hidden)) {
        return false;
    }
    std::uint32_t other_ordering = 0;
    if (!Read(host.chain) || !Read(other_ordering))
        return false;
    const std::size_t count = mode == kNumber ? 3 : 1;
    std::vector<std::uint32_t> words(
        count * render::kUiTexturedQuadWords + kGuardWords * 2);
    if (!ReadAll(words))
        return false;
    host.primitive_address = kPrimitives + kGuardWords * sizeof(std::uint32_t);
    for (std::size_t index = 0; index < count; ++index) {
        render::UiTexturedQuadPacket packet { };
        std::copy_n(
            words.begin()
                + static_cast<std::ptrdiff_t>(kGuardWords + index * packet.size()),
            packet.size(), packet.begin());
        host.storage.push_back(packet);
    }
    const bool accepted = mode == kNumber
                            ? score::DrawEnemyNumber(number, host)
                            : score::DrawEnemyNavigation(direction, step, hidden, host);
    if (!accepted)
        return false;
    for (std::size_t index = 0; index < host.packets.size(); ++index) {
        std::copy(host.packets[index].begin(), host.packets[index].end(),
            words.begin()
                + static_cast<std::ptrdiff_t>(
                    kGuardWords + index * host.packets[index].size()));
    }
    std::cout << host.primitive_address
                     + host.packets.size() * sizeof(render::UiTexturedQuadPacket)
              << ' ' << host.chain << ' ' << other_ordering;
    Print(words);
    PrintCalls(host.calls);
    return true;
}

bool ReadCursor(score::GazetteCursorMemory& cursor)
{
    return Read(cursor.titles.first_row) && Read(cursor.titles.selected_row)
        && Read(cursor.enemies.first_row) && Read(cursor.enemies.selected_row)
        && Read(cursor.statistics.first_row) && Read(cursor.statistics.selected_row)
        && Read(cursor.time_trial) && Read(cursor.overview_row);
}

bool ReadState(score::GazetteState& state)
{
    return Read(state.phase) && Read(state.next_result) && Read(state.animation_step)
        && Read(state.camera.pitch) && Read(state.camera.yaw) && Read(state.camera.roll)
        && Read(state.camera.distance) && Read(state.camera.height_offset)
        && Read(state.initial_camera_distance) && Read(state.selected_enemy)
        && Read(state.enemy_skip_delay) && Read(state.navigation_step)
        && Read(state.enemy_button_or_slide) && Read(state.hide_ui)
        && Read(state.rotation_color) && Read(state.zoom_color);
}

void PrintState(const score::GazetteState& state)
{
    const fixture::Call words { static_cast<int>(state.phase), state.next_result,
        state.animation_step, state.camera.pitch, state.camera.yaw, state.camera.roll,
        state.camera.distance, state.camera.height_offset, state.initial_camera_distance,
        state.selected_enemy, state.enemy_skip_delay, state.navigation_step,
        state.enemy_button_or_slide, state.hide_ui, state.rotation_color,
        state.zoom_color };
    Print(words);
}

bool ReadInput(score::GazetteInput& input)
{
    return Read(input.pressed) && Read(input.repeated) && Read(input.held)
        && Read(input.released) && Read(input.left_x) && Read(input.left_y)
        && Read(input.right_x) && Read(input.right_y);
}

bool Controller()
{
    fixture::TerminalHost host;
    RawEntries raw { };
    std::uint16_t pad = 0;
    std::uint32_t file = 0;
    std::uint32_t request = 0;
    std::uint32_t descriptions = 0;
    std::uint16_t rank = 0;
    int preview_reply = 0;
    int read_state = 0;
    if (!ReadState(host.state) || !Read(pad) || !ReadInput(host.input)
        || !ReadCursor(host.cursor) || !ReadEnemies(host.data, raw) || !ReadAll(host.box)
        || !ReadAll(host.delays) || !Read(host.top_result) || !Read(host.overview_cursor)
        || !Read(host.preset->distance) || !Read(host.preset->height_offset)
        || !Read(preview_reply) || !Read(read_state) || !Read(host.file_sector)
        || !Read(host.file_bytes) || !Read(host.box_present) || !Read(rank) || !Read(file)
        || !Read(request) || !Read(descriptions)) {
        return false;
    }
    host.preview_result = preview_reply;
    host.read_status = read_state == 4 ? score::GazetteReadStatus::kLoaded
                                       : score::GazetteReadStatus::kPending;
    host.rank = { static_cast<std::uint8_t>(0x10 + rank), 0x20, 0xE7 };
    const auto result = host.Tick();
    if (!result.has_value())
        return false;
    std::cout << *result;
    PrintState(host.state);
    std::cout << ' ' << pad;
    Print(fixture::CursorWords(host.cursor));
    Print(host.box);
    std::cout << ' ' << (host.data_started ? kEnemyFile : file) << ' '
              << (host.data_started ? kRequest : request) << ' '
              << (host.description_bound ? kDescriptionTable : descriptions);
    PrintEnemies(host.data, raw);
    PrintCalls(host.calls);
    return true;
}

bool Screen()
{
    fixture::ScreenHost host;
    if (!Read(host.phase) || !Read(host.state.initialization_delay)
        || !Read(host.state.progress_complete) || !Read(host.state.selected_result)
        || !Read(host.environment.hud_slide_offset)
        || !Read(host.environment.background_fade_step)
        || !Read(host.environment.current_menu) || !Read(host.environment.hide_menu)
        || !Read(host.progress_reply) || !Read(host.idle_reply)
        || !Read(host.menu_reply)) {
        return false;
    }
    const auto result = host.Tick();
    std::cout << result << ' ' << +host.phase << ' ' << host.state.initialization_delay
              << ' ' << host.state.progress_complete << ' ' << host.state.selected_result
              << ' ' << host.environment.hud_slide_offset << ' '
              << host.environment.background_fade_step << ' '
              << +host.environment.current_menu << ' ' << +host.environment.hide_menu;
    PrintCalls(host.calls);
    return true;
}
} // namespace

int main()
{
    int mode = 0;
    while (std::cin >> mode) {
        bool accepted = false;
        switch (mode) {
        case kDescription:
        case kInitialize:
            accepted = DescriptionOrInitialize(mode);
            break;
        case kList:
            accepted = List();
            break;
        case kViewer:
            accepted = Viewer();
            break;
        case kNumber:
        case kNavigation:
            accepted = Primitives(mode);
            break;
        case kController:
            accepted = Controller();
            break;
        case kScreen:
            accepted = Screen();
            break;
        default:
            return 2;
        }
        if (!accepted)
            return 3;
    }
    return 0;
}
