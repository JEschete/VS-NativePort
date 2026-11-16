#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "Battle/ActorMarkers.hpp"

namespace {
namespace b = VS::Battle;
constexpr std::size_t kActors = 16;
constexpr std::size_t kActorWords = 10;
constexpr std::size_t kFirstActor = 31;
constexpr std::size_t kInputWords = kFirstActor + kActors * kActorWords;
constexpr std::size_t kStorageWords = 512;
constexpr std::size_t kEventWords = 5;
constexpr std::uint32_t kStorageStep = 0x9E3779B9;
constexpr std::uint32_t kOrderingStep = 0x01000193;
constexpr std::uint32_t kViewMatrix = 0x1F800014;
constexpr std::uint32_t kFailed = 0xFFFFFFFF;
using Words = std::array<std::uint32_t, kInputWords>;
using Event = std::array<std::uint32_t, kEventWords>;

std::int16_t Half(std::uint32_t value) { return static_cast<std::int16_t>(value); }

class World final : public b::ActorMarkerWorld {
public:
    explicit World(const Words& words)
        : words_(words)
    {
        cursor_ = words[7];
        first_ = words[7];
        base_ = words[8];
        for (std::size_t index = 0; index < kStorageWords; ++index) {
            storage_[index] =
                words[10] ^ static_cast<std::uint32_t>(index * kStorageStep);
        }
        for (const int slot : CandidateSlots()) {
            ordering_[slot] = words[9] + static_cast<std::uint32_t>(slot) * kOrderingStep;
        }
    }

    std::optional<b::ProjectedMarkerAnchor> ProjectActorAnchor(int actor) override
    {
        Emit({ 1, static_cast<std::uint32_t>(actor), 251, 0 });
        if (static_cast<std::size_t>(actor) >= kActors || Actor(actor, 0) == 0) {
            return std::nullopt;
        }
        return b::ProjectedMarkerAnchor { .screen = { .x = Half(Actor(actor, 1)),
                                              .y = Half(Actor(actor, 2)) },
            .depth = Half(Actor(actor, 3)) };
    }
    std::optional<b::MarkerWorldPoint> ActorAnchorPosition(int actor) override
    {
        Emit({ 2, static_cast<std::uint32_t>(actor), 255, 0 });
        if (static_cast<std::size_t>(actor) >= kActors || Actor(actor, 4) == 0) {
            return std::nullopt;
        }
        return b::MarkerWorldPoint { .x = Half(Actor(actor, 5)),
            .y = Half(Actor(actor, 6)),
            .z = Half(Actor(actor, 7)) };
    }
    b::MarkerScreenPoint ProjectThroughView(b::MarkerWorldPoint point) override
    {
        Emit({ 3, kViewMatrix });
        Emit({ 4, kViewMatrix });
        Emit({ 5, static_cast<std::uint32_t>(point.x),
            static_cast<std::uint32_t>(point.y), static_cast<std::uint32_t>(point.z) });
        const std::uint32_t screen = Actor(static_cast<int>(views_ % kActors), 9);
        ++views_;
        return b::UnpackMarkerPoint(screen);
    }
    int GimmickSubtype(int actor) override
    {
        Emit({ 6, static_cast<std::uint32_t>(actor) });
        return static_cast<std::int32_t>(
            static_cast<std::size_t>(actor) < kActors ? Actor(actor, 8) : kFailed);
    }
    bool DrawSprite(const b::ActorMarkerSprite& sprite) override
    {
        constexpr std::uint32_t kBrightness = 0x80;
        constexpr std::uint32_t kColor = 0x64808080;
        const std::uint32_t chain = Chain(sprite.ordering_slot);
        const std::uint32_t xy = b::PackMarkerPoint(sprite.position);
        const std::uint32_t wh = b::PackMarkerSize(sprite.size);
        Emit({ 7, kBrightness, xy, wh, chain });
        std::uint32_t& link = ordering_[sprite.ordering_slot];
        const std::array<std::uint32_t, 6> packet { (link & 0xFFFFFF) | 0x04000000,
            sprite.draw_mode.value_or(0), kColor, xy,
            b::PackMarkerTexture(sprite.texture), wh };
        return Write(packet, link);
    }
    bool DrawFrame(const b::ActorMarkerFrame& frame) override
    {
        std::uint32_t& link = ordering_[frame.ordering_slot];
        return Write(b::BuildActorMarkerFramePacket(frame, link), link);
    }
    bool DrawArrow(const b::OffscreenActorArrow& arrow) override
    {
        Emit({ 8, Chain(b::kOffscreenArrowOrderingSlot), cursor_ });
        std::uint32_t& link = ordering_[b::kOffscreenArrowOrderingSlot];
        b::MarkerFramePacket packet { };
        const std::size_t at = (cursor_ - first_) / 4;
        for (std::size_t index = 0; index < packet.size(); ++index) {
            packet[index] = storage_.at(at + index);
        }
        b::WriteOffscreenArrowPacket(packet, arrow, link);
        return Write(packet, link);
    }

