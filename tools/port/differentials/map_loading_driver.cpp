// Native side of the map area list, background and scene-map loading differentials
// (map_loading_common.py, #276). One vector per line: the mode, then that mode's inputs
// in map_loading_common.py's order. Prints the mode's values, then the recorded call
// count and every call's words, computing the pointers the Live bindings pass from the
// symbolic addresses the vector supplies.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/MapLoading.hpp"

namespace {
namespace map = VS::Menu::map;

enum Mode : int {
    kConfigure,
    kPoll,
    kName,
    kReady,
    kBackground,
    kScene,
    kSelect,
};

constexpr std::int32_t kRefused = -999;
constexpr std::size_t kScenes = 32;
constexpr std::size_t kViewFields = 23;
constexpr std::uint32_t kMenuText = 0x80590000U;
constexpr std::uint32_t kSlot = 0x80560000U;
constexpr std::uint32_t kBuffer = 0x80570000U;
constexpr std::uint32_t kRoomNames = 0x80580000U;
constexpr std::uint32_t kMapNames = 0x801083FCU;
constexpr std::uint32_t kArmFiles = 0x80102B00U;
constexpr std::uint32_t kArmFileBytes = 8;

using Call = std::vector<std::int32_t>;

std::int32_t Word(std::uint32_t value) { return static_cast<std::int32_t>(value); }

std::int32_t Address(std::uint32_t base, std::int64_t offset)
{
    return Word(base + static_cast<std::uint32_t>(offset));
}

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

template <typename Value, std::size_t Count>
bool ReadAll(std::array<Value, Count>& values)
{
    for (Value& value : values) {
        if (!Read(value))
            return false;
    }
    return true;
}

void Print(std::span<const std::int64_t> values, const std::vector<Call>& calls)
{
    for (const std::int64_t value : values)
        std::cout << value << ' ';
    std::cout << calls.size();
    for (const Call& call : calls) {
        for (const std::int32_t item : call)
            std::cout << ' ' << item;
    }
    std::cout << '\n';
}

class AreaHost final : public map::AreaMenuHost {
public:
    std::vector<Call> calls;
    std::int32_t count = 0;
    std::uint8_t cursor_row = 0;
    std::uint8_t first_row = 0;
    std::uint8_t setting = 0;
    std::int32_t selected_row = 0;
    // With names, setMenuRows' text is the listed areas' name pointers (the Live's menu
    // text).
    const std::array<std::int32_t, kScenes>* names = nullptr;
    std::vector<std::int32_t> areas;

    void ListAreas(std::span<const std::int32_t> listed) override
    {
        areas.assign(listed.begin(), listed.end());
        count = static_cast<std::int32_t>(listed.size());
    }
    void SetAreaCursor(map::AreaListCursor cursor) override
    {
        cursor_row = cursor.row;
        first_row = cursor.first_row;
    }
    std::uint8_t AreaCursorRow() override { return cursor_row; }
    std::uint8_t CursorMemorySetting() override { return setting; }
    void SetCursorMemorySetting(std::uint8_t value) override { setting = value; }
    void ShowAreaRows(
        std::int32_t row_info, std::span<const std::int32_t> row_types) override
    {
        Call call { 1, count, row_info };
        if (names == nullptr) {
            call.push_back(Word(kMenuText));
        } else {
            for (const std::int32_t area : areas) {
                const auto offset = (*names)[static_cast<std::size_t>(area)] & 0xFFFF;
                call.push_back(Address(kMapNames, 2LL * offset));
                call.push_back(0);
            }
        }
        call.push_back(setting);
        call.insert(call.end(), row_types.begin(), row_types.end());
        calls.push_back(call);
    }
    std::int32_t SelectedRow() override
    {
        calls.push_back({ 0 });
        return selected_row;
    }
    void ClearMenuExcept(std::int32_t row) override { calls.push_back({ 2, row }); }
    void FlyOutAndHoist(std::int32_t row, std::int32_t offset) override
    {
        calls.push_back({ 3, row, offset });
    }
};

int RunConfigure()
{
    std::int32_t area = 0;
    std::int32_t row = 0;
    AreaHost host;
    if (!Read(host.count) || !Read(area) || !Read(row) || !Read(host.cursor_row)
        || !Read(host.first_row) || !Read(host.setting))
        return 1;
    map::ConfigureAreaList(host, host.count, area, row);
    const std::array<std::int64_t, 4> values { row, host.cursor_row, host.first_row,
        host.setting };
    Print(values, host.calls);
    return 0;
}

int RunPoll()
{
    AreaHost host;
    if (!Read(host.selected_row) || !Read(host.cursor_row))
        return 1;
    const std::array<std::int64_t, 1> values { map::PollAreaSelection(host) };
    Print(values, host.calls);
    return 0;
}

int RunName()
{
    std::int32_t name = 0;
    std::int32_t color = 0;
    if (!Read(name) || !Read(color))
        return 1;
    const map::AreaNameRow row = map::AreaNameRowOf(color);
    const std::vector<Call> calls { { 1, row.placement.row, row.placement.x,
        row.placement.y, row.placement.width, row.placement.background_width, name } };
    const std::array<std::int64_t, 4> values { row.state, row.target_x, row.selected,
        row.font_color };
    Print(values, calls);
    return 0;
}

class ReadyHost final : public map::MenuReadyHost {
public:
    std::vector<Call> calls;
    std::int32_t ready = 0;

