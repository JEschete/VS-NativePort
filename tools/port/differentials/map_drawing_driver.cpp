// Native side of the map drawing differentials (map_drawing_common.py, #276).
// One vector per line: the mode, then that mode's inputs as
// map_drawing_common.py lists them. Prints what the original's observation
// holds: the globals it writes, the primitive cursor's advance, every linked
// packet's fields in link order and the recorded host calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/MapDrawing.hpp"

namespace {
namespace map = VS::Menu::map;

enum Mode : int {
    kVisited,
    kRoom,
    kBackdrop,
    kControls,
    kCompass,
    kCorners,
    kLabels,
};

enum Call : std::int64_t {
    kCallAddPrim = 1,
    kCallRotTransPers,
    kCallDepthCue,
    kCallShould,
    kCallDoorState,
    kCallFeature,
    kCallQueue,
    kCallAshley,
    kCallGetGeom,
    kCallSetGeom,
    kCallRoom,
    kCallLabels,
    kCallTextBox,
    kCallButton,
    kCallText,
    kCallBackdrop,
    kCallCompass,
    kCallSine,
    kCallCosine,
    kCallDoorName,
    kCallAreaName,
};

constexpr std::int64_t kRefused = -999;
constexpr std::int64_t kBackdropSlot = 0x1C;
constexpr std::int64_t kTextSlot = 0x18;
constexpr std::int64_t kAshleySlot = 0x20;
constexpr std::int64_t kPrimsLow = 0x500000;
constexpr std::uint32_t kOtPreset = 0x0412345C;
constexpr std::uint32_t kTexturePageWord = 0xE1000303;
constexpr std::uint32_t kMix = 0x9E3779B1U;
constexpr std::uint32_t kSineKey = 0x1234567;
constexpr std::uint32_t kCosineKey = 0x7654321;
constexpr std::size_t kRowCount = 16;
constexpr std::size_t kShouldWords = 8;
constexpr std::int32_t kLabelPreset = 5;

using Calls = std::vector<std::vector<std::int64_t>>;
using Values = std::vector<std::int64_t>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

std::int32_t Trig(std::int32_t angle, std::uint32_t key)
{
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(angle) * kMix + key) >> 19) - 4096;
}

map::Rgb DepthCue(const map::Rgb& color, std::int32_t p)
{
    return { .r = static_cast<std::uint8_t>(color.r * 3 + p),
        .g = static_cast<std::uint8_t>(color.g + (p >> 2)),
        .b = static_cast<std::uint8_t>(color.b ^ p) };
}

void Print(const Values& values, const Calls& calls)
{
    for (const std::int64_t item : values)
        std::cout << item << ' ';
    std::cout << calls.size();
    for (const auto& call : calls)
        for (const std::int64_t word : call)
            std::cout << ' ' << word;
    std::cout << '\n';
}

void Link(Calls& calls, std::int64_t& cursor, std::int64_t slot, std::int64_t bytes)
{
    calls.push_back({ kCallAddPrim, slot, cursor });
    cursor += bytes;
}

void PutColor(Values& packets, const map::Rgb& color) { packets.insert(packets.end(), { color.r, color.g, color.b }); }

void PutPoint(Values& packets, const map::ScreenPoint& point) { packets.insert(packets.end(), { point.x, point.y }); }

// --------------------------------------------------------------------- visited

int RunVisited()
{
    map::ScreenOffset geom;
    map::ScreenOffset previous;
    std::int32_t count = 0;
    std::size_t rooms_count = 0;
    if (!Read(geom.x) || !Read(geom.y) || !Read(previous.x) || !Read(previous.y) || !Read(count)
        || !Read(rooms_count))
        return 1;
    std::vector<map::MapRoom> rooms(rooms_count);
    std::vector<std::int32_t> data(rooms_count);
    for (std::size_t index = 0; index < rooms_count; ++index)
        if (!Read(rooms[index].visited) || !Read(data[index]) || !Read(rooms[index].zone_id)
            || !Read(rooms[index].map_id))
            return 1;

    class Host final : public map::VisitedRoomsHost {
    public:
        Calls calls;
        std::int32_t label_count = kLabelPreset;
        map::ScreenOffset previous;
        std::span<const std::int32_t> data;

        map::ScreenOffset GetScreenOffset() override
        {
            calls.push_back({ kCallGetGeom });
            return previous;
        }
        void SetScreenOffset(const map::ScreenOffset& offset) override
        {
            calls.push_back({ kCallSetGeom, offset.x, offset.y });
        }
        void ClearLabels() override { label_count = 0; }
        void DrawRoom(std::size_t index, std::int32_t zone_id, std::int32_t map_id) override
        {
            calls.push_back({ kCallRoom, static_cast<std::int64_t>(index), data[index], zone_id, map_id });
        }
        void DrawLabels() override { calls.push_back({ kCallLabels }); }
    } host;
    host.previous = previous;
    host.data = data;
    map::DrawVisitedRooms(host, geom.x, geom.y, rooms);
    Print({ host.label_count }, host.calls);
    return 0;
}

