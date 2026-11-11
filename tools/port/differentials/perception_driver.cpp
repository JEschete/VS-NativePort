// Native side of tools\port\differentials\perception.py (F01638, F01639, F01640, F01643): one vector
// per stdin line, one line of results per vector. Kind 0 refreshes one target's perception through a
// world answering from the vector's actors, list, tile flag and terrain queue, recording each
// terrain test. Kind 1 runs the perception pass over the listed actors through worlds answering from
// their models, links, HP, the records and queues, recording each call with the actor's fields.
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

#include "Battle/Ai/ActorPerception.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;
using VS::Core::Math::Position3;
using VS::Core::Math::TileCoordinate;

enum Kind : std::uint8_t { kTarget, kPass };
enum Call : std::uint8_t {
    kDisengage,
    kReset,
    kUnlink,
    kAwareness,
    kTimers,
    kBuildup,
    kTargetPerception,
    kElapsed,
    kRefreshTile,
    kRecover,
    kForget,
    kReaction,
    kResources,
    kReadiness
};

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
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

Position3 ReadPosition(std::istringstream& in)
{
    Position3 position;
    position.x = Read<std::int16_t>(in);
    position.y = Read<std::int16_t>(in);
    position.z = Read<std::int16_t>(in);
    return position;
}

// SHAPE_FIELDS order: point height, radius, body height.
std::optional<PerceptionShape> ReadShape(std::istringstream& in)
{
    const bool present = Read<int>(in) != 0;
    PerceptionShape shape;
    shape.point_height = Read<std::int16_t>(in);
    shape.radius = Read<std::uint16_t>(in);
    shape.body_height = Read<std::int16_t>(in);
    if (!present)
        return std::nullopt;
    return shape;
}

struct TargetActor {
    bool present = false;
    PerceivedModel model;
    std::optional<PerceptionShape> ai;
    std::optional<PerceptionShape> frame;
    bool gimmick = false;
};

class TargetWorld final : public TargetPerceptionWorld {
public:
    std::array<TargetActor, kPerceptionSlots> actors { };
    ObserverPose pose;
    std::vector<int> members;
    bool marked = false;
    TileCoordinate target_tile { };
    // The terrain test is const; it records and answers too.
    mutable std::deque<long long> terrain;
    mutable std::vector<std::vector<long long>> calls;

    [[nodiscard]] std::optional<PerceivedModel> Model(int target) const override
    {
        const TargetActor& actor = actors.at(static_cast<std::size_t>(target));
        if (!actor.present)
            return std::nullopt;
        return actor.model;
    }
    [[nodiscard]] std::optional<ObserverPose> ObserverModel() const override { return pose; }
    [[nodiscard]] std::optional<PerceptionShape> AiStateShape(int target) const override
    {
        return actors.at(static_cast<std::size_t>(target)).ai;
    }
    [[nodiscard]] std::optional<PerceptionShape> FrameShape(int target) const override
    {
        return actors.at(static_cast<std::size_t>(target)).frame;
    }
    [[nodiscard]] bool HasGimmick(int target) const override
    {
        return actors.at(static_cast<std::size_t>(target)).gimmick;
    }
    // The vector flags only the target's tile.
    [[nodiscard]] std::optional<bool> IsMarkedTile(TileCoordinate tile) const override
    {
        if (tile.x != target_tile.x || tile.z != target_tile.z)
            return std::nullopt;
        return marked;
    }
    [[nodiscard]] bool IsTerrainSegmentBlocked(
        Position3 from, Position3 to, int from_height, int to_height) const override
    {
        calls.push_back({ 0, from.x, from.y, from.z, to.x, to.y, to.z, from_height, to_height });
        return Pop(terrain) != 0;
    }
    [[nodiscard]] std::vector<ListedPerceptionActor> ListedActors() const override
    {
        std::vector<ListedPerceptionActor> listed;
        for (const int id : members) {
            ListedPerceptionActor entry;
            entry.id = id;
            const TargetActor& actor = actors.at(static_cast<std::size_t>(id));
            if (actor.present)
                entry.position = actor.model.position;
            listed.push_back(entry);
        }
        return listed;
    }
};

