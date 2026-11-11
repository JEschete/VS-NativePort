// Native side of tools\port\differentials\target_approach.py (F01748, F01749): one vector per
// stdin line, one line of results per vector. The world answers from the vector's actor models,
// battle-actor opcode and queues, and records each call with every approach field.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Battle/Ai/TargetApproach.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kApproachKind = 0;
constexpr std::uint32_t kFlies = 0x200000;
enum Call : std::uint8_t { kCommand, kProject, kDispatch, kRecover, kAwareness, kHesitation, kRefresh };

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

// Every compared field, in the case's FIELDS order.
std::vector<long long> Fields(const TargetApproacher& a)
{
    return { a.close_approach, a.gait, a.height_filter, a.near_target_detour, a.toward_point, a.facing_locked,
        a.approach_directly, a.chases_lost_target, a.approaches_target_point, a.tile, a.saved_destination, a.x, a.z,
        a.actor_id, a.facing_delay, a.range, a.previous_range, a.chasing_lost_target, a.target_id, a.chase_timeout,
        a.recover_in_range, a.walked_close, a.range_mode, a.previous_option, a.previous_target_id,
        a.chase_destination, a.target_half_tile_x, a.target_half_tile_z, a.retarget_timer, a.previous_facing_tile,
        a.facing_pending, a.turn_timer, a.target_x, a.target_z, a.movement_target, a.movement_flags, a.destination,
        a.move_x, a.move_z, a.angle, a.reaction_pending };
}

void ReadFields(std::istringstream& in, TargetApproacher& a)
{
    a.close_approach = Read<std::uint8_t>(in);
    a.gait = Read<std::uint8_t>(in);
    a.height_filter = Read<std::uint8_t>(in);
    a.near_target_detour = Read<std::uint8_t>(in);
    a.toward_point = Read<std::uint8_t>(in);
    a.facing_locked = Read<std::uint8_t>(in);
    a.approach_directly = Read<std::uint8_t>(in);
    a.chases_lost_target = Read<std::uint8_t>(in);
    a.approaches_target_point = Read<std::uint8_t>(in);
    a.tile = Read<std::uint32_t>(in);
    a.saved_destination = Read<std::uint32_t>(in);
    a.x = Read<std::int16_t>(in);
    a.z = Read<std::int16_t>(in);
    a.actor_id = Read<std::uint8_t>(in);
    a.facing_delay = Read<std::uint16_t>(in);
    a.range = Read<std::int32_t>(in);
    a.previous_range = Read<std::int32_t>(in);
    a.chasing_lost_target = Read<std::uint8_t>(in);
    a.target_id = Read<std::uint8_t>(in);
    a.chase_timeout = Read<std::uint8_t>(in);
    a.recover_in_range = Read<std::uint8_t>(in);
    a.walked_close = Read<std::uint8_t>(in);
    a.range_mode = Read<std::uint8_t>(in);
    a.previous_option = Read<std::uint8_t>(in);
    a.previous_target_id = Read<std::uint8_t>(in);
    a.chase_destination = Read<std::uint32_t>(in);
    a.target_half_tile_x = Read<std::uint8_t>(in);
    a.target_half_tile_z = Read<std::uint8_t>(in);
    a.retarget_timer = Read<std::uint16_t>(in);
    a.previous_facing_tile = Read<std::uint32_t>(in);
    a.facing_pending = Read<std::uint8_t>(in);
    a.turn_timer = Read<std::uint16_t>(in);
    a.target_x = Read<std::int16_t>(in);
    a.target_z = Read<std::int16_t>(in);
    a.movement_target = Read<std::uint8_t>(in);
    a.movement_flags = Read<std::uint16_t>(in);
    a.destination = Read<std::uint32_t>(in);
    a.move_x = Read<std::int32_t>(in);
    a.move_z = Read<std::int32_t>(in);
    a.angle = Read<std::int16_t>(in);
    a.reaction_pending = Read<std::uint8_t>(in);
}

