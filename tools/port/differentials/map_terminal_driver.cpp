#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/MapTerminal.hpp"

namespace {
namespace map = VS::Menu::map;
using Words = std::vector<std::int64_t>;
using Calls = std::vector<Words>;
constexpr std::uint32_t kPrimitives = 0x80280000;
constexpr std::uint32_t kOldScene = 0x80570000;
constexpr std::uint32_t kStaging = 0x80580000;
constexpr std::uint32_t kNewScene = 0x80590000;
constexpr std::uint32_t kReadSlot = 0x805A0000;
constexpr std::uint32_t kOldNames = 0x805B0000;
constexpr std::uint32_t kNewNames = 0x805C0000;
constexpr std::uint32_t kBackup = 0x805E0000;
constexpr std::uint32_t kOldBackup = 0x805F0000;
constexpr std::uint32_t kCaption = 0x80108CC4;
constexpr std::uint32_t kLow24 = 0xFFFFFF;
constexpr std::uint32_t kHigh8 = 0xFF000000;
constexpr std::size_t kSceneEntry = 18;
constexpr std::array<int, 3> kSlots { 8, 2046, 2047 };
enum Mode { kFrame, kCamera, kDarken, kPaling, kScreen, kMenu };

template <typename T = std::int32_t> T Read()
{
    std::int64_t value = 0;
    std::cin >> value;
    return static_cast<T>(value);
}

void Record(
    Calls& calls, std::uint32_t at, std::initializer_list<std::int64_t> args = { })
{
    Words words { at, static_cast<std::int64_t>(args.size()) };
    words.insert(words.end(), args);
    calls.push_back(std::move(words));
}

void Print(const Words& values, const Calls& calls = { })
{
    for (const auto value : values)
        std::cout << value << ' ';
    std::cout << calls.size();
    for (const auto& call : calls) {
        for (const auto value : call)
            std::cout << ' ' << value;
    }
    std::cout << '\n';
}

map::MapAngles ReadAngles()
{
    return { Read<std::int16_t>(), Read<std::int16_t>(), Read<std::int16_t>(),
        Read<std::int16_t>() };
}

void AppendAngles(Words& values, map::MapAngles angles)
{
    values.insert(values.end(), { angles.x, angles.y, angles.z, angles.pad });
}

void Camera()
{
    const auto angles = ReadAngles();
    const auto distance = Read();
    const auto previous = ReadAngles();
    const auto camera = map::ApplyCamera(angles, distance, previous);
    Words values;
    AppendAngles(values, camera.position);
    for (const auto& row : camera.transform.rotation.m) {
        for (const auto component : row)
            values.push_back(component);
    }
    const auto translation = camera.transform.translation;
    values.insert(values.end(), { translation.x, translation.y, translation.z });
    Print(values);
}

class PacketHost final : public map::FramebufferHost {
public:
    std::vector<std::uint32_t> words;
    std::array<std::uint32_t, 2084> ordering { };
    std::size_t cursor;

    PacketHost(std::size_t start, std::size_t count, std::uint32_t salt)
        : words(count)
        , cursor(start)
    {
        for (std::size_t index = 0; index < count; ++index) {
            words[index] = salt ^ (static_cast<std::uint32_t>(index) * 0x9E3779B9U);
        }
        for (std::size_t index = 0; index < ordering.size(); ++index) {
            ordering[index] =
                (salt & kHigh8) | (0x600000U + static_cast<std::uint32_t>(index) * 4U);
        }
    }
    void Emit(std::int32_t slot, std::span<const std::uint32_t> body)
    {
        auto& head = ordering[kSceneEntry + slot];
        words[cursor] = (static_cast<std::uint32_t>(body.size()) << 24) | (head & kLow24);
        head = (head & kHigh8)
             | ((kPrimitives + static_cast<std::uint32_t>(cursor) * 4U) & kLow24);
        ++cursor;
        for (auto value : body)
            words[cursor++] = value;
    }
    bool TexturePage(std::int32_t slot, std::uint16_t page) override
    {
        Emit(slot, std::array<std::uint32_t, 2> { 0xE1000200U | page, 0 });
        return true;
    }
    bool Strip(std::int32_t slot, const map::FramebufferStrip& strip) override
    {
        const auto xy =
            static_cast<std::uint16_t>(strip.x)
            | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(strip.y)) << 16);
        const auto wh =
            static_cast<std::uint16_t>(strip.width)
            | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(strip.height))
                << 16);
        Emit(slot, std::array<std::uint32_t, 4> {
                       strip.raw_texture ? (words[cursor + 1] & kLow24) | 0x67000000U
                                         : 0x66808080U,
                       xy,
                       (strip.clear_clut ? 0U : words[cursor + 3] & 0xFFFF0000U)
                           | (static_cast<std::uint32_t>(strip.texture_y) << 8),
                       wh });
        return true;
    }
    Words Values() const
    {
        Words values { static_cast<std::int64_t>(cursor * 4) };
        for (auto word : words)
            values.push_back(static_cast<std::int32_t>(word));
        for (const auto slot : kSlots) {
            values.push_back(static_cast<std::int32_t>(ordering[kSceneEntry + slot]));
        }
        return values;
    }
};