// ------------------------------------------------------------------------ room

class RoomHost final : public map::RoomDrawHost {
public:
    Calls calls;
    Values packets;
    std::int64_t cursor = 0;
    std::vector<map::Projection> projections;
    std::size_t next = 0;
    std::array<std::uint32_t, kShouldWords> should {};

    map::Projection Project(const map::RoomVertex& vertex) override
    {
        calls.push_back({ kCallRotTransPers, vertex.x, vertex.y, vertex.z });
        return projections.at(next++);
    }
    map::Rgb DepthCue(const map::Rgb& color, std::int32_t interpolation) override
    {
        calls.push_back({ kCallDepthCue, color.r, color.g, color.b, interpolation });
        return ::DepthCue(color, interpolation);
    }
    void AddTriangle(const map::RoomTriangle& triangle, std::int32_t depth) override
    {
        Link(calls, cursor, std::int64_t { depth } * 4, 0x1C);
        packets.insert(packets.end(), { 6, 0x30 });
        for (std::size_t corner = 0; corner < 3; ++corner) {
            PutColor(packets, triangle.colors[corner]);
            PutPoint(packets, triangle.corners[corner]);
        }
    }
    void AddQuad(const map::RoomQuad& quad, std::int32_t depth) override
    {
        Link(calls, cursor, std::int64_t { depth } * 4, 0x24);
        packets.insert(packets.end(), { 8, 0x38 });
        for (std::size_t corner = 0; corner < 4; ++corner) {
            PutColor(packets, quad.colors[corner]);
            PutPoint(packets, quad.corners[corner]);
        }
    }
    void AddEdge(const map::RoomEdge& edge, std::int32_t depth) override
    {
        Link(calls, cursor, std::int64_t { depth } * 4, 0x14);
        packets.insert(packets.end(), { 4, edge.semi_transparent ? 0x52 : 0x50 });
        PutColor(packets, edge.from_color);
        PutPoint(packets, edge.from);
        PutColor(packets, edge.to_color);
        PutPoint(packets, edge.to);
    }
    void AddTile(const map::RoomTile& tile, std::int32_t depth) override
    {
        Link(calls, cursor, std::int64_t { depth } * 4, 0x10);
        packets.insert(packets.end(), { 3, 0x60 });
        PutColor(packets, tile.color);
        PutPoint(packets, tile.position);
        packets.insert(packets.end(), { tile.width, tile.height });
    }
    bool ShouldDrawDoorState(std::int32_t state_id, std::int32_t zone_id, std::int32_t map_id) override
    {
        calls.push_back({ kCallShould, state_id, zone_id, map_id });
        const auto bit = static_cast<std::uint32_t>(state_id) & 0xFFU;
        return ((should.at(bit / 32) >> (bit % 32)) & 1U) != 0;
    }
    void DrawDoorState(std::int32_t x, std::int32_t y, std::int32_t state_id) override
    {
        calls.push_back({ kCallDoorState, x, y, state_id });
    }
    void DrawFeature(std::int32_t icon, std::int32_t x, std::int32_t y) override
    {
        calls.push_back({ kCallFeature, icon, x, y });
    }
    void QueueLabel(std::int32_t x, std::int32_t y, std::int32_t kind, std::int32_t label_id) override
    {
        calls.push_back({ kCallQueue, x, y, kind, label_id });
    }
    void DrawAshley(std::int32_t position, std::int32_t brightness) override
    {
        calls.push_back({ kCallAshley, 0, position, brightness, kAshleySlot });
    }
};

