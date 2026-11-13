#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <string_view>
#include <vector>

#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionAnimationsLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/ActionAnimation.h"
#include "Core/Diagnostics.h"

Actors_ActorRecord* Actors_actorRecords[17];
u_char Battle_chainAbilityAnimationGroups[168];
int Battle_chainAbilityAnimationSide;
char Battle_specialActionPitch;
}

namespace {
namespace b = VS::Battle;
enum class Kind { kHeader, kPlay, kReset, kBone, kQueue };
struct Event {
    Kind kind { };
    int owner { }, first { }, second { }, third { };
    bool operator==(const Event&) const = default;
};
std::array<Actors_ActorRecord, 3> records;
std::vector<Event> events;
std::function<void(Event)> callback;
SVECTOR target { 1024, 0, 0, 77 };
bool header_present = true;
bool bone_success = true;
u_short header;

int Owner(const void* pointer)
{
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (&records.at(i) == pointer)
            return static_cast<int>(i);
    }
    std::exit(86);
}
void Check(bool valid)
{
    if (!valid)
        std::exit(86);
}
void Record(Event event)
{
    events.push_back(event);
    if (callback)
        callback(event);
}
std::vector<Event> Calls(Kind kind)
{
    std::vector<Event> result;
    for (const auto& event : events) {
        if (event.kind == kind)
            result.push_back(event);
    }
    return result;
}
SVECTOR* UnusedPoint()
{
    return reinterpret_cast<SVECTOR*>(static_cast<std::uintptr_t>(1));
}
void Configure()
{
    for (auto& actor : records) {
        actor.instance.slot = 1;
        actor.instance.weaponDrawn = 1;
        actor.instance.turnFrames = 777;
        actor.instance.facingDelta = 333;
        actor.instance.jumpCountdown = 99;
        actor.collisionRadius = 127;
        actor.modelId = 40;
    }
    Actors_actorRecords[16] = &records.at(0);
}
} // namespace

extern "C" void Core_Errorf(const char* channel, const char* format, ...)
{
    std::fprintf(stderr, "%s: ", channel);
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    std::exit(85);
}

namespace VS::Actors {
int LiveActorBonePosition(int actor_id, int bone, SVECTOR* output, int endpoint)
{
    Record({ Kind::kBone, actor_id, bone, endpoint });
    if (!bone_success)
        return -1;
    *output = { };
    return 0;
}
std::intptr_t FindLiveActorAnimationHeader(void* actor, int animation, u_short** output)
{
    const bool present = header_present;
    Record({ Kind::kHeader, Owner(actor), animation });
    *output = present ? &header : nullptr;
    return present ? 1 : 0;
}
int PlayLiveActorAnimation(int slot, int animation, int unused, int blend)
{
    Record({ Kind::kPlay, slot, animation, unused, blend });
    return 0;
}
int QueueLiveActorAnimation(int slot, int animation, int blend, int start)
{
    Record({ Kind::kQueue, slot, animation, blend, start });
    return 0;
}
void ResetLiveActorHeadTrackingSpeed(Actors_ActorRecord* actor)
{
    Record({ Kind::kReset, Owner(actor) });
}
} // namespace VS::Actors

