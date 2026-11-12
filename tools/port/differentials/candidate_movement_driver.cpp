// Native side of candidate_movement.py: queued external calls with pre-call
// snapshots and scripted mover/candidate writes. Candidate handles remain ranks.
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

#include "Battle/Ai/CandidateMovement.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;
constexpr std::size_t kCandidates = 4;
constexpr int kScalarFields = 20;
enum Call : std::uint8_t {
    kCan, kCooldown, kEngage, kDisengage, kApproach, kTrack, kFlee, kWander, kMove,
    kFind, kRelease, kRecover, kProject, kDispatch, kRandom, kAwareness, kGround, kCallCount
};

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

void SetField(CandidateMover& m, int field, long long value)
{
    switch (field) {
    case 0: m.gait = static_cast<std::uint8_t>(value); break;
    case 1: m.fleeing = static_cast<std::uint8_t>(value); break;
    case 2: m.guarding = static_cast<std::uint8_t>(value); break;
    case 3: m.linked_actor = static_cast<std::int8_t>(value); break;
    case 4: m.alerted = static_cast<std::uint8_t>(value); break;
    case 5: m.guard_delay = static_cast<std::uint8_t>(value); break;
    case 6: m.position.x = static_cast<std::int16_t>(value); break;
    case 7: m.position.y = static_cast<std::int16_t>(value); break;
    case 8: m.position.z = static_cast<std::int16_t>(value); break;
    case 9: m.point_height = static_cast<std::int16_t>(value); break;
    case 10: m.actor_id = static_cast<std::uint8_t>(value); break;
    case 11: m.max_range = static_cast<std::uint8_t>(value); break;
    case 12: m.range_mode = static_cast<std::uint8_t>(value); break;
    case 13: m.plan_mode = static_cast<std::uint8_t>(value); break;
    case 14: m.action_status = static_cast<std::uint8_t>(value); break;
    case 15: m.planned_tile = static_cast<std::uint32_t>(value); break;
    case 16: m.movement_target = static_cast<std::uint8_t>(value); break;
    case 17: m.movement_flags = static_cast<std::uint16_t>(value); break;
    case 18: m.destination = static_cast<std::uint32_t>(value); break;
    case 19: m.candidate_count = static_cast<std::int32_t>(value); break;
    default:
        if (field < kScalarFields + kCandidateTargetSlots)
            m.relations.at(static_cast<std::size_t>(field - kScalarFields)) = static_cast<std::uint32_t>(value);
        else
            m.distances_squared.at(static_cast<std::size_t>(field - kScalarFields - kCandidateTargetSlots)) =
                static_cast<std::uint32_t>(value);
    }
}

std::vector<long long> Fields(const CandidateMover& m)
{
    std::vector<long long> fields { m.gait, m.fleeing, m.guarding, m.linked_actor, m.alerted, m.guard_delay,
        m.position.x, m.position.y, m.position.z, m.point_height, m.actor_id, m.max_range, m.range_mode,
        m.plan_mode, m.action_status, m.planned_tile, m.movement_target, m.movement_flags, m.destination,
        m.candidate_count };
    fields.insert(fields.end(), m.relations.begin(), m.relations.end());
    fields.insert(fields.end(), m.distances_squared.begin(), m.distances_squared.end());
    return fields;
}

std::vector<long long> ReadQueue(std::istringstream& in)
{
    std::vector<long long> queue(Read<std::size_t>(in));
    for (auto& value : queue)
        value = Read<long long>(in);
    return queue;
}

struct Effect {
    int code, occurrence, field;
    long long value;
    int candidate, word;
    std::uint32_t replacement;
};