class FrameHost final : public map::MapFrameHost {
public:
    PacketHost& packets;
    Calls calls;
    std::uint32_t caption;
    FrameHost(PacketHost& target, std::uint32_t text)
        : packets(target)
        , caption(text)
    {
    }
    void DrawControls() override { Record(calls, 0x80106C84); }
    bool EnableMaskWrite() override
    {
        packets.Emit(2047, std::array<std::uint32_t, 2> { 0xE6000001, 0 });
        return true;
    }
    void DrawDistortion() override { Record(calls, 0x801066E0); }
    void SetPalingCaption() override
    {
        Record(calls, 0x800C6BF0, { 0, kCaption + caption * 2 });
    }
    void ApplyView() override { Record(calls, 0x80104384); }
    void Darken() override { Record(calls, 0x8010625C); }
    void DrawRooms() override { Record(calls, 0x801046B0, { kOldScene }); }
};

void Packets(int mode)
{
    const auto start = Read<std::size_t>();
    const auto salt = Read<std::uint32_t>();
    const auto back = Read();
    const auto frame = Read();
    const std::size_t packet_words = mode == kFrame ? 3 : (mode == kDarken ? 19 : 3827);
    PacketHost packets(start, start + packet_words + 4, salt);
    if (mode == kFrame) {
        const auto jammed = Read();
        const auto leaving = Read();
        const auto caption = Read<std::uint32_t>();
        FrameHost host(packets, caption);
        static_cast<void>(map::DrawFrame(host, jammed != 0, leaving != 0));
        Print(packets.Values(), host.calls);
    } else {
        if (mode == kDarken) {
            static_cast<void>(map::DarkenBackground(packets, back != 0));
        } else {
            static_cast<void>(map::DrawPalingDistortion(packets, back != 0, frame));
        }
        Print(packets.Values());
    }
}

map::MapMenuState ReadMenuState()
{
    map::MapMenuState state;
    auto& v = state.view;
    v.trigger_hide_ui = Read();
    v.hide_ui = Read();
    v.camera_angles = ReadAngles();
    v.camera_position = ReadAngles();
    v.current_room = Read();
    v.current_scene = Read();
    v.current_scene_row = Read();
    v.menu_state = Read();
    v.background_brightness = Read();
    v.menu_frames = Read();
    v.geometry_offset_x = Read();
    v.geometry_offset_y = Read();
    v.quit_state = Read();
    v.camera_distance = Read();
    v.menu_commands_state = Read<std::int16_t>();
    v.ui_animation_state = Read<std::int16_t>();
    v.is_paling = Read();
    state.selected_map = Read();
    state.label_y = Read();
    return state;
}

void AppendState(Words& values, const map::MapMenuState& state)
{
    const auto& v = state.view;
    values.insert(values.end(), { v.trigger_hide_ui, v.hide_ui });
    AppendAngles(values, v.camera_angles);
    AppendAngles(values, v.camera_position);
    values.insert(values.end(),
        { v.current_room, v.current_scene, v.current_scene_row, v.menu_state,
            v.background_brightness, v.menu_frames, v.geometry_offset_x,
            v.geometry_offset_y, v.quit_state, v.camera_distance, v.menu_commands_state,
            v.ui_animation_state, v.is_paling, state.selected_map, state.label_y });
}

class MenuHost final : public map::MapMenuHost {
public:
    map::MapMenuState& state;
    Calls calls;
    int selection = Read();
    int read_state = Read();
    int initial = Read();
    int adjacent = Read();
    std::array<std::int16_t, 3> draw_position { Read<std::int16_t>(),
        Read<std::int16_t>(), Read<std::int16_t>() };
    std::int16_t draw_animation = Read<std::int16_t>();
    std::array<std::uint8_t, 32> paling { };
    std::array<std::int32_t, 32> rows { };
    std::array<map::DiscFileRange, 32> files { };
    std::array<std::uint16_t, 32> names { };
    std::uint32_t scene_buffer = kOldScene;
    std::uint32_t scene_slot = kReadSlot;
    std::uint32_t arm_buffer = kStaging;
    std::uint32_t room_names = kOldNames;

