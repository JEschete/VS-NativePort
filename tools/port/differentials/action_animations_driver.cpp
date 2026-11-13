#include <cstdint>
#include <iostream>

#include "TestSupport/ActionAnimationsFixture.hpp"

namespace t = VS::test_support::action_animations;
namespace {
std::int64_t Read()
{
    std::int64_t value { };
    std::cin >> value;
    return value;
}
void Emit(std::uint32_t value) { std::cout << ' ' << value; }
void ReadActor(t::Actor& actor)
{
    actor.flags = static_cast<std::uint32_t>(Read());
    actor.slot = static_cast<std::uint8_t>(Read());
    actor.jump = static_cast<std::uint8_t>(Read());
    actor.turn = static_cast<std::uint16_t>(Read());
    actor.position.x = static_cast<std::int16_t>(Read());
    actor.position.y = static_cast<std::int16_t>(Read());
    actor.position.z = static_cast<std::int16_t>(Read());
    actor.position.w = static_cast<std::int16_t>(Read());
    actor.facing = static_cast<std::int16_t>(Read());
    actor.delta = static_cast<std::int16_t>(Read());
    actor.radius = static_cast<std::uint16_t>(Read());
    actor.model = static_cast<std::uint16_t>(Read());
    actor.bones = static_cast<std::uint8_t>(Read());
    actor.sequence_model = static_cast<std::uint8_t>(Read());
    actor.common_entries = static_cast<std::uint8_t>(Read());
    actor.battle_entries = static_cast<std::uint8_t>(Read());
    actor.special_entries = static_cast<std::uint8_t>(Read());
    actor.map_present = Read() != 0;
}
void EmitActor(const t::Actor& actor)
{
    Emit(actor.flags);
    Emit(actor.slot);
    Emit(actor.jump);
    Emit(actor.turn);
    for (const auto value : { actor.position.x, actor.position.y, actor.position.z,
             actor.position.w, actor.facing, actor.delta }) {
        Emit(static_cast<std::uint16_t>(value));
    }
    Emit(actor.radius);
    Emit(actor.model);
    Emit(actor.bones);
    Emit(actor.sequence_model);
    Emit(actor.common_entries);
    Emit(actor.battle_entries);
    Emit(actor.special_entries);
    Emit(actor.map_present ? 1U : 0U);
}
} // namespace

int main()
{
    int function { };
    while (std::cin >> function) {
        t::World world;
        const auto slot = static_cast<int>(Read());
        const auto parameter = static_cast<int>(Read());
        const auto sequence = static_cast<int>(Read());
        world.table_owner = static_cast<int>(Read());
        const bool target_present = Read() != 0;
        world.controlled_helpers = Read() != 0;
        world.helper_facing = static_cast<std::int32_t>(Read());
        world.helper_pitch = static_cast<std::int32_t>(Read());
        world.loaded_headers = Read() != 0;
        world.headers.at(0) = Read() != 0;
        world.headers.at(1) = Read() != 0;
        world.group = static_cast<std::uint8_t>(Read());
        world.side = static_cast<std::uint32_t>(Read());
        world.special_pitch = static_cast<std::uint8_t>(Read());
        world.bone_y = static_cast<std::int16_t>(Read());
        world.bone_success = Read() != 0;
        world.preset = static_cast<int>(Read());
        VS::Actors::MotionVector point;
        point.x = static_cast<std::int16_t>(Read());
        point.y = static_cast<std::int16_t>(Read());
        point.z = static_cast<std::int16_t>(Read());
        point.w = static_cast<std::int16_t>(Read());
        world.target = target_present ? std::optional { point } : std::nullopt;
        for (auto& actor : world.actors)
            ReadActor(actor);
        if (!std::cin)
            return 2;
        const auto result = t::Run(function, slot, parameter, sequence, world);
        std::cout << static_cast<int>(result);
        Emit(static_cast<std::uint32_t>(world.table_owner + 1));
        Emit(world.side);
        Emit(world.special_pitch);
        for (const auto& actor : world.actors)
            EmitActor(actor);
        const auto target = world.target.value_or(point);
        for (const auto value : { target.x, target.y, target.z, target.w }) {
            Emit(static_cast<std::uint16_t>(value));
        }
        Emit(static_cast<std::uint32_t>(world.events.size()));
        for (const auto& event : world.events) {
            Emit(static_cast<std::uint32_t>(event.kind));
            Emit(event.owner);
            Emit(event.first);
            Emit(event.second);
            Emit(event.third);
        }
        std::cout << '\n';
    }
}