void RunTarget(std::istringstream& in)
{
    const auto target = Read<int>(in);
    PerceivingObserver observer;
    observer.decays_hostility = Read<int>(in) != 0;
    observer.ignores_body_line = Read<int>(in) != 0;
    observer.tile = Read<std::uint32_t>(in);
    observer.position = ReadPosition(in);
    observer.point_height = Read<std::int16_t>(in);
    observer.actor_id = Read<std::uint8_t>(in);
    observer.near_distance_squared = Read<std::uint32_t>(in);
    observer.view_cosine = Read<std::uint32_t>(in);
    observer.far_distance_squared = Read<std::uint32_t>(in);
    observer.body_height = Read<std::int16_t>(in);
    observer.phase = Read<std::uint8_t>(in);
    for (auto& relation : observer.relations)
        relation = DecodePerceivedRelation(Read<std::uint32_t>(in));
    for (auto& distance : observer.distances_squared)
        distance = Read<std::uint32_t>(in);
    TargetWorld world;
    world.pose.position = ReadPosition(in);
    world.pose.facing = Read<std::int16_t>(in);
    world.pose.look_yaw = Read<std::int16_t>(in);
    for (auto& actor : world.actors) {
        actor.present = Read<int>(in) != 0;
        actor.model.position = ReadPosition(in);
        actor.model.tile = Read<std::uint32_t>(in);
        actor.model.last_tile = Read<std::uint32_t>(in);
        actor.model.locomoting = Read<int>(in) != 0;
        actor.ai = ReadShape(in);
        actor.frame = ReadShape(in);
        actor.gimmick = Read<int>(in) != 0;
    }
    const auto members = Read<std::size_t>(in);
    for (std::size_t i = 0; i < members; ++i)
        world.members.push_back(Read<int>(in));
    world.marked = Read<int>(in) != 0;
    world.terrain = ReadQueue(in);
    world.target_tile
        = VS::Core::Math::UnpackTileCoordinate(world.actors.at(static_cast<std::size_t>(target)).model.tile);

    const bool updated = UpdateTargetPerception(world, observer, target);
    std::cout << (updated ? 0 : 1) << ' ' << static_cast<int>(observer.phase);
    for (const auto& relation : observer.relations)
        std::cout << ' ' << EncodePerceivedRelation(relation);
    for (const auto distance : observer.distances_squared)
        std::cout << ' ' << distance;
    std::cout << ' ' << world.calls.size();
    for (const auto& call : world.calls)
        for (const auto value : call)
            std::cout << ' ' << value;
    std::cout << '\n';
}

// Every compared field, in the case's SNAPSHOT_NAMES order.
std::vector<long long> Snapshot(const PerceivingActor& a)
{
    return { a.stayed_on_tile, a.alert, a.perception_skipped, a.alerted, a.holds_stall_tile, a.partial_perception,
        a.tile, a.previous_tile, a.target_memory_frames, a.detour_step_frames, a.stall_frames, a.position.x,
        a.position.y, a.position.z, a.position_pad, a.history.x_words[0], a.history.x_words[1], a.history.z_words[0],
        a.history.z_words[1], a.history_index, a.history.mask, a.action_delay, a.detour_step, a.detour_target,
        a.fine_tile, a.observed_command, a.movement_wait, a.recovery_frames, a.navigation_counter,
        a.battle.command_flags };
}

struct PassSlot {
    PerceivingActor actor;
    PerceiverModel model;
    std::uint32_t link_tile = 0;
    LinkedPoint point;
    PerceivedHp hp;
};

struct PassShared {
    std::vector<int> ids;
    std::array<std::optional<std::uint32_t>, kPerceptionRecordSlots> records { };
    std::int32_t budget = 0;
    std::int32_t second = 0;
    std::int32_t last_tick = 0;
    std::deque<long long> elapsed;
    std::deque<long long> readiness;
    std::vector<std::vector<long long>> calls;
};

class PassWorld final : public ActorPerceptionWorld {
public:
    PassWorld(PassShared& shared, PassSlot& slot, int index)
        : shared_(shared)
        , slot_(slot)
        , index_(index)
    {
    }