class VectorWorld final : public TargetApproachWorld {
public:
    std::array<ApproachTargetModel, kTargetApproachActorSlots> models { };
    std::uint8_t opcode = 0;
    std::uint8_t walk_tests = 0;
    std::vector<std::uint32_t> projected;
    std::size_t next_projected = 0;
    std::vector<int> hesitations;
    std::size_t next_hesitation = 0;
    std::vector<std::vector<long long>> calls;

    [[nodiscard]] std::optional<ApproachTargetModel> TargetModel(int actor_id) const override
    {
        if (actor_id < 0 || actor_id >= kTargetApproachActorSlots)
            return std::nullopt;
        return models.at(static_cast<std::size_t>(actor_id));
    }
    [[nodiscard]] std::optional<std::uint8_t> CommandOpcode() const override { return opcode; }
    void IssueCommand(TargetApproacher& a, CommandIntent intent) override
    {
        Note(a, kCommand, static_cast<long long>(intent));
    }
    void ProjectClearRadialDestination(
        TargetApproacher& a, std::uint32_t source, int angle, std::uint32_t distance) override
    {
        Note(a, kProject, source, angle, distance);
        if (next_projected < projected.size())
            a.destination = projected[next_projected++];
    }
    void DispatchLocomotion(TargetApproacher& a, int kind, int angle) override { Note(a, kDispatch, kind, angle); }
    void RecoverMovement(TargetApproacher& a) override { Note(a, kRecover); }
    void SetTemporaryAwareness(TargetApproacher& a, int marker, int frames) override
    {
        Note(a, kAwareness, marker, frames);
    }
    [[nodiscard]] int HesitationTurn(TargetApproacher& a, int chance, int kind, int slot) override
    {
        Note(a, kHesitation, chance, kind, slot);
        return next_hesitation < hesitations.size() ? hesitations[next_hesitation++] : 0;
    }
    void ClearWalkTests() override { walk_tests = 0; }
    void RefreshTargetTile(TargetApproacher& a) override { Note(a, kRefresh); }

private:
    void Note(const TargetApproacher& a, Call code, long long first = 0, long long second = 0, long long third = 0)
    {
        std::vector<long long> call { code, first, second, third };
        const auto fields = Fields(a);
        call.insert(call.end(), fields.begin(), fields.end());
        calls.push_back(std::move(call));
    }
};
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const auto kind = Read<int>(in);
        const auto argument = Read<int>(in);
        TargetApproacher approacher;
        ReadFields(in, approacher);
        approacher.flies = (Read<std::uint32_t>(in) & kFlies) != 0;
        approacher.model_look_yaw = Read<std::int16_t>(in);
        approacher.model_facing = Read<std::int16_t>(in);
        for (auto& relation : approacher.relations)
            relation = Read<std::uint32_t>(in);
        for (auto& distance : approacher.distances_squared)
            distance = Read<std::uint32_t>(in);
        VectorWorld world;
        for (auto& model : world.models) {
            model.moving = Read<int>(in) != 0;
            model.x = Read<std::int16_t>(in);
            model.z = Read<std::int16_t>(in);
        }
        world.opcode = Read<std::uint8_t>(in);
        world.walk_tests = Read<std::uint8_t>(in);
        world.projected.resize(Read<std::size_t>(in));
        for (auto& value : world.projected)
            value = Read<std::uint32_t>(in);
        world.hesitations.resize(Read<std::size_t>(in));
        for (auto& value : world.hesitations)
            value = Read<int>(in);

        const auto result = kind == kApproachKind ? UpdateTargetApproachMovement(world, approacher, argument)
                                                  : UpdateTargetFacing(world, approacher, argument);
        std::ostringstream out;
        out << (result ? 0 : 1) << ' ' << (result && *result ? 1 : 0);
        for (const auto value : Fields(approacher))
            out << ' ' << value;
        out << ' ' << int { world.walk_tests } << ' ' << world.calls.size();
        for (const auto& call : world.calls)
            for (const auto value : call)
                out << ' ' << value;
        std::cout << out.str() << '\n';
    }
    return 0;
}
