// Native side of tools\port\differentials\actor_reaction.py (F01705, F01706) and
// action_notification.py (F01719): one vector per stdin line, one line of results per
// vector. The host keeps sixteen AI states and models the stubbed callees as the cases
// do: the alliance calls set and clear the alliance flag and MakeHostile marks the
// relation hostile.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/Relations.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

enum Kind : std::uint8_t { kReact, kPropagate, kNotify };
enum Call : std::uint8_t { kBeneficial, kHarmful, kBegin, kEnd, kHostile, kApply };
constexpr std::size_t kActors = 16;
constexpr std::uint32_t kHostileKeep = 0xFFFF803F;
constexpr std::uint32_t kHostileClass = 0xC0;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

struct Actor {
    bool present = false;
    std::uint8_t behavior = 0;
    ReactionSettings settings;
    std::array<std::uint32_t, kActors> relations { };
    std::uint16_t grace = 0;
};

struct World {
    std::array<Actor, kActors> actors { };
    bool beneficial = false;
    bool harmful = false;
    bool orbit = false;
    int orbiting = 0;
    std::vector<std::array<long long, 3>> calls;

    [[nodiscard]] Actor& At(int actor)
    {
        return actors.at(static_cast<std::size_t>(actor));
    }
    [[nodiscard]] const Actor& At(int actor) const
    {
        return actors.at(static_cast<std::size_t>(actor));
    }
};

class VectorHost final : public ActorReactionHost {
public:
    VectorHost(World& world, int own)
        : world_(world)
        , own_(own)
    {
    }

    [[nodiscard]] int ActorId() const override { return own_; }
    [[nodiscard]] std::uint8_t BehaviorMode() const override
    {
        return world_.At(own_).behavior;
    }
    void SetBehaviorMode(std::uint8_t mode) override { world_.At(own_).behavior = mode; }
    [[nodiscard]] ReactionSettings Settings() const override
    {
        return world_.At(own_).settings;
    }
    [[nodiscard]] std::uint32_t Relation(int target) const override
    {
        return world_.At(own_).relations.at(static_cast<std::size_t>(target));
    }
    void SetRelation(int target, std::uint32_t word) override
    {
        world_.At(own_).relations.at(static_cast<std::size_t>(target)) = word;
    }
    [[nodiscard]] std::optional<ReactionSettings> PeerSettings(int actor) const override
    {
        const Actor& peer = world_.At(actor);
        return peer.present ? std::optional<ReactionSettings> { peer.settings }
                            : std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> PeerRelation(
        int actor, int target) const override
    {
        const Actor& peer = world_.At(actor);
        if (!peer.present)
            return std::nullopt;
        return peer.relations.at(static_cast<std::size_t>(target));
    }
    void SetPeerRelation(int actor, int target, std::uint32_t word) override
    {
        Actor& peer = world_.At(actor);
        if (!peer.present)
            std::abort();
        peer.relations.at(static_cast<std::size_t>(target)) = word;
    }
    [[nodiscard]] bool IsBeneficial(int action) const override
    {
        world_.calls.push_back({ kBeneficial, action, own_ });
        return world_.beneficial;
    }
    [[nodiscard]] bool IsHarmful(int action) const override
    {
        world_.calls.push_back({ kHarmful, action, own_ });
        return world_.harmful;
    }
    void TryBeginAlliance() override
    {
        world_.calls.push_back({ kBegin, own_, 0 });
        world_.At(own_).settings.allied_with_ashley = true;
    }
    void EndAlliance() override
    {
        world_.calls.push_back({ kEnd, own_, 0 });
        world_.At(own_).settings.allied_with_ashley = false;
    }
    void MakeHostile(int target) override
    {
        world_.calls.push_back({ kHostile, own_, target });
        std::uint32_t& word =
            world_.At(own_).relations.at(static_cast<std::size_t>(target));
        word = (word & kHostileKeep) | kHostileClass;
    }
    void StopArenaOrbit() override
    {
        if (world_.orbit)
            world_.orbiting = 0;
    }

private:
    World& world_;
    int own_;
};

class VectorNotification final : public ActionNotificationHost {
public:
    explicit VectorNotification(World& world)
        : world_(world)
    {
    }

