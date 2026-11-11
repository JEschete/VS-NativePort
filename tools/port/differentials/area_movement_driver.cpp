// Native side of tools\port\differentials\area_movement.py (F01740): one vector per stdin
// line, one line of results per vector. The world is area_route's vector world; the case
// maps the returned movement step to the calls UpdateLiveNavigationAreaMovement makes.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "area_route_world.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }

int main()
{
    using namespace differential;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        VectorWorld world;
        world.ends = ReadEnds(in);
        world.contacts = ReadContacts(in);
        world.ReadMaps(in);
        AreaMovementActor actor { .route = ReadActor(in) };
        actor.is_stuck = Read<std::uint8_t>(in);
        actor.near_target_detour = Read<std::uint8_t>(in);
        actor.height_filtered_movement = Read<std::uint8_t>(in);
        actor.area_lock = Read<std::uint8_t>(in);
        const auto step = UpdateNavigationAreaMovement(world, actor);
        std::ostringstream out;
        out << (step ? 0 : 1) << ' ' << (step ? static_cast<int>(step->action) : -1)
            << ' ' << (step ? step->value : 0) << ' '
            << (step && step->clear_direction_cache ? 1 : 0);
        WriteActor(out, actor.route);
        out << ' ' << int { actor.is_stuck } << ' ' << int { actor.near_target_detour }
            << ' ' << int { actor.height_filtered_movement } << ' '
            << int { actor.area_lock };
        world.WriteCalls(out);
        std::cout << out.str() << '\n';
    }
    return 0;
}