class VectorWorld final : public CandidateMovementWorld {
public:
    CandidateMover* mover = nullptr;
    std::array<ActionCandidate, kCandidates> candidates {};
    std::array<bool, kCandidateTargetSlots> frames {};
    std::array<PursuitTarget, kCandidateTargetSlots> models {};
    std::array<std::int16_t, kCandidateTargetSlots> heights {};
    std::array<std::array<std::uint32_t, kCandidateTargetSlots>, kCandidateTargetSlots> distances {};
    ActorRoster roster;
    std::uint16_t readiness = 0;
    std::uint32_t approach_moves = 0;
    std::array<std::vector<long long>, kCallCount> answers;
    std::array<std::size_t, kCallCount> occurrences {};
    std::vector<long long> tiles;
    std::vector<Effect> effects;
    std::vector<std::vector<long long>> calls;
    std::size_t ActorHead() const override { return roster.count ? 1 : 0; }
    std::optional<int> ActorId(std::size_t node) const override
    {
        return node && node <= roster.count ? std::optional<int>(roster.ids.at(node - 1)) : std::nullopt;
    }
    std::size_t ActorNext(std::size_t node) const override { return node < roster.count ? node + 1 : 0; }

    std::vector<long long> Snapshot() const
    {
        auto fields = Fields(*mover);
        fields.push_back(approach_moves);
        for (const auto& candidate : candidates)
            fields.insert(fields.end(), candidate.RawWords().begin(), candidate.RawWords().end());
        return fields;
    }
    [[nodiscard]] std::optional<std::size_t> RankedHandle(std::size_t rank) const override
    {
        return rank < candidates.size() ? std::optional(rank) : std::nullopt;
    }
    [[nodiscard]] std::optional<ActionCandidate> Candidate(std::size_t handle) const override
    {
        return handle < candidates.size() ? std::optional(candidates.at(handle)) : std::nullopt;
    }
    [[nodiscard]] bool HasFrameActor(int target) const override { return frames.at(target); }
    [[nodiscard]] std::optional<PursuitTarget> TargetModel(int target) const override { return models.at(target); }
    [[nodiscard]] std::optional<std::int16_t> TargetHeight(int target) const override { return heights.at(target); }
    [[nodiscard]] std::optional<std::uint32_t> FrameDistance(int observer, int target) const override
    {
        return distances.at(observer).at(target);
    }
    [[nodiscard]] std::optional<ActorRoster> BattleActors() const override { return roster; }
    [[nodiscard]] int GroundHeight(int x, int z) const override
    {
        return static_cast<int>(const_cast<VectorWorld*>(this)->Note(kGround, x, z));
    }
    [[nodiscard]] std::uint16_t ReadinessLevel() const override { return readiness; }
    void CountApproachMove() override { ++approach_moves; }
    [[nodiscard]] bool CanApproach(CandidateMover&, std::size_t handle) override { return Note(kCan, handle) != 0; }
    [[nodiscard]] bool CooldownReady(CandidateMover&, std::size_t handle) override
    {
        return Note(kCooldown, handle) != 0;
    }
    void TryEngage(CandidateMover&) override { Note(kEngage); }
    void Disengage(CandidateMover&) override { Note(kDisengage); }
    int Approach(CandidateMover&, const ApproachRequest& request) override
    {
        return static_cast<int>(Note(kApproach, request.target, request.distance, request.option));
    }
    int Track(CandidateMover&, int target) override { return static_cast<int>(Note(kTrack, target)); }
    int Flee(CandidateMover&, int kind) override { return static_cast<int>(Note(kFlee, kind)); }
    void Wander(CandidateMover&, std::uint32_t tile, int radius) override { Note(kWander, tile, radius); }
    void MoveToTile(CandidateMover&, std::uint32_t tile, int radius) override { Note(kMove, tile, radius); }
    RelatedActor FindRelated(CandidateMover&, int relation, bool want_tile) override
    {
        const auto at = occurrences[kFind];
        const int actor = static_cast<int>(Note(kFind, relation, want_tile ? 1 : 0));
        return { .actor_id = actor, .tile = want_tile ? static_cast<std::uint32_t>(tiles.at(at)) : 0 };
    }
    void ReleaseReservations(CandidateMover&) override { Note(kRelease); }
    void Recover(CandidateMover&) override { Note(kRecover); }
    void ProjectRadial(CandidateMover&, std::uint32_t tile, int angle, std::uint32_t distance) override
    {
        Note(kProject, tile, angle, distance);
    }
    void DispatchLocomotion(CandidateMover&, int kind, int angle) override { Note(kDispatch, kind, angle); }
    [[nodiscard]] int RandomMasked(int mask) override { return static_cast<int>(Note(kRandom, mask)); }
    void SetAwareness(CandidateMover&, int marker, int frames) override { Note(kAwareness, marker, frames); }

private:
    long long Note(Call code, long long a = 0, long long b = 0, long long c = 0)
    {
        std::vector<long long> call { code, a, b, c };
        const auto fields = Snapshot();
        call.insert(call.end(), fields.begin(), fields.end());
        calls.push_back(std::move(call));
        const auto at = occurrences.at(code)++;
        for (const auto& effect : effects) {
            if (effect.code != code || effect.occurrence != static_cast<int>(at))
                continue;
            if (effect.field == kScalarFields + 2 * kCandidateTargetSlots)
                approach_moves = static_cast<std::uint32_t>(effect.value);
            else
                SetField(*mover, effect.field, effect.value);
            if (effect.candidate >= 0) {
                auto words = candidates.at(effect.candidate).RawWords();
                words.at(effect.word) = effect.replacement;
                candidates.at(effect.candidate) = ActionCandidate(words);
            }
        }
        return at < answers.at(code).size() ? answers.at(code).at(at) : 0;
    }
};
} // namespace

