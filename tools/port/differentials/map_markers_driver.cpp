// Native side of the map marker and label differentials (map_markers_common.py, #276).
// One vector per line: mode, x, y, x1, y1, id, kind, _mapLabelY, the label count, the state
// flag, the sigil answer, zone, map, the caller primitive (0 for none), the four icon RECTs,
// the two upper and two lower door-state RECTs, the 40-entry name table, the two area flag
// words and the eight labels' four halfwords. Prints the result, _mapLabelY, the count and
// labels, the sprite's texture and colour words, the row background width, row 0's state,
// the primitive cursor's advance, every linked packet's fields and the host calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/MapMarkers.hpp"

namespace {
namespace map = VS::Menu::map;

enum Mode : int {
    kAshley,
    kFeature,
    kDoorState,
    kDoorLabel,
    kLine,
    kAreaLabel,
    kQueue,
    kShould,
};

enum Call : std::int64_t {
    kCallAddPrim = 1,
    kCallSprite,
    kCallSetItem,
    kCallRenderItem,
    kCallGetItem,
    kCallSigil,
};

constexpr std::int64_t kRefused = -999;
constexpr std::int64_t kQuadSlot = 28;
constexpr std::int64_t kLineSlot = 32;
constexpr std::int64_t kQuadBytes = 40;
constexpr std::int64_t kLineBytes = 16;
constexpr std::int64_t kSpriteBytes = 24;
constexpr std::int64_t kItemPreset = 0xEE;
constexpr std::int64_t kStatePreset = 0xFF;
constexpr std::size_t kNames = 40;
constexpr std::int32_t kCallerPrimitive = static_cast<std::int32_t>(0x80528000U);

class DriverHost final : public map::MapMarkerHost {
public:
    std::vector<std::vector<std::int64_t>> calls;
    std::vector<std::int64_t> packets;
    std::int64_t cursor = 0;
    std::int64_t background = kItemPreset;
    std::int64_t state = kStatePreset;
    std::uint32_t sprite_texture = 0;
    std::uint32_t sprite_color = 0;
    std::int32_t sigil = 0;

    void AddMarkerQuad(const map::MarkerQuad& quad) override
    {
        calls.push_back({ kCallAddPrim, kQuadSlot, cursor });
        cursor += kQuadBytes;
        packets.insert(packets.end(), { 9, 0x2D });
        for (const map::ScreenPoint& corner : quad.corners)
            packets.insert(packets.end(), { corner.x, corner.y });
        for (const map::TexturePoint& point : quad.texture)
            packets.insert(packets.end(), { point.u, point.v });
        packets.insert(packets.end(), { quad.clut, quad.texture_page });
    }

    void AddLabelLine(const map::LabelLine& line) override
    {
        calls.push_back({ kCallAddPrim, kLineSlot, cursor });
        cursor += kLineBytes;
        packets.insert(packets.end(),
            { 3, map::kLabelLineShade, map::kLabelLineShade, map::kLabelLineShade, 0x40,
                line.from.x, line.from.y, line.to.x, line.to.y });
    }

    void AddAshleyMarker(const map::AshleyMarkerSprite& sprite) override
    {
        calls.push_back({ kCallSprite, sprite.brightness_transparency, sprite.packed_position,
            sprite.packed_size, sprite.list == map::SpriteList::kOverlay ? 0 : 1 });
        cursor = kSpriteBytes;
        sprite_texture = sprite.texture_word;
        sprite_color = sprite.color_word;
    }

    void DrawLabelText(const map::LabelText& text) override
    {
        calls.push_back({ kCallSetItem, 0, text.x, text.y, text.width, 0,
            text.table == map::LabelTextTable::kDoorNames ? 0 : 1,
            static_cast<std::int64_t>(text.text_offset) });
        background = text.background_width;
        calls.push_back({ kCallRenderItem, 1 });
        calls.push_back({ kCallGetItem, 0 });
        state = 0;
    }