int main(int argc, char** argv)
{
    if (argc != 2)
        return 86;
    Configure();
    const std::string_view name = argv[1];
    auto& first = records.at(0);
    auto& second = records.at(1);
    auto& third = records.at(2);
    if (name == "null-melee" || name == "null-cast" || name == "null-chain"
        || name == "unused-special" || name == "unused-break") {
        Actors_actorRecords[16] = nullptr;
        if (name == "null-melee") {
            b::PlayLiveMeleeAttackAnimation(16, UnusedPoint(), -2147483647);
        } else if (name == "null-cast") {
            b::PlayLiveSpellCastAnimation(16, UnusedPoint(), 0x10000);
        } else if (name == "null-chain") {
            b::PlayLiveChainAbilityAnimation(16, UnusedPoint(), -1, 0x10001);
        } else if (name == "unused-special") {
            b::PlayLiveSpecialActionAnimation(16, UnusedPoint(), 0x10001);
        } else {
            b::PlayLiveBreakArtAnimation(16, UnusedPoint(), 0x10001);
        }
        Check(events.empty());
    } else if (name == "outside-record-table" || name == "negative-record-slot") {
        const int slot = name == "negative-record-slot"
                           ? -1
                           : static_cast<int>(std::size(Actors_actorRecords));
        b::PlayLiveMeleeAttackAnimation(slot, UnusedPoint(), 0);
    } else if (name == "missing-defense" || name == "missing-special") {
        Actors_actorRecords[16] = nullptr;
        if (name == "missing-defense") {
            b::PlayLiveDefenseAnimation(16, UnusedPoint());
        } else {
            b::PlayLiveSpecialActionAnimation(16, UnusedPoint(), 1);
        }
    } else if (name == "invalid-cast" || name == "invalid-chain"
               || name == "physical-not-semantic-bound" || name == "full-word-chain") {
        if (name == "invalid-cast") {
            b::PlayLiveSpellCastAnimation(16, UnusedPoint(), 0x10000);
        } else {
            const int ability = name == "invalid-chain"               ? -1
                              : name == "physical-not-semantic-bound" ? 18
                                                                      : 0x10000;
            b::PlayLiveChainAbilityAnimation(16, UnusedPoint(), ability, 1);
        }
    } else if (name == "unused-defense-point") {
        first.instance.slot = 0;
        first.instance.weaponDrawn = 0;
        first.instance.movementMode = 2;
        first.instance.motionLocked = 1;
        first.instance.isJumping = 1;
        first.instance.reactionState = 9;
        b::PlayLiveDefenseAnimation(16, UnusedPoint());
        Check(events.empty() && first.instance.jumpCountdown == 0
              && first.instance.movementMode == 0 && first.instance.motionLocked
              && first.instance.isJumping && first.instance.reactionState == 9);
    } else if (name == "null-target-melee" || name == "null-target-chain") {
        if (name == "null-target-melee") {
            b::PlayLiveMeleeAttackAnimation(16, nullptr, 0);
            Check(Calls(Kind::kPlay)
                  == std::vector<Event> { { Kind::kPlay, 16, 0x49, 0, 6 } });
        } else {
            Battle_chainAbilityAnimationSide = -1;
            b::PlayLiveChainAbilityAnimation(16, nullptr, 17, 0x10001);
            Check(Calls(Kind::kPlay)
                  == std::vector<Event> { { Kind::kPlay, 16, 202, 0, 8 } });
            Check(Battle_chainAbilityAnimationSide == -2);
        }
        Check(first.instance.facingDelta == 0 && Calls(Kind::kBone).empty());
    } else if (name == "negative-root-chain" || name == "negative-root-melee") {
        first.instance.position.vx = -32768;
        first.instance.position.vz = -32768;
        target = { };
        if (name == "negative-root-chain") {
            b::PlayLiveChainAbilityAnimation(16, &target, 0, 1);
        } else {
            b::PlayLiveMeleeAttackAnimation(16, &target, 0);
        }
    } else if (name == "missing-bone-chain" || name == "missing-bone-special") {
        bone_success = false;
        if (name == "missing-bone-chain") {
            b::PlayLiveChainAbilityAnimation(16, &target, 0, 1);
        } else {
            b::PlayLiveSpecialActionAnimation(16, &target, 1);
        }
    } else if (name == "melee-current-model") {
        first.modelId = 2;
        target.vy = 5000;
        callback = [&](Event event) {
            if (event.kind == Kind::kBone) {
                first.modelId = 40;
                Actors_actorRecords[16] = &second;
            }
        };
        b::PlayLiveMeleeAttackAnimation(16, &target, 0);
        Check(
            Calls(Kind::kPlay) == std::vector<Event> { { Kind::kPlay, 16, 0x4E, 0, 6 } });
        Check(Calls(Kind::kReset) == std::vector<Event> { { Kind::kReset, 0 } });
        Check(second.instance.turnFrames == 777 && second.instance.facingDelta == 333);
    } else if (name == "defense-current-move-two"
               || name == "defense-current-move-three") {
        first.instance.reactionState = 13;
        first.instance.isVisible = 1;
        callback = [&](Event event) {
            if (event.kind == Kind::kReset) {
                first.instance.slot = 0;
                first.instance.movementMode =
                    name == "defense-current-move-three" ? 3 : 2;
                first.instance.jumpCountdown = 199;
                Actors_actorRecords[16] = &second;
            }
        };
        b::PlayLiveDefenseAnimation(16, &target);
        Check(first.instance.movementMode
              == (name == "defense-current-move-three" ? 3 : 0));
        Check(first.instance.jumpCountdown == 0 && first.instance.reactionState == 13
              && first.instance.isVisible == 1 && first.instance.weaponDrawn == 1);
        Check(second.instance.jumpCountdown == 99);
    } else if (name == "cast-fresh-drawn") {
        first.instance.weaponDrawn = 0;
        header_present = false;
        callback = [&](Event event) {
            if (event.kind == Kind::kHeader) {
                first.instance.weaponDrawn = 1;
                Actors_actorRecords[16] = &second;
            }
        };
        b::PlayLiveSpellCastAnimation(16, UnusedPoint(), 1);
        Check(Calls(Kind::kHeader) == std::vector<Event> { { Kind::kHeader, 0, 0x58 } });
        Check(Calls(Kind::kPlay)
              == std::vector<Event> { { Kind::kPlay, 16, 0x59 + 100, 0, 6 } });
        Check(Calls(Kind::kReset) == std::vector<Event> { { Kind::kReset, 0 } });
    } else if (name == "special-radius-128") {
        first.collisionRadius = 128;
        b::PlayLiveSpecialActionAnimation(16, nullptr, 0);
        Check(first.instance.turnFrames == 20);
        Check(
            Calls(Kind::kPlay) == std::vector<Event> { { Kind::kPlay, 16, 0xBF, 0, 8 } });
    } else if (name == "special-fallback-replacement") {
        header_present = false;
        target.vy = 5000;
        second.collisionRadius = 128;
        callback = [&](Event event) {
            if (event.kind == Kind::kHeader)
                Actors_actorRecords[16] = &second;
        };
        b::PlayLiveSpecialActionAnimation(16, &target, 1);
        Check(first.instance.turnFrames == 777 && second.instance.turnFrames == 10);
        Check(
            Calls(Kind::kPlay) == std::vector<Event> { { Kind::kPlay, 16, 0x4A, 0, 6 } });
        Check(Calls(Kind::kReset) == std::vector<Event> { { Kind::kReset, 1 } });
    } else if (name == "special-retains-original") {
        target.vy = 1900;
        callback = [&](Event event) {
            if (event.kind == Kind::kHeader) {
                first.instance.slot = 5;
                Actors_actorRecords[16] = &second;
            }
        };
        b::PlayLiveSpecialActionAnimation(16, &target, 1);
        Check(Calls(Kind::kBone) == std::vector<Event> { { Kind::kBone, 5, 0, 0 } });
        Check(Battle_specialActionPitch == 2 && Calls(Kind::kReset).empty());
    } else if (name == "fallback-null" || name == "follow-up-null") {
        header_present = false;
        callback = [](Event event) {
            if (event.kind == Kind::kHeader)
                Actors_actorRecords[16] = nullptr;
        };
        if (name == "fallback-null") {
            b::PlayLiveSpecialActionAnimation(16, UnusedPoint(), 1);
            Check(Calls(Kind::kPlay).empty() && Calls(Kind::kHeader).size() == 1);
        } else {
            b::PlayLiveBreakArtAnimation(16, UnusedPoint(), 1);
        }
    } else if (name == "break-followup-reload" || name == "break-fallback-reload"
               || name == "break-fe-missing") {
        if (name == "break-fallback-reload")
            header_present = false;
        callback = [&](Event event) {
            if (event.kind == Kind::kHeader && event.first == 0xFF
                && name == "break-fallback-reload") {
                Actors_actorRecords[16] = &second;
                header_present = true;
            }
            if (event.kind == Kind::kPlay) {
                Actors_actorRecords[16] = &third;
                if (name == "break-fe-missing")
                    header_present = false;
            }
        };
        b::PlayLiveBreakArtAnimation(16, nullptr, 1);
        Check(Calls(Kind::kHeader)
              == std::vector<Event> {
                  { Kind::kHeader, 0, 0xFF }, { Kind::kHeader, 2, 0xFE } });
        if (name == "break-fe-missing") {
            Check(Calls(Kind::kQueue).empty());
        } else {
            Check(Calls(Kind::kQueue)
                  == std::vector<Event> { { Kind::kQueue, 16, 0xFE, 8, 0 } });
        }
        if (name == "break-fallback-reload") {
            Check(Calls(Kind::kReset) == std::vector<Event> { { Kind::kReset, 1 } });
        }
    } else if (name == "aliased-target-after-bone") {
        first.instance.position = { };
        first.instance.facing = 30000;
        callback = [&](Event event) {
            if (event.kind == Kind::kBone)
                first.instance.position.vy = -1500;
        };
        b::PlayLiveChainAbilityAnimation(16, &first.instance.position, 0, 0);
        Check(
            Calls(Kind::kPlay) == std::vector<Event> { { Kind::kPlay, 16, 203, 0, 8 } });
        Check(first.instance.facingDelta == 0);
    } else {
        return 86;
    }
    return 0;
}
