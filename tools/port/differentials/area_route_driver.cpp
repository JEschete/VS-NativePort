// Native side of tools\port\differentials\area_route.py (F01721-F01728): one vector per stdin
// line, one line of results per vector. The world is the vector's room, maps and platforms; the
// tile and exit words are composed as AreaRouteLive.cpp composes them.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/AreaRoute.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) {}

namespace {
using namespace VS::Battle::Ai;
using VS::Field::PlatformContact;

constexpr int kRefused = -2;
constexpr int kGrid = kNavigationGridSide;

template <typename T>
T Read(std::istringstream& in) {
  long long value = 0;
  in >> value;
  if (!in) std::abort();
  return static_cast<T>(value);
}

std::array<RouteGateEnd, kRouteGateEndCount> ReadEnds(std::istringstream& in) {
  std::array<RouteGateEnd, kRouteGateEndCount> ends{};
  for (auto& end : ends) {
    const auto value = Read<std::uint32_t>(in);
    end = {.x = static_cast<std::uint8_t>(value & 31), .z = static_cast<std::uint8_t>((value >> 5) & 31)};
  }
  return ends;
}

std::vector<PlatformContact> ReadContacts(std::istringstream& in) {
  const auto count = Read<std::size_t>(in);
  std::vector<PlatformContact> reported;
  for (std::size_t report = 0; report < 4; ++report) {
    PlatformContact contact{.x = Read<std::int16_t>(in),
                            .height = Read<std::int16_t>(in),
                            .z = Read<std::int16_t>(in),
                            .platform_index = Read<std::int16_t>(in)};
    if (report < count) reported.push_back(contact);
  }
  return reported;
}

class VectorWorld final : public AreaRouteWorld {
 public:
  std::array<RouteGateEnd, kRouteGateEndCount> ends{};
  std::vector<PlatformContact> contacts;
  std::array<std::uint32_t, 256> routes{};
  std::array<std::uint16_t, kGrid * kGrid> flags{}, heights{};
  int width = 0;
  std::vector<std::uint8_t> regions;
  NavigationDirectionCodes codes{};
  std::array<std::uint8_t, 4> steps{};
  std::array<int, 4> dwell{};
  std::uint8_t closed = 0;
  std::array<bool, 16> actors{};
  std::array<AreaRoutePoint, 16> positions{};
  std::array<int, 2> occupancy{};

  std::vector<std::array<std::uint32_t, 3>> occupancy_calls;
  std::vector<int> contact_calls, dwell_calls;
  int collect_calls = 0;

  [[nodiscard]] std::optional<AreaRouteEndpoints> Route(int index) const override {
    const auto word = routes.at(static_cast<std::size_t>(index));
    return AreaRouteEndpoints{static_cast<std::uint16_t>(word), static_cast<std::uint16_t>(word >> 16)};
  }
  [[nodiscard]] bool HasTileMaps() const override { return true; }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return flags.at(static_cast<std::size_t>((z * kGrid) + x));
  }
  [[nodiscard]] std::uint16_t TileHeight(int x, int z) const override {
    return heights.at(static_cast<std::size_t>((z * kGrid) + x));
  }
  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(int x, int z) const override {
    const auto cell = static_cast<std::size_t>((z * width) + x);
    if (x < 0 || x >= width || cell >= regions.size()) return std::nullopt;
    return regions[cell];
  }
  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override { return codes; }
  [[nodiscard]] std::uint8_t RouteStepDirection(int kind) const override {
    return steps.at(static_cast<std::size_t>(kind));
  }
  [[nodiscard]] std::optional<std::array<RouteGateEnd, kRouteGateEndCount>> RouteGateEnds() const override {
    return ends;
  }
  [[nodiscard]] std::vector<PlatformContact> PlatformPositions() const override {
    ++const_cast<VectorWorld*>(this)->collect_calls;
    return contacts;
  }
  [[nodiscard]] std::optional<std::uint16_t> PlatformDwellTicks(int platform) const override {
    const_cast<VectorWorld*>(this)->dwell_calls.push_back(platform);
    const int ticks = dwell.at(static_cast<std::size_t>(platform));
    if (ticks < 0) return std::nullopt;
    return static_cast<std::uint16_t>(ticks);
  }
  [[nodiscard]] std::optional<std::uint8_t> ClosedGates() const override { return closed; }
  void SetContactOccupancy(int bit) override { contact_calls.push_back(bit); }
  [[nodiscard]] std::optional<int> DestinationOccupancy(const AreaRouteActor& actor,
                                                        std::uint32_t tile) override {
    occupancy_calls.push_back({tile, actor.area, actor.target_distance_filter});
    return occupancy.at(occupancy_calls.size() > 1 ? 1 : 0);
  }
  [[nodiscard]] bool HasActor(int slot) const override { return actors.at(static_cast<std::size_t>(slot)); }
  [[nodiscard]] std::optional<AreaRoutePoint> ActorPosition(int slot) const override {
    return positions.at(static_cast<std::size_t>(slot));
  }
};