    [[nodiscard]] std::optional<PerceiverModel> Model() const override { return slot_.model; }
    [[nodiscard]] std::optional<std::uint32_t> LinkedTile() const override { return slot_.link_tile; }
    [[nodiscard]] std::optional<LinkedPoint> LinkedPosition() const override { return slot_.point; }
    [[nodiscard]] std::optional<PerceivedHp> Hp() const override { return slot_.hp; }
    [[nodiscard]] std::vector<int> ListedActorIds() const override { return shared_.ids; }
    [[nodiscard]] std::optional<std::uint32_t> RecordTile(int id) const override
    {
        return shared_.records.at(static_cast<std::size_t>(id));
    }
    [[nodiscard]] std::int32_t FrameTickBudget() const override { return shared_.budget; }
    [[nodiscard]] std::int32_t SecondPerceptionTarget() const override { return shared_.second; }
    [[nodiscard]] std::int32_t ElapsedFrameTicks() override
    {
        Note(kElapsed, slot_.actor);
        return static_cast<std::int32_t>(Pop(shared_.elapsed));
    }
    void SetLastBudgetTick(std::int32_t tick) override { shared_.last_tick = tick; }

    void Disengage(PerceivingActor& actor) override { Note(kDisengage, actor); }
    void ResetAwarenessIndicator(PerceivingActor& actor) override { Note(kReset, actor); }
    void UnlinkEngagedActor(PerceivingActor& actor) override { Note(kUnlink, actor); }
    void UpdateTemporaryAwareness(PerceivingActor& actor) override { Note(kAwareness, actor); }
    void TickDecisionTimers(PerceivingActor& actor) override { Note(kTimers, actor); }
    void UpdateEngagementBuildup(PerceivingActor& actor) override { Note(kBuildup, actor); }
    [[nodiscard]] bool UpdateTargetPerception(PerceivingActor& actor, int target) override
    {
        Note(kTargetPerception, actor, target);
        return true;
    }
    void RefreshTargetTileWhenTracking(PerceivingActor& actor) override { Note(kRefreshTile, actor); }
    void RecoverMovement(PerceivingActor& actor) override { Note(kRecover, actor); }
    void ForgetActionTarget(PerceivingActor& actor) override { Note(kForget, actor); }
    void SetReactionWithDelay(PerceivingActor& actor, int reaction, int delay) override
    {
        Note(kReaction, actor, reaction, delay);
    }
    void RefreshResourceRestrictions(PerceivingActor& actor) override { Note(kResources, actor); }
    [[nodiscard]] bool RefreshCandidateReadiness(PerceivingActor& actor) override
    {
        Note(kReadiness, actor);
        return Pop(shared_.readiness) != 0;
    }

private:
    void Note(Call code, const PerceivingActor& actor, long long first = 0, long long second = 0)
    {
        std::vector<long long> call { code, index_, first, second };
        const auto fields = Snapshot(actor);
        call.insert(call.end(), fields.begin(), fields.end());
        shared_.calls.push_back(std::move(call));
    }

    PassShared& shared_;
    PassSlot& slot_;
    int index_;
};

