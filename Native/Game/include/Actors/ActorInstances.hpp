#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace VS::Battle {
struct ActorStats;
}
namespace VS::Battle::Ai {
struct ActorAiState;
}
namespace VS::Field {
struct GimmickInstance;
}

// Stable actor identity and ownership (#55). The original addressed an actor
// only by its slot in vs_battle_actors, and its removal (removeActorById,
// 146C.c:582) chose what to release from the actor's kind, a field scripts
// rewrite. Natively each allocation gets a generation-checked SpawnedActor and
// an immutable ActorDefinition when its storage is allocated. The instance
// table, not the mutable actor record, says which slot resources the actor
// owns, and removal claims them once.
namespace VS::Actors {
struct ActorRecord;
struct ActorModelSlot;
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
  // An enemy with an independently owned second body (allocation kind 2).
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
  // The source model chosen at creation, not the instance's current model
  // after a script or equipment reload.
  std::uint16_t model_id = 0;

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

// Component addresses are fixed at registration, independently of the
// mutable actor's compatibility views.
struct ActorInstanceComponents {
  Battle::ActorStats* stats = nullptr;
  ActorRecord* body = nullptr;
  ActorRecord* second_body = nullptr;
  Field::GimmickInstance* gimmick = nullptr;
  std::array<ActorModelSlot*, 2> equipment{};
};

// The actor instances of the spawn slots: each slot's allocation, generation,
// definition and removal state. An address is retained only in this table and
// is never part of a handle.
class ActorInstanceTable {
 public:
  // Records a new allocation of `definition` for `slot`, pending until
  // Publish; kNone when the slot already has a live or pending instance.
  [[nodiscard]] SpawnedActor Begin(std::size_t slot, std::uintptr_t address,
                                   ActorDefinition definition,
                                   bool owns_storage = false,
                                   ActorInstanceComponents components = {}) {
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
    entry.owns_storage = owns_storage;
    entry.components = components;
    return Encode(slot, entry.generation);
  }

  // Lookup never creates, replaces or retires an identity.
  [[nodiscard]] SpawnedActor AllocatedAt(std::size_t slot) const {
    if (slot >= entries_.size() || entries_[slot].address == 0) {
      return SpawnedActor::kNone;
    }
    return Encode(slot, entries_[slot].generation);
  }

  [[nodiscard]] SpawnedActor At(std::size_t slot) const {
    if (slot >= entries_.size()) {
      return SpawnedActor::kNone;
    }
    const Entry& entry = entries_[slot];
    return entry.installed ? Encode(slot, entry.generation)
                           : SpawnedActor::kNone;
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

  [[nodiscard]] bool Installed(SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr && entry->installed;
  }

  [[nodiscard]] bool Removing(SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr && entry->removing;
  }

  [[nodiscard]] bool OwnsStorage(SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr && entry->owns_storage;
  }

  [[nodiscard]] std::optional<ActorInstanceComponents> Components(
      SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr ? std::optional(entry->components) : std::nullopt;
  }

  [[nodiscard]] Battle::Ai::ActorAiState* AiState(SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr ? entry->ai_state : nullptr;
  }

  [[nodiscard]] bool BindAiState(SpawnedActor actor,
                                 Battle::Ai::ActorAiState* state) {
    Entry* entry = Find(actor);
    if (entry == nullptr || !entry->installed || entry->removing ||
        entry->ai_state != nullptr || state == nullptr) {
      return false;
    }
    entry->ai_state = state;
    return true;
  }

  [[nodiscard]] bool UnbindAiState(SpawnedActor actor,
                                   Battle::Ai::ActorAiState* expected) {
    Entry* entry = Find(actor);
    if (entry == nullptr || expected == nullptr ||
        entry->ai_state != expected) {
      return false;
    }
    entry->ai_state = nullptr;
    return true;
  }

  void ResetAiBindings() {
    for (Entry& entry : entries_) entry.ai_state = nullptr;
  }

  // The unpublished allocation at `address`; kNone when no pending instance
  // has it. This changes nothing.
  [[nodiscard]] SpawnedActor Pending(std::uintptr_t address) const {
    if (address == 0) {
      return SpawnedActor::kNone;
    }
    for (std::size_t slot = 0; slot < entries_.size(); ++slot) {
      const Entry& entry = entries_[slot];
      if (entry.address == address && !entry.installed) {
        return Encode(slot, entry.generation);
      }
    }
    return SpawnedActor::kNone;
  }

  // The definition `actor` was allocated with; nothing for a stale handle.
  [[nodiscard]] std::optional<ActorDefinition> Definition(
      SpawnedActor actor) const {
    const Entry* entry = Find(actor);
    return entry != nullptr ? std::optional(entry->definition) : std::nullopt;
  }

  // The slot `actor` names; nothing for a stale or invalid handle.
  [[nodiscard]] std::optional<std::size_t> Slot(SpawnedActor actor) const {
    std::size_t slot = 0;
    std::uint64_t generation = 0;
    if (Find(actor) == nullptr || !Decode(actor, slot, generation)) {
      return std::nullopt;
    }
    return slot;
  }

  // Starts removing the installed `actor` in `slot` and hands over the
  // resources it owns, once. A stale handle, an unpublished allocation or an
  // actor already being removed claims nothing.
  [[nodiscard]] std::optional<ActorResources> ClaimRemoval(std::size_t slot,
                                                           SpawnedActor actor) {
    Entry* entry = Find(actor, slot);
    if (entry == nullptr || !entry->installed || entry->removing) {
      return std::nullopt;
    }
    entry->removing = true;
    return entry->definition.Resources();
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
    ActorDefinition definition;
    bool installed = false;
    bool removing = false;
    bool owns_storage = false;
    ActorInstanceComponents components;
    Battle::Ai::ActorAiState* ai_state = nullptr;
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
    entry.definition = {};
    entry.removing = false;
    entry.owns_storage = false;
    entry.components = {};
    entry.ai_state = nullptr;
    Advance(entry);
  }

  std::array<Entry, kSpawnActorSlots> entries_{};
};
}  // namespace VS::Actors