AreaRouteActor ReadActor(std::istringstream& in) {
  AreaRouteActor actor;
  actor.target_distance_filter = Read<std::uint8_t>(in);
  actor.tile = Read<std::uint32_t>(in);
  actor.x = Read<std::int16_t>(in);
  actor.z = Read<std::int16_t>(in);
  actor.actor_id = Read<std::uint8_t>(in);
  actor.turn_timer = Read<std::uint16_t>(in);
  actor.turn_period = Read<std::uint16_t>(in);
  actor.area = Read<std::uint8_t>(in);
  actor.movement_target = Read<std::uint8_t>(in);
  actor.destination = Read<std::uint32_t>(in);
  actor.next_tile = Read<std::uint32_t>(in);
  actor.move_x = Read<std::int32_t>(in);
  actor.move_z = Read<std::int32_t>(in);
  actor.heading = Read<std::int16_t>(in);
  actor.angle = Read<std::int16_t>(in);
  actor.route_gate = Read<std::uint8_t>(in);
  actor.route_exit = Read<std::uint32_t>(in);
  return actor;
}

void WriteActor(std::ostringstream& out, const AreaRouteActor& actor) {
  out << ' ' << int{actor.target_distance_filter} << ' ' << actor.tile << ' ' << actor.x << ' ' << actor.z << ' '
      << int{actor.actor_id} << ' ' << actor.turn_timer << ' ' << actor.turn_period << ' ' << int{actor.area} << ' '
      << int{actor.movement_target} << ' ' << actor.destination << ' ' << actor.next_tile << ' ' << actor.move_x
      << ' ' << actor.move_z << ' ' << actor.heading << ' ' << actor.angle << ' ' << int{actor.route_gate} << ' '
      << actor.route_exit;
}

std::string AreaRoute(std::istringstream& in) {
  VectorWorld world;
  world.ends = ReadEnds(in);
  world.contacts = ReadContacts(in);
  const int area = Read<int>(in);
  const int index = Read<int>(in);
  for (auto& route : world.routes) route = Read<std::uint32_t>(in);
  for (auto& cell : world.flags) cell = Read<std::uint16_t>(in);
  for (auto& cell : world.heights) cell = Read<std::uint16_t>(in);
  world.width = Read<int>(in);
  world.regions.resize(static_cast<std::size_t>(world.width * kGrid));
  for (auto& cell : world.regions) cell = Read<std::uint8_t>(in);
  for (auto& code : world.codes) code = Read<std::uint8_t>(in);
  for (auto& step : world.steps) step = Read<std::uint8_t>(in);
  for (auto& ticks : world.dwell) ticks = Read<int>(in);
  world.closed = Read<std::uint8_t>(in);
  for (auto&& present : world.actors) present = Read<int>(in) != 0;
  for (auto& position : world.positions) {
    position.x = Read<std::int16_t>(in);
    position.z = Read<std::int16_t>(in);
  }
  for (auto& value : world.occupancy) value = Read<int>(in);
  auto actor = ReadActor(in);
  const auto intent = ResolveNavigationAreaRoute(world, actor, index, area);
  std::ostringstream out;
  out << (intent ? static_cast<int>(*intent) : kRefused);
  WriteActor(out, actor);
  out << ' ' << world.occupancy_calls.size();
  for (const auto& call : world.occupancy_calls) out << ' ' << call[0] << ' ' << call[1] << ' ' << call[2];
  out << ' ' << world.contact_calls.size();
  for (const int bit : world.contact_calls) out << ' ' << bit;
  out << ' ' << world.dwell_calls.size();
  for (const int platform : world.dwell_calls) out << ' ' << platform;
  out << ' ' << world.collect_calls;
  return out.str();
}
}  // namespace

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    const int kind = Read<int>(in);
    std::ostringstream out;
    if (kind == 0) {
      // ResolveLiveRouteGateExit: the exit's three bytes, or the whole entry word.
      const auto ends = ReadEnds(in);
      const auto entry = Read<std::uint32_t>(in);
      auto exit = Read<std::uint32_t>(in);
      if (const auto resolved = ResolveRouteGateExit(ends, entry)) {
        exit = (exit & 0xFF000000u) | (std::uint32_t{resolved->z} << 16) | (std::uint32_t{resolved->platform} << 8) |
               resolved->x;
      } else {
        exit = entry;
      }
      out << exit;
    } else if (kind == 1) {
      // FindLiveNearestRouteGateEntry: bytes 0 and 2 only.
      const auto ends = ReadEnds(in);
      const int platform = Read<int>(in);
      const auto world_x = Read<std::int32_t>(in);
      const auto world_z = Read<std::int32_t>(in);
      auto tile = Read<std::uint32_t>(in);
      const auto end = FindNearestRouteGateEntry(ends, platform, world_x, world_z);
      if (!end) std::abort();
      tile = (tile & 0xFF00FF00u) | (std::uint32_t{end->z} << 16) | end->x;
      out << tile;
    } else if (kind == 2) {
      const auto ends = ReadEnds(in);
      const auto contacts = ReadContacts(in);
      const auto tile = Read<std::uint32_t>(in);
      const auto match = Read<std::uint32_t>(in);
      const auto link = FindRouteGateAtTile(ends, contacts, tile, match != 0);
      out << (link ? *link : kRefused) << ' ' << 1;
    } else {
      out << AreaRoute(in);
    }
    std::cout << out.str() << '\n';
  }
  return 0;
}