    std::int32_t MenuRowsIdle() override
    {
        calls.push_back({ 1 });
        return ready;
    }
};

int RunReady()
{
    ReadyHost host;
    if (!Read(host.ready))
        return 1;
    map::PollMenuReady(host);
    Print({ }, host.calls);
    return 0;
}

class BackgroundHost final : public map::BackgroundLoadHost {
public:
    std::vector<Call> calls;
    std::int32_t state = 0;
    std::int32_t read = 0;
    std::int32_t image = 0;

    void AllocateBackgroundRead(map::DiscFileRange file) override
    {
        calls.push_back({ 1, file.sector, file.bytes });
        read = Word(kSlot);
    }
    void AllocateBackgroundImage(std::int32_t bytes) override
    {
        calls.push_back({ 2, bytes });
        image = Word(kBuffer);
    }
    void EnqueueBackgroundRead() override { calls.push_back({ 3, read, image }); }
    std::int32_t BackgroundReadState() override { return state; }
    void ReleaseBackgroundRead() override { calls.push_back({ 4, read }); }
    void LoadBackgroundClut(
        std::int32_t row, std::int32_t first, std::int32_t count) override
    {
        calls.push_back({ 5, image, row, first, count });
    }
    void UploadBackground(std::int32_t x, std::int32_t y, std::int32_t offset,
        std::int32_t width, std::int32_t height) override
    {
        calls.push_back({ 6,
            Word((static_cast<std::uint32_t>(y) << 16U) | static_cast<std::uint32_t>(x)),
            Address(static_cast<std::uint32_t>(image), 2LL * offset),
            Word((static_cast<std::uint32_t>(height) << 16U)
                 | static_cast<std::uint32_t>(width)) });
    }
    void SetBackgroundPriority(
        std::int32_t order_offset, std::int32_t brightness) override
    {
        calls.push_back({ 7, order_offset, brightness });
    }
    void FreeBackgroundImage() override { calls.push_back({ 8, image }); }
};

int RunBackground()
{
    std::int32_t begin = 0;
    std::int32_t step = 0;
    BackgroundHost host;
    if (!Read(begin) || !Read(step) || !Read(host.state) || !Read(host.read)
        || !Read(host.image))
        return 1;
    host.state = static_cast<std::int16_t>(host.state);
    const std::int32_t result = map::LoadBackground(host, step, begin);
    const std::array<std::int64_t, 4> values { result, step, host.read, host.image };
    Print(values, host.calls);
    return 0;
}

class SceneHost final : public map::SceneMapHost {
public:
    std::vector<Call> calls;
    std::int32_t buffer = 0;
    std::int32_t read = 0;
    std::int32_t state = 0;
    std::int32_t room_names = 0;
    std::uint16_t yaw = 0;
    std::int32_t scene = 0;
    std::int32_t initial = 0;
    std::int32_t projection = 0;
    std::array<std::int32_t, kScenes> names { };

