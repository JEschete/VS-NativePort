// Native side of tools\port\differentials\boss_orbit.py (F01846-F01849): one vector per stdin line,
// one line of results per vector. The world answers from the vector's retreat path, player,
// candidate threshold, readiness level, battle actor and queues, and records each call with every
// boss and orbit field.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Battle/Ai/BossOrbit.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

constexpr int kPathSteps = 31;
constexpr std::uint8_t kCommandPending = 0x80;
enum Kind : std::uint8_t { kRetreat, kReady, kTracking, kBehavior };
enum Call : std::uint8_t {
    kCommand,
    kRecover,
    kCooldown,
    kEngageReady,
    kEngage,
    kDisengage,
    kProgressRead,
    kProgressWrite,
    kRandom
};

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

// Every compared field, in the case's NAMES order.
std::vector<long long> Fields(const OrbitingBoss& b, const ArenaOrbit& o)
{
    return { b.model_flag.value_or(false) ? 1 : 0, b.movement_target, b.move_x, b.move_z, b.move_y,
        b.candidate_count, o.heading, o.retreat_phase, o.distance_cached, o.distance_squared, o.is_orbiting,
        o.path_step, o.cooldown, o.move_requested, o.retreat_angle, o.saved_height, o.health_phase, o.plan_saved,
        o.saved_plan_count, o.start_x, o.start_z, o.started, o.angular_velocity, o.height_offset };
}

void ReadOrbit(std::istringstream& in, ArenaOrbit& o)
{
    o.heading = Read<std::uint16_t>(in);
    o.retreat_phase = Read<std::uint8_t>(in);
    o.distance_cached = Read<std::uint8_t>(in);
    o.distance_squared = Read<std::int32_t>(in);
    o.is_orbiting = Read<std::uint8_t>(in);
    o.path_step = Read<std::uint8_t>(in);
    o.cooldown = Read<std::int16_t>(in);
    o.move_requested = Read<std::int16_t>(in);
    o.retreat_angle = Read<std::int16_t>(in);
    o.saved_height = Read<std::int16_t>(in);
    o.health_phase = Read<std::uint8_t>(in);
    o.plan_saved = Read<std::uint8_t>(in);
    o.saved_plan_count = Read<std::uint8_t>(in);
    o.start_x = Read<std::int16_t>(in);
    o.start_z = Read<std::int16_t>(in);
    o.started = Read<std::uint8_t>(in);
    o.angular_velocity = Read<std::int32_t>(in);
    o.height_offset = Read<std::int32_t>(in);
}

std::deque<long long> ReadQueue(std::istringstream& in)
{
    std::deque<long long> queue(Read<std::size_t>(in));
    for (auto& value : queue)
        value = Read<long long>(in);
    return queue;
}

long long Pop(std::deque<long long>& queue)
{
    if (queue.empty())
        return 0;
    const long long value = queue.front();
    queue.pop_front();
    return value;
}

class VectorWorld final : public BossOrbitWorld {
public:
    std::array<RetreatStep, kPathSteps> path { };
    BossPlayer player;
    std::uint16_t threshold = 0;
    std::uint16_t readiness = 0;
    std::deque<long long> cooldowns;
    std::deque<long long> engagements;
    std::deque<long long> randoms;
    std::uint8_t progress = 0;
    BossBattleActor actor;
    std::uint8_t command_state = 0;
    const OrbitingBoss* boss = nullptr;
    const ArenaOrbit* orbit = nullptr;
    // The progress read is const; it records too.
    mutable std::vector<std::vector<long long>> calls;

