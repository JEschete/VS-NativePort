#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>

#include "Effects/PrimaryEffect.hpp"

namespace {
namespace fx = VS::Effects;

// The modes primary_effect_differential.py compares.
enum Mode : int {
    kCanStart,
    kPlayable,
    kRequeue,
    kIsLoaded,
    kSetPhase,
    kReadPhase,
    kAllocate,
    kRelease,
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

bool ReadState(fx::PrimaryEffectState& state)
{
    std::uint32_t phase = 0;
    if (!Read(phase) || !Read(state.track_count) || !Read(state.started_tracks) || !Read(state.pending_tracks))
        return false;
    state.phase = static_cast<fx::PrimaryEffectPhase>(phase);
    return true;
}

bool ReadPool(fx::EffectTokenPool& pool)
{
    for (auto& word : pool) {
        if (!Read(word))
            return false;
    }
    return true;
}

void PrintPool(const fx::EffectTokenPool& pool)
{
    for (const std::uint32_t word : pool)
        std::cout << ' ' << word;
}

bool Track(int mode)
{
    fx::PrimaryEffectState state;
    if (!ReadState(state))
        return false;
    if (mode == kCanStart) {
        std::cout << (fx::CanStartTrack(state) ? 1 : 0) << '\n';
    } else if (mode == kPlayable) {
        std::cout << fx::PlayableTrackCount(state) << '\n';
    } else {
        int track = 0;
        if (!Read(track))
            return false;
        const bool requeued = fx::RequeueTrack(state, track);
        std::cout << (requeued ? 0 : 1) << ' ' << state.started_tracks << ' ' << state.pending_tracks << '\n';
    }
    return true;
}

bool Phase(int mode)
{
    std::uint32_t stored = 0;
    if (!Read(stored))
        return false;
    fx::PrimaryEffectState state { .phase = static_cast<fx::PrimaryEffectPhase>(stored) };
    if (mode == kReadPhase) {
        std::cout << std::to_underlying(fx::PrimaryEffectPhaseOf(state)) << '\n';
        return true;
    }
    std::uint32_t value = 0;
    if (!Read(value))
        return false;
    const std::optional<fx::PrimaryEffectPhase> phase = fx::ToPrimaryEffectPhase(value);
    if (phase)
        fx::SetPrimaryEffectPhase(state, *phase);
    std::cout << (phase ? 0 : 1) << ' ' << std::to_underlying(state.phase) << '\n';
    return true;
}

bool Tokens(int mode)
{
    fx::EffectTokenPool pool { };
    if (!ReadPool(pool))
        return false;
    if (mode == kAllocate) {
        const std::optional<std::uint8_t> token = fx::AllocateEffectToken(pool);
        std::cout << static_cast<int>(token.value_or(0));
    } else {
        int token = 0;
        if (!Read(token))
            return false;
        std::cout << (fx::ReleaseEffectToken(pool, token) ? 0 : 1);
    }
    PrintPool(pool);
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        bool read = true;
        switch (mode) {
        case kCanStart:
        case kPlayable:
        case kRequeue:
            read = Track(mode);
            break;
        case kIsLoaded: {
            int loaded = 0;
            read = Read(loaded);
            const fx::PrimaryEffectState state { .loaded = loaded != 0 };
            std::cout << (fx::IsPrimaryEffectLoaded(state) ? 1 : 0) << '\n';
            break;
        }
        case kSetPhase:
        case kReadPhase:
            read = Phase(mode);
            break;
        case kAllocate:
        case kRelease:
            read = Tokens(mode);
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
