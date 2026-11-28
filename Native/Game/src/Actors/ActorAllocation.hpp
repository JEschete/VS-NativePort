#pragma once
// Native instance storage. Definitions and generations live in
// ActorInstanceTable; these allocations own only mutable components.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Actors/ActorModelSlot.hpp"
#include "Actors/ActorRecord.hpp"
#include "Battle/Actor.hpp"
#include "Field/Gimmicks.hpp"

namespace VS::Actors {
namespace layout {
inline constexpr std::size_t kActorAllocationSlots = 2;
}  // namespace layout

// decomp: the unnamed unk0 member of Actor_dat2
// (src/BATTLE/BATTLE.PRG/actor.h:152-155)
struct ActorAndStats {
  VS::Battle::Actor actor;
  VS::Battle::ActorStats stats;
};

// decomp: Actor_dat2 (src/BATTLE/BATTLE.PRG/actor.h:152-162)
struct ActorCore {
  ActorAndStats actor;
  Field::GimmickInstance gimmick;
};

// decomp: Actor_dat (src/BATTLE/BATTLE.PRG/actor.h:164-168)
struct ActorAllocation {
  ActorAndStats actor;
  ActorRecord body;
  std::array<ActorModelSlot, layout::kActorAllocationSlots> model_slots;
};

struct SecondaryBodyAllocation {
  ActorAndStats actor;
  ActorRecord body;
  ActorRecord second_body;
};
}  // namespace VS::Actors