    [[nodiscard]] std::optional<RetreatStep> RetreatPathStep(int step) const override
    {
        if (step < 0 || step >= kPathSteps)
            return std::nullopt;
        return path.at(static_cast<std::size_t>(step));
    }
    [[nodiscard]] std::optional<BossPlayer> Player() const override { return player; }
    [[nodiscard]] std::optional<bool> IsFirstCandidateCooldownReady(OrbitingBoss&) override
    {
        Note(kCooldown);
        return Pop(cooldowns) != 0;
    }
    [[nodiscard]] std::optional<std::uint16_t> FirstCandidateThreshold() const override { return threshold; }
    [[nodiscard]] std::optional<std::uint16_t> ReadinessLevel() const override { return readiness; }
    [[nodiscard]] bool IsEngagementReady(OrbitingBoss&) override
    {
        Note(kEngageReady);
        return Pop(engagements) != 0;
    }
    void TryEngage(OrbitingBoss&) override { Note(kEngage); }
    void Disengage(OrbitingBoss&) override { Note(kDisengage); }
    void IssueCommand(OrbitingBoss&, CommandIntent intent) override
    {
        Note(kCommand, static_cast<long long>(intent));
    }
    void RecoverMovement(OrbitingBoss&) override { Note(kRecover); }
    [[nodiscard]] std::uint8_t Progress() const override
    {
        Note(kProgressRead);
        return progress;
    }
    void SetProgress(std::uint8_t value) override { Note(kProgressWrite, value); }
    [[nodiscard]] int Random() override
    {
        Note(kRandom);
        return static_cast<int>(Pop(randoms));
    }
    [[nodiscard]] std::optional<BossBattleActor> BattleActor() const override { return actor; }
    void QueueBattleCommand(const ActorCommandPacket& packet) override
    {
        actor.command = packet;
        command_state = kCommandPending;
    }

private:
    void Note(Call code, long long argument = 0) const
    {
        std::vector<long long> call { code, argument, 0 };
        const auto fields = Fields(*boss, *orbit);
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
        OrbitingBoss boss;
        boss.x = Read<std::int16_t>(in);
        boss.y = Read<std::int16_t>(in);
        boss.z = Read<std::int16_t>(in);
        boss.model_flag = Read<int>(in) != 0;
        BossStats stats;
        stats.current_hp = Read<std::int16_t>(in);
        stats.max_hp = Read<std::int16_t>(in);
        stats.core_hp = Read<std::int16_t>(in);
        boss.stats = stats;
        boss.movement_target = Read<std::uint8_t>(in);
        boss.move_x = Read<std::int32_t>(in);
        boss.move_z = Read<std::int32_t>(in);
        boss.move_y = Read<std::int32_t>(in);
        boss.candidate_count = Read<std::int32_t>(in);
        ArenaOrbit orbit;
        ReadOrbit(in, orbit);
        VectorWorld world;
        for (auto& step : world.path) {
            step.distance = Read<std::int16_t>(in);
            step.height = Read<std::int16_t>(in);
        }
        world.player.moving = Read<int>(in) != 0;
        world.player.x = Read<std::int16_t>(in);
        world.player.y = Read<std::int16_t>(in);
        world.player.z = Read<std::int16_t>(in);
        world.player.collision_height = Read<std::uint16_t>(in);
        world.threshold = Read<std::uint16_t>(in);
        world.readiness = Read<std::uint16_t>(in);
        world.cooldowns = ReadQueue(in);
        world.engagements = ReadQueue(in);
        world.randoms = ReadQueue(in);
        world.progress = Read<std::uint8_t>(in);
        world.actor.core_hp = Read<std::int16_t>(in);
        world.command_state = Read<std::uint8_t>(in);
        world.actor.command.opcode = static_cast<CommandOpcode>(Read<std::uint8_t>(in));
        world.actor.command.arg0 = Read<std::int32_t>(in);
        world.actor.command.arg1 = Read<std::int32_t>(in);
        world.actor.command.arg2 = Read<std::int32_t>(in);
        world.boss = &boss;
        world.orbit = &orbit;

        std::optional<bool> result;
        switch (kind) {
        case kRetreat:
            result = UpdateBossRadialRetreat(world, boss, orbit);
            break;
        case kReady:
            result = IsBossActionReady(world, boss);
            break;
        case kTracking:
            if (UpdateBossOrbitTracking(world, boss, orbit))
                result = true;
            break;
        default:
            if (UpdateOrbitingBossBehavior(world, boss, orbit))
                result = true;
            break;
        }
        std::ostringstream out;
        out << (result ? 0 : 1) << ' ' << (result && *result ? 1 : 0);
        for (const auto value : Fields(boss, orbit))
            out << ' ' << value;
        out << ' ' << int { world.command_state } << ' ' << static_cast<int>(world.actor.command.opcode) << ' '
            << world.actor.command.arg0 << ' ' << world.actor.command.arg1 << ' ' << world.actor.command.arg2;
        out << ' ' << world.calls.size();
        for (const auto& call : world.calls)
            for (const auto value : call)
                out << ' ' << value;
        std::cout << out.str() << '\n';
    }
    return 0;
}