    bool IsSigilDoorUnlocked(std::int32_t zone_id, std::int32_t map_id, std::uint32_t kind) override
    {
        calls.push_back({ kCallSigil, zone_id, map_id, static_cast<std::int64_t>(kind) });
        return sigil != 0;
    }
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadRect(map::MarkerRect& rect)
{
    return Read(rect.x) && Read(rect.y) && Read(rect.w) && Read(rect.h);
}

int RunVector(int mode)
{
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t x1 = 0;
    std::int32_t y1 = 0;
    std::int32_t id = 0;
    std::int32_t kind = 0;
    std::int32_t label_y = 0;
    std::int32_t count = 0;
    std::int32_t flag = 0;
    std::int32_t sigil = 0;
    std::int32_t zone = 0;
    std::int32_t map_id = 0;
    std::int32_t caller = 0;
    if (!Read(x) || !Read(y) || !Read(x1) || !Read(y1) || !Read(id) || !Read(kind)
        || !Read(label_y) || !Read(count) || !Read(flag) || !Read(sigil) || !Read(zone)
        || !Read(map_id) || !Read(caller))
        return 1;
    std::array<map::MarkerRect, 4> icons {};
    std::array<map::MarkerRect, 2> upper {};
    std::array<map::MarkerRect, 2> lower {};
    for (map::MarkerRect& rect : icons)
        if (!ReadRect(rect))
            return 1;
    for (map::MarkerRect& rect : upper)
        if (!ReadRect(rect))
            return 1;
    for (map::MarkerRect& rect : lower)
        if (!ReadRect(rect))
            return 1;
    std::array<std::uint16_t, kNames> names {};
    for (std::uint16_t& name : names)
        if (!Read(name))
            return 1;
    std::array<std::uint32_t, 2> area_flags {};
    for (std::uint32_t& flags : area_flags)
        if (!Read(flags))
            return 1;
    std::array<map::MapLabel, map::kMapLabelCapacity> labels {};
    for (map::MapLabel& label : labels)
        if (!Read(label.x) || !Read(label.y) || !Read(label.kind) || !Read(label.id))
            return 1;

    DriverHost host;
    host.sigil = sigil;
    std::int64_t result = 0;
    const auto state_flag = static_cast<std::uint8_t>(flag);
    switch (mode) {
    case kAshley:
        map::DrawAshleyMarker(host, id, x, y,
            caller == kCallerPrimitive ? map::SpriteList::kCaller : map::SpriteList::kOverlay);
        break;
    case kFeature:
        if (!map::DrawFeatureMarker(host, icons, id, x, y))
            result = kRefused;
        break;
    case kDoorState:
        if (!map::DrawDoorStateMarker(host, upper, lower, labels, count, x, y, id, state_flag))
            result = kRefused;
        break;
    case kDoorLabel:
        if (!map::DrawDoorLabel(host, names, label_y, x, y, id))
            result = kRefused;
        break;
    case kLine:
        map::AppendLabelLine(host, x, y, x1, y1);
        break;
    case kAreaLabel:
        if (!map::DrawConnectingAreaLabel(host, names, area_flags, label_y, x, y, id))
            result = kRefused;
        break;
    case kQueue:
        if (!map::QueueLabel(labels, count, x, y, kind, id))
            result = kRefused;
        break;
    case kShould:
        result = map::ShouldDrawDoorStateMarker(host, id, zone, map_id, state_flag) ? 1 : 0;
        break;
    default:
        return 1;
    }

    std::cout << result << ' ' << label_y << ' ' << count;
    for (const map::MapLabel& label : labels)
        std::cout << ' ' << label.x << ' ' << label.y << ' ' << label.kind << ' ' << label.id;
    std::cout << ' ' << host.sprite_texture << ' ' << host.sprite_color << ' ' << host.background
              << ' ' << host.state << ' ' << host.cursor;
    for (const std::int64_t item : host.packets)
        std::cout << ' ' << item;
    std::cout << ' ' << host.calls.size();
    for (const auto& call : host.calls)
        for (const std::int64_t word : call)
            std::cout << ' ' << word;
    std::cout << '\n';
    return 0;
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
