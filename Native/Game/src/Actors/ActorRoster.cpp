// Hand-maintained (tools/port/owned.csv).
// The room actor roster (BATTLE.PRG 146C.c:884-946, 1842-1864, 6028 and
// 74088.c:1001).
#include "Actors/ActorRoster.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

namespace VS::Actors {
namespace {
// The auxiliary ids whose placeholder does not cancel hit results.
constexpr int kMarkedAuxiliary = 1;
constexpr int kUncancelledAuxiliaryA = 2;
constexpr int kUncancelledAuxiliaryB = 4;
constexpr unsigned kAuxiliaryIdMask = 0xFF;
// An actor with an allocation of kind 2 also owns the model at slot + 4.
constexpr std::size_t kSecondModelOffset = 4;

std::optional<std::size_t> FirstLinkingSlot(const RosterView& view) {
  const auto link = std::ranges::find_if(view, [](const RosterSlotView& slot) {
    return slot.occupied && slot.links_to_actor;
  });
  if (link == view.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(view.begin(), link));
}
}  // namespace

RosterDecision LinkActorAfterAshley(const RosterView& view,
                                    RosterCandidate candidate) {
  if (!candidate.linkable_kind || candidate.leader ||
      FirstLinkingSlot(view).has_value()) {
    return {};
  }
  if (!view.front().occupied) {
    return {.edit = RosterEdit::kRefusedNoLeader, .slot = 0};
  }
  return {.edit = RosterEdit::kInsertAfterLeader, .slot = 0};
}

RosterDecision UnlinkActorFromRoomList(const RosterView& view,
                                       RosterCandidate candidate) {
  if (candidate.leader) {
    return {};
  }
  const std::optional<std::size_t> link = FirstLinkingSlot(view);
  if (!link.has_value()) {
    return {};
  }
  return {.edit = RosterEdit::kBypass, .slot = *link};
}

RemovalResult RemoveActorAtSlot(ActorRemovalHost& host, std::uint32_t index,
                                int hidden) {
  if (index >= kRosterSlots) {
    return RemovalResult::kOutOfRange;
  }
  const std::size_t slot = index;
  const std::optional<RosterOccupant> occupant = host.Occupant(slot);
  if (!occupant.has_value()) {
    return RemovalResult::kEmpty;
  }
  host.Remember(slot, hidden);
  if (occupant->gimmick) {
    host.ReleaseGimmick(slot);
  } else {
    host.ReleaseModel(slot);
    if (occupant->second_model) {
      host.ReleaseModel(slot + kSecondModelOffset);
    }
  }
  host.FreeAiState(slot);
  if (const std::optional<std::size_t> link =
          FirstLinkingSlot(host.Links(slot));
      link.has_value()) {
    host.Bypass(*link, slot);
  }
  host.Free(slot);
  return RemovalResult::kRemoved;
}

void FreezeAllActorAnimations(DisplayFlagsHost& host) {
  for (std::size_t slot = 0; slot < kRosterSlots; ++slot) {
    const std::uint32_t flags = host.StateFlags(slot);
    if ((flags & kAnimationFrozenFlag) == 0) {
      host.SetDisplayFlags(slot, flags | kAnimationFrozenFlag);
    }
  }
}

void UnfreezeAllActorAnimations(DisplayFlagsHost& host) {
  for (std::size_t slot = 0; slot < kRosterSlots; ++slot) {
    const std::uint32_t flags = host.StateFlags(slot);
    if ((flags & kAnimationFrozenFlag) != 0) {
      host.SetDisplayFlags(slot, flags & ~kAnimationFrozenFlag);
    }
  }
}

PlaceholderActorStats BuildPlaceholderActorStats(
    int auxiliary, std::span<const std::uint8_t, kActorNameBytes> name) {
  PlaceholderActorStats stats;
  std::ranges::copy(name, stats.name.begin());
  switch (
      static_cast<int>(static_cast<unsigned>(auxiliary) & kAuxiliaryIdMask)) {
    case kMarkedAuxiliary:
      stats.cancels_hit_results = true;
      stats.marker_flags = kPlaceholderMarkerFlags;
      break;
    case kUncancelledAuxiliaryA:
    case kUncancelledAuxiliaryB:
      stats.cancels_hit_results = false;
      break;
    default:
      stats.cancels_hit_results = true;
      break;
  }
  return stats;
}

std::optional<std::size_t> CombatLimbIndex(int limb) {
  if (limb < 0 || static_cast<std::size_t>(limb) >= kCombatLimbs) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(limb);
}
}  // namespace VS::Actors