void RunMovementLine(std::istringstream& in)
{
        const int kind = Read<int>(in);
        const auto handle = Read<std::size_t>(in);
        CandidateMover mover;
        for (int field = 0; field < kScalarFields + 2 * kCandidateTargetSlots; ++field)
            SetField(mover, field, Read<long long>(in));
        VectorWorld world;
        world.mover = &mover;
        world.approach_moves = Read<std::uint32_t>(in);
        world.readiness = Read<std::uint16_t>(in);
        for (auto& candidate : world.candidates) {
            ActionCandidate::Words words {};
            for (auto& word : words)
                word = Read<std::uint32_t>(in);
            candidate = ActionCandidate(words);
        }
        for (auto& frame : world.frames)
            frame = Read<int>(in) != 0;
        for (std::size_t actor = 0; actor < world.models.size(); ++actor) {
            auto& model = world.models.at(actor);
            model.position.x = Read<std::int16_t>(in);
            model.position.y = Read<std::int16_t>(in);
            model.position.z = Read<std::int16_t>(in);
            model.tile = Read<std::uint32_t>(in);
            world.heights.at(actor) = Read<std::int16_t>(in);
        }
        for (auto& row : world.distances)
            for (auto& distance : row)
                distance = Read<std::uint32_t>(in);
        world.roster.count = Read<std::size_t>(in);
        for (std::size_t rank = 0; rank < world.roster.count; ++rank)
            world.roster.ids.at(rank) = Read<std::uint8_t>(in);
        for (auto& queue : world.answers)
            queue = ReadQueue(in);
        world.tiles = ReadQueue(in);
        world.effects.resize(Read<std::size_t>(in));
        for (auto& effect : world.effects) {
            effect.code = Read<int>(in);
            effect.occurrence = Read<int>(in);
            effect.field = Read<int>(in);
            effect.value = Read<long long>(in);
            effect.candidate = Read<int>(in);
            effect.word = Read<int>(in);
            effect.replacement = Read<std::uint32_t>(in);
        }
        std::optional<int> result;
        switch (kind) {
        case 0: result = MoveForActionCandidate(world, mover, handle); break;
        case 1: result = UpdateCombatPursuit(world, mover); break;
        case 2: result = TryApproachPlannedTarget(world, mover, handle); break;
        case 3: result = UpdateCloseFollowBehavior(world, mover); break;
        case 4: result = UpdateEnemyPursuitBehavior(world, mover); break;
        case 6: result = UpdateEscortCombatBehavior(world, mover); break;
        default: result = UpdateGuardMovementBehavior(world, mover); break;
        }
        std::cout << (!result.has_value() ? 1 : 0) << ' ' << result.value_or(0);
        for (const auto value : world.Snapshot())
            std::cout << ' ' << value;
        std::cout << ' ' << world.calls.size();
        for (const auto& call : world.calls)
            for (const auto value : call)
                std::cout << ' ' << value;
        std::cout << '\n';
}

#ifndef VS_MOVEMENT_DRIVER_INCLUDED
int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        RunMovementLine(in);
    }
}
#endif