    bool HasSceneBuffer() override { return buffer != 0; }
    std::uint16_t CameraYaw() override { return yaw; }
    std::int32_t CurrentSceneId() override
    {
        calls.push_back({ 1 });
        return scene;
    }
    void AllocateSceneBuffer(std::int32_t bytes) override
    {
        calls.push_back({ 2, bytes });
        buffer = Word(kBuffer);
    }
    void AllocateSceneRead(std::int32_t at) override
    {
        calls.push_back(
            { 3, Address(kArmFiles, static_cast<std::int64_t>(kArmFileBytes) * at) });
        read = Word(kSlot);
    }
    void EnqueueSceneRead() override { calls.push_back({ 4, read, buffer }); }
    std::int32_t SceneReadState() override { return state; }
    void ReleaseSceneRead() override { calls.push_back({ 5, read }); }
    void ResolveRoomNames() override
    {
        calls.push_back({ 6, buffer });
        room_names = Word(kRoomNames);
    }
    void RestoreRoomVisits() override { calls.push_back({ 7, buffer }); }
    void ScaleRooms(std::int32_t factor) override
    {
        calls.push_back({ 8, buffer, factor });
    }
    std::int32_t InitialRoom() override
    {
        calls.push_back({ 9, buffer });
        return initial;
    }
    void CenterOnRoom(std::int32_t room) override
    {
        calls.push_back({ 10, buffer, room });
    }
    void SetFogNear(std::int32_t near_distance) override
    {
        calls.push_back({ 11, near_distance, projection });
    }
    void SetFarColor(std::int32_t red, std::int32_t green, std::int32_t blue) override
    {
        calls.push_back({ 12, red, green, blue });
    }
    void ShowAreaName(std::int32_t at, std::int32_t font_color) override
    {
        const auto offset = names[static_cast<std::size_t>(at)] & 0xFFFF;
        calls.push_back({ 13, Address(kMapNames, 2LL * offset), font_color });
    }
};

int RunScene()
{
    SceneHost host;
    std::uint32_t room_flags = 0;
    std::array<std::int32_t, kViewFields> fields { };
    std::array<std::uint8_t, kScenes> paling { };
    std::array<std::int32_t, kScenes> sizes { };
    if (!Read(host.buffer) || !Read(room_flags) || !Read(host.yaw) || !Read(host.scene)
        || !Read(host.read) || !Read(host.state) || !Read(host.initial)
        || !Read(host.projection) || !Read(host.room_names) || !ReadAll(fields)
        || !ReadAll(paling) || !ReadAll(sizes) || !ReadAll(host.names))
        return 1;
    host.state = static_cast<std::int16_t>(host.state);
    const auto half = [](std::int32_t value) { return static_cast<std::int16_t>(value); };
    map::SceneMapView view { .trigger_hide_ui = fields[0],
        .hide_ui = fields[1],
        .camera_angles = { half(fields[2]), half(fields[3]), half(fields[4]),
            half(fields[5]) },
        .camera_position = { half(fields[6]), half(fields[7]), half(fields[8]),
            half(fields[9]) },
        .current_room = fields[10],
        .current_scene = fields[11],
        .current_scene_row = fields[12],
        .menu_state = fields[13],
        .background_brightness = fields[14],
        .menu_frames = fields[15],
        .geometry_offset_x = fields[16],
        .geometry_offset_y = fields[17],
        .quit_state = fields[18],
        .camera_distance = fields[19],
        .menu_commands_state = half(fields[20]),
        .ui_animation_state = half(fields[21]),
        .is_paling = fields[22] };
    std::array<map::DiscFileRange, kScenes> files { };
    for (std::size_t scene = 0; scene < kScenes; ++scene) {
        files[scene] = { .sector = static_cast<std::int32_t>(0x1000 + scene),
            .bytes = sizes[scene] };
    }
    map::SceneMapProgress progress { .map_paling = paling, .room_flags = room_flags };
    const map::SceneMapLoad load = map::LoadSceneMap(host, view, progress, files);
    std::int64_t result = kRefused;
    if (load == map::SceneMapLoad::kLoading)
        result = 0;
    else if (load == map::SceneMapLoad::kLoaded)
        result = 1;
    std::vector<std::int64_t> values { result, host.buffer, host.read, host.room_names,
        Word(progress.room_flags), view.trigger_hide_ui, view.hide_ui,
        view.camera_angles.x, view.camera_angles.y, view.camera_angles.z,
        view.camera_angles.pad, view.camera_position.x, view.camera_position.y,
        view.camera_position.z, view.camera_position.pad, view.current_room,
        view.current_scene, view.current_scene_row, view.menu_state,
        view.background_brightness, view.menu_frames, view.geometry_offset_x,
        view.geometry_offset_y, view.quit_state, view.camera_distance,
        view.menu_commands_state, view.ui_animation_state, view.is_paling };
    values.insert(values.end(), paling.begin(), paling.end());
    Print(values, host.calls);
    return 0;
}

int RunSelect()
{
    std::int32_t initialize = 0;
    map::AreaSelection selection;
    std::uint32_t visited = 0;
    map::AreaSelectionInput input;
    std::int32_t row = 0;
    std::array<std::int32_t, kScenes> scene_rows { };
    std::array<std::int32_t, kScenes> names { };
    AreaHost host;
    if (!Read(initialize) || !Read(selection.step) || !Read(selection.selected)
        || !Read(visited) || !Read(input.current_scene) || !Read(input.current_scene_row)
        || !Read(row) || !Read(host.cursor_row) || !Read(host.first_row)
        || !Read(host.setting) || !Read(host.selected_row) || !ReadAll(scene_rows)
        || !ReadAll(names))
        return 1;
    input.visited_areas = visited;
    host.names = &names;
    const std::int32_t result =
        map::UpdateAreaSelection(host, selection, input, scene_rows, row, initialize);
    std::vector<std::int64_t> values { result, selection.step, selection.selected, row,
        host.cursor_row, host.first_row, host.setting };
    values.insert(values.end(), scene_rows.begin(), scene_rows.end());
    Print(values, host.calls);
    return 0;
}

int RunVector(int mode)
{
    switch (mode) {
    case kConfigure:
        return RunConfigure();
    case kPoll:
        return RunPoll();
    case kName:
        return RunName();
    case kReady:
        return RunReady();
    case kBackground:
        return RunBackground();
    case kScene:
        return RunScene();
    case kSelect:
        return RunSelect();
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
