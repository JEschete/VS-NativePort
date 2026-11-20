#pragma once
// The two blocks an actor is allocated in: the auxiliary core (the actor, its
// stats, its model and eight matrices) and the full allocation, which adds
// the actor record's room and the two equipment model slots.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Actors/ActorModelSlot.hpp"
#include "Actors/ModelInstance.hpp"
#include "Battle/Actor.hpp"
#include "Core/Gte.hpp"

namespace VS::Actors {
namespace layout {
inline constexpr std::size_t kActorCoreMatrices = 8;
inline constexpr std::size_t kActorAllocationSlots = 2;
// The PS1 slot after the core held 0x1788 bytes; the native actor record that
// starts at core.model is larger, so the slot grows by 80 bytes.
inline constexpr std::size_t kActorRecordRoomBytes = 0x1788 + 80;
}  // namespace layout

// decomp: the unnamed unk0 member of Actor_dat2
// (src/BATTLE/BATTLE.PRG/actor.h:152-155)
struct ActorAndStats {
  VS::Battle::Actor actor;
  VS::Battle::ActorStats stats;
};

// decomp: Actor_dat2 (src/BATTLE/BATTLE.PRG/actor.h:151-162)
struct ActorCore {
  ActorAndStats actor;
  VS::Actors::ModelInstance model;
  std::array<VS::Core::Gte::MATRIX, layout::kActorCoreMatrices> reserved_6c;
  // The model's object header word (0x16C); bits 0-2 hold its object kind.
  std::int32_t object_word;
  std::int32_t reserved_170;
  std::int32_t reserved_174;
};

// decomp: Actor_dat (src/BATTLE/BATTLE.PRG/actor.h:164-168)
struct ActorAllocation {
  ActorCore core;
  std::array<std::uint8_t, layout::kActorRecordRoomBytes> reserved_b2c;
  std::array<ActorModelSlot, layout::kActorAllocationSlots> model_slots;
};
}  // namespace VS::Actors
