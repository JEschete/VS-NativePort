#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>

#include "Actors/ActorInstances.hpp"
#include "Battle/ActorVitals.hpp"
#include "Battle/CombatAction.hpp"
#include "Battle/StatusLifecycle.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Battle {
struct CombatNumberEvent {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  int limb{};
  std::uint32_t amount{};
  int label{};
  bool stat_change = true;
  friend bool operator==(const CombatNumberEvent&,
                         const CombatNumberEvent&) = default;
};

struct CombatStatusEvent {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  std::uint32_t statuses{};
  bool removing{};
  friend bool operator==(const CombatStatusEvent&,
                         const CombatStatusEvent&) = default;
};

struct CombatEquipmentEvent {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  int label{};
  int stat{};
  std::uint32_t amount{};
  friend bool operator==(const CombatEquipmentEvent&,
                         const CombatEquipmentEvent&) = default;
};

struct CombatEquipmentFlush {
  friend bool operator==(const CombatEquipmentFlush&,
                         const CombatEquipmentFlush&) = default;
};

struct CombatMovementChange {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  std::uint8_t walk{}, run{};
  friend bool operator==(const CombatMovementChange&,
                         const CombatMovementChange&) = default;
};

// clang-format's alias continuation disagrees with cpplint.
// NOLINTBEGIN
using CombatImpactEvent =
    std::variant<CombatNumberEvent, CombatStatusEvent, CombatEquipmentEvent,
                 CombatEquipmentFlush, CombatMovementChange>;
// NOLINTEND

struct CombatImpactInputs {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  std::uint8_t slot{};
  const ActorVitalsState& state;
  const StatusDurations& durations;
  Items::ItemRandom& random;
};

struct CombatImpactResult {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  ActorVitalsState state{};
  std::vector<CombatImpactEvent> events;
};

// Apply at impact time, using the current owned target state, not the
// calculation-time snapshot. Nothing is published on refusal.
[[nodiscard]] std::expected<CombatImpactResult, std::string> ApplyCombatImpact(
    const CombatHitResult& hit, const CombatImpactInputs& inputs);
}  // namespace VS::Battle