void ReadSlot(std::istringstream& in, PassSlot& slot)
{
    PerceivingActor& a = slot.actor;
    a.battle.id = Read<std::uint8_t>(in);
    a.battle.command_state = Read<std::uint8_t>(in);
    a.battle.command_opcode = Read<std::uint8_t>(in);
    a.battle.command_flags = Read<std::uint8_t>(in);
    a.battle.action_recovery_timer = Read<std::uint16_t>(in);
    slot.hp.current = Read<std::uint16_t>(in);
    slot.hp.max = Read<std::uint16_t>(in);
    slot.model.moving = Read<int>(in) != 0;
    slot.model.running = Read<int>(in) != 0;
    slot.model.forces_perception = Read<int>(in) != 0;
    slot.model.tile = Read<std::uint32_t>(in);
    slot.link_tile = Read<std::uint32_t>(in);
    const auto xy = Read<std::uint32_t>(in);
    const auto z = Read<std::uint32_t>(in);
    slot.point.position.x = static_cast<std::int16_t>(xy & 0xFFFF);
    slot.point.position.y = static_cast<std::int16_t>(xy >> 16);
    slot.point.position.z = static_cast<std::int16_t>(z & 0xFFFF);
    slot.point.pad = static_cast<std::int16_t>(z >> 16);
    a.stayed_on_tile = Read<std::uint8_t>(in);
    a.alert = Read<std::uint8_t>(in);
    a.perception_skipped = Read<std::uint8_t>(in);
    a.alerted = Read<std::uint8_t>(in);
    a.holds_stall_tile = Read<std::uint8_t>(in);
    a.partial_perception = Read<std::uint8_t>(in);
    a.tile = Read<std::uint32_t>(in);
    a.previous_tile = Read<std::int32_t>(in);
    a.target_memory_frames = Read<std::uint16_t>(in);
    a.detour_step_frames = Read<std::uint8_t>(in);
    a.stall_frames = Read<std::uint16_t>(in);
    a.position.x = Read<std::int16_t>(in);
    a.position.y = Read<std::int16_t>(in);
    a.position.z = Read<std::int16_t>(in);
    a.position_pad = Read<std::int16_t>(in);
    a.history.x_words[0] = Read<std::uint32_t>(in);
    a.history.x_words[1] = Read<std::uint32_t>(in);
    a.history.z_words[0] = Read<std::uint32_t>(in);
    a.history.z_words[1] = Read<std::uint32_t>(in);
    a.history_index = Read<std::uint8_t>(in);
    a.history.mask = Read<std::uint8_t>(in);
    a.action_delay = Read<std::uint16_t>(in);
    a.detour_step = Read<std::uint8_t>(in);
    a.detour_target = Read<std::uint8_t>(in);
    a.fine_tile = Read<std::uint32_t>(in);
    a.observed_command = Read<std::uint8_t>(in);
    a.movement_wait = Read<std::uint16_t>(in);
    a.recovery_frames = Read<std::uint8_t>(in);
    a.alert_rule = Read<std::uint8_t>(in);
    a.remembered_target_tile = Read<std::uint32_t>(in);
    a.flee_timer = Read<std::uint16_t>(in);
    a.awareness_frames_remaining = Read<std::uint8_t>(in);
    a.action_status = Read<std::uint8_t>(in);
    a.awareness_marker = Read<std::uint16_t>(in);
    a.has_navigation_override = Read<int>(in) != 0;
    a.navigation_counter = Read<std::uint8_t>(in);
    for (auto& relation : a.relations)
        relation = DecodePerceivedRelation(Read<std::uint32_t>(in));
}

void RunPass(std::istringstream& in)
{
    const auto count = Read<std::size_t>(in);
    PerceptionRoom room;
    room.readiness_level = Read<std::uint16_t>(in);
    room.approach_moves = Read<std::int32_t>(in);
    room.candidate_engaged = Read<std::int32_t>(in);
    PassShared shared;
    shared.last_tick = Read<std::int32_t>(in);
    shared.budget = Read<std::int32_t>(in);
    shared.second = Read<std::int32_t>(in);
    for (auto& record : shared.records) {
        const bool present = Read<int>(in) != 0;
        const auto tile = Read<std::uint32_t>(in);
        if (present)
            record = tile;
    }
    shared.elapsed = ReadQueue(in);
    shared.readiness = ReadQueue(in);
    std::vector<PassSlot> slots(count);
    for (auto& slot : slots) {
        ReadSlot(in, slot);
        shared.ids.push_back(slot.actor.battle.id);
    }

    BeginActorPerception(room);
    bool refused = false;
    for (std::size_t i = 0; i < count && !refused; ++i) {
        PassWorld world(shared, slots[i], static_cast<int>(i));
        refused = !UpdatePerceivingActor(world, slots[i].actor);
    }
    std::cout << (refused ? 1 : 0) << ' ' << room.readiness_level << ' ' << room.approach_moves << ' '
              << room.candidate_engaged << ' ' << shared.last_tick;
    for (const auto& slot : slots)
        for (const auto value : Snapshot(slot.actor))
            std::cout << ' ' << value;
    std::cout << ' ' << shared.calls.size();
    for (const auto& call : shared.calls)
        for (const auto value : call)
            std::cout << ' ' << value;
    std::cout << '\n';
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        if (Read<int>(in) == kTarget)
            RunTarget(in);
        else
            RunPass(in);
    }
    return 0;
}