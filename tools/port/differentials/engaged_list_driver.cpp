// Native side of tools\port\differentials\engaged_list.py (F01678, F01679, F01680): one
// vector per stdin line, one line of results per vector. The list holds the vector's
// links, phases and command flags; the host records markers and remembered targets and
// unlinks through the typed UnlinkEngagedActor over the same list.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/Engagement.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }
extern "C" void Core_Warnf(const char*, const char*, ...) { }
extern "C" void Core_Debugf(const char*, const char*, ...) { }

namespace {
using namespace VS::Battle::Ai;

enum Kind : std::uint8_t { kUnlink, kBuildup, kAnimate };
enum Call : std::uint8_t { kMarker, kRemember };
constexpr std::uint32_t kPoseMask = 0x0C000000;
constexpr int kPoseShift = 26;

template <typename T> T Read(std::istringstream& in)
{
    long long value = 0;
    in >> value;
    if (!in)
        std::abort();
    return static_cast<T>(value);
}

std::optional<int> ReadActor(std::istringstream& in)
{
    const auto value = Read<int>(in);
    return value < 0 ? std::nullopt : std::optional<int> { value };
}

struct Member {
    std::optional<int> next;
    EngagementTimers timers;
    EngagementPhase phase;
    std::uint32_t flags = 0;
    unsigned ashley = 0;
};

class VectorList final : public EngagedList {
public:
    std::optional<int> head;
    std::optional<int> tail;
    std::array<Member, 16> members { };

    [[nodiscard]] std::optional<int> Head() const override { return head; }
    void SetHead(std::optional<int> actor_id) override { head = actor_id; }
    [[nodiscard]] std::optional<int> Tail() const override { return tail; }
    void SetTail(std::optional<int> actor_id) override { tail = actor_id; }
    [[nodiscard]] std::optional<int> Next(int actor_id) const override
    {
        return At(actor_id).next;
    }
    void SetNext(int actor_id, std::optional<int> next) override
    {
        At(actor_id).next = next;
    }
    [[nodiscard]] EngagementPhase Phase(int actor_id) const override
    {
        return At(actor_id).phase;
    }
    void SetPhase(int actor_id, EngagementPhase phase) override
    {
        At(actor_id).phase = phase;
    }
    void SetPose(int actor_id, unsigned pose) override
    {
        Member& member = At(actor_id);
        member.flags = (member.flags & ~kPoseMask) | (pose << kPoseShift);
    }
    [[nodiscard]] unsigned AshleyClassification(int actor_id) const override
    {
        return At(actor_id).ashley;
    }

    [[nodiscard]] Member& At(int actor_id)
    {
        return members.at(static_cast<std::size_t>(actor_id));
    }
    [[nodiscard]] const Member& At(int actor_id) const
    {
        return members.at(static_cast<std::size_t>(actor_id));
    }
};

class VectorHost final : public EngagementBuildupHost {
public:
    VectorHost(VectorList& list, int actor, bool single)
        : list_(list)
        , actor_(actor)
        , single_(single)
    {
    }

    std::vector<std::array<long long, 3>> calls;
    bool refused = false;

    void SetMarker(int actor_id, int marker) override
    {
        calls.push_back({ kMarker, actor_id, marker });
    }
    [[nodiscard]] bool CandidateEngagedThisUpdate() const override { std::abort(); }
    void ClearEngagementRequest() override { std::abort(); }
    void LinkEngagedActor() override { std::abort(); }
    void UnlinkEngagedActor() override
    {
        refused |= !VS::Battle::Ai::UnlinkEngagedActor(list_, actor_);
    }
    [[nodiscard]] std::optional<bool> HasSingleOpponent() const override
    {
        return single_;
    }
    void RememberActionTarget() override { calls.push_back({ kRemember, actor_, 0 }); }

private:
    VectorList& list_;
    int actor_;
    bool single_;
};
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const auto kind = Read<int>(in);
        const auto actor = Read<int>(in);
        VectorList list;
        list.head = ReadActor(in);
        list.tail = ReadActor(in);
        EngagementRoster roster;
        roster.engaged_mask = Read<std::uint16_t>(in);
        roster.engaged_count = Read<std::uint8_t>(in);
        const bool single = Read<int>(in) != 0;
        for (Member& member : list.members) {
            member.next = ReadActor(in);
            member.timers.engaged_frames = Read<std::uint16_t>(in);
            member.timers.cooldown_frames = Read<std::uint16_t>(in);
            member.timers.grace_frames = Read<std::uint16_t>(in);
            member.phase.phase = Read<std::int8_t>(in);
            member.phase.step = Read<std::int8_t>(in);
            member.flags = Read<std::uint32_t>(in);
            member.ashley = Read<unsigned>(in);
        }

        VectorHost host(list, actor, single);
        bool accepted = false;
        if (kind == kUnlink) {
            accepted = UnlinkEngagedActor(list, actor);
        } else if (kind == kBuildup) {
            // The host's unlink changes the list's phases and links, not the timers.
            EngagementTimers timers = list.At(actor).timers;
            accepted = UpdateEngagementBuildup(host, actor, timers, &roster);
            list.At(actor).timers = timers;
        } else {
            accepted = AnimateEngagementPhase(list);
        }

        std::ostringstream out;
        out << (accepted && !host.refused ? 0 : 1);
        for (const Member& member : list.members) {
            out << ' ' << member.next.value_or(-1) << ' ' << member.timers.engaged_frames
                << ' ' << member.timers.cooldown_frames << ' '
                << member.timers.grace_frames << ' ' << int { member.phase.phase } << ' '
                << int { member.phase.step } << ' ' << member.flags;
        }
        out << ' ' << list.head.value_or(-1) << ' ' << list.tail.value_or(-1) << ' '
            << roster.engaged_mask << ' ' << int { roster.engaged_count } << ' '
            << host.calls.size();
        for (const auto& call : host.calls)
            for (const auto value : call)
                out << ' ' << value;
        std::cout << out.str() << '\n';
    }
    return 0;
}
