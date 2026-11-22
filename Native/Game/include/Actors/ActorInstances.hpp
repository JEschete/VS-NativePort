#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

// Stable actor identity and ownership (#55). The original addressed an actor
// only by its slot in vs_battle_actors, and its removal (removeActorById,
// 146C.c:582) chose what to release from the actor's kind, a field scripts
// rewrite. Natively each allocation gets a generation-checked SpawnedActor and
// an immutable ActorDefinition when its storage is allocated. The instance
// table, not the mutable actor record, says which slot resources the actor
// owns, and removal claims them once.
namespace VS::Actors {
// The slots room actors, scripted actors and gimmicks use.
inline constexpr std::size_t kSpawnActorSlots = 16;
// The second body's model takes model slot actor + 4.
inline constexpr std::size_t kSecondBodySlotOffset = 4;

// An actor allocation the host owns, before and after it joins its slot.
// The low byte stores slot + 1; the upper bits store a nonzero generation.
// This token never contains the actor's address.
enum class SpawnedActor : std::uint64_t { kNone = 0 };

enum class ActorShape : std::uint8_t {
  // A gimmick actor: the actor, its stats and its model (allocation kind 0).
  kAuxiliary,
  // An actor with weapon and shield model slots (allocation kind 1).
  kEquipped,
  // An enemy whose second body takes the weapon model slot (kind 2).
  kSecondaryBody,
};

// The slot resources an actor owns; removing the actor releases each once.
struct ActorResources {
  // The gimmick instance in the actor's slot.
  bool gimmick = false;
  // The model in the actor's slot.
  bool model = false;
  // The second body's model, kSecondBodySlotOffset slots on.
  bool second_model = false;

  friend constexpr bool operator==(const ActorResources&,
                                   const ActorResources&) = default;
};

// What an actor is, fixed when its storage is allocated.
struct ActorDefinition {
  ActorShape shape = ActorShape::kEquipped;

  // A gimmick actor owns its slot's gimmick instance; any other actor owns its
  // slot's model, and a second body also owns the model four slots on. The
  // original tested the actor's kind (0x10) and allocation kind (2) instead.
  [[nodiscard]] constexpr ActorResources Resources() const {
    switch (shape) {
      case ActorShape::kAuxiliary:
        return {.gimmick = true};
      case ActorShape::kEquipped:
        return {.model = true};
      case ActorShape::kSecondaryBody:
        return {.model = true, .second_model = true};
    }
    return {};
  }

  friend constexpr bool operator==(const ActorDefinition&,
                                   const ActorDefinition&) = default;
};

// The actor instances of the spawn slots: each slot's allocation, generation,
// definition and removal state. An address is retained only in this table and
// is never part of a handle.
class ActorInstanceTable {
 public:
  // Records a new allocation of `definition` for `slot`, pending until
  // Publish; kNone when the slot already has a live or pending instance.
  [[nodiscard]] SpawnedActor Begin(std::size_t slot, std::uintptr_t address,
                                   ActorDefinition definition) {
    if (slot >= entries_.size() || address == 0) {
      return SpawnedActor::kNone;
    }
    Entry& entry = entries_[slot];
    if (entry.address != 0) {
      return SpawnedActor::kNone;
    }
    Advance(entry);
    entry.address = address;
    entry.installed = false;
    entry.definition = definition;
    entry.removing = false;
    return Encode(slot, entry.generation);
  }

  // Reconciles an externally changed actor-table slot. A null slot invalidates
  // an installed actor but leaves a pending, unpublished allocation intact. An
  // address the table did not allocate becomes an instance without a
  // definition.
  [[nodiscard]] SpawnedActor Observe(std::size_t slot, std::uintptr_t address) {
    if (slot >= entries_.size()) {
      return SpawnedActor::kNone;
    }
    Entry& entry = entries_[slot];
    if (address == 0) {
      if (entry.installed) {
        Clear(entry);
      }
      return SpawnedActor::kNone;
    }
    if (entry.address != address) {
      Advance(entry);
      entry.address = address;
      entry.definition.reset();
      entry.removing = false;
    }
    entry.installed = true;
    return Encode(slot, entry.generation);
  }

  [[nodiscard]] bool Publish(std::size_t slot, SpawnedActor actor) {
    Entry* entry = Find(actor, slot);
    if (entry == nullptr || entry->installed) {
      return false;
    }
    entry->installed = true;
    return true;
  }