    void Print(const b::ActorMarkerState& state, const b::AbilityTimingCue& cue,
        std::uint32_t pad) const
    {
        for (const auto kind : state.kinds)
            Word(kind);
        Word(state.sides);
        Word(static_cast<std::uint16_t>(cue.frames));
        Word(static_cast<std::uint16_t>(cue.anchor.screen.x));
        Word(static_cast<std::uint16_t>(cue.anchor.screen.y));
        Word(static_cast<std::uint16_t>(cue.anchor.depth));
        Word(pad & 0xFFFF);
        Word(cursor_);
        for (const int slot : CandidateSlots())
            Word(ordering_.at(slot));
        Word(events_.size());
        for (const auto& event : events_) {
            for (const auto value : event)
                Word(value);
        }
        const std::size_t used = (cursor_ - first_) / 4;
        Word(used);
        for (std::size_t index = 0; index < used; ++index)
            Word(storage_.at(index));
        std::cout << '\n';
    }

private:
    const Words& words_;
    std::array<std::uint32_t, kStorageWords> storage_ { };
    std::map<int, std::uint32_t> ordering_;
    std::vector<Event> events_;
    std::uint32_t cursor_ = 0;
    std::uint32_t first_ = 0;
    std::uint32_t base_ = 0;
    std::size_t views_ = 0;

    static void Word(std::uint64_t value)
    {
        std::cout << static_cast<std::uint32_t>(value) << ' ';
    }
    [[nodiscard]] std::uint32_t Actor(int actor, std::size_t field) const
    {
        return words_.at(
            kFirstActor + static_cast<std::size_t>(actor) * kActorWords + field);
    }
    [[nodiscard]] std::vector<int> CandidateSlots() const
    {
        std::vector<int> slots { -4, 0 };
        for (std::size_t actor = 0; actor < kActors; ++actor) {
            const int depth = Half(Actor(static_cast<int>(actor), 3)) - 16;
            slots.push_back(depth < 64 ? 64 : (depth > 2047 ? 2047 : depth));
        }
        return slots;
    }
    [[nodiscard]] std::uint32_t Chain(int slot) const
    {
        return base_ + static_cast<std::uint32_t>(slot) * 4;
    }
    void Emit(std::initializer_list<std::uint32_t> values)
    {
        Event event { };
        std::size_t index = 0;
        for (const auto value : values)
            event.at(index++) = value;
        events_.push_back(event);
    }
    bool Write(std::span<const std::uint32_t> packet, std::uint32_t& link)
    {
        const std::size_t at = (cursor_ - first_) / 4;
        for (std::size_t index = 0; index < packet.size(); ++index) {
            storage_.at(at + index) = packet[index];
        }
        // The packet's address is cursor_ (KSEG0); links keep the low 24 bits.
        link = cursor_ & 0xFFFFFF;
        cursor_ += static_cast<std::uint32_t>(packet.size() * 4);
        return true;
    }
};

void Run(const Words& words)
{
    World world(words);
    b::ActorMarkerState state;
    for (std::size_t index = 0; index < kActors; ++index) {
        state.kinds.at(index) = static_cast<std::uint8_t>(words.at(15 + index));
    }
    state.sides = words[4];
    state.hidden = Half(words[3]);
    b::AbilityTimingCue cue {
        .anchor = { .screen = { .x = Half(words[11]), .y = Half(words[12]) },
            .depth = Half(words[13]) },
        .frames = Half(words[5])
    };
    const auto argument = static_cast<std::int32_t>(words[1]);
    bool accepted = true;
    switch (words[0]) {
    case 0:
        accepted = b::DrawActorMarker(
            world, state, argument, static_cast<std::int32_t>(words[2]));
        break;
    case 1:
        accepted = b::DrawActorMarkers(world, state);
        break;
    case 2:
        accepted = b::DrawOffscreenActorArrow(
            world, { .x = Half(words[11]), .y = Half(words[12]) }, argument);
        break;
    default:
        b::ShowAbilityTimingCue(world, (words[6] & 0x20) != 0, cue, argument);
        break;
    }
    if (!accepted) {
        std::cout << "refused\n";
        return;
    }
    world.Print(state, cue, words[14]);
}
} // namespace

int main()
{
    while (true) {
        Words words { };
        for (auto& word : words) {
            if (!(std::cin >> word))
                return std::cin.eof() ? 0 : 1;
        }
        Run(words);
    }
}