    void ClearReengagementGrace(int actor) override
    {
        Actor& state = world_.At(actor);
        if (state.present)
            state.grace = 0;
    }
    void ApplyRelationshipChanges() override { world_.calls.push_back({ kApply, 1, 0 }); }

private:
    World& world_;
};

ReactionHit ReadHit(std::istringstream& in)
{
    ReactionHit hit;
    hit.actor = Read<int>(in);
    hit.hp_change = Read<std::int16_t>(in);
    hit.hp_effect = Read<unsigned>(in);
    hit.skipped = Read<int>(in) != 0;
    return hit;
}

ReactionSettings ReadSettings(std::istringstream& in)
{
    ReactionSettings settings;
    settings.allied_with_ashley = Read<int>(in) != 0;
    settings.settings_bit25 = Read<int>(in) != 0;
    settings.settings_bit26 = Read<int>(in) != 0;
    settings.settings_bit27 = Read<int>(in) != 0;
    settings.shares_ally_hostility = Read<int>(in) != 0;
    settings.category = Read<std::uint8_t>(in);
    return settings;
}

void PrintCalls(std::ostringstream& out, const World& world)
{
    out << ' ' << world.calls.size();
    for (const auto& call : world.calls)
        for (const auto value : call)
            out << ' ' << value;
}

std::string Notify(std::istringstream& in)
{
    World world;
    ReactionHit acting;
    acting.actor = Read<int>(in);
    acting.skipped = Read<int>(in) != 0;
    for (Actor& actor : world.actors) {
        actor.present = Read<int>(in) != 0;
        actor.grace = Read<std::uint16_t>(in);
    }
    VectorNotification host(world);
    const bool accepted = NotifyActionRelationshipChanges(host, acting);
    std::ostringstream out;
    out << (accepted ? 0 : 1);
    for (const Actor& actor : world.actors)
        out << ' ' << actor.grace;
    PrintCalls(out, world);
    return out.str();
}

std::string React(std::istringstream& in, Kind kind)
{
    World world;
    const auto own = Read<int>(in);
    AppliedAction applied;
    applied.action = Read<int>(in);
    world.beneficial = Read<int>(in) != 0;
    world.harmful = Read<int>(in) != 0;
    std::array<std::uint8_t, 2> prerequisites { };
    for (auto& prerequisite : prerequisites)
        prerequisite = Read<std::uint8_t>(in);
    applied.hit_prerequisites = prerequisites;
    world.orbit = Read<int>(in) != 0;
    world.orbiting = Read<int>(in);
    applied.acting = ReadHit(in);
    std::vector<ReactionHit> hits(Read<std::size_t>(in));
    for (ReactionHit& hit : hits)
        hit = ReadHit(in);
    applied.targets = hits;
    std::vector<std::uint8_t> listed(Read<std::size_t>(in));
    for (auto& id : listed)
        id = Read<std::uint8_t>(in);
    for (Actor& actor : world.actors) {
        actor.present = Read<int>(in) != 0;
        actor.behavior = Read<std::uint8_t>(in);
        actor.settings = ReadSettings(in);
        for (auto& word : actor.relations)
            word = Read<std::uint32_t>(in);
    }

    VectorHost host(world, own);
    const bool accepted = kind == kReact ? ApplyActorActionReaction(host, applied)
                                         : PropagateActorRelationships(host, listed);
    std::ostringstream out;
    out << (accepted ? 0 : 1);
    for (const Actor& actor : world.actors) {
        out << ' ' << int { actor.behavior } << ' '
            << (actor.settings.allied_with_ashley ? 1 : 0);
        for (const auto word : actor.relations)
            out << ' ' << word;
    }
    out << ' ' << world.orbiting;
    PrintCalls(out, world);
    return out.str();
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const auto kind = Read<int>(in);
        std::cout << (kind == kNotify ? Notify(in) : React(in, static_cast<Kind>(kind)))
                  << '\n';
    }
    return 0;
}
