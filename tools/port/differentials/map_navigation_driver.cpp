// Native side of the map room navigation differentials (map_navigation_common.py, #276).
// One vector per line: mode, the room count word, the argument, the center's three halves,
// _currentRoomIndex, _isCurrentScene, the minimap scene's first room zone and map, then per
// room its visited word, zone, map, vertex count word and every vertex's four halves, then
// per room its previous and next links. Prints the result, the center, _isCurrentScene,
// _currentRoomIndex, per room its visited word and vertex halves, and the sound calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Field/MinimapScene.hpp"
#include "Menu/MapNavigation.hpp"

namespace {
namespace nav = VS::Menu::map;

enum Mode : int {
    kMark,
    kCenterOn,
    kSetTarget,
    kAnimate,
    kScale,
    kInitial,
    kAdjacent,
};

constexpr std::int32_t kRefused = -999;

class DriverHost final : public nav::MapNavigationHost {
public:
    std::vector<std::array<std::int32_t, 2>> calls;

    void PlaySound(std::int32_t sound, std::int32_t volume) override
    {
        calls.push_back({ sound, volume });
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

struct Room {
    nav::MapRoom record;
    std::int32_t vertex_count = 0;
    std::vector<nav::MapVertex> vertices;
    std::vector<std::int16_t> pads;
};

int RunVector(int mode)
{
    std::int32_t count = 0;
    std::int32_t argument = 0;
    nav::MapPoint center { };
    std::int32_t current_room = 0;
    std::int32_t is_current_scene = 0;
    VS::Field::SceneRoomAddress here { };
    if (!Read(count) || !Read(argument) || !Read(center.x) || !Read(center.y)
        || !Read(center.z) || !Read(current_room) || !Read(is_current_scene)
        || !Read(here.zone_id) || !Read(here.map_id))
        return 1;
    const std::size_t room_count = count > 0 ? static_cast<std::size_t>(count) : 0;
    std::vector<Room> rooms(room_count);
    for (Room& room : rooms) {
        if (!Read(room.record.visited) || !Read(room.record.zone_id)
            || !Read(room.record.map_id) || !Read(room.vertex_count))
            return 1;
        const std::size_t vertices =
            room.vertex_count > 0 ? static_cast<std::size_t>(room.vertex_count) : 0;
        room.vertices.resize(vertices);
        room.pads.resize(vertices);
        for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
            if (!Read(room.vertices[vertex].x) || !Read(room.vertices[vertex].y)
                || !Read(room.vertices[vertex].z) || !Read(room.pads[vertex]))
                return 1;
        }
    }
    std::vector<nav::RoomLink> links(room_count);
    for (nav::RoomLink& link : links) {
        if (!Read(link.previous) || !Read(link.next))
            return 1;
    }

    std::vector<nav::RoomVertices> views;
    std::vector<nav::MapRoom> records;
    for (Room& room : rooms) {
        views.emplace_back(room.vertices);
        records.push_back(room.record);
    }
    DriverHost host;
    std::int32_t result = 0;
    switch (mode) {
    case kMark:
        nav::MarkAllRoomsVisited(records);
        break;
    case kCenterOn:
        if (!nav::CenterOnRoom(views, static_cast<std::size_t>(argument), center))
            result = kRefused;
        break;
    case kSetTarget:
        if (argument < 0 || static_cast<std::size_t>(argument) >= views.size())
            return 1;
        center = nav::RoomCenterOf(views[static_cast<std::size_t>(argument)]);
        break;
    case kAnimate:
        nav::AnimateCentering(views, center);
        break;
    case kScale:
        nav::ScaleRoomVertices(views, argument);
        break;
    case kInitial: {
        const nav::InitialRoom initial = nav::InitialRoomOf(records, here);
        result = initial.selected;
        is_current_scene = initial.current;
        break;
    }
    case kAdjacent: {
        const auto selected =
            nav::SelectAdjacentVisitedRoom(host, links, records, current_room, argument);
        current_room = selected.has_value() ? *selected : kRefused;
        break;
    }
    default:
        return 1;
    }

    std::cout << result << ' ' << center.x << ' ' << center.y << ' ' << center.z << ' '
              << is_current_scene << ' ' << current_room;
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        std::cout << ' ' << records[index].visited;
        const Room& room = rooms[index];
        for (std::size_t vertex = 0; vertex < room.vertices.size(); ++vertex) {
            std::cout << ' ' << room.vertices[vertex].x << ' ' << room.vertices[vertex].y
                      << ' ' << room.vertices[vertex].z << ' ' << room.pads[vertex];
        }
    }
    std::cout << ' ' << host.calls.size();
    for (const auto& call : host.calls) {
        std::cout << ' ' << call[0] << ' ' << call[1];
    }
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