  [[nodiscard]] std::uintptr_t Resolve(SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr ? entry->address : 0;
  }

  // The definition `actor` was allocated with; nothing for a stale handle or
  // an instance the table only observed.
  [[nodiscard]] std::optional<ActorDefinition> Definition(
      SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr ? entry->definition : std::nullopt;
  }

  // Starts removing the installed `actor` in `slot` and hands over the
  // resources it owns, once. A stale handle, an unpublished allocation or an
  // actor already being removed claims nothing; an observed instance without
  // a definition owns no slot resources.
  [[nodiscard]] std::optional<ActorResources> ClaimRemoval(std::size_t slot,
                                                           SpawnedActor actor) {
    Entry* entry = Find(actor, slot);
    if (entry == nullptr || !entry->installed || entry->removing) {
      return std::nullopt;
    }
    entry->removing = true;
    return entry->definition ? entry->definition->Resources()
                             : ActorResources{};
  }

  [[nodiscard]] bool Discard(SpawnedActor actor) {
    Entry* entry = Find(actor);
    if (entry == nullptr || entry->installed) {
      return false;
    }
    Clear(*entry);
    return true;
  }

  [[nodiscard]] bool Remove(std::size_t slot, SpawnedActor actor) {
    Entry* entry = Find(actor, slot);
    if (entry == nullptr || !entry->installed) {
      return false;
    }
    Clear(*entry);
    return true;
  }

  void Reset() {
    for (Entry& entry : entries_) {
      Clear(entry);
    }
  }

 private:
  static constexpr unsigned kSlotBits = 8;
  static constexpr std::uint64_t kSlotMask =
      (std::uint64_t{1} << kSlotBits) - 1;
  static constexpr std::uint64_t kGenerationMask =
      std::numeric_limits<std::uint64_t>::max() >> kSlotBits;

  struct Entry {
    std::uintptr_t address = 0;
    std::uint64_t generation = 0;
    std::optional<ActorDefinition> definition;
    bool installed = false;
    bool removing = false;
  };

  [[nodiscard]] static constexpr SpawnedActor Encode(std::size_t slot,
                                                     std::uint64_t generation) {
    // The enum's nonzero values encode slot-generation pairs.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<SpawnedActor>((generation << kSlotBits) |
                                     (static_cast<std::uint64_t>(slot) + 1));
  }

  [[nodiscard]] static constexpr bool Decode(SpawnedActor actor,
                                             std::size_t& slot,
                                             std::uint64_t& generation) {
    const std::uint64_t value = static_cast<std::uint64_t>(actor);
    const std::uint64_t encoded_slot = value & kSlotMask;
    generation = value >> kSlotBits;
    if (encoded_slot == 0 || encoded_slot > kSpawnActorSlots ||
        generation == 0) {
      return false;
    }
    slot = static_cast<std::size_t>(encoded_slot - 1);
    return true;
  }

  // The live entry `actor` names, or null for a stale or invalid handle.
  [[nodiscard]] Entry* Find(SpawnedActor actor) {
    std::size_t slot = 0;
    std::uint64_t generation = 0;
    if (!Decode(actor, slot, generation)) {
      return nullptr;
    }
    Entry& entry = entries_[slot];
    return entry.generation == generation && entry.address != 0 ? &entry
                                                                : nullptr;
  }
  [[nodiscard]] const Entry* Find(SpawnedActor actor) const {
    std::size_t slot = 0;
    std::uint64_t generation = 0;
    if (!Decode(actor, slot, generation)) {
      return nullptr;
    }
    const Entry& entry = entries_[slot];
    return entry.generation == generation && entry.address != 0 ? &entry
                                                                : nullptr;
  }
  // As Find, for a handle that must name `slot`.
  [[nodiscard]] Entry* Find(SpawnedActor actor, std::size_t slot) {
    Entry* entry = Find(actor);
    return entry != nullptr && slot < entries_.size() &&
                   entry == &entries_[slot]
               ? entry
               : nullptr;
  }

  static void Advance(Entry& entry) {
    entry.generation =
        entry.generation == kGenerationMask ? 1 : entry.generation + 1;
  }

  static void Clear(Entry& entry) {
    entry.address = 0;
    entry.installed = false;
    entry.definition.reset();
    entry.removing = false;
    Advance(entry);
  }

  std::array<Entry, kSpawnActorSlots> entries_{};
};
}  // namespace VS::Actors
