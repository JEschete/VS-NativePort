// Native side of tools\port\differentials\secondary_effects_*.py (F00058, F00060, F00085,
// F00086, F00092): one vector per stdin line, one line of integers per vector, in the
// order secondary_effects_common.py records the original's calls.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Effects/SecondaryEffectRequests.hpp"

namespace {
namespace fx = VS::Effects;

enum Mode : int { kHit, kImpacts, kGround, kTrap, kLearned };
constexpr int kBeginCall = 1, kBoneCall = 2, kSoundCall = 3;
// The single-target request's count and other count, which the live binding writes.
constexpr int kCount = 1, kOtherCount = 0;

long long Next(std::istringstream& in)
{
    long long value = 0;
    if (!(in >> value))
        std::abort();
    return value;
}

fx::ImpactTarget ReadTarget(std::istringstream& in)
{
    fx::ImpactTarget target;
    target.actor = static_cast<std::uint8_t>(Next(in));
    target.limb = static_cast<std::int8_t>(Next(in));
    target.threshold = static_cast<std::uint8_t>(Next(in));
    target.impact = static_cast<std::uint8_t>(Next(in));
    target.hp = static_cast<std::int16_t>(Next(in));
    target.mp = static_cast<std::int16_t>(Next(in));
    target.at_position = Next(in) != 0;
    return target;
}

class World final : public fx::ActionImpactWorld {
public:
    World(int mode)
        : mode_(mode)
    {
    }
    fx::QueuedEffectAction action { };
    std::uint32_t prerequisite_word = 0;
    std::vector<std::int32_t> bone_tags;
    std::vector<fx::ImpactTarget> targets;
    std::uint32_t busy = 0;
    std::vector<std::vector<long long>> calls;

    void Begin(
        const fx::SingleTargetEffectRequest& request, std::int32_t parameter) override
    {
        const auto& source = request.source;
        const auto& target = request.target;
        std::vector<long long> call { kBeginCall, parameter, request.effect_id, kCount,
            kOtherCount, static_cast<long long>(source.kind) };
        if (mode_ == kHit || mode_ == kImpacts) {
            call.insert(call.end(),
                { source.actor, source.part, static_cast<long long>(target.kind),
                    target.color_preset });
            if (target.kind == fx::EffectEndpointKind::kActor)
                call.insert(call.end(), { target.actor, target.slot, target.part });
            else
                call.insert(call.end(), { target.position.x, target.position.y,
                                            target.position.z, target.position.w });
        } else if (mode_ == kGround || mode_ == kTrap) {
            call.insert(call.end(),
                { source.actor, static_cast<long long>(target.kind), target.position.x,
                    target.position.y, target.position.z });
        } else {
            call.insert(call.end(),
                { static_cast<long long>(target.kind), target.actor, target.part });
        }
        calls.push_back(call);
    }
    std::uint32_t BusyFlags() override { return busy; }
    std::optional<fx::QueuedEffectAction> QueuedAction() override { return action; }
    std::optional<fx::ImpactTarget> QueuedTarget(std::size_t index) override
    {
        if (index >= targets.size())
            std::abort();
        return targets[index];
    }
    std::uint8_t HitPrerequisite(std::uint16_t) override
    {
        return static_cast<std::uint8_t>((prerequisite_word >> 7) & 0x3F);
    }
    std::int32_t BoneTag(std::uint8_t actor, std::int8_t limb, std::int32_t kind) override
    {
        calls.push_back({ kBoneCall, actor, limb, kind });
        if (next_tag_ >= bone_tags.size())
            return 0;
        return bone_tags[next_tag_++];
    }
    void PlayWeaponHitSounds(std::int32_t attacker) override
    {
        calls.push_back({ kSoundCall, attacker });
    }

private:
    int mode_;
    std::size_t next_tag_ = 0;
};
} // namespace

int main(int, char**)
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        const int mode = static_cast<int>(Next(in));
        World world(mode);
        std::vector<long long> values;
        if (mode == kHit || mode == kImpacts) {
            world.action.action_id = static_cast<std::uint16_t>(Next(in));
            world.action.effect_variant = static_cast<std::uint8_t>(Next(in));
            world.action.source_actor = static_cast<std::uint8_t>(Next(in));
            world.prerequisite_word = static_cast<std::uint32_t>(Next(in));
        }
        if (mode == kHit) {
            world.bone_tags.push_back(static_cast<std::int32_t>(Next(in)));
            // The live entry narrows the effect id and limb arguments as the original's
            // stores do.
            const auto effect = static_cast<std::uint16_t>(Next(in));
            const auto limb = static_cast<std::uint8_t>(Next(in));
            const fx::ImpactTarget target = ReadTarget(in);
            world.action.target_count = 1;
            if (!fx::QueueActionHitEffect(effect, target, limb, world))
                std::abort();
        } else if (mode == kImpacts) {
            const auto attacker = static_cast<std::int32_t>(Next(in));
            const auto count = static_cast<std::size_t>(Next(in));
            world.action.target_count = static_cast<std::uint16_t>(count);
            for (std::size_t i = 0; i < count; ++i)
                world.bone_tags.push_back(static_cast<std::int32_t>(Next(in)));
            for (std::size_t i = 0; i < count; ++i)
                world.targets.push_back(ReadTarget(in));
            values.push_back(fx::QueueActionImpactEffects(attacker, world));
        } else if (mode == kGround) {
            const auto x = static_cast<std::int16_t>(Next(in));
            const auto z = static_cast<std::int16_t>(Next(in));
            fx::SpawnGroundPlaneImpactEffect(x, z, world);
        } else if (mode == kTrap) {
            fx::TrapTile tile;
            tile.tile_x = static_cast<std::uint8_t>(Next(in));
            tile.tile_z = static_cast<std::uint8_t>(Next(in));
            tile.height = static_cast<std::int16_t>(Next(in));
            world.busy = static_cast<std::uint32_t>(Next(in));
            static_cast<void>(fx::SpawnTrapTileEffect(tile, world));
        } else {
            fx::SpawnAbilityLearnedEffect(world);
        }
        values.push_back(static_cast<long long>(world.calls.size()));
        for (const auto& call : world.calls)
            values.insert(values.end(), call.begin(), call.end());
        for (std::size_t i = 0; i < values.size(); ++i)
            std::cout << (i ? " " : "") << values[i];
        std::cout << '\n';
    }
}