int RunRoom()
{
    map::RoomContext context;
    std::array<std::int32_t, map::kAshleyBrightnessSteps> brightness {};
    RoomHost host;
    if (!Read(context.room_index) || !Read(context.current_room) || !Read(context.scene_room)
        || !Read(context.entered_door) || !Read(context.frame_counter) || !Read(context.zone_id)
        || !Read(context.map_id))
        return 1;
    for (std::int32_t& step : brightness)
        if (!Read(step))
            return 1;
    for (std::uint32_t& bits : host.should)
        if (!Read(bits))
            return 1;
    map::RoomWorkspace workspace {};
    for (std::size_t index = 0; index < kRowCount; ++index) {
        map::ProjectedVertex& row = workspace.at(index);
        if (!Read(row.x) || !Read(row.y) || !Read(row.depth) || !Read(row.interpolation))
            return 1;
    }
    std::size_t projections = 0;
    if (!Read(projections))
        return 1;
    host.projections.resize(projections);
    for (map::Projection& projection : host.projections)
        if (!Read(projection.screen.x) || !Read(projection.screen.y) || !Read(projection.depth)
            || !Read(projection.interpolation))
            return 1;
    std::size_t length = 0;
    if (!Read(length))
        return 1;
    std::vector<std::byte> room(length);
    for (std::byte& item : room)
        if (!Read(item))
            return 1;
    context.brightness = brightness;

    const map::RoomDrawResult result = map::DrawRoom(host, room, context, workspace);
    if (result == map::RoomDrawResult::kMalformed) {
        Print({ kRefused }, {});
        return 0;
    }
    Values values { result == map::RoomDrawResult::kDrawn ? 1 : 0, host.cursor };
    for (std::size_t index = 0; index < kRowCount; ++index) {
        const map::ProjectedVertex& row = workspace.at(index);
        values.insert(values.end(), { row.x, row.y, row.depth, row.interpolation });
    }
    values.insert(values.end(), host.packets.begin(), host.packets.end());
    Print(values, host.calls);
    return 0;
}

// -------------------------------------------------------------------- backdrop

int RunBackdrop()
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    if (!Read(x) || !Read(y) || !Read(width) || !Read(height))
        return 1;

    class Host final : public map::BackdropHost {
    public:
        Calls calls;
        Values packets;
        std::int64_t cursor = 0;

        void AddBackdropLine(const map::BackdropLine& line) override
        {
            Link(calls, cursor, kBackdropSlot, 0x14);
            packets.insert(packets.end(), { 4, 0x50 });
            PutColor(packets, map::kBackdropLeftColor);
            PutPoint(packets, line.from);
            PutColor(packets, map::kBackdropRightColor);
            PutPoint(packets, line.to);
        }
        void AddBackdropShadow(const std::array<map::ScreenPoint, map::kQuadCorners>& corners) override
        {
            Link(calls, cursor, kBackdropSlot, 0x18);
            packets.insert(packets.end(), { 5, 0x28, 0, 0, 0 });
            for (const map::ScreenPoint& corner : corners)
                PutPoint(packets, corner);
        }
    } host;
    if (!map::DrawControlBackdrop(host, x, y, width, height)) {
        Print({ kRefused }, {});
        return 0;
    }
    Values values { host.cursor };
    values.insert(values.end(), host.packets.begin(), host.packets.end());
    Print(values, host.calls);
    return 0;
}

// -------------------------------------------------------------------- controls

int RunControls()
{
    std::int32_t animation = 0;
    std::int32_t offset = 0;
    std::int32_t box = 0;
    std::int16_t box_y = 0;
    std::int16_t box_indent = 0;
    std::int32_t hide = 0;
    std::int32_t paling = 0;
    std::int32_t commands = 0;
    std::int16_t angle = 0;
    if (!Read(animation) || !Read(offset) || !Read(box) || !Read(box_y) || !Read(box_indent) || !Read(hide)
        || !Read(paling) || !Read(commands) || !Read(angle))
        return 1;

    class Host final : public map::ControlsHost {
    public:
        Calls calls;
        bool box = false;
        std::int16_t box_y = 0;
        std::int16_t box_indent = 0;
        bool jammed = false;
        std::int64_t cursor = 0;
        std::uint32_t ot = kOtPreset;
        std::array<std::uint32_t, 3> words {};

        void ShiftTextBox(std::int32_t delta) override
        {
            if (!box)
                return;
            box_y = static_cast<std::int16_t>(box_y + delta);
            box_indent = static_cast<std::int16_t>(box_indent + delta);
        }
        std::optional<bool> IsJammed() override { return jammed; }
        void DrawButton(map::ControlButton button, std::int32_t x, std::int32_t y) override
        {
            calls.push_back({ kCallButton, static_cast<std::int64_t>(button), x, y, kTextSlot });
        }
        void DrawCaption(map::ControlCaption caption, std::int32_t packed_xy, std::uint32_t color) override
        {
            calls.push_back({ kCallText, static_cast<std::int64_t>(caption), packed_xy, color, kTextSlot });
        }
        void DrawBackdrop(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) override
        {
            calls.push_back({ kCallBackdrop, x, y, width, height });
        }
        void AddTexturePage() override
        {
            words = { 0x02000000U | (ot & 0xFFFFFFU), kTexturePageWord, 0 };
            ot = (ot & 0xFF000000U) | static_cast<std::uint32_t>(kPrimsLow + cursor);
            cursor += 0xC;
        }
        void DrawCompass(std::int32_t x, std::int32_t y, std::int32_t compass_angle) override
        {
            calls.push_back({ kCallCompass, x, y, compass_angle });
        }
    } host;
    host.calls.push_back({ kCallTextBox, 0 });
    host.box = box != 0;
    host.box_y = box_y;
    host.box_indent = box_indent;
    host.jammed = paling != 0;
    map::ControlsState state { .animation_state = static_cast<std::int16_t>(animation), .x_offset = offset };
    const map::ControlsFrame frame {
        .hide_ui = hide != 0, .commands_state = static_cast<std::int16_t>(commands), .compass_angle = angle
    };
    if (!map::DrawControls(host, state, frame)) {
        Print({ kRefused }, {});
        return 0;
    }
    Values values { state.animation_state, state.x_offset, host.box_y, host.box_indent, host.cursor,
        static_cast<std::int32_t>(host.ot) };
    for (const std::uint32_t item : host.words)
        values.push_back(static_cast<std::int32_t>(item));
    Print(values, host.calls);
    return 0;
}