    explicit MenuHost(map::MapMenuState& menu)
        : state(menu)
    {
        for (auto& value : paling)
            value = Read<std::uint8_t>();
        for (auto& value : rows)
            value = Read();
        for (auto& file : files)
            file.bytes = Read();
        for (auto& value : names)
            value = Read<std::uint16_t>();
    }
    void Rec(std::uint32_t address, std::initializer_list<std::int64_t> args = { })
    {
        Words words { address, static_cast<std::int64_t>(args.size() + 5) };
        words.insert(words.end(), args);
        const auto& v = state.view;
        words.insert(words.end(), { v.menu_commands_state, v.current_scene,
                                      v.current_room, v.menu_state, v.menu_frames });
        calls.push_back(std::move(words));
    }
    void SetFarColorBlack() override { Rec(0x80041558, { 0, 0, 0 }); }
    void SetBackgroundPriority(std::int32_t slot, std::int32_t brightness) override
    {
        Rec(0x80100414, { slot, brightness });
    }
    void SetVisible(bool visible) override { Rec(0x80102CD8, { visible ? 1 : 0 }); }
    void PlaySound(std::int32_t volume) override { Rec(0x80045988, { 0x7E, volume }); }
    bool InitializeRoomText() override
    {
        Rec(0x800CCF08, { 0, 0, 60, 224, 16, 1, 60, 224 });
        return true;
    }
    bool ShowRoomName(std::int32_t room) override
    {
        Rec(0x800C6BF0, { 0, room_names + static_cast<std::uint32_t>(room) * 36 });
        return true;
    }
    void DismissRoomText() override { Rec(0x800CD064, { 0 }); }
    std::int32_t PollAreas(bool initialize) override
    {
        Rec(0x8010451C, { initialize ? 1 : 0 });
        return selection;
    }
    void ShowAreaName(std::int32_t scene, bool current) override
    {
        Rec(0x80102F30, { 0x801083FCU + names[scene] * 2U, current ? 1 : 0 });
    }
    bool BeginSceneRead(std::int32_t scene, map::DiscFileRange file) override
    {
        Rec(0x80043E3C, { file.bytes });
        arm_buffer = kStaging;
        Rec(0x80044B10, { 0x80102B00U + static_cast<std::uint32_t>(scene) * 8 });
        scene_slot = kReadSlot;
        Rec(0x80044BC4, { scene_slot, arm_buffer });
        return true;
    }
    std::int32_t SceneReadState() override { return read_state; }
    bool InstallSceneRead(std::int32_t /*scene*/, map::DiscFileRange file) override
    {
        Rec(0x80044B80, { scene_slot });
        Rec(0x80043C60, { scene_buffer });
        Rec(0x80043E3C, { file.bytes });
        scene_buffer = kNewScene;
        Rec(0x800490B0, { scene_buffer, arm_buffer, file.bytes });
        Rec(0x80043C60, { arm_buffer });
        Rec(0x80098F4C, { scene_buffer });
        room_names = kNewNames;
        Rec(0x8008B4D8, { scene_buffer });
        Rec(0x801060E0, { scene_buffer, 4 });
        return true;
    }
    bool FreeScene() override
    {
        Rec(0x80043C60, { scene_buffer });
        scene_buffer = 0;
        return true;
    }
    std::int32_t InitialRoom() override
    {
        Rec(0x80106570, { scene_buffer });
        return initial;
    }
    void CenterOnRoom(std::int32_t room) override
    {
        Rec(0x80105B18, { scene_buffer, room });
    }
    std::int32_t SelectRoom(std::int32_t direction, std::int32_t /*current*/) override
    {
        Rec(0x80106618, { direction });
        state.view.current_room = adjacent;
        return adjacent;
    }
    void SetCenterTarget(std::int32_t room) override
    {
        Rec(0x80105D3C, { scene_buffer, room });
    }
    void DrawFrame() override
    {
        Rec(0x801042B0);
        state.view.camera_position.x = draw_position[0];
        state.view.camera_position.y = draw_position[1];
        state.view.camera_position.z = draw_position[2];
        state.view.ui_animation_state = draw_animation;
    }
    void AnimateCentering() override { Rec(0x80105EC0, { scene_buffer }); }
};