// --------------------------------------------------------------------- compass

class TrigHost : public map::CompassHost {
public:
    Calls calls;
    Values packets;
    std::int64_t cursor = 0;

    std::int32_t Sine(std::int32_t angle) override
    {
        calls.push_back({ kCallSine, angle });
        return Trig(angle, kSineKey);
    }
    std::int32_t Cosine(std::int32_t angle) override
    {
        calls.push_back({ kCallCosine, angle });
        return Trig(angle, kCosineKey);
    }
    void AddCompassQuad(const map::CompassQuad& quad) override
    {
        Link(calls, cursor, quad.slot_bytes, 0x28);
        packets.insert(packets.end(), { 9, quad.semi_transparent ? 0x2F : 0x2D });
        for (std::size_t corner = 0; corner < map::kQuadCorners; ++corner) {
            PutPoint(packets, quad.quad.corners[corner]);
            packets.insert(packets.end(), { quad.quad.texture[corner].u, quad.quad.texture[corner].v });
        }
        packets.insert(packets.end(), { quad.quad.clut, quad.quad.texture_page });
    }
};

void PutCorners(Values& values, const map::CompassCorners& corners)
{
    for (const map::CompassPoint& corner : corners)
        values.insert(values.end(), { corner.x, corner.y });
}

int RunCompass()
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t angle = 0;
    std::int16_t roll = 0;
    if (!Read(x) || !Read(y) || !Read(angle) || !Read(roll))
        return 1;
    TrigHost host;
    const map::CompassCorners needle = map::DrawCompass(host, x, y, angle, roll);
    Values values { host.cursor };
    PutCorners(values, needle);
    values.insert(values.end(), host.packets.begin(), host.packets.end());
    Print(values, host.calls);
    return 0;
}

int RunCorners()
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t angle = 0;
    std::int32_t sine = 0;
    std::int32_t cosine = 0;
    if (!Read(x) || !Read(y) || !Read(width) || !Read(height) || !Read(angle) || !Read(sine) || !Read(cosine))
        return 1;
    // _computeRotatedRect asks rsin and rcos itself; the typed core takes
    // their answers.
    const Calls calls { { kCallSine, angle }, { kCallCosine, angle } };
    Values values;
    PutCorners(values, map::ComputeCompassCorners(x, y, width, height, sine, cosine));
    Print(values, calls);
    return 0;
}

// ---------------------------------------------------------------------- labels

int RunLabels()
{
    std::int16_t commands = 0;
    std::int32_t hide = 0;
    std::int32_t count = 0;
    if (!Read(commands) || !Read(hide) || !Read(count))
        return 1;
    std::array<map::MapLabel, map::kMapLabelCapacity> labels {};
    for (map::MapLabel& label : labels)
        if (!Read(label.x) || !Read(label.y) || !Read(label.kind) || !Read(label.id))
            return 1;

    class Host final : public map::LabelDrainHost {
    public:
        Calls calls;

        void DrawDoorLabel(std::int32_t x, std::int32_t y, std::int32_t door_id) override
        {
            calls.push_back({ kCallDoorName, x, y, door_id });
        }
        void DrawAreaLabel(std::int32_t x, std::int32_t y, std::int32_t area_id) override
        {
            calls.push_back({ kCallAreaName, x, y, area_id });
        }
    } host;
    if (!map::DrawLabels(host, commands, hide != 0, labels, count)) {
        Print({ kRefused }, {});
        return 0;
    }
    Print({}, host.calls);
    return 0;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        int status = 1;
        switch (mode) {
        case kVisited:
            status = RunVisited();
            break;
        case kRoom:
            status = RunRoom();
            break;
        case kBackdrop:
            status = RunBackdrop();
            break;
        case kControls:
            status = RunControls();
            break;
        case kCompass:
            status = RunCompass();
            break;
        case kCorners:
            status = RunCorners();
            break;
        case kLabels:
            status = RunLabels();
            break;
        default:
            break;
        }
        if (status != 0)
            return 1;
    }
    return 0;
}