void Menu()
{
    auto state = ReadMenuState();
    const map::MapInput input { .pressed = Read<std::uint32_t>(),
        .held = Read<std::uint32_t>(),
        .repeated = Read<std::uint32_t>(),
        .left_x = Read<std::uint8_t>(),
        .left_y = Read<std::uint8_t>(),
        .right_x = Read<std::uint8_t>(),
        .right_y = Read<std::uint8_t>(),
        .field_yaw = Read<std::uint16_t>() };
    MenuHost host(state);
    const auto result =
        map::UpdateMapMenu(host, state, input, host.paling, host.rows, host.files);
    Words values { result.value_or(-9999) };
    AppendState(values, state);
    values.insert(values.end(),
        { host.scene_buffer, host.scene_slot, host.arm_buffer, host.room_names });
    Print(values, host.calls);
}

class ScreenHost final : public map::MapScreenHost {
public:
    map::MapScreenState& state;
    std::uint8_t& step;
    int background = Read();
    int scene = Read();
    int ready = Read();
    int result = Read();
    int fade = Read();
    int row = Read();
    int hud = Read();
    std::uint8_t destination = Read<std::uint8_t>();
    std::uint8_t hide = Read<std::uint8_t>();
    std::array<std::int16_t, 8> rects { };
    std::uint32_t backup = kOldBackup;
    Calls calls;
    ScreenHost(map::MapScreenState& screen, std::uint8_t& phase)
        : state(screen)
        , step(phase)
    {
        for (auto& value : rects)
            value = Read<std::int16_t>();
    }
    void Rec(std::uint32_t address, std::initializer_list<std::int64_t> args = { })
    {
        Words words { address, static_cast<std::int64_t>(args.size() + 4) };
        words.insert(words.end(), args);
        words.insert(words.end(),
            { step, state.load_delay, state.scene_loaded, state.menu_result });
        calls.push_back(std::move(words));
    }
    bool CaptureBackground() override
    {
        row = -1;
        Rec(0x80043E3C, { 0xB600 });
        backup = kBackup;
        rects = { 768, 227, 256, 1, 672, 256, 96, 240 };
        Rec(0x8002895C, { 768, 227, 256, 1, backup });
        Rec(0x8002895C, { 672, 256, 96, 240, backup + 512 });
        return true;
    }
    std::int32_t LoadBackground(bool begin) override
    {
        Rec(0x80102FCC, { begin ? 1 : 0 });
        return background;
    }
    std::int32_t LoadScene() override
    {
        Rec(0x80103418);
        return scene;
    }
    void FadeBackground(bool fade_in) override { Rec(0x800FFB68, { fade_in ? 1 : 0 }); }
    void SetFieldRendering(bool enabled) override
    {
        Rec(0x8008A4DC, { enabled ? 1 : 0 });
    }
    void SetHudHidden(bool hidden) override { Rec(0x800CB654, { hidden ? 1 : 0 }); }
    void SetHudSlide(std::int32_t offset) override { hud = offset; }
    std::int32_t MenuRowsIdle() override
    {
        Rec(0x800FA9D0);
        return ready;
    }
    std::int32_t UpdateMenu() override
    {
        Rec(0x80103684);
        return result;
    }
    void RestoreMenuPriority() override { Rec(0x80100414, { -4, 128 }); }
    void ClearCommand() override { Rec(0x800FFA88, { 0 }); }
    void ClearRows() override { Rec(0x800FA8E0, { 40 }); }
    bool RestoreBackground() override
    {
        rects = { 640, 256, 32, 240, 768, 256, 32, 240 };
        Rec(0x800287D4, { 640, 256, 32, 240, 0, 0, 0 });
        Rec(0x800287D4, { 768, 256, 32, 240, 0, 0, 0 });
        Rec(0x80048A64, { backup, 3, 0, 256 });
        Rec(0x800CCDA8, { 0x010002A0, backup + 512, 0x00F00060 });
        return true;
    }
    bool ReleaseBackground() override
    {
        Rec(0x80043C60, { backup });
        return true;
    }
    void SetDestination(std::uint8_t menu) override { destination = menu; }
    void ShowMenu() override { hide = 0; }
};

void Screen()
{
    auto step = Read<std::uint8_t>();
    map::MapScreenState state { Read(), Read(), Read() };
    ScreenHost host(state, step);
    const auto result = map::UpdateScreen(host, state, step, host.fade);
    Words values { result.value_or(-9999), step, state.load_delay, state.scene_loaded,
        state.menu_result, host.row, host.backup };
    for (const auto value : host.rects)
        values.push_back(value);
    values.insert(values.end(), { host.hud, host.destination, host.hide });
    Print(values, host.calls);
}
} // namespace

int main()
{
    int mode = 0;
    while (std::cin >> mode) {
        if (mode == kCamera) {
            Camera();
        } else if (mode == kMenu) {
            Menu();
        } else if (mode == kScreen) {
            Screen();
        } else {
            Packets(mode);
        }
    }
}
